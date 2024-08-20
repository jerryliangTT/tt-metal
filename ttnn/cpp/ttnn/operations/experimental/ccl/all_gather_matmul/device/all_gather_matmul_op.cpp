// SPDX-FileCopyrightText: © 2023 Tenstorrent Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "common/core_coord.h"
#include "ttnn/operations/ccl/all_gather/device/all_gather_op.hpp"
#include "ttnn/deprecated/tt_dnn/op_library/math.hpp"

#include "tt_metal/host_api.hpp"

#include "ttnn/tensor/tensor_utils.hpp"

#include "eth_l1_address_map.h"


#include "ttnn/operations/experimental/ccl/all_gather_matmul/device/all_gather_matmul_op.hpp"

/* All Gather Matmul fusion includes */
#include "ttnn/cpp/ttnn/operations/ccl/all_gather/device/all_gather_op.hpp"
#include "ttnn/cpp/ttnn/operations/matmul/device/matmul_op.hpp"
#include "ttnn/cpp/ttnn/operations/matmul/matmul.hpp"

namespace ttnn {
namespace experimental {

void AllGatherMatmul::validate(const std::vector<Tensor> &input_tensors, const std::vector<std::optional<const ttnn::Tensor>>& optional_input_tensors) const {

    TT_ASSERT(input_tensors.size() == 4, "AllGatherMatmul requires 4 input tensors: [input, weight, all_gather_output, datacopy_output]");

    // All Gather validate
    this->all_gather_struct.validate({input_tensors[0]});

    // Matmul validate.
    this->matmul_struct.validate({input_tensors[1], input_tensors[2]}, optional_input_tensors);
}

std::vector<tt::tt_metal::Shape> AllGatherMatmul::compute_output_shapes(const std::vector<Tensor> &input_tensors) const {

    // All Gather shape
    tt::tt_metal::Shape all_gather_output_shape = this->all_gather_struct.compute_output_shapes({input_tensors[0]})[0];
    tt::tt_metal::Shape datacopy_output_shape = all_gather_output_shape;


    // Matmul shape
    tt::tt_metal::Shape matmul_output_shapes = this->matmul_struct.compute_output_shapes({input_tensors[1], input_tensors[2]})[0];

    return {all_gather_output_shape, matmul_output_shapes, datacopy_output_shape};
}

std::vector<Tensor> AllGatherMatmul::create_output_tensors(const std::vector<Tensor> &input_tensors) const {

    // All Gather output tensor
    auto& all_gather_output_tensor = input_tensors[1]; // this->all_gather_out_tensor = this->all_gather_struct.create_output_tensors(input_tensors)[0];
    auto& datacopy_output_tensor = input_tensors[3]; // this->all_gather_out_tensor = this->all_gather_struct.create_output_tensors(input_tensors)[0];


    // Matmul output tensor
    ttnn::Tensor matmul_output_tensor = this->matmul_struct.create_output_tensors({input_tensors[1], input_tensors[2]})[0];

    return {all_gather_output_tensor, matmul_output_tensor, datacopy_output_tensor};
}

operation::ProgramWithCallbacks AllGatherMatmul::create_program(const std::vector<Tensor> & input_tensors, const std::vector<std::optional<const ttnn::Tensor>>& optional_input_tensors, std::vector<Tensor> &output_tensors) const {

    // Return the AllGatherMatmul program with callbacks
    return all_gather_matmul_multi_core_with_workers(input_tensors[0], output_tensors[0], output_tensors[2], this->all_gather_struct.dim, this->all_gather_struct.num_links, this->all_gather_struct.ring_size, this->all_gather_struct.ring_index, this->all_gather_struct.receiver_device_id, this->all_gather_struct.sender_device_id, this->all_gather_struct.topology, this->all_gather_core_grid_offset);
}

}  // namespace experimental

namespace operations {
namespace experimental {
namespace ccl {

std::vector <ttnn::Tensor> all_gather_matmul(
    const ttnn::Tensor& input_tensor,
    const ttnn::Tensor& weight_tensor,
    const uint32_t dim,
    const CoreCoord all_gather_core_grid_offset,
    const uint32_t num_links,
    const std::optional<MemoryConfig>& memory_config,
    const bool transpose_a,
    const bool transpose_b,
    const std::optional<const DataType> dtype,
    const std::optional<const operations::matmul::MatmulProgramConfig> program_config,
    const std::optional<const std::string>& activation,
    const std::optional<const DeviceComputeKernelConfig> compute_kernel_config,
    const std::optional<const ttnn::CoreGrid> core_grid
) {

    TT_FATAL(std::getenv("TT_METAL_SLOW_DISPATCH_MODE") == nullptr, "AllGatherMatmul is only supported for Fast Dispatch");

    auto devices = input_tensor.get_workers();
    std::vector<Tensor> output_tensors = {ttnn::Tensor(operation::get_workers_for_op_output({input_tensor, weight_tensor})),
                                            ttnn::Tensor(operation::get_workers_for_op_output({input_tensor, weight_tensor})),
                                            ttnn::Tensor(operation::get_workers_for_op_output({input_tensor, weight_tensor}))};
    std::vector<std::optional<const ttnn::Tensor>> optional_input_tensors = {std::nullopt};


    operation::launch_op(
        [dim, all_gather_core_grid_offset, num_links, memory_config, transpose_a, transpose_b, dtype, program_config, activation, compute_kernel_config, core_grid, devices](
            const std::vector<Tensor>& input_tensors,
            const std::vector<std::optional<const ttnn::Tensor>>& optional_input_tensors,
            const std::vector<std::optional<Tensor>>& optional_output_tensors) mutable -> std::vector<Tensor> {

            const auto& input_tensor = input_tensors[0];
            const auto& weight_tensor = input_tensors[1];

            /* AllGather setup */
            ttnn::AllGather all_gather_struct = ttnn::create_all_gather_struct(input_tensor, dim, num_links, memory_config, devices);

            // Create the all gather output tensor used as input (activation) to the matmul
            ttnn::Tensor all_gather_out_tensor = all_gather_struct.create_output_tensors({input_tensor})[0];
            ttnn::Tensor datacopy_out_tensor = all_gather_struct.create_output_tensors({input_tensor})[0];


            /* Matmul setup */
            bool user_run_batched = ttnn::operations::matmul::detail::is_input_batched(weight_tensor.get_shape());
            std::optional<CoreCoord> user_core_coord;
            if (core_grid.has_value()) {
                user_core_coord = CoreCoord(core_grid->x, core_grid->y);
            }

            operations::matmul::Matmul matmul_struct =
                operations::matmul::create_matmul_struct(
                    all_gather_out_tensor,
                    weight_tensor,
                    /*parameters=*/operations::matmul::Matmul{
                        program_config,
                        /*bcast_batch=*/std::nullopt,
                        memory_config.value_or(input_tensor.memory_config()),
                        dtype.value_or(input_tensor.get_dtype()),
                        compute_kernel_config,
                        /*untilize_out=*/false,
                        user_core_coord,
                        ttnn::operations::matmul::get_fused_activation(activation),
                        user_run_batched,
                        transpose_a,
                        transpose_b
                    }
                );

            return operation::run(
                ttnn::experimental::AllGatherMatmul{
                    /* All Gather Params */
                    all_gather_struct,
                    /* Matmul params */
                    matmul_struct,
                    /* Fusion params */
                    all_gather_core_grid_offset},
                {input_tensor, all_gather_out_tensor, weight_tensor, datacopy_out_tensor}, optional_input_tensors);
        },
        {input_tensor, weight_tensor}, output_tensors, optional_input_tensors);
    return {output_tensors[0], output_tensors[1], output_tensors[2]};
}


} // namespace ccl
} // namespace experimental
} // namespace operations

}  // namespace ttnn
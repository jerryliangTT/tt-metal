// SPDX-FileCopyrightText: © 2023 Tenstorrent Inc.
//
// SPDX-License-Identifier: Apache-2.0
///
#include <algorithm>

#include "tt_metal/common/core_coord.h"
#include "eth_l1_address_map.h"
#include "impl/buffers/buffer.hpp"
#include "ttnn/tensor/tensor_impl.hpp"
#include "ttnn/operations/ccl/all_gather/device/all_gather_op.hpp"
#include "ttnn/operations/ccl/shared_with_host/hetergeneous_data_structs.hpp"
#include "ttnn/operations/ccl/ccl_host_datastructures.hpp"
#include "ttnn/operations/ccl/ccl_common.hpp"
#include "ttnn/deprecated/tt_dnn/op_library/math.hpp"
#include "ttnn/deprecated/tt_dnn/op_library/work_split.hpp"
#include "tt_metal/common/constants.hpp"
#include "tt_metal/detail/util.hpp"
#include "tt_metal/host_api.hpp"
#include <sstream>
#include <type_traits>

#include "ttnn/cpp/ttnn/operations/experimental/ccl/all_gather_matmul/device/all_gather_matmul_op.hpp"
#include "ttnn/operations/experimental/ccl/ccl_op_fusion.hpp"


using namespace tt::constants;

namespace ttnn {

using namespace experimental::ccl;
using Tensors = std::vector<Tensor>;

// Used to hold the return values for setup_datacopy
struct DatacopyParams {
    std::vector<CoreCoord> datacopy_cores;
    std::vector<uint32_t> datacopy_signal_semaphore_ids;
    std::optional<operation::OverrideRuntimeArgumentsCallback<Tensors>> datacopy_override_runtime_arguments_callback;
};

DatacopyParams setup_datacopy(
    tt::tt_metal::Program& program,
    const Tensor& input_tensor,
    const Tensor& all_gather_output_tensor,
    Tensor& datacopy_output_tensor,
    const uint32_t dim,
    const uint32_t num_links,
    const uint32_t ring_size,
    const uint32_t ring_index,
    all_gather_op::Topology topology,

    CoreCoord datacopy_core_coord
) {

    std::size_t num_edm_buffers_per_channel = 2;
    auto const& all_gather_config = ttnn::AllGatherConfig(input_tensor, all_gather_output_tensor, dim, ring_size, num_links, topology, num_edm_buffers_per_channel, true);
    const uint32_t num_transfers = 4; // ring_size - 1;

    auto tensor_slicer = ttnn::ccl::InterleavedRingAllGatherTensorSlicer (
        input_tensor,
        all_gather_output_tensor,
        dim,
        ring_index
    );


    // Select cores for datacopy (single core for now)
    CoreRangeSet datacopy_workers = CoreRangeSet({CoreRange(datacopy_core_coord)});
    std::vector<CoreCoord> all_datacopy_cores = corerange_to_cores(datacopy_workers, std::nullopt, true);

    // Setup semaphores used to signal datacopy. TODO: instead of datacopy, this should be matmul cores
    // Dir0: first half of all gather (clockwise), Dir1: second half of all gather (counter-clockwise)
    auto datacopy_signal_semaphore_id_dir0 = CreateSemaphore(program, datacopy_workers, 0);
    auto datacopy_signal_semaphore_id_dir1 = CreateSemaphore(program, datacopy_workers, 0);

    // Setup args for the kernel
    const uint32_t tile_size = 32;
    const uint32_t page_size = all_gather_output_tensor.buffer()->page_size();

    const tt::DataFormat cb_data_format = tt::tt_metal::datatype_to_dataformat_converter(all_gather_output_tensor.get_dtype());


    auto all_gather_output_buffer = all_gather_output_tensor.buffer();
    auto datacopy_output_buffer = datacopy_output_tensor.buffer();

    bool all_gather_output_is_dram = all_gather_output_buffer->buffer_type() == tt::tt_metal::BufferType::DRAM ? 1 : 0;
    bool datacopy_output_is_dram = datacopy_output_buffer->buffer_type() == tt::tt_metal::BufferType::DRAM ? 1 : 0;

    uint32_t last_output_page_offset = (ring_size - 1) * tensor_slicer.output_page_offset;
    uint32_t num_rows = input_tensor.get_legacy_shape()[2] / tile_size ;
    bool is_clockwise_dir = true; // Specifically for the first half of the all gather

    uint32_t datacopy_buffer_size = 200;

    // Compile time args
    std::vector<uint32_t> datacopy_ct_args = {
        static_cast<uint32_t>(all_gather_output_is_dram),
        static_cast<uint32_t>(datacopy_output_is_dram),
        static_cast<uint32_t>(num_transfers),
        static_cast<uint32_t>(page_size),
        static_cast<uint32_t>(ring_index),
        static_cast<uint32_t>(ring_size),
        static_cast<uint32_t>(all_gather_output_tensor.get_legacy_shape()[3] / tile_size), // tesnor width
        static_cast<uint32_t>(all_gather_output_tensor.get_legacy_shape()[2] / tile_size), // tensor height
        static_cast<uint32_t>(tensor_slicer.num_cols), // tensor slice width in tiles
        static_cast<uint32_t>(num_rows), // tnesor slice height in tiles
        static_cast<uint32_t>(tensor_slicer.output_page_offset),
        static_cast<uint32_t>(last_output_page_offset),
        static_cast<bool>(is_clockwise_dir),
        static_cast<uint32_t>(datacopy_signal_semaphore_id_dir0),
        static_cast<uint32_t>(datacopy_signal_semaphore_id_dir1),
        static_cast<uint32_t>(datacopy_buffer_size),
    };

    uint32_t cb_id_in0 = tt::CB::c_in0;
    tt::tt_metal::CircularBufferConfig cb_in0_config =
        tt::tt_metal::CircularBufferConfig(
            page_size * datacopy_buffer_size, {{cb_id_in0, cb_data_format}})
            .set_page_size(cb_id_in0, page_size);
    auto cb_input = tt::tt_metal::CreateCircularBuffer(program, datacopy_workers, cb_in0_config);

    // Runtime args
    std::vector<uint32_t> datacopy_rt_args = {
        static_cast<uint32_t>(all_gather_output_buffer->address()),
        static_cast<uint32_t>(datacopy_output_buffer->address()),
    };

    std::map<string, string> kernel_defines = {
        {"TILED_LAYOUT", "1"},
        {"INTERLEAVED_MEM_LAYOUT", "1"}
    };


    // Create the kernel
    tt::tt_metal::KernelHandle datacopy_kernel_id = tt::tt_metal::CreateKernel(
        program,
        "ttnn/cpp/ttnn/operations/experimental/ccl/all_gather_matmul/device/kernels/datacopy.cpp",
        datacopy_workers,
        tt::tt_metal::WriterDataMovementConfig(datacopy_ct_args, kernel_defines));

    // Set runtime args
    tt::tt_metal::SetRuntimeArgs(
        program,
        datacopy_kernel_id,
        datacopy_workers,
        datacopy_rt_args
    );

    auto override_runtime_arguments_callback = [datacopy_kernel_id, all_datacopy_cores] (
        const void* operation,
        Program& program,
        const std::vector<Tensor>& input_tensors,
        const std::vector<std::optional<const Tensor>>& optional_input_tensors,
        const std::vector<Tensor>& output_tensors
    ) {

        auto datacopy_output_buffer = output_tensors[2].buffer();
        auto all_gather_output_buffer = output_tensors[0].buffer();

        auto &cached_args = GetRuntimeArgs(program, datacopy_kernel_id);

        for (auto core : all_datacopy_cores) {
            auto &cached_rt_args = cached_args.at(core.x).at(core.y);

            cached_rt_args[0] = static_cast<uint32_t>(all_gather_output_buffer->address());
            cached_rt_args[1] = static_cast<uint32_t>(datacopy_output_buffer->address());
        }

    };

    // Return the core coordinates and semaphore address
    return {
        .datacopy_cores = all_datacopy_cores,
        .datacopy_signal_semaphore_ids = {datacopy_signal_semaphore_id_dir0, datacopy_signal_semaphore_id_dir1},
        .datacopy_override_runtime_arguments_callback = override_runtime_arguments_callback
    };
}


// For ring all-gather, we can send sub-sections of input tensor in opposite directions
// For linear all-gather though, we must ensure we send full tensors in BOTH directions
//   (in other words, disable the "bidirectional" send flag)
operation::ProgramWithCallbacks experimental::all_gather_matmul_multi_core_with_workers(const Tensor& input_tensor, Tensor& all_gather_output_tensor, Tensor& datacopy_output_tensor, const uint32_t dim, const uint32_t num_links, const uint32_t ring_size, const uint32_t ring_index, const std::optional<chip_id_t> receiver_device_id, const std::optional<chip_id_t> sender_device_id, all_gather_op::Topology topology, const CoreCoord core_grid_offset) {

    tt::tt_metal::Program program{};

    DatacopyParams datacopy_params = setup_datacopy(program, input_tensor, all_gather_output_tensor, datacopy_output_tensor, dim, num_links, ring_size, ring_index, topology, {0, 0});
    const auto& datacopy_override_runtime_arguments_callback = datacopy_params.datacopy_override_runtime_arguments_callback;

    std::optional<AllGatherFusedOpSignaler> fused_op_signaler = AllGatherFusedOpSignaler(datacopy_params.datacopy_cores, datacopy_params.datacopy_signal_semaphore_ids);

    // Pass in the datacopy cores and sempahore address (Using optional arguments)
    operation::ProgramWithCallbacks program_with_callbacks = ttnn::all_gather_multi_core_with_workers_helper(program, input_tensor, all_gather_output_tensor, dim, num_links, ring_size, ring_index, receiver_device_id, sender_device_id, topology, fused_op_signaler, core_grid_offset);
    auto all_gather_override_runtime_arguments_callback = program_with_callbacks.override_runtime_arguments_callback;

    // Fuse the datacopy and all-gather overriden runtime arguments callbacks
    auto override_runtime_arguments_callback = [all_gather_override_runtime_arguments_callback, datacopy_override_runtime_arguments_callback] (
        const void* operation,
        Program& program,
        const std::vector<Tensor>& input_tensors,
        const std::vector<std::optional<const Tensor>>& optional_input_tensors,
        const std::vector<Tensor>& output_tensors
    ) {

        if (all_gather_override_runtime_arguments_callback.has_value()) {
            all_gather_override_runtime_arguments_callback.value()(operation, program, input_tensors, optional_input_tensors, output_tensors);
        }

        if (datacopy_override_runtime_arguments_callback.has_value()) {
            datacopy_override_runtime_arguments_callback.value()(operation, program, input_tensors, optional_input_tensors, output_tensors);
        }
    };

    program_with_callbacks.override_runtime_arguments_callback = override_runtime_arguments_callback;

    return program_with_callbacks;
}

}  // namespace ttnn

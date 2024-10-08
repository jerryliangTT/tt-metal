// SPDX-FileCopyrightText: © 2024 Tenstorrent Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <optional>

#include "ttnn/common/constants.hpp"
#include "ttnn/tensor/tensor.hpp"
#include "ttnn/deprecated/tt_dnn/op_library/compute_kernel_config.hpp"

namespace ttnn::operations::experimental::matmul {

operation::ProgramWithCallbacks multi_core_attn_matmul(const Tensor &a, const Tensor &b, Tensor& output, std::optional<const uint32_t> num_tokens, std::optional<const bool> transpose_hw, CoreCoord compute_with_storage_grid_size, DeviceComputeKernelConfig compute_kernel_config);

struct AttnMatmulDeviceOperation {
    std::optional<const uint32_t> num_tokens;
    std::optional<const bool> transpose_hw;
    CoreCoord compute_with_storage_grid_size;
    MemoryConfig output_mem_config;
    DataType output_dtype;
    const DeviceComputeKernelConfig compute_kernel_config;

    void validate(const std::vector<Tensor>& input_tensors) const;
    std::vector<tt::tt_metal::Shape> compute_output_shapes(const std::vector<Tensor>& input_tensors) const;
    std::vector<Tensor> create_output_tensors(const std::vector<Tensor>& input_tensors) const;
    operation::ProgramWithCallbacks create_program(
        const std::vector<Tensor>& input_tensors, std::vector<Tensor>& output_tensors) const;
    const operation::Hash compute_program_hash(const std::vector<Tensor> &input_tensors) const;
};

}  // namespace ttnn::operations::experimental::matmul

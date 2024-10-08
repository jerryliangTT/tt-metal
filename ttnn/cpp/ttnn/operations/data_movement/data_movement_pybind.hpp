// SPDX-FileCopyrightText: © 2023 Tenstorrent Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once


#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "ttnn/cpp/pybind11/decorators.hpp"
#include "ttnn/operations/data_movement/concat/concat_pybind.hpp"
#include "ttnn/operations/data_movement/pad/pad_pybind.hpp"
#include "ttnn/operations/data_movement/permute/permute_pybind.hpp"
#include "ttnn/operations/data_movement/slice/slice_pybind.hpp"
#include "ttnn/operations/data_movement/tilize/tilize_pybind.hpp"
#include "ttnn/operations/data_movement/tilize_with_val_padding/tilize_with_val_padding_pybind.hpp"
#include "ttnn/operations/data_movement/repeat_interleave/repeat_interleave_pybind.hpp"
#include "ttnn/operations/data_movement/transpose/transpose_pybind.hpp"
#include "ttnn/operations/data_movement/split/split_pybind.hpp"
#include "ttnn/operations/data_movement/untilize/untilize_pybind.hpp"
#include "ttnn/operations/data_movement/untilize_with_unpadding/untilize_with_unpadding_pybind.hpp"
#include "ttnn/operations/data_movement/untilize_with_halo_v2/untilize_with_halo_v2_pybind.hpp"
#include "ttnn/operations/data_movement/non_zero_indices/non_zero_indices_pybind.hpp"
#include "ttnn/operations/data_movement/fill_rm/fill_rm_pybind.hpp"
#include "ttnn/operations/data_movement/repeat/repeat_pybind.hpp"
#include "ttnn/operations/data_movement/fold/fold_pybind.hpp"
#include "ttnn/operations/data_movement/sharded_partial/sharded_to_interleaved_partial/sharded_to_interleaved_partial_pybind.hpp"
#include "ttnn/operations/data_movement/sharded_partial/interleaved_to_sharded_partial/interleaved_to_sharded_partial_pybind.hpp"
#include "ttnn/operations/data_movement/reshape/reshape_pybind.hpp"

#include "ttnn/operations/data_movement/indexed_fill/indexed_fill_pybind.hpp"
#include "ttnn/cpp/ttnn/operations/data_movement/copy/copy_pybind.hpp"
#include "ttnn/cpp/ttnn/operations/data_movement/move/move_pybind.hpp"

namespace py = pybind11;

namespace ttnn {
namespace operations {
namespace data_movement {


void py_module(py::module& module) {
    detail::bind_permute(module);
    detail::bind_concat(module);
    detail::bind_pad(module);
    detail::bind_slice(module);
    detail::bind_repeat_interleave(module);
    detail::bind_tilize(module);
    detail::bind_tilize_with_val_padding(module);
    detail::bind_tilize_with_zero_padding(module);
    detail::bind_transpose(module);
    detail::bind_split(module);
    detail::bind_untilize(module);
    detail::bind_untilize_with_unpadding(module);
    detail::bind_untilize_with_halo_v2(module);
    bind_non_zero_indices(module);
    bind_fill_rm(module);
    py_bind_repeat(module);
    py_bind_reshape(module);
    detail::bind_indexed_fill(module);
    bind_fold_operation(module);
    py_bind_sharded_to_interleaved_partial(module);
    py_bind_interleaved_to_sharded_partial(module);
    detail::py_bind_copy(module);
    detail::py_bind_clone(module);
    detail::py_bind_assign(module);
    detail::py_bind_move(module);
}

}  // namespace data_movement
}  // namespace operations
}  // namespace ttnn

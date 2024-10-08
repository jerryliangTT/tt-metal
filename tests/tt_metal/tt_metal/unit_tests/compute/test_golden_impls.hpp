// SPDX-FileCopyrightText: © 2023 Tenstorrent Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <random>
#include <vector>

//TODO: RT these functions should be templated for different data formats
namespace unit_tests::compute {

std::vector<uint32_t> gold_standard_untilize(const std::vector<uint32_t> &src_vec, const std::vector<uint32_t> &shape);

std::vector<uint32_t> gold_standard_tilize(const std::vector<uint32_t> &src_vec, const std::vector<uint32_t> &shape);

// input shape.x is assumed to have the full number of elements in bfloat16
// src_vec is expected to be untilized
// result is also untilized
std::vector<uint16_t> gold_transpose_wh(const std::vector<uint16_t> &src_vec, const std::vector<uint32_t> &shape);

// input shape.x is assumed to have the full number of elements in bfloat16
// src_vec is expected to be untilized
// result is also untilized
std::vector<uint16_t> gold_reduce_h(const std::vector<uint16_t> &src_vec, const std::vector<uint32_t> &shape, float scaler, bool red_max = false, bool zeropad = true);

// input shape.x is assumed to have the full number of elements in bfloat16
// src_vec is expected to be untilized
// result is also untilized
std::vector<uint16_t> gold_reduce_w(const std::vector<uint16_t> &src_vec, const std::vector<uint32_t> &shape, float scaler, bool red_max = false, bool zeropad = true);

// input shape.x is assumed to have the full number of elements in bfloat16
// src_vec is expected to be untilized
// result is also untilized
std::vector<uint16_t> gold_reduce_hw(const std::vector<uint16_t> &src_vec, const std::vector<uint32_t> &shape, float scaler, bool red_max = false, bool zeropad = true);

// Takes untilized src0_vec and tilized src1_vec
// returns tilized result of eltwise addition
// Assumes all elements in bfloat16
std::vector<uint32_t> gold_standard_tilize_w_elwadd(const std::vector<uint32_t> &src0_vec, const std::vector<uint32_t> &src1_vec, const std::vector<uint32_t> &shape);

}   // unit_tests::compute

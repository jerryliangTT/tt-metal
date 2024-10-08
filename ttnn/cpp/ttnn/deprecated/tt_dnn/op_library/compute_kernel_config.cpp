// SPDX-FileCopyrightText: © 2023 Tenstorrent Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "compute_kernel_config.hpp"

namespace tt::tt_metal {

DeviceComputeKernelConfig init_device_compute_kernel_config(
    ARCH arch,
    const std::optional<const DeviceComputeKernelConfig>& device_kernel_config,
    const MathFidelity default_fidelity,
    bool default_approx_mode,
    bool default_fp32_acc,
    bool default_l1_acc) {

    DeviceComputeKernelConfig defaultConfig;
    if (device_kernel_config.has_value()) {
        auto compute_kernel_config = device_kernel_config.value();
        std::visit(
            [&](auto&& compute_kernel_config) {
                using T = std::decay_t<decltype(compute_kernel_config)>;
                if constexpr (std::is_same_v<T, GrayskullComputeKernelConfig>) {
                    TT_ASSERT(arch == ARCH::GRAYSKULL, "kernel config is not for graykull");
                    MathFidelity math_fidelity = compute_kernel_config.math_fidelity;
                    bool math_approx_mode = compute_kernel_config.math_approx_mode;
                    defaultConfig = GrayskullComputeKernelConfig{
                        .math_fidelity = math_fidelity, .math_approx_mode = math_approx_mode};
                } else if constexpr (std::is_same_v<T, WormholeComputeKernelConfig>) {
                    TT_ASSERT(ttnn::device::is_wormhole_or_blackhole(arch), "kernel config is not for wormhole_b0 or blackhole");
                    MathFidelity math_fidelity = compute_kernel_config.math_fidelity;
                    bool math_approx_mode = compute_kernel_config.math_approx_mode;
                    bool fp32_dest_acc_en = compute_kernel_config.fp32_dest_acc_en;
                    bool packer_l1_acc = compute_kernel_config.packer_l1_acc;
                    defaultConfig = WormholeComputeKernelConfig{
                        .math_fidelity = math_fidelity,
                        .math_approx_mode = math_approx_mode,
                        .fp32_dest_acc_en = fp32_dest_acc_en,
                        .packer_l1_acc = packer_l1_acc};
                } else {
                    TT_FATAL("arch not supported");
                }
            },
            compute_kernel_config);
        return defaultConfig;
    } else {
        if (arch == ARCH::GRAYSKULL) {
            return GrayskullComputeKernelConfig{
                .math_fidelity = default_fidelity, .math_approx_mode = default_approx_mode};
        } else {
            return WormholeComputeKernelConfig{
                .math_fidelity = default_fidelity,
                .math_approx_mode = default_approx_mode,
                .fp32_dest_acc_en = default_fp32_acc,
                .packer_l1_acc = default_l1_acc};
        }
    }
}

bool get_fp32_dest_acc_en(const std::optional<DeviceComputeKernelConfig>& compute_kernel_config) {
    if (not compute_kernel_config.has_value()) {
        return false;
    }
    return std::visit(
        [](auto&& compute_kernel_config) -> bool {
            using T = std::decay_t<decltype(compute_kernel_config)>;
            if constexpr (std::is_same_v<T, GrayskullComputeKernelConfig>) {
                return false;
            } else if constexpr (std::is_same_v<T, WormholeComputeKernelConfig>) {
                return compute_kernel_config.fp32_dest_acc_en;
            } else {
                TT_THROW("arch not supported");
            }
        },
        compute_kernel_config.value());
}

std::tuple<MathFidelity, bool, bool, bool> get_compute_kernel_config_args(
    ARCH arch, const DeviceComputeKernelConfig compute_kernel_config) {

    MathFidelity math_fidelity;
    bool math_approx_mode;
    bool fp32_dest_acc_en;
    bool packer_l1_acc;

    std::visit(
        [&](auto&& compute_kernel_config) {
            using T = std::decay_t<decltype(compute_kernel_config)>;
            if constexpr (std::is_same_v<T, GrayskullComputeKernelConfig>) {
                TT_ASSERT(arch == ARCH::GRAYSKULL, "kernel config is not for graykull");
                math_fidelity = compute_kernel_config.math_fidelity;
                math_approx_mode = compute_kernel_config.math_approx_mode;
                fp32_dest_acc_en = false;
                packer_l1_acc = false;
            } else if constexpr (std::is_same_v<T, WormholeComputeKernelConfig>) {
                TT_ASSERT(ttnn::device::is_wormhole_or_blackhole(arch), "kernel config is not for wormhole_b0 or blackhole");
                math_fidelity = compute_kernel_config.math_fidelity;
                math_approx_mode = compute_kernel_config.math_approx_mode;
                fp32_dest_acc_en = compute_kernel_config.fp32_dest_acc_en;
                packer_l1_acc = compute_kernel_config.packer_l1_acc;
            } else {
                TT_FATAL("arch not supported");
            }
        },
        compute_kernel_config);

    return std::make_tuple(math_fidelity, math_approx_mode, fp32_dest_acc_en, packer_l1_acc);
}

}  // namespace tt::tt_metal

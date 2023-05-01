#include <iostream>

#include "build_kernels_for_riscv/build_kernels_for_riscv.hpp"




int main() {

    std::string root_dir = tt::utils::get_root_dir();
    std::string arch_name = tt::utils::get_env_arch_name();

    // Create and config an OP
    tt::build_kernel_for_riscv_options_t build_kernel_for_riscv_options("dummy_type","add_two_ints");
    std::string out_dir_path = root_dir + "/built_kernels/" + build_kernel_for_riscv_options.name;

    log_info(tt::LogBuildKernels, "Compiling OP: {} to {}", build_kernel_for_riscv_options.name, out_dir_path);

    build_kernel_for_riscv_options.brisc_kernel_file_name = "tt_metal/kernels/riscv_draft/add_two_ints.cpp";

    generate_binary_for_risc(RISCID::BR, &build_kernel_for_riscv_options, out_dir_path, arch_name);

    // WH doesn't work? FIXME: SFPU issues?
    //generate_all_fw(&build_kernel_for_riscv_options, "wormhole");

    return 0;
}

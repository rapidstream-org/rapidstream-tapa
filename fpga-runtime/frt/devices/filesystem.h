// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef FPGA_RUNTIME_FILESYSTEM_H_
#define FPGA_RUNTIME_FILESYSTEM_H_

#ifdef __cpp_lib_filesystem
#include <filesystem>  // IWYU pragma: export
#else
#include <experimental/filesystem>  // IWYU pragma: export
#endif

namespace fpga {
namespace internal {

#ifdef __cpp_lib_filesystem
namespace fs = std::filesystem;
#else
namespace fs = std::experimental::filesystem;
#endif

}  // namespace internal
}  // namespace fpga

#endif  // FPGA_RUNTIME_FILESYSTEM_H_

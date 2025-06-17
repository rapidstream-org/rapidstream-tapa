// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "xilinx_environ.h"

#include <sstream>
#include <string>
#include <string_view>

#include "frt/subprocess.h"

namespace fpga::xilinx {

namespace {

void UpdateEnviron(std::string_view script, Environ& environ) {
  subprocess::OutBuffer output = subprocess::check_output(
      {
          "bash",
          "-c",
          "source \"$0\" >/dev/null 2>&1 && env -0",
          script,
      },
      subprocess::environment(environ));

  for (size_t n = 0; n < output.length;) {
    std::string_view line = output.buf.data() + n;
    n += line.size() + 1;

    auto pos = line.find('=');
    environ[std::string(line.substr(0, pos))] = line.substr(pos + 1);
  }
}

}  // namespace

Environ GetEnviron() {
  std::string xilinx_tool;
  for (const char* env : {
           "XILINX_VITIS",
           "XILINX_SDX",
           "XILINX_HLS",
           "XILINX_VIVADO",
       }) {
    if (const char* value = getenv(env)) {
      xilinx_tool = value;
      break;
    }
  }

  if (xilinx_tool.empty()) {
    for (std::string hls : {"vitis_hls", "vivado_hls"}) {
      subprocess::OutBuffer buf = subprocess::check_output({
          "bash",
          "-c",
          "\"$0\" -version -help -l /dev/null 2>/dev/null",
          hls,
      });
      std::istringstream lines(std::string(buf.buf.data(), buf.length));
      for (std::string line; getline(lines, line);) {
        std::string_view prefix = "source ";
        std::string suffix = "/scripts/" + hls + "/hls.tcl -notrace";
        if (line.size() > prefix.size() + suffix.size() &&
            line.compare(0, prefix.size(), prefix) == 0 &&
            line.compare(line.size() - suffix.size(), suffix.size(), suffix) ==
                0) {
          xilinx_tool = line.substr(
              prefix.size(), line.size() - prefix.size() - suffix.size());
          break;
        }
      }
    }
  }

  Environ environ;
  UpdateEnviron(xilinx_tool + "/settings64.sh", environ);
  if (const char* xrt = getenv("XILINX_XRT")) {
    UpdateEnviron(std::string(xrt) + "/setup.sh", environ);
  }
  return environ;
}

}  // namespace fpga::xilinx

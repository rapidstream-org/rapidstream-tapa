// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "tapa/host/simulate.h"

#include "tapa/base/task.h"

namespace tapa::hls_simulate {

task::task() { this->mode_override = internal::InvokeMode::kSequential; }

}  // namespace tapa::hls_simulate

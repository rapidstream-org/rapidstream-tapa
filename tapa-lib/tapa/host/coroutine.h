// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef TAPA_HOST_COROUTINE_H_
#define TAPA_HOST_COROUTINE_H_

#include <functional>
#include <string>

namespace tapa {
namespace internal {
void schedule(bool detach, const std::function<void()>&);
void schedule_cleanup(const std::function<void()>&);
void yield(const std::string& msg);
}  // namespace internal
}  // namespace tapa

#endif  // TAPA_HOST_COROUTINE_H_

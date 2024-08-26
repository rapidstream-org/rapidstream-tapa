// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <string>
#include <string_view>

namespace tapa::internal {

std::string StrCat(std::initializer_list<std::string_view> pieces);

}  // namespace tapa::internal

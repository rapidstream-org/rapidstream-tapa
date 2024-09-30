// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#pragma once

#include "tapa/base/logging.h"

#define LOG(level) ::tapa::internal::dummy()
#define LOG_IF(level, cond) ::tapa::internal::dummy()
#define LOG_EVERY_N(level, n) ::tapa::internal::dummy()
#define LOG_IF_EVERY_N(level, cond, n) ::tapa::internal::dummy()
#define LOG_FIRST_N(level, n) ::tapa::internal::dummy()

#define DLOG(level) ::tapa::internal::dummy()
#define DLOG_IF(level, cond) ::tapa::internal::dummy()
#define DLOG_EVERY_N(level, n) ::tapa::internal::dummy()

#define CHECK(cond) ::tapa::internal::dummy()
#define CHECK_NE(lhs, rhs) ::tapa::internal::dummy()
#define CHECK_EQ(lhs, rhs) ::tapa::internal::dummy()
#define CHECK_GE(lhs, rhs) ::tapa::internal::dummy()
#define CHECK_GT(lhs, rhs) ::tapa::internal::dummy()
#define CHECK_LE(lhs, rhs) ::tapa::internal::dummy()
#define CHECK_LT(lhs, rhs) ::tapa::internal::dummy()
#define CHECK_NOTNULL(ptr) (ptr)
#define CHECK_STREQ(lhs, rhs) ::tapa::internal::dummy()
#define CHECK_STRNE(lhs, rhs) ::tapa::internal::dummy()
#define CHECK_STRCASEEQ(lhs, rhs) ::tapa::internal::dummy()
#define CHECK_STRCASENE(lhs, rhs) ::tapa::internal::dummy()
#define CHECK_DOUBLE_EQ(lhs, rhs) ::tapa::internal::dummy()

#define VLOG_IS_ON(level) false
#define VLOG(level) ::tapa::internal::dummy()
#define VLOG_IF(level, cond) ::tapa::internal::dummy()
#define VLOG_EVERY_N(level, n) ::tapa::internal::dummy()
#define VLOG_IF_EVERY_N(level, cond, n) ::tapa::internal::dummy()
#define VLOG_FIRST_N(level, n) ::tapa::internal::dummy()

// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef TAPA_STREAM_H_
#define TAPA_STREAM_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"

#include "type.h"

inline std::string GetPeekVar(const std::string& name) {
  return name + "._peek";
}

inline std::string GetFifoVar(const std::string& name) { return name + "._"; }

const clang::ClassTemplateSpecializationDecl* GetTapaStreamDecl(
    const clang::Type* type);
const clang::ClassTemplateSpecializationDecl* GetTapaStreamDecl(
    const clang::QualType& qual_type);
const clang::ClassTemplateSpecializationDecl* GetTapaStreamsDecl(
    const clang::Type* type);
const clang::ClassTemplateSpecializationDecl* GetTapaStreamsDecl(
    const clang::QualType& qual_type);
std::vector<const clang::CXXMemberCallExpr*> GetTapaStreamOps(
    const clang::Stmt* stmt);

template <typename T>
inline bool IsStreamInterface(T obj) {
  return IsTapaType(obj, "(i|o)stream");
}
template <typename T>
inline bool IsStreamsInterface(T obj) {
  return IsTapaType(obj, "(i|o)streams");
}
template <typename T>
inline bool IsStreamInstance(T obj) {
  return IsTapaType(obj, "stream");
}
template <typename T>
inline bool IsStream(T obj) {
  return IsTapaType(obj, "(i|o)?stream");
}

inline std::string GetStreamElemType(const clang::ParmVarDecl* param) {
  if (IsTapaType(param, "(i|o)streams?")) {
    if (auto arg = GetTemplateArg(param->getType(), 0)) {
      return GetTemplateArgName(*arg);
    }
  }
  return "";
}
inline uint64_t GetStreamsSize(const clang::ParmVarDecl* param) {
  if (IsTapaType(param, "(i|o)streams")) {
    return GetIntegralTemplateArg<1>(param->getType());
  }
  return 0;
}

#endif  // TAPA_STREAM_H_

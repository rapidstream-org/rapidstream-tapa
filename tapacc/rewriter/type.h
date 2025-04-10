// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef TAPA_TYPE_H_
#define TAPA_TYPE_H_

#include <cstdint>
#include <string>
#include <utility>

#include "clang/AST/AST.h"

bool IsTapaType(const clang::RecordDecl* decl, const std::string& type_name);
inline bool IsTapaType(const clang::LValueReferenceType* type,
                       const std::string& type_name) {
  return type != nullptr &&
         IsTapaType(type->getPointeeType()->getAsRecordDecl(), type_name);
}
inline bool IsTapaType(clang::QualType type, const std::string& type_name) {
  return IsTapaType(type->getAsRecordDecl(), type_name) ||
         IsTapaType(type->getAs<clang::LValueReferenceType>(), type_name);
}
inline bool IsTapaType(const clang::Expr* expr, const std::string& type_name) {
  return expr != nullptr && IsTapaType(expr->getType(), type_name);
}
inline bool IsTapaType(const clang::ParmVarDecl* param,
                       const std::string& type_name) {
  return IsTapaType(param->getType(), type_name);
}

inline std::string ArrayNameAt(const std::string& name, int idx) {
  return name + "[" + std::to_string(idx) + "]";
}

inline std::string GetArrayElem(const std::string& name, int idx) {
  return name + "_" + std::to_string(idx);
}

template <int idx>
inline uint64_t GetIntegralTemplateArg(
    const clang::ClassTemplateSpecializationDecl* decl) {
  return *decl->getTemplateArgs()[idx].getAsIntegral().getRawData();
}
template <int idx>
inline uint64_t GetIntegralTemplateArg(const clang::RecordDecl* decl) {
  return GetIntegralTemplateArg<idx>(
      clang::dyn_cast<clang::ClassTemplateSpecializationDecl>(decl));
}
template <int idx>
inline uint64_t GetIntegralTemplateArg(clang::QualType type);
template <int idx>
inline uint64_t GetIntegralTemplateArg(const clang::LValueReferenceType* decl) {
  return GetIntegralTemplateArg<idx>(decl->getPointeeType());
}
template <int idx>
inline uint64_t GetIntegralTemplateArg(clang::QualType type) {
  if (auto ref = type->getAs<clang::LValueReferenceType>()) {
    return GetIntegralTemplateArg<idx>(ref);
  }
  if (auto ref = type->getAsRecordDecl()) {
    return GetIntegralTemplateArg<idx>(ref);
  }
  return 0;
}
template <int idx>
inline uint64_t GetIntegralTemplateArg(const clang::ParmVarDecl* param) {
  return GetIntegralTemplateArg<idx>(param->getType());
}

template <typename... Args>
inline uint64_t GetArraySize(Args... args) {
  return GetIntegralTemplateArg<1>(std::forward<Args>(args)...);
}

const clang::TemplateArgument* GetTemplateArg(clang::QualType type, size_t idx);

inline std::string GetTemplateArgName(const clang::TemplateArgument& arg) {
  std::string name;
  llvm::raw_string_ostream oss{name};
  clang::LangOptions options;
  options.CPlusPlus = true;
  options.Bool = true;
  clang::PrintingPolicy policy(options);
  arg.print(policy, oss, /* include_type */ false);
  return name;
}

#endif  // TAPA_TYPE_H_

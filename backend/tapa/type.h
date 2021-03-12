#ifndef TAPA_TYPE_H_
#define TAPA_TYPE_H_

#include <sstream>
#include <string>
#include <type_traits>

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

inline uint64_t GetArraySize(
    const clang::ClassTemplateSpecializationDecl* decl) {
  return *decl->getTemplateArgs()[1].getAsIntegral().getRawData();
}
inline uint64_t GetArraySize(const clang::RecordDecl* decl) {
  return GetArraySize(
      clang::dyn_cast<clang::ClassTemplateSpecializationDecl>(decl));
}
inline uint64_t GetArraySize(clang::QualType type);
inline uint64_t GetArraySize(const clang::LValueReferenceType* decl) {
  return GetArraySize(decl->getPointeeType());
}
inline uint64_t GetArraySize(clang::QualType type) {
  if (auto ref = type->getAs<clang::LValueReferenceType>()) {
    return GetArraySize(ref);
  }
  if (auto ref = type->getAsRecordDecl()) {
    return GetArraySize(ref);
  }
  return 0;
}
inline uint64_t GetArraySize(const clang::ParmVarDecl* param) {
  return GetArraySize(param->getType());
}

const clang::TemplateArgument* GetTemplateArg(clang::QualType type, int idx);

inline std::string GetTemplateArgName(const clang::TemplateArgument& arg) {
  std::string name;
  llvm::raw_string_ostream oss{name};
  clang::LangOptions options;
  options.CPlusPlus = true;
  options.Bool = true;
  arg.print(options, oss);
  return name;
}

#endif  // TAPA_TYPE_H_

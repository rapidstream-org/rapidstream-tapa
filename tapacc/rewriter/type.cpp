// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "type.h"

#include <regex>
#include <string>

#include "clang/AST/AST.h"
#include "llvm/ADT/StringExtras.h"

using std::regex;
using std::regex_match;
using std::string;

using clang::LValueReferenceType;
using clang::QualType;
using clang::RecordDecl;
using clang::TemplateArgument;
using clang::TemplateSpecializationType;

bool IsTapaType(const RecordDecl* decl, const string& type_name) {
  return decl != nullptr && regex_match(decl->getQualifiedNameAsString(),
                                        regex{"tapa::" + type_name});
}

const TemplateArgument* GetTemplateArg(QualType qual_type, int idx) {
  auto type = qual_type->getAs<TemplateSpecializationType>();
  if (type == nullptr) {
    if (auto lv_ref = qual_type->getAs<LValueReferenceType>()) {
      type = lv_ref->getPointeeType()->getAs<TemplateSpecializationType>();
    }
  }
  if (type != nullptr) {
    auto args = type->template_arguments();
    if (idx >= 0 && idx < args.size()) {
      return &(args.data()[idx]);
    }
  }
  return nullptr;
}

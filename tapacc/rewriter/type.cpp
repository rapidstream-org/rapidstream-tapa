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

using clang::ClassTemplateSpecializationDecl;
using clang::LValueReferenceType;
using clang::QualType;
using clang::RecordDecl;
using clang::RecordType;
using clang::SubstTemplateTypeParmType;
using clang::TemplateArgument;
using clang::TemplateSpecializationType;

bool IsTapaType(const std::string& qualified_name,
                const std::string& type_name) {
  return regex_match(qualified_name, regex{"tapa::" + type_name});
}

bool IsTapaType(clang::QualType type, const std::string& type_name) {
  return IsTapaType(type->getAsRecordDecl(), type_name) ||
         IsTapaType(type->getAs<clang::LValueReferenceType>(), type_name) ||
         IsTapaType(type->getAs<clang::TemplateSpecializationType>(),
                    type_name);
}

const TemplateArgument* GetTemplateArg(QualType qual_type, size_t idx) {
  bool replaced = true;
  while (replaced) {
    replaced = false;
    if (auto lv_ref = qual_type->getAs<LValueReferenceType>()) {
      qual_type = lv_ref->getPointeeType();
      replaced = true;
    }
    if (auto subst_type = qual_type->getAs<SubstTemplateTypeParmType>()) {
      qual_type = subst_type->getReplacementType();
      replaced = true;
    }
  }

  if (auto type = qual_type->getAs<TemplateSpecializationType>()) {
    auto args = type->template_arguments();
    if (idx >= 0 && idx < args.size()) {
      return &(args.data()[idx]);
    }
  }

  if (auto record = qual_type->getAs<RecordType>()) {
    if (auto record_decl = llvm::dyn_cast<ClassTemplateSpecializationDecl>(
            record->getDecl())) {
      auto& args = record_decl->getTemplateArgs();
      if (idx >= 0 && idx < args.size()) {
        return &(args.data()[idx]);
      }
    }
  }

  assert(false && "Invalid template argument");
  return nullptr;
}

// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "task.h"

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "clang/Lex/Lexer.h"
#include "llvm/ADT/StringExtras.h"

#include "nlohmann/json.hpp"

#include "mmap.h"
#include "stream.h"
#include "type.h"

using std::initializer_list;
using std::string;
using std::to_string;
using std::unordered_map;
using std::vector;

using clang::CharSourceRange;
using clang::CXXBindTemporaryExpr;
using clang::CXXMemberCallExpr;
using clang::CXXMethodDecl;
using clang::CXXOperatorCallExpr;
using clang::DeclRefExpr;
using clang::DeclStmt;
using clang::Expr;
using clang::ExprWithCleanups;
using clang::FunctionDecl;
using clang::ImplicitCastExpr;
using clang::Lexer;
using clang::MaterializeTemporaryExpr;
using clang::SourceLocation;
using clang::SourceRange;
using clang::Stmt;
using clang::StringLiteral;
using clang::TapaTargetAttr;
using clang::TemplateArgument;
using clang::TemplateSpecializationType;
using clang::VarDecl;

using llvm::dyn_cast;
using llvm::StringRef;

using nlohmann::json;

namespace tapa {
namespace internal {

extern TapaTargetAttr::TargetType target;
static std::map<TapaTargetAttr::TargetType, Target*> target_map{
    {TapaTargetAttr::TargetType::XilinxAIE, XilinxAIETarget::GetInstance()},
    {TapaTargetAttr::TargetType::XilinxHLS,
     XilinxHLSTarget::GetInstance(/*is_vitis=*/false)},
    {TapaTargetAttr::TargetType::XilinxVitis,
     XilinxHLSTarget::GetInstance(/*is_vitis=*/true)},
    {TapaTargetAttr::TargetType::Ignore, IgnoreTarget::GetInstance()},
};

extern const string* top_name;

// Given a Stmt, find the first tapa::task expression in its children.
const ExprWithCleanups* GetTapaTaskObjectExpr(const Stmt* stmt) {
  if (!stmt) return nullptr;
  for (auto child : stmt->children()) {
    if (auto expr = dyn_cast<ExprWithCleanups>(child)) {
      if (expr->getType().getCanonicalType().getAsString() ==
          "struct tapa::task") {
        return expr;
      }
    }
  }
  return nullptr;
}

// Given a Stmt, find all tapa::task::invoke's via DFS and update invokes.
void GetTapaInvokes(const Stmt* stmt,
                    vector<const CXXMemberCallExpr*>& invokes) {
  for (auto child : stmt->children()) {
    GetTapaInvokes(child, invokes);
  }
  if (const auto invoke = dyn_cast<CXXMemberCallExpr>(stmt)) {
    if (invoke->getRecordDecl()->getQualifiedNameAsString() == "tapa::task" &&
        invoke->getMethodDecl()->getNameAsString() == "invoke") {
      invokes.push_back(invoke);
    }
  }
}

// Given a Stmt, return all tapa::task::invoke's via DFS.
vector<const CXXMemberCallExpr*> GetTapaInvokes(const Stmt* stmt) {
  vector<const CXXMemberCallExpr*> invokes;
  GetTapaInvokes(stmt, invokes);
  return invokes;
}

bool IsFuncTapaTopLevel(const FunctionDecl* func) {
  return *top_name == func->getNameAsString();
}

thread_local const FunctionDecl* Visitor::rewriting_func{nullptr};
thread_local const TapaTask* Visitor::current_task{nullptr};
thread_local Target* Visitor::current_target{nullptr};

void Visitor::VisitTask(const TapaTask& task) {
  current_task = &task;

  TapaTargetAttr::TargetType target;
  if (auto attr = task.func->getAttr<TapaTargetAttr>()) {
    // if overriden by the attribute
    target = attr->getTarget();
  } else {
    target = tapa::internal::target;
  }

  // The task metadata should only be obtained from the function definition
  if (task.func->isThisDeclarationADefinition()) {
    auto& metadata = GetMetadata();
    metadata["target"] = TapaTargetAttr::ConvertTargetTypeToStr(target);

    if (target_map.find(target) == target_map.end()) {
      static const auto diagnostic_id =
          this->context_.getDiagnostics().getCustomDiagID(
              clang::DiagnosticsEngine::Error, "unsupported target: %0");
      this->context_.getDiagnostics()
          .Report(task.func->getLocation(), diagnostic_id)
          .AddString(std::string(metadata["target"]));
      assert(false && "unsupported target");
    } else {
      current_target = target_map[target];
    }
  }

  TraverseDecl(task.func->getASTContext().getTranslationUnitDecl());
}

bool Visitor::isFuncTapaTask(const FunctionDecl* func) {
  for (const auto& task : tapa_tasks_) {
    // Compare the function name with the task function name since
    // template are stored in task.func as the specialized function,
    // while the currently visited function (func) is the original
    // declaration.
    if (task.func->getNameAsString() == func->getNameAsString()) {
      return true;
    }
  }
  return false;
}

// Apply tapa s2s transformations on a function.
bool Visitor::VisitFunctionDecl(FunctionDecl* func) {
  rewriting_func = nullptr;

  // If the visited function is a global function, and it is in the
  // main file, then it is a candidate for transformation.
  if (func->isGlobal() &&
      context_.getSourceManager().isWrittenInMainFile(func->getLocation())) {
    if (is_first_traversal) {
      // For the first traversal, collect all functions with body.
      if (func->hasBody()) funcs_.push_back(func);
    } else {
      // For all later traversals, process tapa task functions.
      assert(rewriters_.count(*current_task) > 0);
      rewriting_func = func;

      // Run this before the function body is purged
      if (func->hasAttrs() && func->hasBody()) {
        HandleAttrOnNodeWithBody(func, func->getBody(), func->getAttrs());
      }

      bool is_top_level_task = IsFuncTapaTopLevel(func);
      // If the first tapa::task is obtained in the function body, this is a
      // upper-level tapa task.
      // FIXME: This is not a perfect way to determine the level of the task,
      // especially when visiting the function signature.
      bool is_upper_level_task =
          GetTapaTaskObjectExpr(func->getBody()) != nullptr;
      // if the task is ignored, it is treated as a leaf task.
      if (IsFuncIgnored(func)) {
        is_upper_level_task = false;
        if (is_top_level_task) {
          static const auto diagnostic_id =
              this->context_.getDiagnostics().getCustomDiagID(
                  clang::DiagnosticsEngine::Error,
                  "tapa top-level task function cannot be ignored");
          this->context_.getDiagnostics().Report(func->getLocation(),
                                                 diagnostic_id);
        }
      }

      // If the task is in the task invocation graph from the top-level task,
      // it is a lower-level tapa task.
      bool is_lower_level_task = !is_upper_level_task && isFuncTapaTask(func);
      // If the decl is the one being visited or its signature.
      bool is_current_task =
          func->getNameAsString() == current_task->func->getNameAsString();
      // If the decl is the one instantiating this template task.
      bool is_invoker_of_current_task =
          current_task->invoker_func && func == current_task->invoker_func;

      // Rewrite the function arguments.
      if (is_lower_level_task) {
        // If the function is a lower-level task, even if the function is the
        // top-level task, we still need to rewrite the function arguments
        // as the lower-level task, as it is directly handled by the vendor
        // toolchain instead of the tapa compiler.
        current_target->RewriteLowerLevelFuncArguments(func, GetRewriter());
      } else if (is_top_level_task) {
        current_target->RewriteTopLevelFuncArguments(func, GetRewriter());
      } else if (is_upper_level_task) {
        current_target->RewriteMiddleLevelFuncArguments(func, GetRewriter());
      } else {
        current_target->RewriteOtherFuncArguments(func, GetRewriter());
      }

      if ((is_upper_level_task || is_lower_level_task) && !is_current_task) {
        // If the function is a tapa task but not the current task,
        // We deal with AIE and HLS differently.
        // For HLS, we reserve the function signature and remove the body.
        // For AIE, we remove the function signature and body.
        // TODO: If the non current task is the invoker of the current task,
        // we should create a specialized version of the function right before
        // the task declaration.
        current_target->ProcessNonCurrentTask(func, GetRewriter(),
                                              IsFuncTapaTopLevel(func));

      } else if (is_upper_level_task) {
        auto task_obj = GetTapaTaskObjectExpr(func->getBody());
        ProcessUpperLevelTaskFunc(task_obj, func);

      } else if (is_lower_level_task) {
        ProcessLowerLevelTaskFunc(func);

      } else {
        // Otherwise, it is a non-tapa task function.
        ProcessOtherFunc(func);
      }

      // Create the mangled wrapper for the current task after the invoker
      // function is processed.
      if (is_invoker_of_current_task) {
        auto& rewriter = GetRewriter();
        auto end_loc = GetEndOfLoc(func->getEndLoc());
        rewriter.InsertTextAfterToken(end_loc,
                                      GenerateWrapperCode(*current_task));
      }
    }
  }

  // Let the recursion continue.
  return clang::RecursiveASTVisitor<Visitor>::VisitFunctionDecl(func);
}

bool Visitor::VisitAttributedStmt(clang::AttributedStmt* stmt) {
  bool rewriting_current_task =
      current_task && rewriting_func == current_task->func;
  bool is_other_func = !isFuncTapaTask(rewriting_func);
  bool should_rewrite = rewriting_current_task || is_other_func;
  if (should_rewrite && rewriters_.count(*current_task) > 0) {
    HandleAttrOnNodeWithBody(stmt, GetLoopBody(stmt->getSubStmt()),
                             stmt->getAttrs());
  }
  return clang::RecursiveASTVisitor<Visitor>::VisitAttributedStmt(stmt);
}

void Visitor::ProcessTaskPorts(const TapaTask& task, nlohmann::json& metadata) {
  for (const auto param : task.func->parameters()) {
    auto arg_width = [&]() -> int {
      return GetTypeWidth(GetTemplateArg(param->getType(), 0)->getAsType());
    };

    const auto param_name = param->getNameAsString();
    auto add_mmap_meta = [&](const string& name) {
      std::string cat;
      if (IsTapaType(param, "immap")) {
        cat = "immap";
      } else if (IsTapaType(param, "ommap")) {
        cat = "ommap";
      } else if (IsTapaType(param, "async_mmap")) {
        cat = "async_mmap";
      } else {
        cat = "mmap";
      }
      metadata["ports"].push_back({{"name", name},
                                   {"cat", cat},
                                   {"width", arg_width()},
                                   {"type", GetMmapElemType(param) + "*"}});
    };
    if (IsTapaType(param, "(async_)?mmap") || IsTapaType(param, "immap") ||
        IsTapaType(param, "ommap")) {
      add_mmap_meta(param_name);
    } else if (IsTapaType(param, "mmaps")) {
      for (size_t i = 0; i < GetArraySize(param); ++i) {
        add_mmap_meta(param_name + "[" + to_string(i) + "]");
      }
    } else if (IsTapaType(param, "hmap")) {
      metadata["ports"].push_back({
          {"name", param_name},
          {"cat", "hmap"},
          {"width", arg_width()},
          {"type", GetMmapElemType(param) + "*"},
          {"chan_count", GetArraySize(param)},
          {"chan_size", GetIntegralTemplateArg<2>(param)},
      });
    } else if (IsStreamInterface(param)) {
      metadata["ports"].push_back(
          {{"name", param_name},
           {"cat", IsTapaType(param, "istream") ? "istream" : "ostream"},
           {"width", arg_width()},
           {"type", GetStreamElemType(param)}});
    } else if (IsStreamsInterface(param)) {
      metadata["ports"].push_back(
          {{"name", param_name},
           {"cat", IsTapaType(param, "istreams") ? "istreams" : "ostreams"},
           {"width", arg_width()},
           {"type", GetStreamElemType(param)},
           {"chan_count", GetIntegralTemplateArg<1>(param)}});
    } else {
      metadata["ports"].push_back({{"name", param_name},
                                   {"cat", "scalar"},
                                   {"width", GetTypeWidth(param->getType())},
                                   {"type", param->getType().getAsString()}});
    }
  }
}

// Apply tapa s2s transformations on a upper-level task.
void Visitor::ProcessUpperLevelTaskFunc(const ExprWithCleanups* task_obj,
                                        const FunctionDecl* func) {
  if (IsFuncTapaTopLevel(func)) {
    current_target->RewriteTopLevelFunc(func, GetRewriter());
  } else {
    current_target->RewriteMiddleLevelFunc(func, GetRewriter());
  }

  // The task metadata should only be obtained from the function definition
  if (!func->isThisDeclarationADefinition()) return;
  assert(task_obj != nullptr);

  // Obtain the connection schema from the task.
  // metadata: {tasks, fifos}
  // tasks: {task_name: [{step, {args: port_name: {var_type, var_name}}}]}
  // fifos: {fifo_name: {depth, produced_by, consumed_by}}
  auto& metadata = GetMetadata();
  metadata["fifos"] = json::object();
  ProcessTaskPorts(*current_task, metadata);

  // Process stream declarations.
  unordered_map<string, const VarDecl*> fifo_decls;
  for (const auto child : func->getBody()->children()) {
    if (const auto decl_stmt = dyn_cast<DeclStmt>(child)) {
      if (const auto var_decl = dyn_cast<VarDecl>(*decl_stmt->decl_begin())) {
        if (auto decl = GetTapaStreamDecl(var_decl->getType())) {
          const auto args = decl->getTemplateArgs().asArray();
          const string elem_type = GetTemplateArgName(args[0]);
          const uint64_t fifo_depth{*args[1].getAsIntegral().getRawData()};
          const string var_name{var_decl->getNameAsString()};
          metadata["fifos"][var_name]["depth"] = fifo_depth;
          fifo_decls[var_name] = var_decl;
        } else if (auto decl = GetTapaStreamsDecl(var_decl->getType())) {
          const auto args = decl->getTemplateArgs().asArray();
          const string elem_type = GetTemplateArgName(args[0]);
          const uint64_t fifo_depth = *args[2].getAsIntegral().getRawData();
          for (size_t i = 0; i < GetArraySize(decl); ++i) {
            const string var_name = ArrayNameAt(var_decl->getNameAsString(), i);
            metadata["fifos"][var_name]["depth"] = fifo_depth;
            fifo_decls[var_name] = var_decl;
          }
        }
      }
    }
  }

  // Instanciate tasks.
  vector<const CXXMemberCallExpr*> invokes = GetTapaInvokes(task_obj);

  unordered_map<string, int> istreams_access_pos;
  unordered_map<string, int> ostreams_access_pos;
  unordered_map<string, int> mmaps_access_pos;
  unordered_map<const Expr*, int> seq_access_pos;

  for (auto invoke : invokes) {
    int step = -1;
    bool has_name = false;
    bool has_executable = false;
    uint64_t vec_length = 1;
    if (const auto method = dyn_cast<CXXMethodDecl>(invoke->getCalleeDecl())) {
      auto args = method->getTemplateSpecializationArgs()->asArray();
      if (args.size() > 0 && args[0].getKind() == TemplateArgument::Integral) {
        step =
            *reinterpret_cast<const int*>(args[0].getAsIntegral().getRawData());
      } else {
        step = 0;  // default to join
      }
      if (args.size() > 1 && args[1].getKind() == TemplateArgument::Integral) {
        vec_length = *args[1].getAsIntegral().getRawData();
      }
      if (args.rbegin()->getKind() == TemplateArgument::Integral) {
        has_name = true;
      }
    } else {
      static const auto diagnostic_id =
          this->context_.getDiagnostics().getCustomDiagID(
              clang::DiagnosticsEngine::Error, "unexpected invocation: %0");
      this->context_.getDiagnostics()
          .Report(invoke->getCallee()->getBeginLoc(), diagnostic_id)
          .AddString(invoke->getStmtClassName());
    }
    const FunctionDecl* task = nullptr;
    string task_name;
    auto get_name = [&](const string& name, uint64_t i,
                        const DeclRefExpr* decl_ref) -> string {
      if (IsTapaType(decl_ref, "(mmaps|(i|o)?streams)")) {
        const auto ts_type =
            decl_ref->getType()->getAs<TemplateSpecializationType>();
        assert(ts_type != nullptr);
        const auto ts_args = ts_type->template_arguments();
        assert(ts_args.size() > 1);
        const auto length = this->EvalAsInt(ts_args[1].getAsExpr());
        if (length < 0 || i >= size_t(length)) {
          auto& diagnostics = context_.getDiagnostics();
          static const auto diagnostic_id =
              diagnostics.getCustomDiagID(clang::DiagnosticsEngine::Remark,
                                          "invocation #%0 accesses '%1[%2]'");
          auto diagnostics_builder =
              diagnostics.Report(decl_ref->getBeginLoc(), diagnostic_id);
          diagnostics_builder.AddString(to_string(i));
          diagnostics_builder.AddString(decl_ref->getNameInfo().getAsString());
          diagnostics_builder.AddString(to_string(i % length));
          diagnostics_builder.AddString(decl_ref->getType().getAsString());
          diagnostics_builder.AddSourceRange(
              GetCharSourceRange(decl_ref->getSourceRange()));
        }
        return ArrayNameAt(name, i % length);
      }
      return name;
    };
    for (uint64_t i_vec = 0; i_vec < vec_length; ++i_vec) {
      for (unsigned i = 0; i < invoke->getNumArgs(); ++i) {
        const auto arg = invoke->getArg(i);
        const auto decl_ref = dyn_cast<DeclRefExpr>(arg);  // a variable
        clang::Expr::EvalResult arg_eval_as_int_result;
        const bool arg_is_int =
            arg->EvaluateAsInt(arg_eval_as_int_result, this->context_);

        // skip if arg[i] is tapa::executable, which is used to specify the
        // bitstream file of a task. if the task is being synthesized, the
        // bitstream file of its sub-tasks should be simply ignored.
        if (IsTapaType(arg, "executable")) {
          has_executable = true;
          continue;
        }

        // element in an array
        auto materialize_temporary = dyn_cast<MaterializeTemporaryExpr>(arg);
        auto op_call = materialize_temporary
                           ? dyn_cast<CXXOperatorCallExpr>(
                                 materialize_temporary->getSubExpr())
                           : nullptr;

        const auto arg_is_seq = IsTapaType(arg, "seq");
        if (decl_ref || op_call || arg_is_int || arg_is_seq) {
          string arg_name;
          if (decl_ref) {
            arg_name = decl_ref->getNameInfo().getName().getAsString();
          }
          if (op_call) {
            auto decl_ref = dyn_cast<DeclRefExpr>(op_call->getArg(0));
            if (decl_ref == nullptr) {
              const auto implicit_cast =
                  dyn_cast<ImplicitCastExpr>(op_call->getArg(0));
              if (implicit_cast) {
                decl_ref = dyn_cast<DeclRefExpr>(implicit_cast->getSubExpr());
              }
            }
            const auto array_name =
                decl_ref->getNameInfo().getName().getAsString();
            const auto array_idx = this->EvalAsInt(op_call->getArg(1));
            arg_name = ArrayNameAt(array_name, array_idx);
          }
          if (arg_is_int) {
            arg_name = "64'd" +
                       std::to_string(uint64_t(
                           arg_eval_as_int_result.Val.getInt().getExtValue()));
          }

          if (i == 0) {
            auto func_decl = decl_ref->getDecl()->getAsFunction();
            if (func_decl->isFunctionTemplateSpecialization()) {
              // use mangled name for template specialization
              task_name = GetMangledFuncName(func_decl);
            } else {
              task_name = arg_name;
            }
            metadata["tasks"][task_name].push_back({{"step", step}});
            task = decl_ref->getDecl()->getAsFunction();
          } else {
            assert(task != nullptr);
            auto skip_params = (has_name ? 1 : 0) + (has_executable ? 1 : 0);
            auto param = task->getParamDecl(i - 1 - skip_params);
            auto param_name = param->getNameAsString();
            string param_cat;

            // register this argument to task
            auto register_arg = [&](string arg = "", string port = "") {
              if (arg.empty())
                arg = arg_name;  // use global arg_name by default
              if (port.empty()) port = param_name;
              (*metadata["tasks"][task_name].rbegin())["args"][port] = {
                  {"cat", param_cat}, {"arg", arg}};
            };

            // regsiter stream info to task
            auto register_consumer = [&, ast_arg = arg](string arg = "") {
              // use global arg_name by default
              if (arg.empty()) arg = arg_name;
              if (metadata["fifos"][arg].contains("consumed_by")) {
                static const auto diagnostic_id =
                    this->context_.getDiagnostics().getCustomDiagID(
                        clang::DiagnosticsEngine::Error,
                        "tapa::stream '%0' consumed more than once");
                auto diagnostics_builder =
                    this->context_.getDiagnostics().Report(
                        ast_arg->getBeginLoc(), diagnostic_id);
                diagnostics_builder.AddString(arg);
                diagnostics_builder.AddSourceRange(GetCharSourceRange(ast_arg));
              }
              metadata["fifos"][arg]["consumed_by"] = {
                  task_name, metadata["tasks"][task_name].size() - 1};
            };
            auto register_producer = [&, ast_arg = arg](string arg = "") {
              // use global arg_name by default
              if (arg.empty()) arg = arg_name;
              if (metadata["fifos"][arg].contains("produced_by")) {
                static const auto diagnostic_id =
                    this->context_.getDiagnostics().getCustomDiagID(
                        clang::DiagnosticsEngine::Error,
                        "tapa::stream '%0' produced more than once");
                auto diagnostics_builder =
                    this->context_.getDiagnostics().Report(
                        ast_arg->getBeginLoc(), diagnostic_id);
                diagnostics_builder.AddString(arg);
                diagnostics_builder.AddSourceRange(GetCharSourceRange(ast_arg));
              }
              metadata["fifos"][arg]["produced_by"] = {
                  task_name, metadata["tasks"][task_name].size() - 1};
            };
            if (IsTapaType(param, "mmap")) {
              param_cat = "mmap";
              // vector invocation can map mmaps to mmap
              register_arg(
                  get_name(arg_name, mmaps_access_pos[arg_name]++, decl_ref));

            } else if (IsTapaType(param, "immap")) {
              param_cat = "immap";
              // vector invocation can map mmaps to mmap
              register_arg(
                  get_name(arg_name, mmaps_access_pos[arg_name]++, decl_ref));

            } else if (IsTapaType(param, "ommap")) {
              param_cat = "ommap";
              // vector invocation can map mmaps to mmap
              register_arg(
                  get_name(arg_name, mmaps_access_pos[arg_name]++, decl_ref));

            } else if (IsTapaType(param, "async_mmap")) {
              param_cat = "async_mmap";
              // vector invocation can map mmaps to async_mmap
              register_arg(
                  get_name(arg_name, mmaps_access_pos[arg_name]++, decl_ref));
            } else if (IsTapaType(param, "istream")) {
              param_cat = "istream";
              // vector invocation can map istreams to istream
              auto arg =
                  get_name(arg_name, istreams_access_pos[arg_name]++, decl_ref);
              register_consumer(arg);
              register_arg(arg);
            } else if (IsTapaType(param, "ostream")) {
              param_cat = "ostream";
              // vector invocation can map ostreams to ostream
              auto arg =
                  get_name(arg_name, ostreams_access_pos[arg_name]++, decl_ref);
              register_producer(arg);
              register_arg(arg);
            } else if (IsTapaType(param, "istreams")) {
              param_cat = "istream";
              for (size_t i = 0; i < GetArraySize(param); ++i) {
                auto arg = get_name(arg_name, istreams_access_pos[arg_name]++,
                                    decl_ref);
                register_consumer(arg);
                register_arg(arg, ArrayNameAt(param_name, i));
              }
            } else if (IsTapaType(param, "ostreams")) {
              param_cat = "ostream";
              for (size_t i = 0; i < GetArraySize(param); ++i) {
                auto arg = get_name(arg_name, ostreams_access_pos[arg_name]++,
                                    decl_ref);
                register_producer(arg);
                register_arg(arg, ArrayNameAt(param_name, i));
              }
            } else if (arg_is_seq) {
              param_cat = "scalar";
              register_arg("64'd" + std::to_string(seq_access_pos[arg]++));
            } else {
              param_cat = "scalar";
              register_arg();
            }
          }
          continue;
        } else if (const auto string_literal = dyn_cast<StringLiteral>(arg)) {
          if (i == 1 && has_name) {
            (*metadata["tasks"][task_name].rbegin())["name"] =
                string_literal->getString();
            continue;
          }
        }
        static const auto diagnostic_id =
            this->context_.getDiagnostics().getCustomDiagID(
                clang::DiagnosticsEngine::Error, "unexpected argument: %0");
        auto diagnostics_builder = this->context_.getDiagnostics().Report(
            arg->getBeginLoc(), diagnostic_id);
        diagnostics_builder.AddString(arg->getStmtClassName());
        diagnostics_builder.AddSourceRange(GetCharSourceRange(arg));
      }
    }
  }

  for (auto fifo = metadata["fifos"].begin();
       fifo != metadata["fifos"].end();) {
    const auto is_consumed = fifo.value().contains("consumed_by");
    const auto is_produced = fifo.value().contains("produced_by");
    const auto& fifo_name = fifo.key();
    const auto fifo_decl = fifo_decls.find(fifo_name);
    auto& diagnostics = context_.getDiagnostics();
    if (!is_consumed && !is_produced) {
      static const auto diagnostic_id = diagnostics.getCustomDiagID(
          clang::DiagnosticsEngine::Warning, "unused stream: %0");
      auto diagnostics_builder =
          diagnostics.Report(fifo_decl->second->getBeginLoc(), diagnostic_id);
      diagnostics_builder.AddString(fifo_name);
      diagnostics_builder.AddSourceRange(
          GetCharSourceRange(fifo_decl->second->getSourceRange()));
      fifo = metadata["fifos"].erase(fifo);
    } else {
      ++fifo;
      if (fifo_decl != fifo_decls.end() && is_consumed != is_produced) {
        static const auto consumed_diagnostic_id =
            diagnostics.getCustomDiagID(clang::DiagnosticsEngine::Error,
                                        "consumed but not produced stream: %0");
        static const auto produced_diagnostic_id =
            diagnostics.getCustomDiagID(clang::DiagnosticsEngine::Error,
                                        "produced but not consumed stream: %0");
        auto diagnostics_builder = diagnostics.Report(
            fifo_decl->second->getBeginLoc(),
            is_consumed ? consumed_diagnostic_id : produced_diagnostic_id);
        diagnostics_builder.AddString(fifo_name);
        diagnostics_builder.AddSourceRange(
            GetCharSourceRange(fifo_decl->second->getSourceRange()));
      }
    }
  }
}

// Apply tapa s2s transformations on a lower-level task.
void Visitor::ProcessLowerLevelTaskFunc(const clang::FunctionDecl* func) {
  current_target->RewriteLowerLevelFunc(func, GetRewriter());

  // The task metadata should only be obtained from the function definition
  if (!func->isThisDeclarationADefinition()) return;

  auto& metadata = GetMetadata();
  ProcessTaskPorts(*current_task, metadata);
}

void Visitor::ProcessOtherFunc(const FunctionDecl* func) {
  current_target->RewriteOtherFunc(func, GetRewriter());
}

SourceLocation Visitor::GetEndOfLoc(SourceLocation loc) {
  return loc.getLocWithOffset(Lexer::MeasureTokenLength(
      loc, GetRewriter().getSourceMgr(), GetRewriter().getLangOpts()));
}
CharSourceRange Visitor::GetCharSourceRange(SourceRange range) {
  return CharSourceRange::getCharRange(range.getBegin(),
                                       GetEndOfLoc(range.getEnd()));
}
CharSourceRange Visitor::GetCharSourceRange(const Stmt* stmt) {
  return GetCharSourceRange(stmt->getSourceRange());
}

int64_t Visitor::EvalAsInt(const Expr* expr) {
  clang::Expr::EvalResult result;
  if (expr->EvaluateAsInt(result, this->context_)) {
    return result.Val.getInt().getExtValue();
  }
  static const auto diagnostic_id =
      this->context_.getDiagnostics().getCustomDiagID(
          clang::DiagnosticsEngine::Error,
          "fail to evaluate as integer at compile time");
  this->context_.getDiagnostics()
      .Report(expr->getBeginLoc(), diagnostic_id)
      .AddSourceRange(this->GetCharSourceRange(expr));
  return -1;
}

template <typename T>
void Visitor::HandleAttrOnNodeWithBody(
    const T* node, const clang::Stmt* body,
    llvm::ArrayRef<const clang::Attr*> attrs) {
#define HANDLE_ATTR(FUNC_DECL, FUNC_STMT)                                      \
  if (std::is_base_of<clang::Decl, T>()) {                                     \
    current_target->FUNC_DECL((const clang::Decl*)(node), attr, GetRewriter(), \
                              body);                                           \
  } else if (std::is_base_of<clang::Stmt, T>()) {                              \
    current_target->FUNC_STMT((const clang::Stmt*)(node), attr, GetRewriter(), \
                              body);                                           \
  }

  for (const auto* attr : attrs) {
    if (clang::isa<clang::TapaPipelineAttr>(attr)) {
      HANDLE_ATTR(RewritePipelinedDecl, RewritePipelinedStmt);
    } else if (clang::isa<clang::TapaUnrollAttr>(attr)) {
      HANDLE_ATTR(RewriteUnrolledDecl, RewriteUnrolledStmt);
    }
  }
}

}  // namespace internal
}  // namespace tapa

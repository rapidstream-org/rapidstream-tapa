#include "base_target.h"

namespace tapa {
namespace internal {

void BaseTarget::AddCodeForTopLevelFunc(ADD_FOR_FUNC_ARGS_DEF) {}

void BaseTarget::AddCodeForStream(ADD_FOR_PARAMS_ARGS_DEF) {}
void BaseTarget::AddCodeForTopLevelStream(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForStream(ADD_FOR_PARAMS_ARGS);
}
void BaseTarget::AddCodeForMiddleLevelStream(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForStream(ADD_FOR_PARAMS_ARGS);
}
void BaseTarget::AddCodeForLowerLevelStream(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForStream(ADD_FOR_PARAMS_ARGS);
}

void BaseTarget::AddCodeForAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF) {}
void BaseTarget::AddCodeForTopLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForAsyncMmap(ADD_FOR_PARAMS_ARGS);
}
void BaseTarget::AddCodeForMiddleLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForAsyncMmap(ADD_FOR_PARAMS_ARGS);
}
void BaseTarget::AddCodeForLowerLevelAsyncMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForAsyncMmap(ADD_FOR_PARAMS_ARGS);
}

void BaseTarget::AddCodeForMmap(ADD_FOR_PARAMS_ARGS_DEF) {}
void BaseTarget::AddCodeForTopLevelMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForMmap(ADD_FOR_PARAMS_ARGS);
}
void BaseTarget::AddCodeForMiddleLevelMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForMmap(ADD_FOR_PARAMS_ARGS);
}
void BaseTarget::AddCodeForLowerLevelMmap(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForMmap(ADD_FOR_PARAMS_ARGS);
}

void BaseTarget::AddCodeForScalar(ADD_FOR_PARAMS_ARGS_DEF) {}
void BaseTarget::AddCodeForTopLevelScalar(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForScalar(ADD_FOR_PARAMS_ARGS);
}
void BaseTarget::AddCodeForMiddleLevelScalar(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForScalar(ADD_FOR_PARAMS_ARGS);
}
void BaseTarget::AddCodeForLowerLevelScalar(ADD_FOR_PARAMS_ARGS_DEF) {
  AddCodeForScalar(ADD_FOR_PARAMS_ARGS);
}

void BaseTarget::RewriteFuncBody(REWRITE_FUNC_ARGS_DEF,
                                 std::vector<std::string> lines) {
  rewriter.InsertTextAfterToken(func->getBody()->getBeginLoc(),
                                llvm::join(lines, "\n"));
}

void BaseTarget::RewriteTopLevelFuncBody(REWRITE_FUNC_ARGS_DEF,
                                         std::vector<std::string> lines) {
  RewriteFuncBody(REWRITE_FUNC_ARGS, lines);
}
void BaseTarget::RewriteMiddleLevelFuncBody(REWRITE_FUNC_ARGS_DEF,
                                            std::vector<std::string> lines) {
  RewriteFuncBody(REWRITE_FUNC_ARGS, lines);
}
void BaseTarget::RewriteLowerLevelFuncBody(REWRITE_FUNC_ARGS_DEF,
                                           std::vector<std::string> lines) {
  RewriteFuncBody(REWRITE_FUNC_ARGS, lines);
}

void BaseTarget::RewriteFuncArguments(REWRITE_FUNC_ARGS_DEF, bool top) {}

}  // namespace internal
}  // namespace tapa

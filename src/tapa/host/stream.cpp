#include "tapa/host/stream.h"

#include <memory>
#include <string>
#include <string_view>

#include <glog/logging.h>

#include "tapa/host/internal_util.h"

namespace tapa {
namespace internal {

std::unique_ptr<type_erased_queue::LogContext>
type_erased_queue::LogContext::New(std::string_view name) {
  if (name.empty()) return nullptr;

  const char* debug_stream_dir = getenv("TAPA_STREAM_LOG_DIR");
  if (debug_stream_dir == nullptr) return nullptr;

  const std::string file_path = StrCat({debug_stream_dir, "/", name, ".txt"});
  std::ofstream ofs(file_path);
  if (ofs.fail()) {
    LOG(ERROR) << "failed to log channel '" << name << "' in '" << file_path
               << "'";
    return nullptr;
  }

  LOG(INFO) << "channel '" << name << "' is logged in '" << file_path << "'";
  auto log_context = std::make_unique<type_erased_queue::LogContext>();
  log_context->ofs = std::move(ofs);
  return log_context;
}

const std::string& type_erased_queue::get_name() const { return this->name; }
void type_erased_queue::set_name(const std::string& name) { this->name = name; }

type_erased_queue::type_erased_queue(const std::string& name)
    : name(name), log(LogContext::New(name)) {}

void type_erased_queue::check_leftover() const {
  if (!this->empty()) {
    LOG(WARNING) << "channel '" << this->name
                 << "' destructed with leftovers; hardware behavior may be "
                    "unexpected in consecutive invocations";
  }
}

}  // namespace internal
}  // namespace tapa

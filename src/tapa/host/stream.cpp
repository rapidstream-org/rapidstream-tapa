#include "tapa/host/stream.h"

#include <glog/logging.h>

namespace tapa {
namespace internal {

const std::string& type_erased_queue::get_name() const { return this->name; }
void type_erased_queue::set_name(const std::string& name) { this->name = name; }

type_erased_queue::type_erased_queue(const std::string& name) : name(name) {}

void type_erased_queue::check_leftover() const {
  if (!this->empty()) {
    LOG(WARNING) << "channel '" << this->name
                 << "' destructed with leftovers; hardware behavior may be "
                    "unexpected in consecutive invocations";
  }
}

}  // namespace internal
}  // namespace tapa

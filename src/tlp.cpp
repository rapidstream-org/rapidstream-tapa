#include "tlp.h"

namespace tlp {

pull_type* current_handle;

void task::wait_for(int step) {
  for (bool active = true; active;) {
    active = false;
    for (auto& p : this->coroutines) {
      if (p.first > step) break;
      for (auto& coroutine : p.second) {
        if (coroutine) {
          active |= p.first == step;
          current_handle = handle_table[&coroutine];
          coroutine();
        }
      }
    }
  }
  this->coroutines.erase(step);
}

}  // namespace tlp
#include "config.h"
#include <atomic>

namespace config {
    std::atomic<bool> isRunning{true};
}

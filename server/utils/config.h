#pragma once
#include <string>
#include <atomic>

namespace config {
    constexpr int SERVER_PORT = 8888;
    constexpr int MAX_EVENTS = 1024;
    constexpr int HEARTBEAT_TIMEOUT = 90;
    constexpr int HEARTBEAT_CHECK_INTERVAL = 10;
    const std::string HEARTBEAT_REQUEST = "PING";
    const std::string HEARTBEAT_RESPONSE = "PONG";

    extern std::atomic<bool> isRunning;
}

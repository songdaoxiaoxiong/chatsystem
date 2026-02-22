#pragma once
#include <string>
#include <atomic>

namespace config {
    constexpr int SERVER_PORT = 8888;
    constexpr int MAX_EVENTS = 10240;
    constexpr int HEARTBEAT_TIMEOUT = 90;
    constexpr int HEARTBEAT_CHECK_INTERVAL = 10;
    const std::string HEARTBEAT_REQUEST = "PING";
    const std::string HEARTBEAT_RESPONSE = "PONG";

    constexpr int MSG_TYPE_LEN = 8;          // 八位消息类型
    constexpr int LOGIN_MSG_TYPE = 1;        // 登录消息类型标识
    constexpr int NORMAL_MSG_TYPE = 2;       // 普通消息类型标识

    extern std::atomic<bool> isRunning;
}

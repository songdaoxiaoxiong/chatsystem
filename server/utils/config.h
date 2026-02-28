// server/utils/config.h 新增UDP和语音相关配置
#pragma once
#include <string>
#include <atomic>

namespace config {
    constexpr int SERVER_PORT = 8888;
    constexpr int UDP_VOICE_PORT = 8889; // 语音UDP服务端口
    constexpr int MAX_EVENTS = 10240;
    constexpr int HEARTBEAT_TIMEOUT = 90;
    constexpr int HEARTBEAT_CHECK_INTERVAL = 10;
    const std::string HEARTBEAT_REQUEST = "PING";
    const std::string HEARTBEAT_RESPONSE = "PONG";

    constexpr int MSG_TYPE_LEN = 8;          
    constexpr int LOGIN_MSG_TYPE = 1;        
    constexpr int NORMAL_MSG_TYPE = 2;       
    constexpr int UDP_PORT_BIND_TYPE = 3;   // 新增：UDP端口绑定消息类型

    extern std::atomic<bool> isRunning;
}

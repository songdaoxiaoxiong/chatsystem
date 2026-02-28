// server/udp/voice_server.h
#pragma once
#include <atomic>
#include <thread>

namespace voice_udp {
    void startVoiceServer();
    void stopVoiceServer();
}

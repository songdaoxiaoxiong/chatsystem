#include "heartbeat_checker.h"
#include <thread>
#include <chrono>
#include <iostream>
#include <atomic>
#include "../client/client_manager.h"
#include "../utils/config.h"

static std::thread hbThread;
static std::atomic<bool> hbRunning{false};

static void hbLoop() {
    using namespace std::chrono;
    while (hbRunning && config::isRunning) {
        std::this_thread::sleep_for(seconds(config::HEARTBEAT_CHECK_INTERVAL));
        if (!config::isRunning) break;

        auto now = steady_clock::now();
            auto snap = ClientManager::snapshot();
            for (const auto& p : snap) {
                int sock = p.first;
                const ClientInfo& client = p.second;
                auto duration = duration_cast<seconds>(now - client.last_active_time);
                if (duration.count() >= config::HEARTBEAT_TIMEOUT) {
                    std::string clientKey = client.ip + ":" + std::to_string(client.port);
                    std::cout << "⏰ 客户端 [" << clientKey << "] 心跳超时（" << config::HEARTBEAT_TIMEOUT << "秒无交互），主动断开" << std::endl;
                    ClientManager::removeClient(sock);
                }
            }
    }
}

void heartbeat::startHeartbeatChecker() {
    hbRunning = true;
    hbThread = std::thread(hbLoop);
}

void heartbeat::stopHeartbeatChecker() {
    hbRunning = false;
    if (hbThread.joinable()) hbThread.join();
}

#include "message_forwarder.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <iostream>
#include <sys/socket.h>
#include "../client/client_manager.h"
#include "../utils/config.h"

static std::queue<std::tuple<std::string, std::string, std::string>> msgQueue;
static std::mutex queueMutex;
static std::condition_variable queueCond;
static std::thread forwarderThread;
static std::atomic<bool> forwarderRunning{false};

void message::enqueueMessage(const std::string& target, const std::string& sender, const std::string& msg) {
    std::lock_guard<std::mutex> lock(queueMutex);
    msgQueue.emplace(target, sender, msg);
    queueCond.notify_one();
}

static void forwarderLoop() {
    while (forwarderRunning && config::isRunning) {
        std::unique_lock<std::mutex> lock(queueMutex);
        queueCond.wait_for(lock, std::chrono::seconds(5), [] { return !msgQueue.empty(); });
        if (!config::isRunning) break;
        if (msgQueue.empty()) continue;
        auto [target, sender, msg] = msgQueue.front();
        msgQueue.pop();
        lock.unlock();

        int targetSock = ClientManager::findSockByKey(target);
        if (targetSock != -1) {
            std::string fullMsg = sender + ":" + msg;
            ssize_t ret = send(targetSock, fullMsg.c_str(), fullMsg.length(), MSG_NOSIGNAL);
            if (ret > 0) continue;
        }

        // 未在线或发送失败，重新入队
        std::lock_guard<std::mutex> lk(queueMutex);
        msgQueue.emplace(target, sender, msg);
        std::cout << "⚠️ 目标 " << target << " 未在线，消息已缓存" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void message::startForwarder() {
    forwarderRunning = true;
    forwarderThread = std::thread(forwarderLoop);
}

void message::stopForwarder() {
    forwarderRunning = false;
    queueCond.notify_one();
    if (forwarderThread.joinable()) forwarderThread.join();
}

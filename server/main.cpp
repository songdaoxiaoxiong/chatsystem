#include <iostream>
#include <thread>
#include <signal.h>
#include "reactor/epoll_reactor.h"
#include "thread/thread_pool.h"
#include "message/message_forwarder.h"
#include "heartbeat/heartbeat_checker.h"
#include "utils/config.h"

#include "client/client_manager.h"

void printConnectionCount() {
    while (config::isRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (!config::isRunning) break;
        std::cout << "\n📊 当前长连接数量：" << ClientManager::size() << std::endl;
    }
}

int main() {
    std::thread connCountThread(printConnectionCount);
    message::startForwarder();
    heartbeat::startHeartbeatChecker();
    std::thread reactorThread(reactor::reactorMain);

    std::cout << "服务端运行中，按Ctrl+C退出..." << std::endl;
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    int sig;
    sigwait(&sigset, &sig);

    config::isRunning = false;
    message::stopForwarder();
    heartbeat::stopHeartbeatChecker();

    if (reactorThread.joinable()) reactorThread.join();
    if (connCountThread.joinable()) connCountThread.join();

    std::cout << "服务端已退出" << std::endl;
    return 0;
}

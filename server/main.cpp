// server/main.cpp 新增UDP语音服务启动/停止
//#define NO_COUT

#ifdef NO_COUT
    #include <iostream>
    struct DisableCout {
        DisableCout() {
            std::ios_base::sync_with_stdio(false);
            std::cout.setstate(std::ios_base::failbit);
            std::cerr.setstate(std::ios_base::failbit);
            std::clog.setstate(std::ios_base::failbit);
        }
    };
    static DisableCout disable_cout_obj;
#else
    #include <iostream>
#endif

#include <thread>
#include <signal.h>
#include "reactor/epoll_reactor.h"
#include "thread/thread_pool.h"
#include "message/message_forwarder.h"
#include "heartbeat/heartbeat_checker.h"
#include "utils/config.h"
#include "client/client_manager.h"
#include "udp/voice_server.h" // 新增UDP语音服务头文件

void printConnectionCount() {
    while (config::isRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (!config::isRunning) break;
        std::cout << "\n 当前长连接数量：" << ClientManager::size() << message::getmesqueue() << std::endl;
    }
}

int main() {
    std::thread connCountThread(printConnectionCount);
    message::startForwarder();
    heartbeat::startHeartbeatChecker();
    voice_udp::startVoiceServer(); // 启动UDP语音服务
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
    voice_udp::stopVoiceServer(); // 停止UDP语音服务

    if (reactorThread.joinable()) reactorThread.join();
    if (connCountThread.joinable()) connCountThread.join();

    std::cout << "服务端已退出" << std::endl;
    return 0;
}

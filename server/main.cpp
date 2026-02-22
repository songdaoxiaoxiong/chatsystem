//#define NO_COUT

#ifdef NO_COUT
    #include <iostream>
    // 程序启动时自动关闭所有标准输出
    struct DisableCout {
        DisableCout() {
            // 1. 关闭cout/cerr/clog的缓冲区同步
            std::ios_base::sync_with_stdio(false);
            // 2. 设置流为失败状态，所有输出操作都会直接返回
            std::cout.setstate(std::ios_base::failbit);
            std::cerr.setstate(std::ios_base::failbit);
            std::clog.setstate(std::ios_base::failbit);
        }
    };
    // 全局对象，程序启动时自动执行构造函数（无需手动调用）
    static DisableCout disable_cout_obj;
#else
    // 正常模式：不做任何修改，保留标准输出
    #include <iostream>
#endif

//#include <iostream>
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
        std::cout << "\n📊 当前长连接数量：" << ClientManager::size() <<message::getmesqueue()<< std::endl;
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

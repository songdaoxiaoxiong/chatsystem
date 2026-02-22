#include "epoll_reactor.h"
#include <iostream>
#include <cstdio>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include "../client/client_manager.h"
#include "../message/message_parser.h"
#include "../thread/thread_pool.h"
#include "../utils/config.h"

static bool setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) return false;
    return true;
}

static int createListenSocket() {
    int listenSock = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSock == -1) { perror("socket"); return -1; }

    int optVal = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(optVal));

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(config::SERVER_PORT);
    if (bind(listenSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("bind");
        close(listenSock);
        return -1;
    }
    if (listen(listenSock, SOMAXCONN) == -1) {
        perror("listen");
        close(listenSock);
        return -1;
    }
    return listenSock;
}

static void handleAcceptEvent(int epollFd, int listenSock) {
    std::cout << "🔍 接收到新连接请求" << std::endl;
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    int clientSock = accept(listenSock, (struct sockaddr*)&clientAddr, &clientAddrLen);
    if (clientSock == -1) return;
    if (!setNonBlocking(clientSock)) { close(clientSock); return; }

    char clientIp[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));
    uint16_t clientPort = ntohs(clientAddr.sin_port);

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = clientSock;
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, clientSock, &ev) == -1) { close(clientSock); return; }

    ClientInfo c;
    c.sock = clientSock;
    c.ip = clientIp;
    c.port = clientPort;
    c.recvCache = "";
    c.last_active_time = std::chrono::steady_clock::now();
    ClientManager::addClient(c);

    std::cout << "✅ 客户端 [" << clientIp << ":" << clientPort << "] 已连接（epoll Reactor）" << std::endl;
}

static void handleClientReadEvent(int clientSock) {
    char recvBuf[1024] = {0};
    ssize_t ret = recv(clientSock, recvBuf, sizeof(recvBuf)-1, 0);

    ClientInfo client;
    if (!ClientManager::getClient(clientSock, client)) return;
    std::string clientKey = client.ip + ":" + std::to_string(client.port);

    if (ret <= 0) {
        if (ret == 0) std::cout << "❌ 客户端 ["<<clientKey<<"] 主动断开111"<<std::endl;
        else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recv failed");
            std::cout<<"❌ 客户端 ["<<clientKey<<"] 连接异常"<<std::endl;
        } else return;
        std::cout<<222;
        ClientManager::removeClient(clientSock);
        return;
    }
    std::string recvData(recvBuf, ret);
    
    if (recvData.length() >= 4 && recvData.substr(0, 4) == "PING") {
        send(clientSock, config::HEARTBEAT_RESPONSE.c_str(), config::HEARTBEAT_RESPONSE.length(), MSG_NOSIGNAL);
        //std::cout << "❤️ 收到客户端 ["<<clientKey<<"] 心跳包，响应PONG"<<std::endl;
        ClientManager::updateLastActive(clientSock);
        return;
    }
//std::cout<< "📨 收到客户端 ["<<clientKey<<"] 消息：" << recvData << std::endl;
    ClientManager::updateLastActive(clientSock);
    ClientManager::appendRecvCache(clientSock, recvBuf);
    // 将解析任务交给线程池，在 ClientManager 内部安全地移动并更新缓存
    SingletonThreadPool::getInstance().submit([clientSock, clientKey](){
        ClientManager::processRecvCache(clientSock, clientKey);
    });
}

void reactor::reactorMain() {
    int epollFd = epoll_create1(0);
    if (epollFd == -1) { return; }
    int listenSock = createListenSocket();
    if (listenSock == -1) { std::cerr << "createListenSocket failed" << std::endl; close(epollFd); return; }
    if (!setNonBlocking(listenSock)) { close(listenSock); close(epollFd); return; }

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listenSock;
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, listenSock, &ev) == -1) {  close(listenSock); close(epollFd); return; }

    struct epoll_event events[config::MAX_EVENTS];
    std::cout << "🔍 等待客户端连接...（按Ctrl+C退出）" << std::endl;

    while (config::isRunning) {
        int nfds = epoll_wait(epollFd, events, config::MAX_EVENTS, 1000);
        if (nfds == -1) {
            if (errno == EINTR) continue;
            break;
        }
        for (int i=0;i<nfds;++i) {
            int fd = events[i].data.fd;
            if (fd == listenSock) handleAcceptEvent(epollFd, listenSock);
            else if (events[i].events & EPOLLIN) handleClientReadEvent(fd);
        }
    }

    close(listenSock);
    close(epollFd);
    ClientManager::closeAll();
}

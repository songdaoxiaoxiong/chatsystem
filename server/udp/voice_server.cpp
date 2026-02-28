// server/udp/voice_server.cpp
#include "voice_server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <cstring>
#include "../client/client_manager.h"
#include "../utils/config.h"

static int udpSock = -1;
static std::thread udpThread;
static std::atomic<bool> udpRunning{false};

// 设置UDP套接字为非阻塞
static bool setUdpNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

// 解析语音包：格式 [senderId:receiverId][语音数据]
static bool parseVoicePacket(const char* buf, int len, std::string& senderId, std::string& receiverId, const char*& voiceData, int& voiceLen) {
    if (len < 16) return false; // 最小长度校验（senderId:receiverId至少占一定长度）
    
    std::string header(buf, len);
    size_t colonPos = header.find(':');
    size_t splitPos = header.find('|'); // 分隔符：senderId:receiverId|语音数据
    if (colonPos == std::string::npos || splitPos == std::string::npos) {
        return false;
    }

    senderId = header.substr(0, colonPos);
    receiverId = header.substr(colonPos + 1, splitPos - colonPos - 1);
    voiceData = buf + splitPos + 1;
    voiceLen = len - (splitPos + 1);
    
    return !senderId.empty() && !receiverId.empty() && voiceLen > 0;
}

// UDP语音服务主循环
static void udpLoop() {
    char recvBuf[4096] = {0}; // 语音包缓冲区（适配常见音频帧大小）
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    while (udpRunning && config::isRunning) {
        // 接收UDP数据
        ssize_t recvLen = recvfrom(udpSock, recvBuf, sizeof(recvBuf), 0, 
                                  (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (recvLen <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000); // 无数据时短暂休眠，降低CPU占用
                continue;
            }
            perror("recvfrom failed");
            break;
        }

        // 解析语音包（仅用于日志和校验，不影响转发）
        std::string senderId, receiverId;
        const char* voiceData = nullptr;
        int voiceLen = 0;
        if (!parseVoicePacket(recvBuf, recvLen, senderId, receiverId, voiceData, voiceLen)) {
            std::cerr << "❌ 语音包解析失败，长度：" << recvLen << std::endl;
            continue;
        }

        // 获取接收方UDP信息
        std::string targetIp;
        uint16_t targetPort = 0;
        std::cout << "📨 收到语音包 - 发送者：" << senderId << " 接收者：" << receiverId 
                  << " 完整包长度：" << recvLen << " 语音数据长度：" << voiceLen << std::endl;
        if (!ClientManager::getClientUdpInfo(receiverId, targetIp, targetPort)) {
            std::cerr << "❌ 接收方[" << receiverId << "]未绑定UDP信息，丢弃语音包" << std::endl;
            continue;
        }

        // 构造目标地址
        struct sockaddr_in targetAddr;
        memset(&targetAddr, 0, sizeof(targetAddr));
        targetAddr.sin_family = AF_INET;
        targetAddr.sin_port = htons(targetPort);
        if (inet_pton(AF_INET, targetIp.c_str(), &targetAddr.sin_addr) <= 0) {
            std::cerr << "❌ 接收方IP格式错误：" << targetIp << std::endl;
            continue;
        }

        // 核心修改：转发完整的原始数据包（包含包头+语音数据）
        ssize_t sendLen = sendto(udpSock, recvBuf, recvLen, 0,  // 改为recvBuf和recvLen
                                (struct sockaddr*)&targetAddr, sizeof(targetAddr));
        if (sendLen != recvLen) { // 校验长度改为recvLen
            std::cerr << "⚠️ 语音包转发失败 - 发送者：" << senderId << " 接收者：" << receiverId 
                      << " 预期发送：" << recvLen << " 实际发送：" << sendLen << std::endl;
        } else {
            std::cout << "🎤 语音包转发成功 - 发送者：" << senderId << " 接收者：" << receiverId 
                      << " 完整包长度：" << recvLen << std::endl;
        }

        memset(recvBuf, 0, sizeof(recvBuf)); // 清空缓冲区
    }
}

// 启动UDP语音服务器
void voice_udp::startVoiceServer() {
    // 创建UDP套接字
    udpSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSock == -1) {
        perror("create udp socket failed");
        return;
    }

    // 设置非阻塞
    if (!setUdpNonBlocking(udpSock)) {
        perror("set udp non-blocking failed");
        close(udpSock);
        udpSock = -1;
        return;
    }

    // 绑定端口
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(config::UDP_VOICE_PORT);

    if (bind(udpSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("udp bind failed");
        close(udpSock);
        udpSock = -1;
        return;
    }

    // 启动UDP线程
    udpRunning = true;
    udpThread = std::thread(udpLoop);
    std::cout << "🎧 UDP语音服务器启动成功，端口：" << config::UDP_VOICE_PORT << std::endl;
}

// 停止UDP语音服务器
void voice_udp::stopVoiceServer() {
    udpRunning = false;
    if (udpThread.joinable()) {
        udpThread.join();
    }
    if (udpSock != -1) {
        close(udpSock);
        udpSock = -1;
    }
    std::cout << "🎧 UDP语音服务器已停止" << std::endl;
}

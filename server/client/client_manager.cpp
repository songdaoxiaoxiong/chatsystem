#include "client_manager.h"
#include <unistd.h>
#include <iostream>
#include <vector>
#include <utility>
#include <unordered_map>
#include <mutex>
#include "../message/message_parser.h"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <errno.h>

// ===================== 静态成员变量定义（类内声明，类外定义） =====================
// 原有基础成员
std::unordered_map<int, ClientInfo> ClientManager::clients{};
std::mutex ClientManager::clientsMutex{};

// 新增登录映射成员
std::unordered_map<std::string, std::string> ClientManager::userIdToClientKey{};
std::unordered_map<std::string, std::string> ClientManager::clientKeyToUserId{};
std::mutex ClientManager::idMapMutex{};

// 发送缓存静态成员
std::unordered_map<int, std::string> ClientManager::s_sendCache{};
std::mutex ClientManager::s_sendCacheMutex{};
int ClientManager::s_epollFd = -1;

// ===================== 原有基础函数实现（已存在，补充到此处） =====================
void ClientManager::addClient(const ClientInfo& c) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    clients[c.sock] = c;
}

void ClientManager::removeClient(int sock) {
    std::string clientKey;
    {
        std::lock_guard<std::mutex> lock(clientsMutex); 
        auto it = clients.find(sock);
        if (it == clients.end()) {
            return; 
        }

        clientKey = it->second.ip + ":" + std::to_string(it->second.port);
        close(sock);
        clients.erase(it);
        std::cout << "sasa" << std::endl;
    } 


    if (!clientKey.empty()) {
        unbindUserId(clientKey); // 解绑用户ID
        std::cout << "❌ 客户端 [" << clientKey << "] 已断开并解绑用户ID" << std::endl;
    }
}

bool ClientManager::getClient(int sock, ClientInfo& out) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    auto it = clients.find(sock);
    if (it == clients.end()) return false;
    out = it->second;
    return true;
}

int ClientManager::findSockByKey(const std::string& key) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (const auto& p : clients) {
        std::string k = p.second.ip + ":" + std::to_string(p.second.port);
        if (k == key) return p.first;
    }
    return -1;
}

void ClientManager::updateLastActive(int sock) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    auto it = clients.find(sock);
    if (it != clients.end()) {
        it->second.last_active_time = std::chrono::steady_clock::now();
    }
}

void ClientManager::appendRecvCache(int sock, const std::string& data) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    auto it = clients.find(sock);
    if (it != clients.end()) {
        it->second.recvCache.append(data);
    }
}

size_t ClientManager::size() {
    std::lock_guard<std::mutex> lock(clientsMutex);
    return clients.size();
}

std::vector<std::pair<int, ClientInfo>> ClientManager::snapshot() {
    std::lock_guard<std::mutex> lock(clientsMutex);
    std::vector<std::pair<int, ClientInfo>> out;
    out.reserve(clients.size());
    for (const auto& p : clients) {
        out.emplace_back(p.first, p.second);
    }
    return out;
}

void ClientManager::closeAll() {
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (auto& p : clients) {
        close(p.first);
        // 解绑所有客户端的用户ID
        std::string clientKey = p.second.ip + ":" + std::to_string(p.second.port);
        unbindUserId(clientKey);
    }
    clients.clear();
}

// ===================== 新增登录映射函数实现（补充缺失的实现） =====================
bool ClientManager::bindUserId(const std::string& userId, const std::string& clientKey) {
    std::lock_guard<std::mutex> lock(idMapMutex);
    
    // 1. 先解绑该客户端已绑定的旧用户ID（避免一个客户端绑定多个用户）
    auto oldUserIdIt = clientKeyToUserId.find(clientKey);
    if (oldUserIdIt != clientKeyToUserId.end()) {
        userIdToClientKey.erase(oldUserIdIt->second); // 删除旧用户ID的映射
    }
    
    // 2. 绑定新的用户ID <-> 客户端Key
    userIdToClientKey[userId] = clientKey;
    clientKeyToUserId[clientKey] = userId;
    
    // 3. 同步更新ClientInfo中的userId字段
    std::lock_guard<std::mutex> clientLock(clientsMutex);
    for (auto& p : clients) {
        std::string ck = p.second.ip + ":" + std::to_string(p.second.port);
        if (ck == clientKey) {
            p.second.userId = userId;
            break;
        }
    }
    
    std::cout << "✅ 用户[" << userId << "] 绑定客户端[" << clientKey << "] 成功" << std::endl;
    return true;
}

std::string ClientManager::findClientKeyByUserId(const std::string& userId) {
    std::lock_guard<std::mutex> lock(idMapMutex);
    auto it = userIdToClientKey.find(userId);
    return (it != userIdToClientKey.end()) ? it->second : "";
}

std::string ClientManager::findUserIdByClientKey(const std::string& clientKey) {
    std::lock_guard<std::mutex> lock(idMapMutex);
    auto it = clientKeyToUserId.find(clientKey);
    return (it != clientKeyToUserId.end()) ? it->second : "";
}

void ClientManager::unbindUserId(const std::string& clientKey) {
    std::lock_guard<std::mutex> lock(idMapMutex);
    auto it = clientKeyToUserId.find(clientKey);
    if (it != clientKeyToUserId.end()) {
        // 删除双向映射
        std::cout<<333;
        userIdToClientKey.erase(it->second);
        clientKeyToUserId.erase(it);
        
        // 同步清空ClientInfo中的userId
        std::lock_guard<std::mutex> clientLock(clientsMutex);
        for (auto& p : clients) {
            std::string ck = p.second.ip + ":" + std::to_string(p.second.port);
            if (ck == clientKey) {
                p.second.userId = "";
                break;
            }
        }

    }
    std::cout << "❌ 客户端[" << clientKey << "] 解绑用户ID成功" << std::endl;
}

void ClientManager::processRecvCache(int sock, const std::string& clientKey) {
    std::string buf;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        auto it = clients.find(sock);
        if (it == clients.end()) return;
        buf = std::move(it->second.recvCache);
    }
    

    message_parser::processRecvCache(buf, clientKey);

    // 将剩余未处理的数据写回到客户端缓存
    std::lock_guard<std::mutex> lock(clientsMutex);
    auto it = clients.find(sock);
    if (it != clients.end()) {
        it->second.recvCache = std::move(buf);
        std::cout << "✅ 客户端[" << clientKey << "] 剩余缓存长度: " << it->second.recvCache.length() << std::endl;
    }
    
}

void ClientManager::setEpollFd(int epfd) {
    s_epollFd = epfd;
}

void ClientManager::appendSendCache(int fd, const std::string& data) {
    if (fd < 0) return;
    {
        std::lock_guard<std::mutex> lock(s_sendCacheMutex);
        s_sendCache[fd].append(data);
    }
    // 确保 epoll 监听写事件以便稍后可写时继续发送
    enableWriteEvent(fd);
}

void ClientManager::enableWriteEvent(int fd) {
    if (s_epollFd == -1) return;
    struct epoll_event ev;
    // 保持边缘触发和读事件，同时添加写事件
    ev.events = EPOLLIN | EPOLLET | EPOLLOUT;
    ev.data.fd = fd;
    if (epoll_ctl(s_epollFd, EPOLL_CTL_MOD, fd, &ev) == -1) {
        // 如果 MOD 失败，可能是还未注册为 fd，尝试 ADD
        if (epoll_ctl(s_epollFd, EPOLL_CTL_ADD, fd, &ev) == -1) {
            // 无法注册写事件，记录但不致命
            // perror("enableWriteEvent epoll_ctl");
        }
    }
}

void ClientManager::disableWriteEvent(int fd) {
    if (s_epollFd == -1) return;
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET; // 取消写事件
    ev.data.fd = fd;
    if (epoll_ctl(s_epollFd, EPOLL_CTL_MOD, fd, &ev) == -1) {
        // 若失败则记录但继续
        // perror("disableWriteEvent epoll_ctl");
    }
}

void ClientManager::handleWriteEvent(int fd) {
    // 尝试发送缓存中的数据，直到发生 EAGAIN 或全部发送完
    std::string buf;
    {
        std::lock_guard<std::mutex> lock(s_sendCacheMutex);
        auto it = s_sendCache.find(fd);
        if (it == s_sendCache.end() || it->second.empty()) {
            // 无缓存，取消写事件监听
            disableWriteEvent(fd);
            return;
        }
        buf = it->second; // 复制出来后释放锁再发送
    }

    ssize_t sent = 0;
    while (!buf.empty()) {
        ssize_t ret = send(fd, buf.data(), buf.size(), MSG_NOSIGNAL | MSG_DONTWAIT);
        if (ret > 0) {
            buf.erase(0, ret);
            sent += ret;
            continue;
        }
        if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break; // 发送缓冲区已满，等待下次可写
        }
        // 其他错误：关闭客户端并清理缓存
        std::cerr << "⚠️ send error on fd " << fd << " errno=" << errno << std::endl;
        removeClient(fd);
        std::lock_guard<std::mutex> lock(s_sendCacheMutex);
        s_sendCache.erase(fd);
        return;
    }

    // 更新缓存（可能为空）
    {
        std::lock_guard<std::mutex> lock(s_sendCacheMutex);
        if (buf.empty()) {
            s_sendCache.erase(fd);
            // 全部发送完，取消写事件
            disableWriteEvent(fd);
        } else {
            s_sendCache[fd] = buf;
            // 保持写事件监听
            enableWriteEvent(fd);
        }
    }
}
// 新增：绑定客户端UDP信息
bool ClientManager::bindClientUdpInfo(const std::string& userId, uint16_t udpPort, const std::string& udpIp) {
    std::lock_guard<std::mutex> idLock(idMapMutex);
    std::lock_guard<std::mutex> clientLock(clientsMutex);

    // 1. 通过用户ID找到客户端Key
    auto clientKeyIt = userIdToClientKey.find(userId);
    if (clientKeyIt == userIdToClientKey.end()) {
        std::cerr << "用户[" << userId << "]未登录，无法绑定UDP信息" << std::endl;
        return false;
    }
    std::string clientKey = clientKeyIt->second;

    // 2. 更新客户端UDP信息
    for (auto& p : clients) {
        std::string ck = p.second.ip + ":" + std::to_string(p.second.port);
        if (ck == clientKey) {
            p.second.udpIp = udpIp;
            p.second.udpPort = udpPort;
            std::cout << "✅ 用户[" << userId << "] 绑定UDP信息成功 - IP：" << udpIp << " 端口：" << udpPort << std::endl;
            return true;
        }
    }
    return false;
}

// 新增：通过用户ID获取UDP信息
bool ClientManager::getClientUdpInfo(const std::string& userId, std::string& outIp, uint16_t& outPort) {
    std::lock_guard<std::mutex> idLock(idMapMutex);
    std::lock_guard<std::mutex> clientLock(clientsMutex);

    // 1. 通过用户ID找到客户端Key
    auto clientKeyIt = userIdToClientKey.find(userId);
    if (clientKeyIt == userIdToClientKey.end()) {
        return false;
    }
    std::string clientKey = clientKeyIt->second;

    // 2. 查找客户端UDP信息
    for (const auto& p : clients) {
        std::string ck = p.second.ip + ":" + std::to_string(p.second.port);
        if (ck == clientKey) {
            outIp = p.second.udpIp;
            outPort = p.second.udpPort;
            return !outIp.empty() && outPort != 0;
        }
    }
    return false;
}
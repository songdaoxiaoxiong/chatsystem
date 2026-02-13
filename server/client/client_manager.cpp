#include "client_manager.h"
#include <unistd.h>
#include <iostream>
#include <vector>
#include <utility>
#include <unordered_map>
#include <mutex>

// ===================== 静态成员变量定义（类内声明，类外定义） =====================
// 原有基础成员
std::unordered_map<int, ClientInfo> ClientManager::clients{};
std::mutex ClientManager::clientsMutex{};

// 新增登录映射成员
std::unordered_map<std::string, std::string> ClientManager::userIdToClientKey{};
std::unordered_map<std::string, std::string> ClientManager::clientKeyToUserId{};
std::mutex ClientManager::idMapMutex{};

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

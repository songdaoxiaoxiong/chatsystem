#include "client_manager.h"
#include <unistd.h>
#include <iostream>
#include <vector>
#include <utility>

std::unordered_map<int, ClientInfo> ClientManager::clients;
std::mutex ClientManager::clientsMutex;

void ClientManager::addClient(const ClientInfo& c) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    clients[c.sock] = c;
}

void ClientManager::removeClient(int sock) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    auto it = clients.find(sock);
    if (it != clients.end()) {
        close(sock);
        clients.erase(it);
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
    if (it != clients.end()) it->second.last_active_time = std::chrono::steady_clock::now();
}

void ClientManager::appendRecvCache(int sock, const std::string& data) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    auto it = clients.find(sock);
    if (it != clients.end()) it->second.recvCache.append(data);
}

size_t ClientManager::size() {
    std::lock_guard<std::mutex> lock(clientsMutex);
    return clients.size();
}

std::vector<std::pair<int, ClientInfo>> ClientManager::snapshot() {
    std::lock_guard<std::mutex> lock(clientsMutex);
    std::vector<std::pair<int, ClientInfo>> out;
    out.reserve(clients.size());
    for (const auto& p : clients) out.emplace_back(p.first, p.second);
    return out;
}

void ClientManager::closeAll() {
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (auto& p : clients) close(p.first);
    clients.clear();
}

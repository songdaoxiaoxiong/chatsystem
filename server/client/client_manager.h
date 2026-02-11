#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <optional>
#include <vector>
#include <utility>
struct ClientInfo {
    int sock;
    std::string ip;
    uint16_t port;
    std::string recvCache;
    std::chrono::steady_clock::time_point last_active_time;
};

class ClientManager {
public:
    static void addClient(const ClientInfo& c);
    static void removeClient(int sock);
    static bool getClient(int sock, ClientInfo& out);
    static int findSockByKey(const std::string& key);
    static void updateLastActive(int sock);
    static void appendRecvCache(int sock, const std::string& data);
    static size_t size();
    static std::vector<std::pair<int, ClientInfo>> snapshot();
    static void closeAll();

private:
    static std::unordered_map<int, ClientInfo> clients;
    static std::mutex clientsMutex;
};

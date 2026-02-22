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
    std::string userId; // 新增：绑定的用户ID
};

class ClientManager {
public:
    static void addClient(const ClientInfo& c);
    static void removeClient(int sock);
    static bool getClient(int sock, ClientInfo& out);
    static int findSockByKey(const std::string& key);
    static void updateLastActive(int sock);
    static void appendRecvCache(int sock, const std::string& data);
    static void processRecvCache(int sock, const std::string& clientKey);
    static size_t size();
    static std::vector<std::pair<int, ClientInfo>> snapshot();
    static void closeAll();

    // 新增：登录相关映射管理
    static bool bindUserId(const std::string& userId, const std::string& clientKey); // clientKey = ip:port
    static std::string findClientKeyByUserId(const std::string& userId);
    static std::string findUserIdByClientKey(const std::string& clientKey);
    static void unbindUserId(const std::string& clientKey);

private:
    static std::unordered_map<int, ClientInfo> clients;
    static std::mutex clientsMutex;
    
    // 新增：ID映射存储
    static std::unordered_map<std::string, std::string> userIdToClientKey; // userId -> ip:port
    static std::unordered_map<std::string, std::string> clientKeyToUserId; // ip:port -> userId
    static std::mutex idMapMutex; // 保护ID映射的互斥锁
};

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
    uint16_t udpPort;         // 新增：客户端UDP语音端口
    std::string udpIp;        // 新增：客户端UDP IP（可能和TCP IP一致
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

    // 新增：绑定客户端UDP信息
    static bool bindClientUdpInfo(const std::string& userId, uint16_t udpPort, const std::string& udpIp);
    // 新增：通过用户ID获取UDP信息
    static bool getClientUdpInfo(const std::string& userId, std::string& outIp, uint16_t& outPort);
private:
    static std::unordered_map<int, ClientInfo> clients;
    static std::mutex clientsMutex;
    
    // 新增：ID映射存储
    static std::unordered_map<std::string, std::string> userIdToClientKey; // userId -> ip:port
    static std::unordered_map<std::string, std::string> clientKeyToUserId; // ip:port -> userId
    static std::mutex idMapMutex; // 保护ID映射的互斥锁

public:

    // 1. 追加发送缓存（缓冲区满时存储未发送的数据）
    static void appendSendCache(int fd, const std::string& data);

    // 2. 启用 EPOLLOUT 事件（监听发送缓冲区是否有空间）
    static void enableWriteEvent(int fd);

    // 3. 禁用 EPOLLOUT 事件（发送完成后取消监听）
    static void disableWriteEvent(int fd);

    // 4. 处理 EPOLLOUT 事件（缓冲区有空间时发送缓存数据）
    static void handleWriteEvent(int fd);

    // 设置 epoll fd（由 reactor 在启动时传入）
    static void setEpollFd(int epfd);

private:
    static std::unordered_map<int, std::string> s_sendCache;
    static std::mutex s_sendCacheMutex;
    // epoll fd，用于修改事件（注册/注销 EPOLLOUT）
    static int s_epollFd;
};

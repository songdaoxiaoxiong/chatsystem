#include "message_parser.h"
#include <string>
#include <iostream>
#include "message_forwarder.h"
#include "../thread/thread_pool.h"
#include "../client/client_manager.h"
#include "../utils/config.h"

// 原有parseTarget方法保持不变...

// 新增：普通消息解析实现
bool message_parser::parseNormalMessage(const std::string& normalMsgBody, std::string& senderId, std::string& receiverId, std::string& content) {
    // 假设普通消息格式：senderId:receiverId:content
    size_t firstColon = normalMsgBody.find(':');
    size_t secondColon = normalMsgBody.find(':', firstColon + 1);
    if (firstColon == std::string::npos || secondColon == std::string::npos) {
        return false;
    }
    senderId = normalMsgBody.substr(0, firstColon);
    receiverId = normalMsgBody.substr(firstColon + 1, secondColon - firstColon - 1);
    content = normalMsgBody.substr(secondColon + 1);
    return !senderId.empty() && !receiverId.empty() && !content.empty();
}

// 新增：登录消息解析实现
bool message_parser::parseLoginMessage(const std::string& loginMsgBody, std::string& userId, std::string& account, std::string& password) {
    // 登录消息格式：userId:account_密码（下划线分隔账号密码）
    size_t firstColon = loginMsgBody.find(':');
    size_t underline = loginMsgBody.find('_', firstColon + 1);
    if (firstColon == std::string::npos || underline == std::string::npos) {
        return false;
    }
    userId = loginMsgBody.substr(0, firstColon);
    account = loginMsgBody.substr(firstColon + 1, underline - firstColon - 1);
    password = loginMsgBody.substr(underline + 1);
    return !userId.empty() && !account.empty() && !password.empty();
}

// 修改processRecvCache方法，新增消息类型处理逻辑
void message_parser::processRecvCache(std::string& recvCache, const std::string& clientKey) {
    const int HEAD_LEN = 4;          // 四位长度
    const int MSG_TYPE_LEN = config::MSG_TYPE_LEN; // 八位消息类型
    const int MIN_BODY_LEN = MSG_TYPE_LEN + 1;     // 最小体部长度（类型+内容）

    while (true) {
        if (recvCache.length() < HEAD_LEN + MIN_BODY_LEN) break;

        // 解析消息头（四位长度）
        std::string headStr = recvCache.substr(0, HEAD_LEN);
        int bodyExpectedLen = 0;
        try {
            bodyExpectedLen = std::stoi(headStr);
        }
        catch (...) {
            recvCache.erase(0, 1);
            continue;
        }

        // 检查消息体长度是否足够
        if (recvCache.length() < HEAD_LEN + bodyExpectedLen) break;

        // 提取完整消息体
        std::string msgBody = recvCache.substr(HEAD_LEN, bodyExpectedLen);
        // 提取消息类型（前8位）
        if (msgBody.length() < MSG_TYPE_LEN) {
            recvCache.erase(0, HEAD_LEN + bodyExpectedLen);
            continue;
        }
        std::string typeStr = msgBody.substr(0, MSG_TYPE_LEN);
        int msgType = 0;
        try {
            msgType = std::stoi(typeStr);
        } catch (...) {
            recvCache.erase(0, HEAD_LEN + bodyExpectedLen);
            continue;
        }
        std::cout<< "📨 收到消息 - 客户端：" << clientKey << " 类型：" << msgType << " 内容长度：" << bodyExpectedLen - MSG_TYPE_LEN << std::endl;
        // 根据消息类型处理
        if (msgType == config::NORMAL_MSG_TYPE) {
            // 普通消息：提取类型字段后的剩余体部
            std::string normalMsgBody = msgBody.substr(MSG_TYPE_LEN);
            std::string senderId, receiverId, content;
            if (message_parser::parseNormalMessage(normalMsgBody, senderId, receiverId, content)) {
                // 异步插入数据库（占位符，后续替换为实际DB逻辑）
                SingletonThreadPool::getInstance().submit([senderId, receiverId, content](){
                    // DB insert: senderId, receiverId, content
                    std::cout << "准备插入数据库 - 发送者：" << senderId << " 接收者：" << receiverId << " 内容：" << content << std::endl;
                });

                // 查找接收者对应的客户端Key（ip:port）
                std::string targetClientKey = ClientManager::findClientKeyByUserId(receiverId);
                if (targetClientKey.empty()) {
                    std::cerr << "接收者[" << receiverId << "] 未登录，无法转发消息" << std::endl;
                    recvCache.erase(0, HEAD_LEN + bodyExpectedLen);
                    continue;
                }

                // 入队转发消息
                message::enqueueMessage(targetClientKey, senderId, headStr+typeStr+senderId+":"+receiverId+":"+content);
                std::cout << "消息入队成功 - 发送者：" << senderId << " 接收者：" << receiverId << " 内容：" << content << std::endl;
            } else {
                std::cerr << "普通消息解析失败，丢弃消息片段" << std::endl;
            }
        } 

        else if (msgType == config::LOGIN_MSG_TYPE) {
            // 登录消息：提取类型字段后的剩余体部
            std::string loginMsgBody = msgBody.substr(MSG_TYPE_LEN);
            std::string userId, account, password;
            if (message_parser::parseLoginMessage(loginMsgBody, userId, account, password)) {
                // 绑定用户ID与客户端Key（ip:port）
                bool bindOk = ClientManager::bindUserId(userId, clientKey);
                if (bindOk) {
                    // 异步记录登录日志（占位符）
                    SingletonThreadPool::getInstance().submit([userId, account, clientKey](){
                        std::cout << "用户[" << userId << "] 登录成功 - 账号：" << account << " 客户端：" << clientKey << std::endl;
                        // 可扩展：插入登录日志到数据库
                    });
                } else {
                    std::cerr << "用户[" << userId << "] 绑定客户端失败" << std::endl;
                }
            } else {
                std::cerr << "登录消息解析失败，丢弃消息片段" << std::endl;
            }
        }
        else {
            // 其他消息类型，暂不处理（可扩展）
            std::cerr << "不支持的消息类型：" << msgType << "，丢弃消息片段" << std::endl;
        }

        // 移除已处理的消息片段
        recvCache.erase(0, HEAD_LEN + bodyExpectedLen);
    }
}

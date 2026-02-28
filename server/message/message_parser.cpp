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

// 修正：UDP端口绑定消息解析（格式：senderId:receiverId:voice_call）
bool message_parser::parseUdpBindMessage(const std::string& udpBindMsgBody, std::string& senderId, std::string& receiverId) {
    // 消息体格式：senderId:receiverId:voice_call
    size_t firstColon = udpBindMsgBody.find(':');
    size_t secondColon = udpBindMsgBody.find(':', firstColon + 1);
    
    // 校验格式完整性 + voice_call 标识
    if (firstColon == std::string::npos || secondColon == std::string::npos) {
        std::cerr << "❌ 语音通话请求格式错误：缺少分隔符" << std::endl;
        return false;
    }

    // 解析字段
    senderId = udpBindMsgBody.substr(0, firstColon);
    receiverId = udpBindMsgBody.substr(firstColon + 1, secondColon - firstColon - 1);
    std::string reqType = udpBindMsgBody.substr(secondColon + 1);

    // 强制校验语音通话标识
    if (reqType != "voice_call") {
        std::cerr << "❌ 非语音通话请求，标识错误：" << reqType << std::endl;
        return false;
    }

    // 基础合法性校验
    if (senderId.empty() || receiverId.empty()) {
        std::cerr << "❌ 发送者/接收者ID为空 - 发送者：" << senderId << " 接收者：" << receiverId << std::endl;
        return false;
    }

    return true;
}


// 修改processRecvCache，增加UDP端口绑定消息处理
void message_parser::processRecvCache(std::string& recvCache, const std::string& clientKey) {
    const int HEAD_LEN = 4;          
    const int MSG_TYPE_LEN = config::MSG_TYPE_LEN; 
    const int MIN_BODY_LEN = MSG_TYPE_LEN + 1;     

    while (true) {
        if (recvCache.length() < HEAD_LEN + MIN_BODY_LEN) break;

        std::string headStr = recvCache.substr(0, HEAD_LEN);
        int bodyExpectedLen = 0;
        try {
            bodyExpectedLen = std::stoi(headStr);
        } catch (...) {
            recvCache.erase(0, 1);
            continue;
        }

        if (recvCache.length() < HEAD_LEN + bodyExpectedLen) break;

        std::string msgBody = recvCache.substr(HEAD_LEN, bodyExpectedLen);
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

        // ========== 修正：UDP端口绑定消息处理 ==========
        if (msgType == config::UDP_PORT_BIND_TYPE) {
            std::string udpBindMsgBody = msgBody.substr(MSG_TYPE_LEN);
            std::string senderId, receiverId;
            
            // 解析语音通话请求（发送者ID:接收者ID:voice_call）
            if (parseUdpBindMessage(udpBindMsgBody, senderId, receiverId)) {
                // 从客户端Key（ip:port）提取发送者的IP（客户端连接的IP）
                size_t colonPos = clientKey.find(':');
                if (colonPos == std::string::npos) {
                    std::cerr << "❌ 提取发送者IP失败 - ClientKey格式错误：" << clientKey << std::endl;
                    break;
                }
                std::string senderIp = clientKey.substr(0, colonPos);
                const uint16_t voiceUdpPort = 8890; // 固定绑定8890端口

                // 绑定：接收者ID 关联 发送者IP + 8890端口（作为接收UDP的地址）
                bool bindOk = ClientManager::bindClientUdpInfo(receiverId, voiceUdpPort, senderIp);
                if (bindOk) {
                    std::cout << "✅ 语音通话UDP地址绑定成功" << std::endl;
                    std::cout << "  ├─ 接收者ID：" << receiverId << std::endl;
                    std::cout << "  ├─ 接收UDP地址：" << senderIp << ":" << voiceUdpPort << std::endl;
                    std::cout << "  └─ 发送者ID：" << senderId << std::endl;
                } else {
                    std::cerr << "❌ 语音通话UDP地址绑定失败 - 接收者ID：" << receiverId << std::endl;
                }
            } else {
                std::cerr << "❌ 语音通话请求解析失败 - 消息体：" << udpBindMsgBody << std::endl;
            }
        }
        // ========== 原有：普通消息处理逻辑 ==========
        else if (msgType == config::NORMAL_MSG_TYPE) {
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
        // ========== 原有：登录消息处理逻辑 ==========
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
        } else {
            // 其他消息类型，暂不处理
            std::cerr << "不支持的消息类型：" << msgType << "，丢弃消息片段" << std::endl;
        }

        // 移除已处理的消息片段
        recvCache.erase(0, HEAD_LEN + bodyExpectedLen);
    }
}
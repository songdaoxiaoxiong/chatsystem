#pragma once
#include <string>
#include <cstdint> 
namespace message_parser {
    bool parseTarget(const std::string& msgBody, std::string& target, std::string& content);
    void processRecvCache(std::string& recvCache, const std::string& clientKey);
    
    bool parseNormalMessage(const std::string& normalMsgBody, std::string& senderId, std::string& receiverId, std::string& content);
    

    bool parseLoginMessage(const std::string& loginMsgBody, std::string& userId, std::string& account, std::string& password);

    bool parseLoginMessage(const std::string& loginMsgBody, std::string& userId, std::string& account, std::string& password);
    // 新增UDP端口绑定消息解析
    bool parseUdpBindMessage(const std::string& udpBindMsgBody, std::string& senderId, std::string& receiverId);
}

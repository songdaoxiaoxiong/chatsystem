#pragma once
#include <string>

namespace message_parser {
    bool parseTarget(const std::string& msgBody, std::string& target, std::string& content);
    void processRecvCache(std::string& recvCache, const std::string& clientKey);
}

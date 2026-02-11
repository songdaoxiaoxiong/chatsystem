#include "message_parser.h"
#include <string>
#include <iostream>
#include "message_forwarder.h"
#include "../thread/thread_pool.h"

bool message_parser::parseTarget(const std::string& msgBody, std::string& target, std::string& content) {
    size_t firstColon = msgBody.find(':');
    size_t secondColon = msgBody.find(':', firstColon + 1);

    if (firstColon == std::string::npos || secondColon == std::string::npos) {
        return false;
    }

    std::string ip = msgBody.substr(0, firstColon);
    std::string portStr = msgBody.substr(firstColon + 1, secondColon - firstColon - 1);

    if (ip.empty() || portStr.empty() || secondColon + 1 >= msgBody.size()) {
        return false;
    }

    try {
        std::stoi(portStr);
    }
    catch (...) {
        return false;
    }

    target = ip + ":" + portStr;
    content = msgBody.substr(secondColon + 1);
    return true;
}

void message_parser::processRecvCache(std::string& recvCache, const std::string& clientKey) {
    const int HEAD_LEN = 4;
    const int MIN_BODY_LEN = 5;

    while (true) {
        if (recvCache.length() < HEAD_LEN + MIN_BODY_LEN) break;

        std::string headStr = recvCache.substr(0, HEAD_LEN);
        int bodyExpectedLen = 0;
        try {
            bodyExpectedLen = std::stoi(headStr);
        }
        catch (...) {
            recvCache.erase(0, 1);
            continue;
        }

        if (recvCache.length() < HEAD_LEN + bodyExpectedLen) break;

        std::string msgBody = recvCache.substr(HEAD_LEN, bodyExpectedLen);
        std::string target, content;
        if (parseTarget(msgBody, target, content)) {
            SingletonThreadPool::getInstance().submit([clientKey, content](){
                // placeholder for DB insert
            });

            message::enqueueMessage(target, clientKey, content);
        }

        recvCache.erase(0, HEAD_LEN + bodyExpectedLen);
    }
}

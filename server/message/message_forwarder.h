#pragma once
#include <string>

namespace message {
    void startForwarder();
    void stopForwarder();
    void enqueueMessage(const std::string& target, const std::string& sender, const std::string& msg);
    int getmesqueue();
}

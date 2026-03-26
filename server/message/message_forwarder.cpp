#include "message_forwarder.h"
#include <atomic>
#include <condition_variable>
#include <thread>
#include <mutex>
#include <iostream>
#include <sys/socket.h>
#include <errno.h>
#include <tuple>
#include "../client/client_manager.h"
#include "../utils/config.h"
#include "../thread/thread_pool.h"

// Vyukov MPSC 无锁队列实现（Multiple Producers, Single Consumer）
struct MsgNode {
    std::atomic<MsgNode*> next;
    std::string target;
    std::string sender;
    std::string msg;
    MsgNode() : next(nullptr) {}
    MsgNode(const std::string& t, const std::string& s, const std::string& m)
        : next(nullptr), target(t), sender(s), msg(m) {}
};

static std::atomic<MsgNode*> lf_head{nullptr}; // 生产者使用（原子）
static MsgNode* lf_tail = nullptr;            // 消费者使用（非原子）
static std::atomic<int> lf_count{0};

static std::condition_variable queueCond;
static std::mutex queueWaitMutex; // 仅用于条件变量等待，无锁队列不依赖该互斥量
static std::thread forwarderThread;
static std::atomic<bool> forwarderRunning{false};

void message::enqueueMessage(const std::string& target, const std::string& sender, const std::string& msg) {
    MsgNode* node = new MsgNode(target, sender, msg);
    node->next.store(nullptr, std::memory_order_relaxed);

    // 原子交换头指针，得到前一个节点，然后把前一个节点的 next 指向新节点
    MsgNode* prev = lf_head.exchange(node, std::memory_order_acq_rel);
    prev->next.store(node, std::memory_order_release);

    lf_count.fetch_add(1, std::memory_order_relaxed);
    queueCond.notify_one();
}

int message::getmesqueue()
{
    return lf_count.load(std::memory_order_relaxed);
}

static void forwarderLoop() {
    while (forwarderRunning && config::isRunning) {
        MsgNode* next = lf_tail->next.load(std::memory_order_acquire);
        if (!next) {
            std::unique_lock<std::mutex> lock(queueWaitMutex);
            queueCond.wait_for(lock, std::chrono::seconds(5));
            if (!config::isRunning) break;
            continue;
        }

        // 推进尾指针并删除旧的占位节点
        MsgNode* old = lf_tail;
        lf_tail = next;
        // 现在 lf_tail 指向真正的数据节点，old 可以删掉
        delete old;

        // 取出数据并处理
        std::string target = lf_tail->target;
        std::string sender = lf_tail->sender;
        std::string msg = lf_tail->msg;

        lf_count.fetch_sub(1, std::memory_order_relaxed);
        // 将发送操作提交到线程池，多线程并发发送（仍由单消费者从队列弹出）
        auto task = [target, sender, msg]() {
            std::string targetClientKey = ClientManager::findClientKeyByUserId(target);
            int targetSock = -1;
            if (!targetClientKey.empty()) {
                targetSock = ClientManager::findSockByKey(targetClientKey);
            } else {
                targetSock = ClientManager::findSockByKey(target);
            }

            if (targetSock != -1) {
                const char* data = msg.c_str();
                size_t toSend = msg.length();
                ssize_t ret = send(targetSock, data, toSend, MSG_NOSIGNAL | MSG_DONTWAIT);
                if (ret >= 0) {
                    if ((size_t)ret == toSend) {
                        std::cout << "📤 消息转发成功 - 发送者：" << sender << " 接收者：" << target << " 内容：" << msg << std::endl;
                        return;
                    } else {
                        // 部分发送：将剩余写入应用层发送缓存，等待 EPOLLOUT 通知继续发送
                        std::string remaining(data + ret, toSend - ret);
                        std::cout << "⚠️ 目标 " << target << " 部分发送（" << ret << " 字节），剩余已写入发送缓存" << std::endl;
                        ClientManager::appendSendCache(targetSock, remaining);
                        return;
                    }
                } else {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // 发送缓冲区已满：将整条消息写入应用层发送缓存
                        std::cout << "⚠️ 目标 " << target << " 发送缓冲区已满，消息写入发送缓存等待重试" << std::endl;
                        ClientManager::appendSendCache(targetSock, msg);
                        return;
                    }
                    // 其他错误（例如连接被重置等）：关闭客户端并丢弃消息
                    std::cout << "⚠️ 目标 " << target << " 发送失败 errno=" << errno << "，关闭连接" << std::endl;
                    ClientManager::removeClient(targetSock);
                    return;
                }
            }

            // 未在线或无法获取 socket，重新入队以便后续重试
            std::cout << "⚠️ 目标 " << target << " 未在线或发送失败，消息已缓存" << std::endl;
            message::enqueueMessage(target, sender, msg);
        };

        // 提交到单例线程池，异步处理发送
        SingletonThreadPool::getInstance().submit(task);
        //std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 退出前清理剩余节点
    MsgNode* node = lf_tail;
    while (node) {
        MsgNode* n = node->next.load(std::memory_order_relaxed);
        delete node;
        node = n;
    }
    lf_tail = nullptr;
}

void message::startForwarder() {
    // 初始化 dummy 节点
    MsgNode* dummy = new MsgNode();
    dummy->next.store(nullptr, std::memory_order_relaxed);
    lf_head.store(dummy, std::memory_order_relaxed);
    lf_tail = dummy;
    lf_count.store(0, std::memory_order_relaxed);

    forwarderRunning = true;
    forwarderThread = std::thread(forwarderLoop);
}

void message::stopForwarder() {
    forwarderRunning = false;
    queueCond.notify_one();
    if (forwarderThread.joinable()) forwarderThread.join();
}

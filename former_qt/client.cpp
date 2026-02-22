#include "client.h"
#include <windows.h>
#include <thread>
#include <cctype>
//Client Client::instance;
// client.cpp 构造函数
// client.cpp 构造函数
// client.cpp 构造函数（完全替换）
Client::Client(QObject *parent)
    : QObject(parent),
    m_clientSock(INVALID_SOCKET),
    m_isRunning(false),
    m_recvThread(nullptr),
    m_connectThread(nullptr)
{
    std::cout << "[Debug] Client constructor executed" << std::endl;
    m_connectThread=new QThread(this);
    // 1. 初始化接收消息线程（原逻辑保留）
    m_recvThread = new QThread(this);
    connect(m_recvThread, &QThread::started, this, &Client::recvThreadFunc);
    connect(m_recvThread, &QThread::finished, m_recvThread, &QThread::deleteLater);


    this->moveToThread(m_connectThread); // 只移动Worker，不移动Client

    // 3. 绑定连接逻辑到Worker线程（闭包传参，线程安全）
    connect(this, &Client::triggerConnect, this, [=](const QString &ip, quint16 port) {
        this->doConnectToServer(ip, port);
    });

    // 4. 线程清理（避免内存泄漏）
    connect(m_connectThread, &QThread::finished, this, &QObject::deleteLater);
    connect(m_connectThread, &QThread::finished, m_connectThread, &QThread::deleteLater);

    // 【删除这行！】彻底移除整个Client的线程移动（罪魁祸首）
    // this->moveToThread(m_connectThread);
}
/*
// 新增：实际执行发送的槽函数（子线程中执行，不阻塞UI）
bool Client::doSendMsg(const std::string &msg)
{
    QMutexLocker locker(&m_mutex);
    if (m_clientSock == INVALID_SOCKET) {
        emit errorMsg("Not connected to server, cannot send message");
        std::cerr << "[Error] Failed to send message: Not connected to server" << std::endl;
        return false;
    }
    std::string msg_="PING";
    // 调用阻塞的send，但此时在子线程，不影响UI
    int ret = send(m_clientSock, msg_.c_str(), msg_.length(), 0);
    CHECK_ERROR(ret, "Failed to send message", m_clientSock);
    std::cout << "[Debug] Message sent successfully: " << msg_ << std::endl;
    return true;
}
*/

Client::~Client()
{
    std::cout << "[Debug] Client destructor executed" << std::endl;

    // 1. 停止接收线程
    stopRecvThread();

    // 2. 停止连接线程
    if (m_connectThread && m_connectThread->isRunning()) {
        m_connectThread->quit();
        m_connectThread->wait(3000); // Wait 3 seconds for timeout
    }

    // 3. 释放Socket资源
    if (m_clientSock != INVALID_SOCKET) {
        closesocket(m_clientSock);
        m_clientSock = INVALID_SOCKET;
    }

    // 4. 清理Winsock
    WSACleanup();
    m_connectThread->quit();
}

// client.cpp startConnect函数
void Client::startConnect(const QString &ip, quint16 port)
{
    std::cout << "[Debug] Trigger connect to server: " << ip.toStdString() << ":" << port << std::endl;

    // 修正：先判断线程是否存在，再启动（避免重复启动）
    if (!m_connectThread) {
        m_connectThread = new QThread(this);
        emit errorMsg("Connect thread not initialized");
        return;
    }

    // 关键：仅当线程未运行时，才启动并触发连接
    if (!m_connectThread->isRunning()) {
        m_connectThread->start();      // 启动连接线程（此时才启动，避免启动程序就卡）
        emit triggerConnect(ip, port); // 触发连接逻辑（跑在子线程）
    } else {
        emit errorMsg("Connect thread is already running, do not connect repeatedly");
        std::cerr << "[Error] Connect thread is already running" << std::endl;
    }
}

// 实际连接逻辑（运行在连接子线程，阻塞不影响UI）
bool Client::doConnectToServer(const QString &ip, quint16 port)
{
    std::cout << "[Debug] Connect thread started, initializing Winsock" << std::endl;

    // 1. 初始化Winsock2
    WSADATA wsaData;
    int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (ret != 0) {
        QString errMsg = "WSAStartup initialization failed. Error code: " + QString::number(ret);
        emit errorMsg(errMsg);
        std::cerr << "[Error] " << errMsg.toStdString() << std::endl;
        m_connectThread->quit();
        return false;
    }
    std::cout << "[Debug] WSAStartup initialized successfully" << std::endl;

    // 2. 创建TCP Socket
    m_clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    std::cout << "[Debug] Socket created, return value: " << (int)m_clientSock << std::endl;
    CHECK_ERROR(m_clientSock, "Failed to create socket", m_clientSock);

    // 3. 配置服务器地址
    sockaddr_in serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port); // Convert port to network byte order
    // 解析IP地址（替换过时的inet_addr）
    int inetRet = inet_pton(AF_INET, ip.toUtf8().constData(), &serverAddr.sin_addr);
    CHECK_INET_ERROR(inetRet, ip);
    std::cout << "[Debug] IP address parsed successfully: " << ip.toStdString() << std::endl;

    // 4. 设置Socket超时（避免connect/recv无限阻塞）
    DWORD timeout = 3000; // 3 seconds timeout
    // Send timeout (for connect)
    setsockopt(m_clientSock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
    // Receive timeout (for recv)
    setsockopt(m_clientSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    std::cout << "[Debug] Socket timeout set to 3 seconds" << std::endl;

    // 5. 连接服务器（阻塞在子线程，不影响UI）
    ret = ::connect(m_clientSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
    CHECK_ERROR(ret, "Failed to connect to server", m_clientSock);
    std::cout << "[Success] Connected to server: " << ip.toStdString() << ":" << port << std::endl;
    //int ret1 = send(m_clientSock, std::string("00250000000112345678:1234_aaa").c_str(), 26, 0);
    std::cout<<"over";
    // 6. 启动接收消息线程
    m_isRunning = true;
    linksucess=true;
    m_recvThread->start();
     emit connectSuccess();
    std::cout << "[Debug] Receive message thread started" << std::endl;

    // 7. 连接完成，退出连接线程
    std::cout << "[Debug] Receive message thread started2" << std::endl;
    return true;
}

// // 发送消息（UI线程调用，线程安全）
// bool Client::sendMsg(const std::string &msg,std::string message_header)
// {
//     QMutexLocker locker(&m_mutex); // Thread-safe lock
//     if (m_clientSock == INVALID_SOCKET) {
//         emit errorMsg("Not connected to server, cannot send message");
//         std::cerr << "[Error] Failed to send message: Not connected to server" << std::endl;
//         return false;
//     }
//     //std::string msg_fill=message_header+":"+msg;
//     std::string msg_fill="00310000000212345678:12345678:asasa";
//     ///std::cout<<msg_fill<<std::endl;
//     int ret1 = send(m_clientSock, std::string("00250000000112345678:1234_aaa").c_str(), 29, 0);
//     int ret = send(m_clientSock, msg_fill.c_str(), msg_fill.length(), 0);
//     CHECK_ERROR(ret, "Failed to send message", m_clientSock);
//     //std::cout << "[Debug] Message sent successfully: " << msg << std::endl;
//     return true;
// }
// 发送消息（UI线程调用，线程安全）
bool Client::sendMsg(const std::string &msg,std::string message_header)
{
    QMutexLocker locker(&m_mutex); // Thread-safe lock
    if (m_clientSock == INVALID_SOCKET) {
        emit errorMsg("Not connected to server, cannot send message");
        std::cerr << "[Error] Failed to send message: Not connected to server" << std::endl;
        return false;
    }

    // ========== 移除所有硬编码，改用传入的msg ==========
    int ret = send(m_clientSock, msg.c_str(), msg.length(), 0);
    CHECK_ERROR(ret, "Failed to send message", m_clientSock);
    std::cout << "[Debug] Message sent successfully: " << msg << std::endl;
    return true;
}
// 停止接收线程（线程安全）
void Client::stopRecvThread()
{
    QMutexLocker locker(&m_mutex);
    m_isRunning = false;

    // Wake up blocked recv (close receive end)
    if (m_clientSock != INVALID_SOCKET) {
        shutdown(m_clientSock, SD_RECEIVE);
    }

    // Wait for receive thread to exit
    if (m_recvThread && m_recvThread->isRunning()) {
        m_recvThread->quit();
        m_recvThread->wait(3000); // Wait 3 seconds for timeout
    }
    std::cout << "[Debug] Receive message thread stopped" << std::endl;
}

// 接收消息线程逻辑（运行在子线程）
void Client::recvThreadFunc()
{
    std::cout << "[Debug] Receive message thread started running" << std::endl;
    char recvBuf[1024] = { 0 };

    auto is_digits = [](const std::string &s)->bool{
        if (s.empty()) return false;
        for (char c : s) if (!std::isdigit((unsigned char)c)) return false;
        return true;
    };

    while (true) {
        // 检查线程是否需要停止（最小化锁范围）
        {
            QMutexLocker locker(&m_mutex);
            if (!m_isRunning) break;
        }

        // 阻塞接收消息（已设置3秒超时）
        int ret = recv(m_clientSock, recvBuf, sizeof(recvBuf) - 1, 0);
        if (ret <= 0) {
            int errCode = WSAGetLastError();
            // Timeout error (10060): Continue waiting, do not exit thread
            if (errCode == 10060) {
                //std::cout << "[Debug] Recv timeout (3 seconds), continue waiting..." << std::endl;
                memset(recvBuf, 0, sizeof(recvBuf));
                continue;
            }

            // Non-timeout error: Trigger disconnect
            QString errMsg = (ret == 0) ? "Server disconnected actively" :
                                 QString("Failed to receive message. Error code: %1").arg(errCode);
            emit errorMsg(errMsg);
            emit disconnectServer();
            std::cerr << "[Error] " << errMsg.toStdString() << std::endl;
            break;
        }

        // 将本次接收的字节追加到流缓冲区（可能粘包/半包）
        m_recvBuffer.append(recvBuf, ret);
        // 循环解析缓冲区中的完整消息：
        // 协议 -> [4位纯数字长度头] + [消息体]
        // 消息体 -> [8位纯数字类型] + [可变业务内容]
        std::cout<<m_recvBuffer<<std::endl;
        // client.cpp recvThreadFunc 中消息解析部分
        while (true) {
            // 需要至少4字节长度头
            if (m_recvBuffer.size() < 4) break;

            std::string lenHead = m_recvBuffer.substr(0, 4);
            if (!is_digits(lenHead)) {
                m_recvBuffer.erase(0, 1);
                continue;
            }

            int bodyLen = 0;
            try {
                bodyLen = std::stoi(lenHead);
            } catch (...) {
                m_recvBuffer.erase(0, 4);
                continue;
            }

            if (bodyLen <= 0 || bodyLen > 10 * 1024 * 1024) {
                m_recvBuffer.erase(0, 4);
                continue;
            }

            if ((int)m_recvBuffer.size() < 4 + bodyLen) break;

            // 修复：正确截取消息体（4字节长度头后取bodyLen字节）
            std::string body = m_recvBuffer.substr(4, bodyLen);
            m_recvBuffer.erase(0, 4 + bodyLen);

            if (body.size() < 8) continue;

            std::string typeHead = body.substr(0, 8);
            if (!is_digits(typeHead)) continue;

            int msgType = 0;
            try {
                msgType = std::stoi(typeHead);
            } catch (...) {
                continue;
            }

            std::string bizContent = body.substr(8);
            emit recvParsed(msgType, bizContent);

            // 修复：添加分隔符，避免内容拼接混乱
            std::string notify = std::to_string(msgType) + "|" + bizContent;
            emit recvMsg(notify);
        }

        // 清空本次原始接收缓冲区
        memset(recvBuf, 0, sizeof(recvBuf));
    }

    std::cout << "[Debug] Receive message thread exited" << std::endl;
}

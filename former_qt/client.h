#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QString>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
// 通用错误检查宏（兼容INVALID_SOCKET和SOCKET_ERROR）
#define CHECK_ERROR(ret, msg, clientSock) \
do { \
        bool isError = false; \
        if (ret == INVALID_SOCKET || ret == SOCKET_ERROR) { \
            isError = true; \
    } \
        if (isError) { \
            int errCode = WSAGetLastError(); \
            QString errMsg = QString(msg) + " 错误码：" + QString::number(errCode); \
            emit errorMsg(errMsg); \
            std::cerr << "[错误] " << msg << " 错误码：" << errCode << std::endl; \
            if (clientSock != INVALID_SOCKET) { \
                closesocket(clientSock); \
        } \
            if (WSAIsBlocking()) { \
                WSACleanup(); \
        } \
            return false; \
    } \
} while(0);

// IP地址解析错误检查宏（inet_pton返回1=成功，0/-1=失败）
#define CHECK_INET_ERROR(ret, ip) \
do { \
        if (ret != 1) { \
            int errCode = WSAGetLastError(); \
            QString errMsg = QString("IP地址 %1 解析失败（错误码：%2）").arg(ip).arg(errCode); \
            emit errorMsg(errMsg); \
            std::cerr << "[错误] " << errMsg.toStdString() << std::endl; \
            closesocket(m_clientSock); \
            WSACleanup(); \
            return false; \
    } \
} while(0);

class Client : public QObject
{
    Q_OBJECT
public:

    ~Client() override;

    // 触发连接（UI线程调用，非阻塞）
    void startConnect(const QString &ip, quint16 port );

    // 发送消息（UI线程调用）
    bool sendMsg(const std::string &msg,std::string message_header);

    // 停止接收线程
    void stopRecvThread();
    bool linksucess=false;
    /*
    static  Client & getInstance() {
        return instance;
    }**/
     explicit Client(QObject *parent = nullptr);
signals:
    // 接收消息信号（抛给UI）
    void recvMsg(const std::string &msg);
    // 解析后的消息信号：type + 业务内容
    void recvParsed(int type, const std::string &content);
    // 错误信息信号（抛给UI）
    void errorMsg(const QString &err);
    // 服务器断开连接信号
    void disconnectServer();
    void connectSuccess();
    // 内部信号：触发连接（用于动态传参）
    void triggerConnect(const QString &ip, quint16 port);
   // void triggerSendMsg(const std::string &msg);
private slots:
    // 实际连接逻辑（运行在连接子线程，阻塞不影响UI）
    bool doConnectToServer(const QString &ip, quint16 port);

    // 接收消息线程逻辑
    void recvThreadFunc();
     //bool doSendMsg(const std::string &msg);
protected:

private:

     //static Client instance;
    SOCKET m_clientSock;             // 客户端Socket
    QThread *m_recvThread;           // 接收消息线程
    QThread *m_connectThread;        // 连接服务器线程（解决UI阻塞）
    QMutex m_mutex;                  // 线程安全锁
    bool m_isRunning;                // 接收线程运行标记
    // 接收缓冲区（流式处理，可能收到粘包/半包）
    std::string m_recvBuffer;
};


#endif // CLIENT_H

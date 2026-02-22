#ifndef CHATCLIENTUI_H
#define CHATCLIENTUI_H
#include"client.h"
#include <QWidget>
#include <QListWidgetItem> // 用于处理列表项点击

namespace Ui {
class chatclientui;
}

class chatclientui : public QWidget
{
    Q_OBJECT

public:
    explicit chatclientui(QWidget *parent = nullptr);
    ~chatclientui();
    // 修复单例实现
    static chatclientui* getInstance() {
        if(instance == nullptr) {
            instance = new chatclientui();
        }
        return instance;
    }
    // 新增：设置当前登录用户ID（senderId）
    void setSenderId(const QString &senderId);
    Client* client;
private slots:
    void on_PushButton();
    // 新增：QListWidget好友列表项点击事件
    void on_listWidget_friends_itemClicked(QListWidgetItem *item);

private:
    static chatclientui* instance;
    static int init; // 保留原有变量，兼容初始化逻辑
    Ui::chatclientui *ui;

    QString m_senderId;    // 登录用户ID（消息发送方）
    QString m_receiverId;  // 选中的好友ID（消息接收方）
    void addMsgToDisplay(const QString &sender, const QString &msg);
    // 新增：构造符合协议的消息包
    std::string buildMsgPacket(const QString &content);
};

extern chatclientui* c;
#endif // CHATCLIENTUI_H

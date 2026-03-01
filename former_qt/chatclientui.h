#ifndef CHATCLIENTUI_H
#define CHATCLIENTUI_H
#include "client.h"
#include "voicemanager.h"
#include <QWidget>
#include <QThread>
#include<QListWidget>
namespace Ui { class chatclientui; }

class chatclientui : public QWidget
{
    Q_OBJECT
public:
    explicit chatclientui(QWidget *parent = nullptr);
    ~chatclientui() override;
    static chatclientui* getInstance();
    void setSenderId(const QString &senderId);
    Client* client;

private slots:
    void on_PushButton();                     // 发送文字消息
    void on_listWidget_friends_itemClicked(QListWidgetItem *item); // 选择好友
    void on_pushButton_2_clicked();           // 语音通话按钮
    // 接收VoiceManager信号
    void onVoiceCallStarted();
    void onVoiceCallStopped();
    void onVoiceError(const QString &msg);

private:
    static chatclientui* instance;
    Ui::chatclientui *ui;
    QString m_senderId;
    QString m_receiverId;
    VoiceManager *m_voiceManager = nullptr;   // 语音管理器
    QThread *m_voiceThread = nullptr;         // 语音独立线程

    void addMsgToDisplay(const QString &sender, const QString &msg);
    std::string buildMsgPacket(const QString &content);
    std::string buildVoiceCallPacket();
};

extern chatclientui* c;

#endif // CHATCLIENTUI_H

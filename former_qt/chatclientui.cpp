#include "chatclientui.h"
#include "ui_chatclientui.h"
#include <QDateTime>
#include <QDebug>
#include <QMessageBox>

chatclientui* chatclientui::instance = nullptr;

chatclientui::chatclientui(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::chatclientui)
    , client(new Client)
{
    ui->setupUi(this);
    this->setWindowTitle("聊天客户端");

    // 绑定UI按钮
    connect(ui->pushButton, &QPushButton::clicked, this, &chatclientui::on_PushButton);
    connect(ui->listWidget_friends, &QListWidget::itemClicked, this, &chatclientui::on_listWidget_friends_itemClicked);
    connect(ui->pushButton_2, &QPushButton::clicked, this, &chatclientui::on_pushButton_2_clicked);

    // 绑定客户端消息接收
    connect(client, &Client::recvMsg, this, [this](const std::string &msg) {
        this->addMsgToDisplay("好友", QString::fromUtf8(msg.c_str()));
    });

    // 初始化好友列表
    ui->listWidget_friends->addItem("12345678（好友A）");
    ui->listWidget_friends->addItem("87654321（好友B）");
    ui->listWidget_friends->addItem("11111111（好友C）");

    // 连接TCP服务器
    client->startConnect("192.168.130.128", 8888);

    // ========== 初始化语音管理器（独立线程） ==========
    m_voiceThread = new QThread(this);
    m_voiceManager = new VoiceManager();
    m_voiceManager->moveToThread(m_voiceThread); // 移到独立线程

    // 绑定语音管理器信号
    connect(m_voiceManager, &VoiceManager::voiceCallStarted, this, &chatclientui::onVoiceCallStarted);
    connect(m_voiceManager, &VoiceManager::voiceCallStopped, this, &chatclientui::onVoiceCallStopped);
    connect(m_voiceManager, &VoiceManager::errorOccurred, this, &chatclientui::onVoiceError);
    connect(m_voiceThread, &QThread::finished, m_voiceManager, &VoiceManager::deleteLater);

    // 启动语音线程
    m_voiceThread->start();

    // 默认发送方ID
    m_senderId = "12345678";
    m_voiceManager->setSenderId(m_senderId);

    c = this;
}

chatclientui::~chatclientui()
{
    // 停止语音线程
    m_voiceThread->quit();
    m_voiceThread->wait();

    delete client;
    delete ui;
}

void chatclientui::setSenderId(const QString &senderId)
{
    m_senderId = senderId;
    m_voiceManager->setSenderId(senderId);
}

void chatclientui::on_listWidget_friends_itemClicked(QListWidgetItem *item)
{
    if (!item) return;
    QString itemText = item->text();
    m_receiverId = itemText.left(itemText.indexOf("（")).trimmed();
    m_voiceManager->setReceiverId(m_receiverId); // 同步到语音管理器
    ui->textEdit->append(QString("\n【已选择聊天对象：%1】\n").arg(itemText));
}

void chatclientui::on_pushButton_2_clicked()
{
    if (!m_voiceManager->isVoiceCalling()) {
        if (m_receiverId.isEmpty()) {
            QMessageBox::warning(this, "警告", "请先选择聊天对象！");
            return;
        }
        // 发起通话
        bool success = m_voiceManager->startVoiceCall();
        if (success) {
            // 发送通话请求包
            std::string packet = buildVoiceCallPacket();
            if (!packet.empty() && client->linksucess) {
                client->sendMsg(packet, "");
            }
        }
    }
}

// 语音通话启动回调（UI线程）
void chatclientui::onVoiceCallStarted()
{
    ui->pushButton_2->setText("结束语音通话");
    ui->textEdit->append("\n【系统】语音通话已启动\n");
}

// 语音通话停止回调（UI线程）
void chatclientui::onVoiceCallStopped()
{
    ui->pushButton_2->setText("发起语音通话");
    ui->textEdit->append("\n【系统】语音通话已结束\n");
}

// 语音错误回调（UI线程）
void chatclientui::onVoiceError(const QString &msg)
{
    QMessageBox::warning(this, "语音错误", msg);
    ui->textEdit->append(QString("\n【系统错误】%1\n").arg(msg));
}

// 文字消息发送（原有逻辑不变）
void chatclientui::on_PushButton()
{
    QString inputMsg = ui->lineEdit->text().trimmed();
    if (inputMsg.isEmpty()) {
        QMessageBox::warning(this, "提示", "消息不能为空！");
        return;
    }

    std::string packet = buildMsgPacket(inputMsg);
    if (packet.empty()) return;

    if (client->linksucess) {
        client->sendMsg(packet, "");
        addMsgToDisplay("我", inputMsg);
        ui->lineEdit->clear();
    } else {
        addMsgToDisplay("系统", "未连接服务器，发送失败！");
    }
}

void chatclientui::addMsgToDisplay(const QString &sender, const QString &msg)
{
    QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    ui->textEdit->append(QString("[%1] %2：%3").arg(time).arg(sender).arg(msg));
}

std::string chatclientui::buildMsgPacket(const QString &content)
{
    if (m_senderId.isEmpty() || m_receiverId.isEmpty() || content.isEmpty()) return "";
    std::string type = "00000002";
    std::string biz = m_senderId.toStdString() + ":" + m_receiverId.toStdString() + ":" + content.toStdString();
    std::string body = type + biz;
    char len[5] = {0};
    snprintf(len, 5, "%04d", (int)body.size());
    return std::string(len) + body;
}

std::string chatclientui::buildVoiceCallPacket()
{
    if (m_senderId.isEmpty() || m_receiverId.isEmpty()) return "";
    std::string type = "00000003";
    std::string biz = m_senderId.toStdString() + ":" + m_receiverId.toStdString() + ":voice_call";
    std::string body = type + biz;
    char len[5] = {0};
    snprintf(len, 5, "%04d", (int)body.size());
    return std::string(len) + body;
}

chatclientui* chatclientui::getInstance()
{
    if (!instance) instance = new chatclientui();
    return instance;
}

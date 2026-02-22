#include "chatclientui.h"
#include "ui_chatclientui.h"
#include<QDateTime>
#include <QDebug>
#include <cstdio> // for snprintf

chatclientui* chatclientui::instance=nullptr;
int chatclientui::init=0;

chatclientui::chatclientui(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::chatclientui)
    , client(new Client)
    , m_senderId("")
    , m_receiverId("")
{
    ui->setupUi(this);

    // 1. 绑定发送按钮点击事件
    QObject::connect(ui->pushButton, &QPushButton::clicked, this, &chatclientui::on_PushButton);
    // 2. 绑定QListWidget好友列表项点击事件（核心修正：绑定QListWidget的itemClicked信号）
    QObject::connect(ui->listWidget_friends, &QListWidget::itemClicked, this, &chatclientui::on_listWidget_friends_itemClicked);
    // 3. 绑定服务器消息接收信号
    QObject::connect(client, &Client::recvMsg, [this](const std::string &msg) {
        std::cout << msg << std::endl;
        this->addMsgToDisplay("好友", QString::fromUtf8(msg));
    });

    // 4. 初始化QListWidget好友列表（示例数据，可替换为服务器拉取）
    ui->listWidget_friends->addItem("12345678（好友A）");
    ui->listWidget_friends->addItem("87654321（好友B）");
    ui->listWidget_friends->addItem("11111111（好友C）");

    // 5. 连接服务器（保留原有配置）
    const QString serverIp = "192.168.130.128";
    const quint16 serverPort = 8888;
    std::cout << "\n[Debug] Start connecting to server " << serverIp.toStdString() << ":" << serverPort << std::endl;
    client->startConnect(serverIp, serverPort);
}

chatclientui::~chatclientui()
{
    delete ui;
}

// 新增：设置发送方ID（登录账号）
void chatclientui::setSenderId(const QString &senderId) {
    m_senderId = senderId;
}

// 新增：QListWidget选中好友项，提取接收方ID
void chatclientui::on_listWidget_friends_itemClicked(QListWidgetItem *item) {
    if (!item) return;
    // 提取括号前的纯数字ID（示例："12345678（好友A）" → "12345678"）
    QString itemText = item->text();
    m_receiverId = itemText.left(itemText.indexOf("（")).trimmed();
    qDebug() << "选中好友ID：" << m_receiverId;
}

// 新增：构造协议消息包（4位长度头+8位类型+业务内容）
std::string chatclientui::buildMsgPacket(const QString &content) {
    // 校验必要参数
    m_senderId="82012092";
    if (m_senderId.isEmpty() || m_receiverId.isEmpty() || content.isEmpty()) {
        qDebug() << "发送方/接收方/消息内容不能为空！";
        return "";
    }
    // 8位消息类型（00000002=普通聊天消息，00000001=登录消息）
    std::string typeHead = "00000002";
    // 业务内容：senderId:receiverId:content
    std::string bizContent = m_senderId.toStdString() + ":" + m_receiverId.toStdString() + ":" + content.toStdString();
    // 消息体 = 8位类型 + 业务内容
    std::string body = typeHead + bizContent;
    // 4位长度头（消息体字节数，不足补0）
    int bodyLen = (int)body.size();
    char lenHead[5] = {0};
    snprintf(lenHead, sizeof(lenHead), "%04d", bodyLen);
    // 完整包 = 4位长度头 + 消息体
    return std::string(lenHead) + body;
}

// 发送按钮点击逻辑
void chatclientui::on_PushButton(){
    QString inputMsg = ui->lineEdit->text().trimmed();
    if (inputMsg.isEmpty()) return;

    // 1. 构造符合协议的消息包
    std::string packet = buildMsgPacket(inputMsg);
    if (packet.empty()) return;

    // 2. 发送消息（移除Client中的硬编码，使用动态构造的packet）
    if (client->linksucess) {
        client->sendMsg(packet, "");
        // 3. 显示自己发送的消息
        this->addMsgToDisplay("我", inputMsg);
        // 清空输入框
        ui->lineEdit->clear();
        qDebug() << "发送消息包：" << QString::fromStdString(packet);
    } else {
        this->addMsgToDisplay("系统", "未连接到服务器，发送失败！");
    }
}

// 消息显示逻辑（保留原有逻辑）
void chatclientui::addMsgToDisplay(const QString &sender, const QString &msg){
    QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString displayMsg = QString("[%1] %2：%3\n").arg(timeStr).arg(sender).arg(msg);
    ui->textEdit->append(displayMsg);
}

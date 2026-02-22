#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    l = new login;
    this->setCentralWidget(l);
    r=nullptr;
    connect(l,&login::on_pushButton_3_clicked,this,&MainWindow::resg);
    connect(l,&login::on_pushButton_clicked,this,&MainWindow::chat);
}

void MainWindow::resg(){
    if(!r)r= new registerDialog;
    this->setCentralWidget(r);
    connect(r,&registerDialog::on_pushButton_clicked,this,&MainWindow::log);
    l=nullptr;
}

void MainWindow::log(){
    if(!l)l = new login;
    this->setCentralWidget(l);
    connect(l,&login::on_pushButton_3_clicked,this,&MainWindow::resg);
    connect(l,&login::on_pushButton_clicked,this,&MainWindow::chat);
    r=nullptr;
}

void MainWindow::chat(){
    std::cout<<0<<std::endl;
    std::cout<<1<<std::endl;

    // 1. 获取登录界面的账号和密码
    QString account = l->getAccount();
    QString password = l->getPassword();
    if (account.isEmpty() || password.isEmpty()) {
        qDebug() << "账号或密码不能为空！";
        return;
    }

    // 2. 构造登录请求包（4位长度头+8位类型00000001+业务内容）
    std::string loginTypeHead = "00000001";
    std::string loginBizContent = account.toStdString() + ":" + account.toStdString() + "_" + password.toStdString();
    std::string loginBody = loginTypeHead + loginBizContent;
    int loginBodyLen = (int)loginBody.size();
    char loginLenHead[5] = {0};
    snprintf(loginLenHead, sizeof(loginLenHead), "%04d", loginBodyLen);
    std::string loginPacket = std::string(loginLenHead) + loginBody;

    // 3. 传递登录账号到聊天窗口，并发送登录请求
    this->hide();
    c->show();
    c->setSenderId(account); // 设置发送方ID
    // 发送登录请求（复用Client的sendMsg）

    if (c->client->linksucess) {
        c->client->sendMsg(loginPacket, "");
        qDebug() << "登录请求发送：" << QString::fromStdString(loginPacket);
    } else {
        qDebug() << "未连接服务器，登录请求发送失败！";
    }

    std::cout<<2<<std::endl;
    //l=nullptr;
    this->setCentralWidget(nullptr);
    delete l;
    l = nullptr;
}

MainWindow::~MainWindow()
{
    delete ui;
}

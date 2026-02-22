#include "login.h"
#include "ui_login.h"

login::login(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::login)
{
    ui->setupUi(this);
    connect(ui->pushButton_3,&QPushButton::clicked,this,&on_pushButton_3_clicked);
    connect(ui->pushButton,&QPushButton::clicked,this,&on_pushButton_clicked);
}

login::~login()
{
    delete ui;
}

// 新增：获取账号
QString login::getAccount() const {
    return ui->lineEdit_account->text().trimmed();
}

// 新增：获取密码
QString login::getPassword() const {
    return ui->lineEdit_pwd->text().trimmed();
}

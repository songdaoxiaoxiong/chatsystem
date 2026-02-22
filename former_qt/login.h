#ifndef LOGIN_H
#define LOGIN_H

#include <QDialog>
QT_BEGIN_NAMESPACE
namespace Ui {
class login;
}
QT_END_NAMESPACE

class login : public QDialog
{
    Q_OBJECT

public:
    explicit login(QWidget *parent = nullptr);
    ~login();
    // 新增：获取账号/密码
    QString getAccount() const;
    QString getPassword() const;

signals:
    void on_pushButton_3_clicked(); // 注册按钮
    void on_pushButton_clicked();   // 登录按钮

private:
    Ui::login *ui;
};

#endif // LOGIN_H

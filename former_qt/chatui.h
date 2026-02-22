#ifndef CHATUI_H
#define CHATUI_H

#include <QWidget>
#include <QStandardItemModel>
#include "client.h"
// 在chatui.h的#include区域添加
#include <QListWidgetItem>
namespace Ui { class chatui; }

class ChatUI : public QWidget
{
    Q_OBJECT

public:
    explicit ChatUI(QWidget *parent = nullptr);
    ~ChatUI() override;

private slots:
    // 发送按钮点击：调用Client发送消息
    void on_pushButton_clicked();
    // 更新聊天记录（接收Client的recvMsg信号）
    void updateChatRecord(const std::string &msg);
    // 显示错误信息（接收Client的errorMsg信号）
    void showError(const QString &err);
    // 处理服务器断开
    void handleDisconnect();
    // 选择聊天目标（listWidget点击）
    void on_listWidget_itemClicked(QListWidgetItem *item);

private:
    Ui::chatui *ui;                  // UI界面对象
    Client *m_client;                // 网络接口实例
    QStandardItemModel *m_chatModel; // listView聊天记录模型
    QString m_selectedTarget;        // 当前选中的聊天目标

    // 编码转换：GBK→UTF-8（Windows控制台→Qt UI）
    QString gbkToUtf8(const std::string &gbkStr);
    // 编码转换：UTF-8→GBK（Qt UI→网络）
    std::string utf8ToGbk(const QString &utf8Str);
};

#endif // CHATUI_H

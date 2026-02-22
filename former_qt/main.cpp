/*#include "mainwindow.h"

#include <QApplication>
#include<QFile>
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QFile qss(":/qss/stylesheet.qss");

    if( qss.open(QFile::ReadOnly))
    {
        qDebug("open success");
        QString style = QLatin1String(qss.readAll());
        a.setStyleSheet(style);
        qss.close();
    }else{
        qDebug("Open failed");
    }
    MainWindow w;
    w.show();
    return a.exec();
}
*/
#include <QCoreApplication>
#include <QTimer>
#include <QThread>
#include <iostream>
#include "client.h"
#include<QObject>
#include<time.h>
#include<thread>
#include <chrono>
#include"chatclientui.h"
#include <QApplication>
#include "mainwindow.h"
#include<QFile>
chatclientui* c = nullptr;
// 全局加载样式表函数
void loadGlobalStyleSheet(QApplication &app) {
    QFile qssFile(":/qss/stylesheet.qss"); // 资源路径
    if (qssFile.open(QFile::ReadOnly)) {
        QString styleSheet = QLatin1String(qssFile.readAll());
        app.setStyleSheet(styleSheet);
        qssFile.close();
        qDebug() << "样式表加载成功！";
    } else {
        qDebug() << "样式表加载失败：" << qssFile.errorString();
    }
}
int main(int argc, char *argv[])
{

    QApplication a(argc, argv);
    loadGlobalStyleSheet(a);
      MainWindow  w;
    // chatclientui::getInstance();
    c = new chatclientui();
      c->show();
    // chatclientui::getInstance() ->show();
     w.show();
    return a.exec();
}

#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include"login.h"
#include <QMainWindow>
#include"registerdialog.h"
#include "chatclientui.h"
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
private slots:
    void resg();
    void log();
    void chat();
private:
    Ui::MainWindow *ui;
    login *l;
    registerDialog *r;
    int now=0;
};
#endif // MAINWINDOW_H

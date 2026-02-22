#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include <QDialog>

namespace Ui {
class registerDialog;
}

class registerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit registerDialog(QWidget *parent = nullptr);
    ~registerDialog();

private:
    Ui::registerDialog *ui;
signals:
    void on_pushButton_clicked();
private slots:

};

#endif // REGISTERDIALOG_H

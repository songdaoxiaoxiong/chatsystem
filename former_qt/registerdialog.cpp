#include "registerdialog.h"
#include "ui_registerdialog.h"

registerDialog::registerDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::registerDialog)
{
    ui->setupUi(this);
    connect(ui->pushButton,&QPushButton::clicked,this,&registerDialog::on_pushButton_clicked);
}

registerDialog::~registerDialog()
{
    delete ui;
}



#include "mensaje.h"
#include "ui_mensaje.h"
#include <QCloseEvent>
#include <QTime>

Mensaje::Mensaje(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Mensaje)
{
    ui->setupUi(this);
}

Mensaje::~Mensaje()
{
    delete ui;
}

void Mensaje::closeEvent(QCloseEvent *event){
    hide();
    event->ignore();
}

void Mensaje::mostrar(QString nombre, bool in_out){
    ui->nombre->setText(nombre);
    setWindowFlags(Qt::WindowStaysOnTopHint);
    show();
    showFullScreen();
    if(in_out){
        QPixmap pixmap = QPixmap(":images/ent.png");
        ui->imagen->setPixmap(pixmap.scaled(ui->imagen->width(), ui->imagen->height(), Qt::KeepAspectRatio));
    }else{
        QPixmap pixmap = QPixmap(":images/sal.png");
        ui->imagen->setPixmap(pixmap.scaled(ui->imagen->width(), ui->imagen->height(), Qt::KeepAspectRatio));
    }
    ui->hora->setText(QTime::currentTime().toString("hh:mm ap"));
}

#include "asistenciasuper.h"
#include "mensaje.h"

#include <QApplication>
#include <QStyleFactory>
#include <QDebug>

int main(int argc, char *argv[])
{
    DPFPInit();
    FX_init();
    MC_init();

    /*QFile styleFile(":/styles/styles.qss");
    styleFile.open(QFile::ReadOnly);
    QString style = QLatin1String(styleFile.readAll());
    a.setStyleSheet(style);
    styleFile.close();*/

    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QApplication a(argc, argv);

    AsistenciaSuper w;
    Mensaje m;
    w.widget = &m;
    w.show();

    a.exec();

    MC_terminate();
    FX_terminate();
    DPFPTerm();

    qDebug() << "Fin del programa";
    return 0;
}

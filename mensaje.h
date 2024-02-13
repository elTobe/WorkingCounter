#ifndef MENSAJE_H
#define MENSAJE_H

#include <QWidget>
#include <QTime>

namespace Ui {
class Mensaje;
}

class Mensaje : public QWidget
{
    Q_OBJECT

public:
    explicit Mensaje(QWidget *parent = nullptr);

    ~Mensaje();

    void closeEvent(QCloseEvent* event) override;

    void mostrar(QString nombre, bool in_out);

private:
    Ui::Mensaje *ui;
};

#endif // MENSAJE_H

// asistenciasuper.h

#ifndef ASISTENCIASUPER_H
#define ASISTENCIASUPER_H

#include <QMainWindow>
#include <QEvent>
#include "dpDefs.h"
#include "dpRCodes.h"
#include "DPDevClt.h"
#include "dpFtrEx.h"
#include "dpMatch.h"
#include "DpUIApi.h"
#include <Windows.h>
#include <QSystemTrayIcon>
#include <QCloseEvent>
#include <QPixmap>
#include <QDate>
#include <QTimer>
#include "mensaje.h"

#include "SMTPClient/email.h"
#include "SMTPClient/smtpclient.h"
#include "SMTPClient/emailaddress.h"

QT_BEGIN_NAMESPACE
namespace Ui { class AsistenciaSuper; }
QT_END_NAMESPACE

class AsistenciaSuper : public QMainWindow
{
    Q_OBJECT

public slots:

    void emit_close();

public:
    Mensaje* widget;

    AsistenciaSuper(QWidget *parent = nullptr);

    ~AsistenciaSuper();

    void cargar_trabajadores();

    void toggle_trabajador(int index);

    bool verificar_huella(QByteArray leida, QByteArray guardada);

    QByteArray leer_archivo_fpt(QString ruta);

    int verificar_coincidencia_trabajadores(QByteArray);

    void write_to_logs(QString nombre, bool in_out, QDate fecha, QTime hora);

    void read_state();

    QString get_log(QDate fecha);

    void set_tabla_entrada_salida(int index, QString hora, bool entrada_salida);

    int get_index_by_string(QString nombre);

    void show_mensaje_and_hide(QString nombre, bool in_out);

    void generar_resumen_semanal(QString nombre, QDate fecha);

    void generar_resumen_hoy();

    float get_horas_extra_semana(QString nombre, QDate fecha, bool force_update=false);

    void read_pass();

    QString set_texto_entendible(float horas);

protected:

    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;

    void closeEvent(QCloseEvent* event) override;

    void timerEvent(QTimerEvent* event);

private slots:
    void on_calendarWidget_selectionChanged();

    void on_salida_admin_clicked();

    void on_entrada_admin_clicked();

    void on_combo_nombres_currentTextChanged(const QString &arg1);

    void on_boton_texto_clicked();

    void on_boton_corregir_extras_clicked();

    void on_boton_borrar_es_clicked();

    void on_boton_carpeta_clicked();

    void on_acceso_admin_pressed();

    void on_fecha_tiempo_admin_dateChanged(const QDate &date);

    void on_combo_admin_currentTextChanged(const QString &arg1);

    void update_combo_registers();

    void on_boton_quitar_registro_clicked();

    void on_boton_correo_clicked();

    void enviar_correo();

    void onStatus(Status::e status, QString errorMessage);

private:

    Ui::AsistenciaSuper *ui;

    QSystemTrayIcon* trayIcon;

    FT_HANDLE m_fxContext; // Contexto para funciones de extracción de características
    FT_HANDLE m_mcContext; // Contexto para funciones de emparejamiento

    HDPOPERATION m_hOperationVerify; // Manejar la operación de verificación

    enum { WMUS_FP_NOTIFY = WM_USER+1 };

    struct Empleado{
        bool has_joined;
        bool working_now;
        QString time_joined;
        QString nombre;
        QVector<QByteArray> huellas;
    };

    QVector<Empleado> empleados;

    bool leyendo = false;

    QDate previousDate;

    QTimer timer_helper;

    QString pass;

    bool acceso_admin = false;

    QColor color_lineas = QColor(197,197,197);
    QColor color_texto = QColor(255,255,255);
    QColor color_fondo = QColor(255,255,255);
    QColor fondo_cabeceras = QColor(116,77,169);
    QColor color_trabajo = QColor(100,200,100);
    QColor fondo_cabecera_trabajando = QColor(0,100,0);
    QColor fondo_dia_actual = QColor(240,240,240);

};

#endif // ASISTENCIASUPER_H

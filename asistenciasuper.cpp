// asistenciasuper.cpp

#include "asistenciasuper.h"
#include "ui_asistenciasuper.h"
#include "mensaje.h"
#include <QDebug>
#include <QDir>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QDate>
#include <QTime>
#include <QTimer>
#include <QPainter>
#include <QTextCharFormat>
#include <QDesktopServices>
#include <QInputDialog>
#include <QStringList>
#include "SMTPClient/email.h"
#include "SMTPClient/smtpclient.h"
#include "SMTPClient/emailaddress.h"

AsistenciaSuper::AsistenciaSuper(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::AsistenciaSuper)
{
    ui->setupUi(this);
    ui->widget_admins->setVisible(false);
    ui->horas_que_se_guardan_hidden->setVisible(false);
    ui->horas_que_se_guardan_hidden_text->setVisible(false);

    trayIcon = new QSystemTrayIcon(this);
    QMenu* menu = new QMenu(this);

    QAction* accion_cerrar = menu->addAction("Cerrar");
    connect(accion_cerrar, &QAction::triggered, this, &QApplication::quit );
    connect(trayIcon, &QSystemTrayIcon::activated, this, &QWidget::show );

    trayIcon->setContextMenu(menu);
    trayIcon->setIcon(QIcon(":images/logo.ico"));
    trayIcon->show();

    FT_RETCODE rc = FX_createContext(&m_fxContext);
    if (rc != FT_OK) { qDebug() << "Error al crear el contexto de extraccion de caracteristicas"; }

    rc = MC_createContext(&m_mcContext);
    if (rc != FT_OK) { qDebug() << "Error al crear el contexto de emparejamiento"; }

    rc = DPFPCreateAcquisition(DP_PRIORITY_HIGH, GUID_NULL, DP_SAMPLE_TYPE_IMAGE, reinterpret_cast<HWND>(winId()), WMUS_FP_NOTIFY, &m_hOperationVerify);
    if (rc != FT_OK) { qDebug() << "Error al crear la operacion de adquisicion"; }

    rc = DPFPStartAcquisition(m_hOperationVerify);
    if (rc != FT_OK) {
        qDebug() << "Error al iniciar la adquisicion";
        QMessageBox msg;
        msg.setText("No se pudo conectar al lector de huella");
        msg.exec();
    }

    cargar_trabajadores();
    read_state();
    read_pass();

    ui->combo_nombres->addItem("Resumen hoy");
    ui->combo_nombres->setCurrentIndex(empleados.length());
    ui->combo_admin->addItem("Seleccione");
    ui->combo_admin->setCurrentIndex(empleados.length());
    ui->combo_empleados->addItem("Seleccione");
    ui->combo_empleados->setCurrentIndex(empleados.length());
    ui->combo_registros->addItem("-");

    startTimer(1000);

    ui->lista_trabajadores->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    ui->lista_trabajadores->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    ui->lista_trabajadores->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
    ui->lista_trabajadores->horizontalHeader()->setSectionResizeMode(3,QHeaderView::ResizeToContents);
    ui->lista_trabajadores->horizontalHeader()->setSectionResizeMode(4,QHeaderView::ResizeToContents);
    ui->lista_trabajadores->setHorizontalHeaderItem(0, new QTableWidgetItem("Nombre"));
    ui->lista_trabajadores->setHorizontalHeaderItem(1, new QTableWidgetItem("Entrada1"));
    ui->lista_trabajadores->setHorizontalHeaderItem(2, new QTableWidgetItem("Salida1"));
    ui->lista_trabajadores->setHorizontalHeaderItem(3, new QTableWidgetItem("Entrada2"));
    ui->lista_trabajadores->setHorizontalHeaderItem(4, new QTableWidgetItem("Salida2"));

    ui->fecha_tiempo_admin->setTime(QTime::currentTime());
    ui->fecha_tiempo_admin->setDate(QDate::currentDate());

    /// variable auxiliar que contiene la fecha del calendario anterior a que se haga click en el, para limpiar su color
    previousDate = QDate::currentDate();

    /// Cada que el programa se abre, calcula las horas extra de la semana anterior y actual
    for(int i = 0; i < empleados.length(); i++){
        generar_resumen_semanal( empleados[i].nombre, QDate::currentDate().addDays(-7) );
        generar_resumen_semanal( empleados[i].nombre, QDate::currentDate() );
    }

    timer_helper.setSingleShot(true);
    connect(&timer_helper, &QTimer::timeout, this, &AsistenciaSuper::emit_close);
    timer_helper.start(1500);

    if( QDate::currentDate().dayOfWeek() == 7 && QTime::currentTime().hour() < 9 ){
        enviar_correo();
    }

    ui->grupo_empleados->hide();
    ui->boton_carpeta->setToolTip("Abrir carpeta contenedora del programa (para cambios que no ofrezca el programa)");
    ui->boton_correo->setToolTip("Reenviar el correo de la semana pasada. Los correos envían al abrir el programa los domingos antes de las 9 am.");
}

AsistenciaSuper::~AsistenciaSuper()
{
    delete ui;
}

void AsistenciaSuper::closeEvent(QCloseEvent *event){
    hide();
    event->ignore();
}

void AsistenciaSuper::timerEvent(QTimerEvent *event){
    ui->hora->setText(QTime::currentTime().toString("h:mm:ss ap").replace("p. m.","pm").replace("a. m.","am") );
    ui->fecha->setText(QLocale::system().toString( QDate::currentDate(), "dddd d"));
    if(ui->combo_nombres->currentText() == "Resumen hoy"){
       generar_resumen_hoy();
    }else{
       QDate fecha = ui->calendarWidget->selectedDate();
       generar_resumen_semanal(ui->combo_nombres->currentText(), fecha);
    }
}

void AsistenciaSuper::read_pass(){
    QFile pass_file("pass");
    if( pass_file.open(QIODevice::ReadOnly) ){
       QTextStream in(&pass_file);
       pass = in.readLine();
    }else{
       QMessageBox msg;
       msg.setText("No se pudo leer la contraseña de administrador");
       msg.exec();
    }
}

void AsistenciaSuper::set_tabla_entrada_salida(int i, QString hora, bool entrada_salida){

    if(i > ui->lista_trabajadores->rowCount() || i < 0){
        return;
    }

    QStringList hora_strings = hora.split(":");
    QTime time = QTime( hora_strings.at(0).toInt(), hora_strings.at(1).toInt(), hora_strings.at(2).toInt() );
    QTableWidgetItem* item = new QTableWidgetItem(time.toString("h:mm ap").replace("p. m.","pm").replace("a. m.","am") );
    if(entrada_salida){
        if( ui->lista_trabajadores->item(i,1) == nullptr ){
            ui->lista_trabajadores->setItem(i, 1, item);
            ui->lista_trabajadores->item(i,0)->setTextColor(color_texto);
            ui->lista_trabajadores->item(i,0)->setBackgroundColor(fondo_cabecera_trabajando);
        }else{
            ui->lista_trabajadores->setItem(i, 3, item);
            ui->lista_trabajadores->item(i,0)->setTextColor(color_texto);
            ui->lista_trabajadores->item(i,0)->setBackgroundColor(fondo_cabecera_trabajando);
            ui->lista_trabajadores->setItem(i, 4, nullptr);
        }

    }else{

        if( ui->lista_trabajadores->item(i,2) == nullptr ){
            ui->lista_trabajadores->setItem(i, 2, item);
            ui->lista_trabajadores->item(i,0)->setTextColor(QColor(85,85,127));
            ui->lista_trabajadores->item(i,0)->setBackgroundColor(QColor(240,240,240));
        }else{
            ui->lista_trabajadores->setItem(i, 4, item);
            ui->lista_trabajadores->item(i,0)->setTextColor(QColor(85,85,127));
            ui->lista_trabajadores->item(i,0)->setBackgroundColor(QColor(240,240,240));
        }
    }
}

void AsistenciaSuper::generar_resumen_hoy(){

    int ancho = ui->historial->width();
    int alto = ui->historial->height();

    QPixmap historial_pixmap(ancho,alto);
    QPainter historial;

    QStringList horas = {"", "7 a 8", "8 a 9", "9 a 10", "10 a 11", "11 a 12", "12 a 1", "1 a 2", "2 a 3", "3 a 4", "4 a 5", "5 a 6", "6 a 7", "7 a 8", "8 a 9", "9 a 10","10 a 11"};
    int ancho_celdas = ancho/(empleados.length()+1);
    int alto_celdas = alto/horas.length();

    historial.begin(&historial_pixmap);
    historial.fillRect(QRect(0,0,ancho, alto), color_fondo);
    ////////////////////////////////////////////////////////////////////////////

    for(int i = 0; i < empleados.length(); i++){
        empleados[i].has_joined = false;
    }

    QFile log(get_log(QDate::currentDate()));
    if(log.open(QIODevice::ReadOnly)){

        QTextStream in(&log);

        while(!in.atEnd()){
            QString line = in.readLine();
            QStringList line_list = line.split(",");

            int index = get_index_by_string(line_list.at(0));
            if(index != -1){
                if(line_list.at(1) == "in"){
                    empleados[index].has_joined = true;
                    empleados[index].time_joined = line_list.at(2);
                }
                if(line_list.at(1) == "out"){
                    if(empleados[index].has_joined){
                        empleados[index].has_joined = false;

                        int hora = empleados[index].time_joined.split(":")[0].toInt();
                        int min = empleados[index].time_joined.split(":")[1].toInt();
                        int hora2 = line_list.at(2).split(":")[0].toInt();
                        int min2 = line_list.at(2).split(":")[1].toInt();

                        int x = (index + 1)*ancho_celdas;
                        int x2 = (index + 2)*ancho_celdas;
                        int y = (hora-6)*alto_celdas + min*(alto_celdas/60.0);
                        int y2 = (hora2-6)*alto_celdas + min2*(alto_celdas/60.0);

                        if(y == y2){y2 = y+1;}

                        historial.fillRect( QRect(x,y,x2-x,y2-y), color_trabajo );
                    }
                }
            }
        }

        for(int i = 0; i < empleados.length(); i++){
            if(empleados[i].has_joined){

                empleados[i].has_joined = false;
                int hora = empleados[i].time_joined.split(":")[0].toInt();
                int min = empleados[i].time_joined.split(":")[1].toInt();
                int hora2 = QVariant(QTime::currentTime().hour()).toInt();
                int min2 = QVariant(QTime::currentTime().minute()).toInt();

                int x = (i + 1)*ancho_celdas;
                int x2 = (i + 2)*ancho_celdas;
                int y = (hora-6)*alto_celdas + min*(alto_celdas/60.0);
                int y2 = (hora2-6)*alto_celdas + min2*(alto_celdas/60.0);

                if(y == y2){y2 = y+1;}

                historial.fillRect( QRect(x,y,x2-x,y2-y), color_trabajo );
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////

    historial.setFont( QFont("Arial",8, QFont::Bold) );
    for(int i = 0; i < empleados.length(); i++){
        QRect rect = QRect((i+1)*ancho_celdas,0,ancho_celdas,alto_celdas);
        if(empleados[i].working_now){
            historial.fillRect(rect, fondo_cabecera_trabajando);
        }else{
            historial.fillRect(rect, fondo_cabeceras);
        }
        historial.setPen(color_texto);
        historial.drawText( rect , Qt::AlignCenter, empleados.at(i).nombre );
        historial.setPen(color_lineas);
        historial.drawLine((i+1)*ancho_celdas,0,(i+1)*ancho_celdas,alto);
    }
    historial.drawLine(0,alto-1,ancho,alto-1);
    for(int i = 0; i < horas.length(); i++){
        QRect rect = QRect(0,i*alto_celdas,ancho_celdas,alto_celdas);
        historial.fillRect(rect, fondo_cabeceras);
        historial.setPen(color_texto);
        historial.drawText( rect , Qt::AlignCenter, horas.at(i) );
        historial.setPen(color_lineas);
        historial.drawLine(0,i*alto_celdas,ancho,i*alto_celdas);
    }
    historial.drawLine(ancho-1,0,ancho-1,alto);

    historial.setPen(QColor(0,0,0));
    int hora = QVariant(QTime::currentTime().hour()).toInt();
    int min = QVariant(QTime::currentTime().minute()).toInt();
    int y = (hora-6)*alto_celdas + min*(alto_celdas/60.0);
    historial.drawLine(ancho_celdas, y, ancho, y);
    y++;
    historial.drawLine(ancho_celdas, y, ancho, y);

    ui->horas_semana_totales->setText( "N/A" );
    ui->dias_semana->setText( "N/A" );
    ui->horas_semana_extra->setText( "N/A" );
    ui->horas_guardadas_extra->setText( "N/A" );
    ui->horas_que_se_guardan_hidden->setText( "N/A" );
    ui->dias_totales->setText( "N/A" );
    ui->horas_que_se_guardan->setText( "N/A" );
    ui->horas_faltantes->setText("N/A");

    historial.end();
    ui->historial->setPixmap(historial_pixmap);
    ui->boton_texto->setEnabled(false);
    ui->calendarWidget->setEnabled(false);
}

QString AsistenciaSuper::set_texto_entendible(float horas){
    QString texto = "";

    int h = (int)( horas );
    int m = (int)( 60*(horas-h) );
    //int s = (int)( 3600*(horas ) );

    texto.append( QVariant(h).toString() );
    texto.append( "h " );
    texto.append( QVariant(m).toString() );
    texto.append( "m " );
    //texto.append( QVariant(s).toString() );
    //texto.append( "s " );

    return texto;
}

float AsistenciaSuper::get_horas_extra_semana(QString nombre, QDate semana, bool force_update){
    QString ruta = "horas_extra/"+ QVariant(semana.toString("yyyy")).toString() + "_" +QVariant(semana.weekNumber()).toString() + ".txt";
    qDebug() << " ---------- Consultando horas extra de " << nombre << " en el archivo " << ruta << " ----------";

    QFile file( ruta );
    file.open(QIODevice::ReadWrite);
    QTextStream in(&file);
    while( !in.atEnd() ){
        QString line = in.readLine();
        if( line.split(":").length() > 0 ){
            if( line.split(":")[0].trimmed() == nombre.trimmed() ){
                float horas_extra = line.split(":")[1].trimmed().toFloat();
                qDebug() << "Se encontro que " << nombre << " tenia " << horas_extra << " horas extra.";
                return horas_extra;
            }
        }
    }

    if(semana < QDate(2024,1,1)){
        qDebug() << "Fecha limite alcanzada, regresando 0";
        return 0.0;
    }

    qDebug() << "No se encontró " << nombre << " recalculando ... ";
    generar_resumen_semanal( nombre, semana );
    float rec = get_horas_extra_semana(nombre, semana);
    qDebug() << "Regresando " << rec;
    return rec;
}

void AsistenciaSuper::generar_resumen_semanal(QString nombre, QDate fecha){

    qDebug() << " ---------- Generando resumen semanal para " << nombre << " ----------";

    QString texto = "";
    texto += "Resumen para " + nombre + "\n\n";
    ui->boton_texto->setEnabled(true);
    ui->calendarWidget->setEnabled(true);

    int ancho = ui->historial->width();
    int alto = ui->historial->height();
    QPixmap historial_pixmap(ancho,alto);
    QPainter historial;

    QStringList dias = {"", "Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado"};
    QStringList horas = {"", "7 a 8", "8 a 9", "9 a 10", "10 a 11", "11 a 12", "12 a 1", "1 a 2", "2 a 3", "3 a 4", "4 a 5", "5 a 6", "6 a 7", "7 a 8", "8 a 9", "9 a 10","10 a 11"};
    int ancho_celdas = ancho/dias.length();
    int alto_celdas = alto/horas.length();
    float horas_dia = 8.5;
    int ms_acumulados = 0;

    historial.begin(&historial_pixmap);
    historial.fillRect(QRect(0,0,ancho, alto), color_fondo);
    historial.fillRect(QRect( (fecha.dayOfWeek()%7 + 1)*ancho_celdas, 0, ancho_celdas, alto) , fondo_dia_actual);

    ////////////////////////////////////////////////////////////////////////////
    int day_of_week = QVariant(fecha.dayOfWeek()).toInt();
    fecha = fecha.addDays(-1 * (day_of_week%7 + 1));
    for(int i = 0; i < 7; i++){
        fecha = fecha.addDays(1);
        texto += fecha.toString("dddd");
        QFile log(get_log(fecha));
        if(log.open(QIODevice::ReadOnly)){
            QTextStream in(&log);
            bool entro = false;
            int hora = 0;
            int min = 0;
            while(!in.atEnd()){
                QString line = in.readLine();
                QStringList line_list = line.split(",");
                if(line_list.at(0).trimmed() == nombre.trimmed()){
                    if(line_list.at(1) == "in"){

                        texto += "\t Entrada a las " + line_list.at(2) + "\n";
                        if(entro){
                            entro = false;
                            hora = 0;
                            min = 0;
                        }else{
                            entro = true;
                            hora = line_list.at(2).split(":")[0].toInt();
                            min = line_list.at(2).split(":")[1].toInt();
                        }

                    }
                    if( line_list.at(1) == "out" ){

                        texto += "\t Salida a las " + line_list.at(2) + "\n";
                        if(entro){
                            entro = false;
                            int hora2 = line_list.at(2).split(":")[0].toInt();
                            int min2 = line_list.at(2).split(":")[1].toInt();

                            ms_acumulados += QTime(hora, min).msecsTo(QTime(hora2,min2));

                            int x = (fecha.dayOfWeek()%7 + 1)*ancho_celdas;
                            int x2 = (fecha.dayOfWeek()%7 + 2)*ancho_celdas;
                            int y = (hora-6)*alto_celdas + min*(alto_celdas/60.0);
                            int y2 = (hora2-6)*alto_celdas + min2*(alto_celdas/60.0);

                            if(y == y2){y2 = y+1;}

                            historial.fillRect( QRect(x,y,x2-x,y2-y), color_trabajo );
                        }

                    }
                }
            }

            if( entro && fecha == QDate::currentDate() ){

                entro = false;
                int hora2 = QVariant(QTime::currentTime().hour()).toInt();
                int min2 = QVariant(QTime::currentTime().minute()).toInt();

                ms_acumulados += QTime(hora, min).msecsTo(QTime(hora2,min2));

                int x = (fecha.dayOfWeek()%7 + 1)*ancho_celdas;
                int x2 = (fecha.dayOfWeek()%7 + 2)*ancho_celdas;
                int y = (hora-6)*alto_celdas + min*(alto_celdas/60.0);
                int y2 = (hora2-6)*alto_celdas + min2*(alto_celdas/60.0);

                if(y == y2){y2 = y+1;}

                historial.fillRect( QRect(x,y,x2-x,y2-y), color_trabajo );
                historial.setPen(QColor(0,0,0));
                historial.drawLine(x, y2, x2, y2);
                y2++;
                historial.drawLine(x, y2, x2, y2);
            }
        }
        texto += "\n";
    }
    ////////////////////////////////////////////////////////////////////////////

    historial.setFont( QFont("Arial",12, QFont::Bold) );
    for(int i = 0; i < dias.length(); i++){
        QRect rect = QRect(i*ancho_celdas,0,ancho_celdas,alto_celdas);
        historial.fillRect(rect, fondo_cabeceras);
        historial.setPen(color_texto);
        historial.drawText( rect , Qt::AlignCenter, dias.at(i) );
        historial.setPen(color_lineas);
        historial.drawLine(i*ancho_celdas,0,i*ancho_celdas,alto);
    }
    historial.drawLine(0,alto-1,ancho,alto-1);
    for(int i = 0; i < horas.length(); i++){
        QRect rect = QRect(0,i*alto_celdas,ancho_celdas,alto_celdas);
        historial.fillRect(rect, fondo_cabeceras);
        historial.setPen(color_texto);
        historial.drawText( rect , Qt::AlignCenter, horas.at(i) );
        historial.setPen(color_lineas);
        historial.drawLine(0,i*alto_celdas,ancho,i*alto_celdas);
    }
    historial.drawLine(ancho-1,0,ancho-1,alto);

    /////////////////////////////////////////////////////////////////////////////////////////////////////////
    float horas_guardadas = get_horas_extra_semana( nombre, fecha.addDays(-7) );
    /////////////////////////////////////////////////////////////////////////////////////////////////////////

    QString temp;
    float horas_totales = (ms_acumulados/3600000.0);
    int dias_semana = (int) ( ms_acumulados/(horas_dia*3600000.0) );
    float horas_semana = (ms_acumulados%((int)(horas_dia*3600000)))/3600000.0;
    int dias_totales = dias_semana + (int) ( (horas_semana + horas_guardadas)/horas_dia );
    float horas_a_guardar = (horas_semana + horas_guardadas) - horas_dia * (int)((horas_semana + horas_guardadas)/horas_dia);

    /////////////////////////////////////////////////////////////////////////////////////////////////////////
    QString ruta = "horas_extra/"+ QVariant(fecha.toString("yyyy")).toString() + "_" +QVariant(fecha.weekNumber()).toString() + ".txt";
    QFile file( ruta );

    file.open(QIODevice::ReadWrite);
    QTextStream in(&file);
    QString buff = "";
    while( !in.atEnd() ){
        QString line = in.readLine();
        if( line.split(":").length() > 0 ){
            if( line.split(":")[0].trimmed() != nombre.trimmed() ){
                buff.append(line);
                buff.append("\n");
            }
        }
    }
    buff.append(nombre);
    buff.append(": ");
    buff.append( QVariant(horas_a_guardar).toString() );
    buff.append("\n");

    file.close();
    file.open(QIODevice::WriteOnly | QIODevice::Truncate);
    QTextStream out(&file);
    out << buff;
    /////////////////////////////////////////////////////////////////////////////////////////////////////////

    ui->horas_que_se_guardan_hidden->setText( temp.setNum( horas_a_guardar ,'f',2) );
    ui->dias_semana->setText( temp.setNum( (float) dias_semana , 'f', 0 ) );
    ui->dias_totales->setText( temp.setNum( (float) dias_totales, 'f', 0 ) );

    ui->horas_faltantes->setText( set_texto_entendible(horas_dia - horas_a_guardar ) );
    ui->horas_semana_totales->setText( set_texto_entendible(horas_totales) );
    ui->horas_semana_extra->setText( set_texto_entendible(horas_semana) );
    ui->horas_guardadas_extra->setText( set_texto_entendible(horas_guardadas) );
    ui->horas_que_se_guardan->setText( set_texto_entendible(horas_a_guardar) );

    historial.end();
    ui->resumen_texto->setText(texto);
    ui->historial->setText("");
    ui->historial->setPixmap(historial_pixmap);
}

void  AsistenciaSuper::toggle_trabajador(int i){
    if( !empleados[i].working_now ){

        empleados[i].working_now = true;
        write_to_logs(empleados[i].nombre,true,QDate::currentDate(), QTime::currentTime());
        set_tabla_entrada_salida(i, QTime::currentTime().toString("hh:mm:ss"), true);
        show_mensaje_and_hide(empleados[i].nombre, true);

    }else{

        empleados[i].working_now = false;
        write_to_logs(empleados[i].nombre,false,QDate::currentDate(), QTime::currentTime());
        set_tabla_entrada_salida(i, QTime::currentTime().toString("hh:mm:ss"), false);
        show_mensaje_and_hide(empleados[i].nombre, false);

    }
    ui->combo_nombres->setCurrentIndex(empleados.length());
}

int AsistenciaSuper::get_index_by_string(QString nombre){
    for(int i = 0; i < empleados.length(); i++){
        if(empleados[i].nombre.trimmed() == nombre.trimmed()){
            return i;
        }
    }
    return -1;
}

void AsistenciaSuper::emit_close(){
    hide();
}

void AsistenciaSuper::show_mensaje_and_hide(QString nombre, bool in_out){
    timer_helper.setSingleShot(true);
    connect(&timer_helper, &QTimer::timeout, widget, &Mensaje::close);
    timer_helper.start(3000);
    widget->mostrar(nombre, in_out);
}

void AsistenciaSuper::read_state(){

    QFile log(get_log(QDate::currentDate()));
    if(log.open(QIODevice::ReadWrite)){
        QTextStream in(&log);
        while(!in.atEnd()){
            QString line = in.readLine();
            QStringList line_list = line.split(",",Qt::KeepEmptyParts);
            int index = get_index_by_string(line_list.at(0));
            if(index >= 0){
                QString time = line_list.at(2);
                if(line_list.at(1) == "in"){
                    empleados[index].working_now = true;
                    set_tabla_entrada_salida( index , time , true );
                }else if(line_list.at(1) == "out"){
                    empleados[index].working_now = false;
                    set_tabla_entrada_salida( index , time, false );
                }
            }
        }
        return;
    }
    qDebug() << "No se pudo leer el estado";
}

QString AsistenciaSuper::get_log(QDate fecha){
    QString ruta_log = "logs/" + fecha.toString("yyyy") + "_" + QVariant(fecha.weekNumber()).toString() + "_" + QVariant(fecha.dayOfWeek()).toString() + ".log";
    return ruta_log;
}

void AsistenciaSuper::write_to_logs(QString nombre, bool in_out, QDate fecha, QTime tiempo){

    QFile file( get_log(fecha) );
    QString content = "";
    bool written = false;
    QString reg;
    if(in_out){
        reg = nombre + "," + "in"  + "," + tiempo.toString("hh:mm:ss") + "\n";
    }else{
        reg = nombre + "," + "out"  + "," + tiempo.toString("hh:mm:ss") + "\n";
    }
    if( file.open(QIODevice::ReadOnly) ){
        QTextStream log(&file);
        while( !log.atEnd() ){
            QString line = log.readLine();
            QStringList list = line.split(",", Qt::SkipEmptyParts);
            if( list.length() > 0 ){
                int h = list.at(2).split(":").at(0).toInt();
                int m = list.at(2).split(":").at(1).toInt();
                int s = list.at(2).split(":").at(2).toInt();
                if( QTime(h,m,s) > tiempo && !written ){
                    content.append(reg);
                    content.append( "\n" );
                    written = true;
                }
                content.append( line );
                content.append( "\n" );
            }
        }
    }
    if(!written){
        content.append(reg);
        content.append( "\n" );
    }
    file.close();
    file.open(QIODevice::ReadWrite | QIODevice::Truncate);
    QTextStream out(&file);
    out << content;
    file.close();
}

bool AsistenciaSuper::nativeEvent(const QByteArray &eventType, void *message, long *result) {

    MSG *msg = reinterpret_cast<MSG*>(message);

    if(msg->message == WMUS_FP_NOTIFY){

        switch (msg->wParam){

        case WN_COMPLETED:{

            qDebug() << "Imagen de huella capturada";
            leyendo = false;
            ui->imagen_huella->setPixmap(QPixmap(":images/huella_verde.png"));

            DATA_BLOB* blob = reinterpret_cast<DATA_BLOB*>(msg->lParam);
            QByteArray huella(reinterpret_cast<const char*>(blob->pbData),
                              static_cast<int>(blob->cbData));

            int index = verificar_coincidencia_trabajadores(huella);
            if( index != -1 ){
                emit this->show();
                toggle_trabajador(index);
            }

            ui->imagen_huella->setPixmap(QPixmap(":images/huella_gris.png"));
        }

        case WN_ERROR:
            qDebug() << "Ocurrio un error";
            ui->imagen_huella->setPixmap(QPixmap(":images/huella_roja.png"));
            ui->debug_huella->setText("Intentelo de nuevo");
            break;

        case WN_DISCONNECT:
            qDebug() << "Lector desconectado";
            ui->imagen_huella->setPixmap(QPixmap(":images/huella_roja.png"));
            break;

        case WN_RECONNECT:
            qDebug() << "Lector conectado";
            ui->imagen_huella->setPixmap(QPixmap(":images/huella_gris.png"));
            break;

        case WN_FINGER_TOUCHED:
            qDebug() << "Se puso el dedo";
            leyendo = true;
            ui->debug_huella->setText("Use el escaner");
            ui->imagen_huella->setPixmap(QPixmap(":images/huella_azul.png"));
            break;

        case WN_FINGER_GONE:
            qDebug() << "Se quito el dedo";
            if(leyendo){
                ui->imagen_huella->setPixmap(QPixmap(":images/huella_roja.png"));
                ui->debug_huella->setText("Intentelo de nuevo");
                leyendo = false;
            }else{
                ui->imagen_huella->setPixmap(QPixmap(":images/huella_gris.png"));
                ui->debug_huella->setText("Use el escaner");
            }
            break;

        case WN_IMAGE_READY:
            qDebug() << "Imagen de huella lista";
            ui->imagen_huella->setPixmap(QPixmap(":images/huella_azul.png"));
            break;

        case WN_OPERATION_STOPPED:
            qDebug() << "Detenido el proceso de adquisicion";
            break;

        default:
            break;
        }
    }

    return QMainWindow::nativeEvent(eventType, message, result);
}

int AsistenciaSuper::verificar_coincidencia_trabajadores(QByteArray huella){
    for(int i = 0; i < empleados.length(); i++){
        for(int j = 0; j < empleados[i].huellas.length(); j++){
            if( verificar_huella(huella, empleados[i].huellas[j]) ){
                return i;
            }
        }
    }
    return -1;
}

void AsistenciaSuper::cargar_trabajadores(){

    QString nombre_carpeta = "trabajadores";
    QDir dir = QDir(nombre_carpeta);
    if(dir.exists()){

        QStringList carpetas = dir.entryList( QDir::NoDotAndDotDot | QDir::Dirs );

        for(int i = 0; i < carpetas.length(); i++){

            Empleado empleado;
            empleado.nombre = carpetas[i];
            empleado.working_now = false;

            QTableWidgetItem* item = new QTableWidgetItem(empleado.nombre);
            ui->lista_trabajadores->setRowCount(i+1);
            ui->lista_trabajadores->setItem(i,0,item);
            ui->combo_nombres->addItem(carpetas[i]);
            ui->combo_admin->addItem(carpetas[i]);
            ui->combo_empleados->addItem(carpetas[i]);

            dir = QDir( nombre_carpeta + "/" + carpetas[i] );
            QStringList archivos = dir.entryList( QStringList() << "*.fpt", QDir::Files );

            for(int i = 0; i < archivos.length(); i++){
                QByteArray huella = leer_archivo_fpt(dir.path() + "/" + archivos[i]);
                empleado.huellas.append(huella);
            }

            empleados.append(empleado);
        }

    }else{
        QMessageBox msg;
        msg.setText("No se encontro la carpeta de trabajadores!");
        msg.exec();
    }
}

QByteArray AsistenciaSuper::leer_archivo_fpt(QString ruta){
    QByteArray data;

    QFile file(ruta);
    if(file.open(QIODevice::ReadOnly)){
        data = file.readAll();
        file.close();
        return data;
    }
    qDebug() << "No se pudo abrir el archivo : " << ruta;
    return data;
}

bool AsistenciaSuper::verificar_huella(QByteArray leida, QByteArray guardada){
    FT_BYTE* featureSet = NULL;
    try{
        int iRecommendedVerFtrLen = 0;
        FT_RETCODE rc = FX_getFeaturesLen(FT_VER_FTR,
                                          &iRecommendedVerFtrLen,
                                          NULL);

        if (rc != FT_OK){ return false; }

        FT_IMG_QUALITY imgQuality;
        FT_FTR_QUALITY ftrQuality;
        FT_BOOL bEextractOK = FT_FALSE;
        featureSet = new FT_BYTE[iRecommendedVerFtrLen];
        rc = FX_extractFeatures(m_fxContext,
                                leida.size(),
                                reinterpret_cast<const FT_IMAGE_PTC>(leida.constData()),
                                FT_VER_FTR,
                                iRecommendedVerFtrLen,
                                featureSet,
                                &imgQuality,
                                &ftrQuality,
                                &bEextractOK);

        if (rc != FT_OK || bEextractOK != FT_TRUE) { return false; }

        FT_BOOL bVerified = FT_FALSE;
        rc = MC_verifyFeaturesEx(m_mcContext,
                                 guardada.size(),
                                 reinterpret_cast<const FT_BYTE*>(guardada.constData()),
                                 iRecommendedVerFtrLen,
                                 featureSet,
                                 0,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 &bVerified);

        if (rc != FT_OK) { return false; }

        if (bVerified == FT_TRUE) {
            qDebug() << "La huella coincide !!!";
            return true;
        }

    } catch (const std::exception &e) {
        qDebug() << "Excepción atrapada:" << e.what();

    } catch(...){
        qDebug() << "Excepcion desconocida";
        return false;
    }
    delete [] featureSet;
    return false;
}

void AsistenciaSuper::on_calendarWidget_selectionChanged()
{
    QDate fecha_temp;
    QDate fecha = ui->calendarWidget->selectedDate();

    ui->fecha_tiempo_admin->setDate(fecha);

    generar_resumen_semanal(ui->combo_nombres->currentText(), fecha);
    QTextCharFormat char_format = ui->calendarWidget->dateTextFormat(fecha);

    fecha_temp = previousDate.addDays( -1 * QVariant(previousDate.dayOfWeek()%7).toInt() - 1 );
    for(int i = 0; i < 7; i ++){
        fecha_temp = fecha_temp.addDays(1);
        ui->calendarWidget->setDateTextFormat(fecha_temp, char_format);
    }

    char_format.setBackground( QBrush(QColor(17,130,220)) );
    fecha_temp = fecha.addDays( -1 * QVariant(fecha.dayOfWeek()%7).toInt() - 1 );
    for(int i = 0; i < 7; i ++){
        fecha_temp = fecha_temp.addDays(1);
        ui->calendarWidget->setDateTextFormat(fecha_temp, char_format);
    }

    previousDate = fecha;
}

void AsistenciaSuper::on_salida_admin_clicked()
{
    QMessageBox msg;
    msg.setStyleSheet("font-size: 12px;");

    if(ui->combo_admin->currentText() != "Seleccione"){
        write_to_logs(ui->combo_admin->currentText(), false, ui->fecha_tiempo_admin->date(), ui->fecha_tiempo_admin->time());
        read_state();
        msg.setText("SALIDA de\n"+ui->combo_admin->currentText()+
                    "\nmarcada exitosamente\nEl dia : "+
                    ui->fecha_tiempo_admin->date().toString("dd/MM/yy")+
                    "\nA las : "+ui->fecha_tiempo_admin->time().toString("hh:mm:ss"));
    }else{
        msg.setText("Seleccione un empleado");
    }

    msg.exec();
    read_state();
    update_combo_registers();
}

void AsistenciaSuper::on_entrada_admin_clicked()
{
    QMessageBox msg;
    msg.setStyleSheet("font-size: 12px;");

    if(ui->combo_admin->currentText() != "Seleccione"){
        write_to_logs(ui->combo_admin->currentText(), true, ui->fecha_tiempo_admin->date(), ui->fecha_tiempo_admin->time());
        read_state();
        msg.setText("ENTRADA de\n"+ui->combo_admin->currentText()+
                    "\nmarcada exitosamente\nEl dia : "+
                    ui->fecha_tiempo_admin->date().toString("dd/MM/yy")+
                    "\nA las : "+ui->fecha_tiempo_admin->time().toString("hh:mm:ss"));

    }else{
        msg.setText("Seleccione un empleado");
    }
    msg.exec();
    read_state();
    update_combo_registers();
}

void AsistenciaSuper::on_combo_nombres_currentTextChanged(const QString &arg1)
{
    ui->combo_admin->setCurrentText(arg1);

    QString nombre = ui->combo_nombres->currentText();
    QDate fecha = ui->calendarWidget->selectedDate();

    if(nombre == "Resumen hoy"){
        generar_resumen_hoy();
    }else{
        generar_resumen_semanal(nombre, fecha);
    }
}

void AsistenciaSuper::on_boton_texto_clicked()
{
    QMessageBox msg;
    msg.setStyleSheet("font-size: 15px;");
    msg.setText( ui->resumen_texto->text() );
    msg.exec();
}

void AsistenciaSuper::on_boton_corregir_extras_clicked()
{
    QMessageBox msg;
    if(ui->combo_admin->currentText() != "Seleccione"){
        get_horas_extra_semana(ui->combo_admin->currentText(),ui->fecha_tiempo_admin->date(),true);
        msg.setText("Recalculadas las horas extra de \n" + ui->combo_admin->currentText() + "\nPara TODA la semana que \ncontiene el dia : " + ui->fecha_tiempo_admin->date().toString("dddd dd/MM/yy") );
    }else{
        msg.setText("Debes seleccionar un empleado");
    }
    msg.exec();
}

void AsistenciaSuper::on_boton_borrar_es_clicked()
{
    QFile file( get_log(ui->fecha_tiempo_admin->date()) );
    if( file.open(QIODevice::ReadOnly) ){
        QString content = "";
        QTextStream log(&file);
        while( !log.atEnd() ){
            QString line = log.readLine();
            QStringList list = line.split(",");
            if( list.length() > 0 ){
                if( list.at(0).trimmed() != ui->combo_admin->currentText() ){
                    content.append( line );
                    content.append( "\n" );
                }
            }
        }
        file.close();
        file.open(QIODevice::ReadWrite | QIODevice::Truncate);
        QTextStream out(&file);
        out << content;
        file.close();
    }
    ui->combo_nombres->setCurrentText(ui->combo_admin->currentText());
    ui->calendarWidget->setSelectedDate(ui->fecha_tiempo_admin->date());
    QMessageBox msg;
    msg.setText("Borradas exitosamente los registros de \n" + ui->combo_admin->currentText() + "\nDel dia : " + ui->fecha_tiempo_admin->date().toString("dddd dd/MM/yy"));
    msg.exec();
}

void AsistenciaSuper::on_boton_carpeta_clicked()
{
    QDesktopServices::openUrl( QApplication::applicationDirPath() );
}

void AsistenciaSuper::on_acceso_admin_pressed()
{
    if(acceso_admin){
        ui->acceso_admin->setText("Acceso Admin");
        ui->widget_admins->setVisible(false);
        acceso_admin = false;
    }else{
        bool ok;
        QString text = QInputDialog::getText(this, tr("QInputDialog::getText()"),
                                             "Escriba la contraseña : ", QLineEdit::Password, "", &ok);
        if (ok && !text.isEmpty() && text==pass){
            ui->widget_admins->setVisible(true);
            ui->acceso_admin->setText("Deshabilitar Admin");
            acceso_admin = true;
        }
    }
}

void AsistenciaSuper::update_combo_registers(){
    ui->combo_registros->clear();
    ui->combo_registros->addItem("-");
    QFile file( get_log(ui->fecha_tiempo_admin->date()) );
    if( file.open(QIODevice::ReadOnly) ){
        QTextStream log(&file);
        while( !log.atEnd() ){
            QString line = log.readLine();
            if(line.split(",").at(0) == ui->combo_admin->currentText()){
                line.replace("in","Entrada");
                line.replace("out", "Salida");
                line.replace(","," - ");
                ui->combo_registros->addItem(line);
            }
        }
        file.close();
    }
}

void AsistenciaSuper::on_fecha_tiempo_admin_dateChanged(const QDate &date)
{
    ui->calendarWidget->setSelectedDate(date);
    update_combo_registers();
}

void AsistenciaSuper::on_combo_admin_currentTextChanged(const QString &arg1)
{
    ui->combo_nombres->setCurrentText(arg1);
    update_combo_registers();
}

void AsistenciaSuper::on_boton_quitar_registro_clicked()
{

    QFile file( get_log(ui->fecha_tiempo_admin->date()) );
    if( file.open(QIODevice::ReadOnly) ){
        QString content = "";
        QTextStream log(&file);
        while( !log.atEnd() ){
            QString line = log.readLine();
            QStringList list = line.split(",");
            if( list.length() > 0 ){

                QString temp = ui->combo_registros->currentText();
                temp.replace("Entrada","in");
                temp.replace("Salida","out");
                temp.replace(" - ",",");

                if( temp != line ){
                    content.append( line );
                    content.append( "\n" );
                }else{
                    QMessageBox msg;
                    msg.setText("Registro borrado exitosamente");
                    msg.exec();
                }
            }
        }
        file.close();
        file.open(QIODevice::ReadWrite | QIODevice::Truncate);
        QTextStream out(&file);
        out << content;
        file.close();
    }
    ui->combo_nombres->setCurrentText(ui->combo_admin->currentText());
    ui->calendarWidget->setSelectedDate(ui->fecha_tiempo_admin->date());

    update_combo_registers();
    read_state();
}

void AsistenciaSuper::enviar_correo(){

    SMTPClient *client_;

    // Create the credentials EmailAddress
    EmailAddress credentials("perrusquia832@gmail.com",
                             "gypgkzcyuifqjabo");

    // Create the from EmailAddress
    EmailAddress from("Asistencia Super");

    // Create the to EmailAddress
    EmailAddress to("supervazq@gmail.com");


    ///////////////////////////////////////////////////////////////////////////

    QString contenido = "Resumen semanal de horas trabajadas desde " + QDate::currentDate().addDays(-1 * (QVariant(QDate::currentDate().dayOfWeek()).toInt()%7 + 7) ).toString("dddd d/M/yyyy") + " hasta " + QDate::currentDate().addDays(-1 * (QVariant(QDate::currentDate().dayOfWeek()).toInt() %7 +1) ).toString("dddd d/M/yyyy") + "\n";
    contenido.append("Si un trabajador tiene menos de 1 hora trabajada en este periodo, no aparecerá en este resumen.\n");

    for(int i = 0; i < empleados.length(); i++){
        generar_resumen_semanal( empleados[i].nombre, QDate::currentDate().addDays(-7) );
        if(ui->horas_semana_totales->text().at(0) != '0'){
            contenido.append("\n-----------------------------------------------------------------------------------------------------------\n");
            contenido.append( empleados[i].nombre.toUpper() + " trabajó en total " + ui->horas_semana_totales->text() + " ( " + ui->dias_semana->text() + " dias y " + ui->horas_semana_extra->text() + ").\n" );
            contenido.append( "Tenía " + ui->horas_guardadas_extra->text() + " acumuladas de semanas pasadas, por lo tanto : \n"  );
            contenido.append( "Dias a pagar : " + ui->dias_totales->text() + ", se le guardarán " + ui->horas_que_se_guardan->text());
        }
    }
    qDebug() << "Contenido del correo : " << contenido;

    //////////////////////////////////////////////////////////////////////////


    // Create the email
    Email email(credentials,
                from,
                to,
                "Resumen Semanal y Backup",
                contenido);

    // Create the SMTPClient
    client_ = new SMTPClient("smtp.gmail.com", 465);

    //connect slots
    connect(client_, SIGNAL(status(Status::e, QString)),
            this, SLOT(onStatus(Status::e, QString)), Qt::UniqueConnection);

    // Try to send the email
    client_->sendEmail(email);
}

void AsistenciaSuper::on_boton_correo_clicked()
{
    enviar_correo();
}

void AsistenciaSuper::onStatus(Status::e status, QString errorMessage){
    switch (status)
    {
    case Status::Success:{
        qDebug() << "Correo enviado correctamente";
        break;
    }
    case Status::Failed:{
        qDebug() << "Error : " << errorMessage;
        break;
    }
    default:
        break;
    }
}

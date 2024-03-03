QT       += core gui network

RC_ICONS = logo.ico

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

INCLUDEPATH += $$PWD/fingerReaderSDK/include
DEPENDPATH += $$PWD/fingerReaderSDK/include

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    asistenciasuper.cpp \
    mensaje.cpp

HEADERS += \
    asistenciasuper.h \
    mensaje.h

FORMS += \
    asistenciasuper.ui \
    mensaje.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

LIBS += -L$$PWD/fingerReaderSDK/lib/ -lDPFPApi -ldpHMatch -ldpHFtrEx -lDPFpUI
LIBS += -L$$PWD/SMTP/lib -lSmtpMime2

PRE_TARGETDEPS += $$PWD/fingerReaderSDK/lib/DPFPApi.lib
PRE_TARGETDEPS += $$PWD/fingerReaderSDK/lib/dpHMatch.lib
PRE_TARGETDEPS += $$PWD/fingerReaderSDK/lib/dpHFtrEx.lib
PRE_TARGETDEPS += $$PWD/fingerReaderSDK/lib/DPFpUI.lib

RESOURCES += \
    images.qrc \
    styles.qrc

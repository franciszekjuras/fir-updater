QT += widgets

CONFIG += c++11 core
CONFIG -= app_bundle

DEFINES += QT_DEPRECATED_WARNINGS
__APPNAME = "FIR Controller"
__APPEXE = "FIR Controller"
DEFINES += APPSEMAPHORE=\"\\\"firgui_update\\\"\"
DEFINES += APPSOURCE=\"\\\"https://github.com/franciszekjuras/firgui\\\"\"
DEFINES += APPNAME=\"\\\"$${__APPNAME}\\\"\"
DEFINES += APPEXE=\"\\\"$${__APPEXE}\\\"\"

SOURCES += \
        main.cpp


RESOURCES += \
    data/data.qrc


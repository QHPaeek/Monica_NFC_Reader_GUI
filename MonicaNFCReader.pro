QT       += core gui widgets network serialport
CONFIG   += static release
TEMPLATE  = app
TARGET    = Monica_NFC_Reader_GUI

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/hidworker.cpp \
    src/banapasscard.cpp \
    src/accesscode.cpp \
    src/portscan.cpp \
    src/bootproto.cpp \
    src/aimeserial.cpp

HEADERS += \
    src/mainwindow.h \
    src/hidworker.h \
    src/flashdata.h \
    src/banapasscard.h \
    src/accesscode.h \
    src/sbox.h \
    src/portscan.h \
    src/bootproto.h \
    src/aimeserial.h

RESOURCES += resources.qrc

INCLUDEPATH += C:/msys64/mingw64/include/hidapi
LIBS += -LC:/msys64/mingw64/lib -lhidapi -lsetupapi -lssl -lcrypto

QT       += core gui widgets network
CONFIG   += static release
TEMPLATE  = app
TARGET    = Monica_NFC_Reader_GUI

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/hidworker.cpp \
    src/banapasscard.cpp

HEADERS += \
    src/mainwindow.h \
    src/hidworker.h \
    src/flashdata.h \
    src/banapasscard.h

RESOURCES += resources.qrc

INCLUDEPATH += C:/msys64/mingw64/include/hidapi
LIBS += -LC:/msys64/mingw64/lib -lhidapi -lsetupapi -lssl -lcrypto

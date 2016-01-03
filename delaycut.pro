QT         += core gui

greaterThan(QT_MAJOR_VERSION, 4) {
QT += widgets
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x000000
}

CONFIG     += qt console
TARGET      = delaycut
TEMPLATE    = app

INCLUDEPATH += src

SOURCES    += src/main.cpp\
              src/delaycut.cpp \
              src/delayac3.cpp \
              src/dragdroplineedit.cpp

HEADERS    += src/delaycut.h \
              src/sil48.h \
              src/delayac3.h \
              src/types.h \
              src/dragdroplineedit.h

FORMS      += src/delaycut.ui

Win32 {
RESOURCES  += src/delaycut_ico.qrc
RC_FILE     = src/delaycut.rc
}

Unix {
RESOURCES  += src/delaycut_png.qrc
target.path = /usr/bin
INSTALLS   += target
}


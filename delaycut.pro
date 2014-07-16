#-------------------------------------------------
#
# Project created by QtCreator 2011-10-26T17:38:40
#
#-------------------------------------------------

Win32 {
QT       += core gui
CONFIG += qt \
console

TARGET = delaycut
TEMPLATE = app


SOURCES += main.cpp\
        delaycut.cpp \
    delayac3.cpp \
    dragdroplineedit.cpp

HEADERS  += delaycut.h \
    sil48.h \
    delayac3.h \
    types.h \
    dragdroplineedit.h

FORMS    += \
    delaycut.ui

RESOURCES += \
    delaycut.qrc
    
RC_FILE = delaycut.rc
}

Linux {
QT       += core gui
CONFIG += qt \
console

TARGET = delaycut
target.path = /usr/bin
INSTALLS += target
TEMPLATE = app


SOURCES += main.cpp\
        delaycut.cpp \
    delayac3.cpp \
    dragdroplineedit.cpp

HEADERS  += delaycut.h \
    sil48.h \
    delayac3.h \
    types.h \
    dragdroplineedit.h

FORMS    += \
    delaycut.ui

RESOURCES += \
    delaycut.qrc
}

QT         += core gui
CONFIG     += qt console
TARGET      = delaycut
TEMPLATE    = app

SOURCES    += main.cpp\
              delaycut.cpp \
              delayac3.cpp \
              dragdroplineedit.cpp

HEADERS    += delaycut.h \
              sil48.h \
              delayac3.h \
              types.h \
              dragdroplineedit.h

FORMS      += delaycut.ui

Win32 {
RESOURCES  += delaycut.qrc
RC_FILE     = delaycut.rc
}

Unix {
target.path = /usr/bin
INSTALLS   += target
}


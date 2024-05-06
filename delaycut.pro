QT         += core gui widgets
CONFIG     += qt console c++11
TARGET      = delaycut
TEMPLATE    = app

INCLUDEPATH += src

SOURCES    += src/main.cpp\
              src/delaycut.cpp \
              src/delayac3.cpp \
              src/dragdroplineedit.cpp \
              src/frame.cpp

HEADERS    += src/delaycut.h \
              src/sil48.h \
              src/delayac3.h \
              src/dc_types.h \
              src/dragdroplineedit.h \
              src/frame.h

FORMS      += src/delaycut.ui

win32 {
  RESOURCES  += src/icon_ico.qrc
  RC_FILE     = src/delaycut.rc
}

!win32 {
  QMAKE_CXXFLAGS += -Wno-unused-but-set-variable
  RESOURCES      += src/icon_png.qrc
  target.path     = /usr/bin
  INSTALLS       += target
}

win32-g++* {
  QMAKE_CXXFLAGS += -Wno-unused-but-set-variable
  !contains(QMAKE_HOST.arch, x86_64):QMAKE_LFLAGS += -Wl,--large-address-aware
}

win32-msvc* {
  QMAKE_CXXFLAGS += /bigobj
  !contains(QMAKE_HOST.arch, x86_64):QMAKE_LFLAGS += /LARGEADDRESSAWARE
}

macx {
  ICON = src/icon.icns
  #If you are using qt from brew, remove line below
  QMAKE_APPLE_DEVICE_ARCHS = x86_64 arm64
}

QMAKE_CLEAN += qrc_icon_ico.cpp \
               qrc_icon_ico.o \
               qrc_icon_png.cpp \
               qrc_icon_png.o


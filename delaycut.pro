QT         += core gui

greaterThan(QT_MAJOR_VERSION, 4) {
  QT += widgets
  DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x000000
}

CONFIG     += qt console c++11
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
              src/dc_types.h \
              src/dragdroplineedit.h

FORMS      += src/delaycut.ui

win32 {
  RESOURCES  += src/icon_ico.qrc
  RC_FILE     = src/delaycut.rc
}

!win32 {
  !greaterThan(QT_MAJOR_VERSION, 4):QMAKE_CXXFLAGS += -std=c++11
  QMAKE_CXXFLAGS += -Wno-unused-but-set-variable -Wno-unused-variable
  RESOURCES  += src/icon_png.qrc
  target.path = /usr/bin
  INSTALLS   += target
}

win32-g++* {
  !greaterThan(QT_MAJOR_VERSION, 4):QMAKE_CXXFLAGS += -std=c++11
  QMAKE_CXXFLAGS += -Wno-unused-but-set-variable

  # tested with MXE (https://github.com/mxe/mxe)
  !contains(QMAKE_HOST.arch, x86_64):QMAKE_LFLAGS += -Wl,--large-address-aware
}

win32-msvc* {
  QMAKE_CXXFLAGS += /bigobj # allow big objects
  !contains(QMAKE_HOST.arch, x86_64):QMAKE_LFLAGS += /LARGEADDRESSAWARE
  QMAKE_CFLAGS_RELEASE += -WX
  QMAKE_CFLAGS_RELEASE_WITH_DEBUGINFO += -WX


  # for Windows XP compatibility
  contains(QMAKE_HOST.arch, x86_64) {
    #message(Going for Windows XP 64bit compatibility)
    #QMAKE_LFLAGS += /SUBSYSTEM:WINDOWS,5.02 # Windows XP 64bit
  } else {
    message(Going for Windows XP 32bit compatibility)
    QMAKE_LFLAGS += /SUBSYSTEM:WINDOWS,5.01 # Windows XP 32bit
  }
}

QMAKE_CLEAN += qrc_icon_ico.cpp \
               qrc_icon_ico.o \
               qrc_icon_png.cpp \
               qrc_icon_png.o


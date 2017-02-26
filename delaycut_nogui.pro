# experimental commandline-only build

CONFIG = qt console c++11

TARGET = delaycut_nogui

INCLUDEPATH += src

SOURCES += src/cli.cpp src/delayac3.cpp
HEADERS += src/cli.h   src/delayac3.h   src/dc_types.h   src/sil48.h

QMAKE_CXXFLAGS += -std=c++11 -Wno-unused-but-set-variable -Wno-unused-variable

# don't link against QtGUi or other unneeded libraries
QMAKE_LFLAGS += -Wl,--as-needed


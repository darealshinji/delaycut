#!/bin/sh -e
# Link against static Qt5
# https://github.com/darealshinji/qt5-static/releases

g++ -s -Wl,--as-needed -Wl,-z,defs -Wl,-z,relro -Wl,-O1 -o delaycut *.o -Wl,-Bstatic \
 -L/var/tmp/qt-static-trusty/lib -lQt5Widgets \
 -L/var/tmp/qt-static-trusty/plugins/platforms -lqxcb \
 -L/var/tmp/qt-static-trusty/plugins/xcbglintegrations -lqxcb-glx-integration \
 -lQt5XcbQpa $(pkg-config --static --libs xcb-xinerama) \
 -lQt5ServiceSupport -lQt5ThemeSupport -lQt5EventDispatcherSupport -lQt5FontDatabaseSupport \
 -lQt5LinuxAccessibilitySupport -lQt5AccessibilitySupport -lQt5GlxSupport -lQt5DBus \
 $(pkg-config --static --libs xcb-xkb xcb-sync xcb-xfixes xcb-randr xcb-image xcb-util) -lX11-xcb \
 $(pkg-config --static --libs xcb-shm xcb-keysyms xcb-icccm xcb-glx xcb-renderutil xcb-render) \
 -L/var/tmp/qt-static-trusty/plugins/imageformats -lqicns -lqico -lqjpeg -ljpeg -lqtga -lqtiff \
 -lqwbmp -lqwebp -lQt5Gui -lqtharfbuzz -lpng12 -lz -lQt5Core -lz -licui18n -licuuc -licudata -lqtpcre \
 -Wl,-Bdynamic -lX11 -lXi -lICE -lSM -lfontconfig -lfreetype -lGL -lglib-2.0 -ldl -lm -lpthread -lrt

readelf -d delaycut | grep NEEDED

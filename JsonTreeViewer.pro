QT += gui core widgets
TARGET  = jsontreeviewer
CONFIG += c++17

TEMPLATE = lib
CONFIG += plugin
TARGET_EXT = .dll
# SOURCES += src/test.cpp

SOURCES += \
    src/jsontreemodel.cpp \
    src/jsontreeview.cpp \
    src/jsontreeviewer.cpp

HEADERS += \
    src/jsonnode.h \
    src/jsontreemodel.h \
    src/jsontreeview.h \
    src/jsontreeviewer.h

DISTFILES += bin/plugin.json

include(sdk.pri)
include(simdjson.pri)

VERSION = 1.0.1
QMAKE_TARGET_COMPANY = "1218.io"
QMAKE_TARGET_PRODUCT = "Seer"
QMAKE_TARGET_DESCRIPTION = "Seer - A Windows Quick Look Tool"
QMAKE_TARGET_COPYRIGHT = "Corey"

QT += gui core widgets

TEMPLATE = lib
CONFIG += plugin
# SOURCES += src/test.cpp

CONFIG += c++17


SOURCES += \
    src/jsontreemodel.cpp \
    src/jsontreeview.cpp \
    src/jsontreeviewer.cpp

HEADERS += \
    src/jsonnode.h \
    src/jsontreemodel.h \
    src/jsontreeview.h \
    src/jsontreeviewer.h

DISTFILES += JsonTreeViewer.json

include(sdk.pri)
include(simdjson.pri)

VERSION = 1.0.0
QMAKE_TARGET_COMPANY = "1218.io"
QMAKE_TARGET_PRODUCT = "Seer"
QMAKE_TARGET_DESCRIPTION = "Seer - A Windows Quick Look Tool"
QMAKE_TARGET_COPYRIGHT = "Corey"
QT += gui core widgets

TEMPLATE = lib
CONFIG += plugin
# SOURCES += test.cpp

CONFIG += c++17


SOURCES += \
    jsontreemodel.cpp \
    jsontreeview.cpp \
    jsontreeviewer.cpp

HEADERS += \
    jsonnode.h \
    jsontreemodel.h \
    jsontreeview.h \
    jsontreeviewer.h

DISTFILES += JsonTreeViewer.json

include(sdk.pri)
include(simdjson.pri)

VERSION = 1.0.0
QMAKE_TARGET_COMPANY = "1218.io"
QMAKE_TARGET_PRODUCT = "Seer"
QMAKE_TARGET_DESCRIPTION = "Seer - A Windows Quick Look Tool"
QMAKE_TARGET_COPYRIGHT = "Corey"
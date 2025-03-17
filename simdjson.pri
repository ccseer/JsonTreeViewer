
# LIBS += -L$$PWD/simdjson/lib/ -lsimdjson

# INCLUDEPATH += $$PWD/simdjson/include/
# DEPENDPATH += $$PWD/simdjson/include/

# # https://github.com/microsoft/vcpkg/issues/16062
# DEFINES += SIMDJSON_USING_LIBRARY



INCLUDEPATH += $$PWD/simdjson/
HEADERS += \
    $$PWD/simdjson/simdjson.h

SOURCES += \
    $$PWD/simdjson/simdjson.cpp

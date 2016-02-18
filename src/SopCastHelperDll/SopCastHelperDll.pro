TARGET = dbghelp

TEMPLATE = lib

CONFIG(release, debug|release):DEFINES += NDEBUG

QMAKE_LFLAGS += -static
QMAKE_CXXFLAGS += -Wpedantic

LIBS += -lgdi32 -lcomctl32 -lole32 -lws2_32

SOURCES += main.cpp

DEF_FILE = SopCastHelperDll.def

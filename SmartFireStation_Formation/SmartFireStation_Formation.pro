QT       += core gui sql widgets printsupport charts

CONFIG   += c++17

TARGET = SmartFireStation_Formation
TEMPLATE = app

SOURCES += \
    main.cpp \
    connection.cpp \
    formation.cpp \
    splashscreen.cpp \
    mainwindow.cpp

HEADERS += \
    connection.h \
    formation.h \
    splashscreen.h \
    mainwindow.h

RESOURCES += resources.qrc
msvc:QMAKE_CXXFLAGS += /utf-8

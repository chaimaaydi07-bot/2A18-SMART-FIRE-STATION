QT += widgets

CONFIG += c++17

TARGET = FireStation

INCLUDEPATH += ../Incidents

SOURCES += \
    main.cpp \
    firestation.cpp \
    pageconnexion.cpp \
    ../Incidents/incidents.cpp

HEADERS += \
    firestation.h \
    pageconnexion.h \
    ../Incidents/incidents.h

RESOURCES += \
    ressources.qrc \
    ../Incidents/images.qrc

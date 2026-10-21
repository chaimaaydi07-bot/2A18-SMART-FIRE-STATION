QT += widgets printsupport network sql

CONFIG += c++17

TARGET = FireStation

INCLUDEPATH += ../Incidents ../Equipements ../Vehicules

SOURCES += \
    connection.cpp \
    main.cpp \
    agent.cpp \
    firestation.cpp \
    logoanime.cpp \
    pageconnexion.cpp \
    ../Incidents/incidents.cpp \
    ../Vehicules/vehicules.cpp \
    ../Equipements/smart.cpp

HEADERS += \
    agent.h \
    connection.h \
    firestation.h \
    logoanime.h \
    pageconnexion.h \
    ../Incidents/incidents.h \
    ../Vehicules/vehicules.h \
    ../Equipements/smart.h

RESOURCES += \
    ressources.qrc \
    ../Incidents/images.qrc \
    ../Equipements/resources.qrc
SOURCES += ../Incidents/incident.cpp
HEADERS += ../Incidents/incident.h
SOURCES += ../Vehicules/vehiculerepository.cpp
HEADERS += ../Vehicules/vehiculerepository.h

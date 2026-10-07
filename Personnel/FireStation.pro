QT += widgets printsupport network

CONFIG += c++17

TARGET = FireStation

INCLUDEPATH += ../Incidents ../Equipements ../Vehicules

SOURCES += \
    main.cpp \
    firestation.cpp \
    logoanime.cpp \
    pageconnexion.cpp \
    ../Incidents/incidents.cpp \
    ../Vehicules/vehicules.cpp \
    ../Equipements/smart.cpp

HEADERS += \
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

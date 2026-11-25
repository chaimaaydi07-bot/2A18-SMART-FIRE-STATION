cmake_minimum_required(VERSION 3.16)

project(SmartFireStation VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Sql Charts PrintSupport Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql Charts PrintSupport Network)

set(PROJECT_SOURCES
    Personnel/main.cpp
    Personnel/connection.cpp
    Personnel/agent.cpp
    Personnel/firestation.cpp
    Personnel/logoanime.cpp
    Personnel/pageconnexion.cpp
    Incidents/incident.cpp
    Incidents/incidents.cpp
    Vehicules/vehicules_view.cpp
    Vehicules/vehicules.cpp
    Vehicules/vehiculerepository.cpp
    Equipements/equipement.cpp
    Equipements/equipement_extra.cpp
    Equipements/smart.cpp
    Formations/formation.cpp
    Formations/inscription.cpp
    Formations/formationswindow.cpp
    Personnel/ressources.qrc
    Incidents/images.qrc
    Equipements/resources.qrc
    Formations/formations_resources.qrc

    Personnel/agent.h
    Personnel/connection.h
    Personnel/firestation.h
    Personnel/logoanime.h
    Personnel/pageconnexion.h
    Incidents/incident.h
    Incidents/incidents.h
    Vehicules/vehiculerepository.h
    Vehicules/vehicules.h
    Vehicules/vehicules_utils.h
    Equipements/equipement.h
    Equipements/equipement_extra.h
    Equipements/smart.h
    Formations/formation.h
    Formations/inscription.h
    Formations/formationswindow.h
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_add_executable(FireStation
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(FireStation ${PROJECT_SOURCES})
endif()

target_include_directories(FireStation PRIVATE
    Personnel
    Incidents
    Vehicules
    Equipements
    Formations
)

target_link_libraries(FireStation PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Charts
    Qt${QT_VERSION_MAJOR}::PrintSupport
    Qt${QT_VERSION_MAJOR}::Network
)

set_target_properties(FireStation PROPERTIES WIN32_EXECUTABLE TRUE)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(FireStation)
endif()

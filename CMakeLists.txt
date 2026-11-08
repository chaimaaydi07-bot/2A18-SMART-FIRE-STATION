cmake_minimum_required(VERSION 3.16)

project(smartfirestation VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Sql Multimedia)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql Multimedia)

set(PROJECT_SOURCES
    main.cpp
    formation.cpp formation.h
    inscription.cpp inscription.h
    formationdialog.cpp formationdialog.h
    inscriptiondialog.cpp inscriptiondialog.h
    formationswindow.cpp formationswindow.h
    statsdialog.cpp statsdialog.h
    connexion.cpp connexion.h
    images.qrc
    resources.qrc
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_add_executable(smartfirestation MANUAL_FINALIZATION ${PROJECT_SOURCES})
else()
    add_executable(smartfirestation ${PROJECT_SOURCES})
endif()

target_link_libraries(smartfirestation PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Multimedia
)

set_target_properties(smartfirestation PROPERTIES WIN32_EXECUTABLE TRUE)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(smartfirestation)
endif()

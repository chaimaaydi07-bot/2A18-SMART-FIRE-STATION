cmake_minimum_required(VERSION 3.16)


project(smartfireincidents VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql Multimedia)

set(PROJECT_SOURCES
    main.cpp
    images.qrc
    resources.qrc
    formation.cpp formation.h
    inscription.cpp inscription.h
    formationdialog.cpp formationdialog.h
    inscriptiondialog.cpp inscriptiondialog.h
    statsdialog.cpp statsdialog.h
    formationswindow.cpp formationswindow.h
    connexion.cpp connexion.h
    connection.cpp connection.h
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_add_executable(smartfireincidents
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(smartfireincidents
        ${PROJECT_SOURCES}
    )
endif()

target_link_libraries(smartfireincidents
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Sql
        Qt${QT_VERSION_MAJOR}::Multimedia
)

set_target_properties(smartfireincidents PROPERTIES
    WIN32_EXECUTABLE TRUE
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(smartfireincidents)
endif()

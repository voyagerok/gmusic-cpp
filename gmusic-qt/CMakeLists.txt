cmake_minimum_required(VERSION 2.8)

project(gmusic-qt)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)

find_package(Qt5Widgets)

qt5_wrap_ui(OUT_UI_FILES mainwindow.ui logindialog.ui)

set(SRC
    ${OUT_UI_FILES}
    main.cpp
    mainwindow.cpp
    mainwindow.h
    logindialog.cpp
    logindialog.h)

add_executable(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME} gmusic Qt5::Widgets)

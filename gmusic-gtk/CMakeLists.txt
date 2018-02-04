cmake_minimum_required(VERSION 2.8)

project(gmusic-gtk)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set (RESOURCE_SRC "${CMAKE_CURRENT_BINARY_DIR}/resources.c")
set (RESOURCE_HEADER "${CMAKE_CURRENT_BINARY_DIR}/resources.h")
set (RESOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

execute_process(
    COMMAND glib-compile-resources --generate-dependencies --sourcedir=${RESOURCES_DIR} "${RESOURCES_DIR}/resources.xml"
    OUTPUT_VARIABLE GLIB_COMPRILE_RESOURCES_DEPS)
string(REPLACE "\n" ";" GLIB_COMPRILE_RESOURCES_DEPS_LIST ${GLIB_COMPRILE_RESOURCES_DEPS})
message(STATUS "GLIB_COMPILE_RESOURCES_DEPS: " ${GLIB_COMPRILE_RESOURCES_DEPS_LIST})
add_custom_command(
    OUTPUT ${RESOURCE_SRC} ${RESOURCE_HEADER}
    COMMAND glib-compile-resources --generate-source --sourcedir ${RESOURCES_DIR} --target ${RESOURCE_SRC} "${RESOURCES_DIR}/resources.xml"
    COMMAND glib-compile-resources --generate-header --sourcedir ${RESOURCES_DIR} --target ${RESOURCE_HEADER} "${RESOURCES_DIR}/resources.xml"
    DEPENDS "${RESOURCES_DIR}/resources.xml" ${GLIB_COMPRILE_RESOURCES_DEPS_LIST}
    COMMENT "Processing resource files...")
add_custom_target(
    GLIB_COMPILE_RESOURCES
    DEPENDS ${RESOURCE_SRC} ${RESOURCE_HEADER}
    )

find_package(PkgConfig REQUIRED)

pkg_check_modules(GTKMM gtkmm-3.0 REQUIRED)

set(SRC
    ${RESOURCE_SRC}
    ${RESOURCE_HEADER}
    "main-window.cpp"
    "main-window.hpp"
    "login-window.cpp"
    "login-window.hpp"
    "application.cpp"
    "application.hpp"
    "main.cpp")

add_executable(${PROJECT_NAME} ${SRC})
add_dependencies(${PROJECT_NAME} GLIB_COMPILE_RESOURCES)
target_link_libraries(${PROJECT_NAME} gmusic ${GTKMM_LIBRARIES})
include_directories(${GTKMM_INCLUDE_DIRS})

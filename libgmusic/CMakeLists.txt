cmake_minimum_required(VERSION 2.8)

project(gmusic)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(OpenSSL 1.0.2 REQUIRED)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED system filesystem)
find_package(PkgConfig REQUIRED)

pkg_check_modules(MPG123 libmpg123 REQUIRED)
pkg_check_modules(AO ao REQUIRED)
pkg_check_modules(SQLITE_3  sqlite3 REQUIRED)

include(CheckLibraryExists)
CHECK_LIBRARY_EXISTS(crypto HMAC_CTX_new "openssl/hmac.h" LCRYPTO_HAS_HMAC_CTX_NEW)
CHECK_LIBRARY_EXISTS(crypto RSA_set0_key "openssl/rsa.h" LCRYPTO_HAS_RSA_SET0_KEY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

set(SRC
    "utilities.cpp"
    "utilities.hpp"
    "http/httpsession.cpp"
    "http/httpsession.hpp"
    "http/httperror.cpp"
    "http/httperror.hpp"
    "api/gmapi.cpp"
    "api/gmapi.hpp"
    "model/model.hpp"
    "operation-queue.cpp"
    "operation-queue.hpp"
    "db/db-engine.cpp"
    "db/db-engine.hpp"
    "db/database.hpp"
    "db/database.cpp"
    "session.cpp"
    "session.hpp"
    "kvstorage.cpp"
    "kvstorage.hpp"
    "player.cpp"
    "player.hpp"
    )

add_library(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME}
    ${OPENSSL_LIBRARIES}
    ${CURL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${SQLITE_3_LIBRARIES}
    ${Boost_LIBRARIES}
    ${MPG123_LIBRARIES}
    ${AO_LIBRARIES})

include_directories(
    ${OPENSSL_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    ${SQLITE_3_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${MPG123_INCLUDE_DIRS}
    ${AO_INCLUDE_DIRS}
    )
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

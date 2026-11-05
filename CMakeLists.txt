cmake_minimum_required(VERSION 3.10)
project(timemachineplus_cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_compile_definitions(_UNICODE)
if(MSVC)
    add_compile_options(/utf-8)
endif()

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(thirdparty/OpenSSL)
include_directories(thirdparty/cpp-httplib)
include_directories(thirdparty/nlohmann)

# Add SQLite3 C++ wrapper around sqlite3 library (and sqlite3 itself provided for ease of use)
# Here you can set CMake variables to avoid building Example, as well as cpplint, cppcheck...
# or set them in the cmake command line (see for instance provided build.bat/build.sh scripts)
set(SQLITECPP_RUN_CPPCHECK OFF CACHE BOOL "" FORCE)
set(SQLITECPP_RUN_CPPLINT OFF CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/SQLiteCpp)

add_executable(timemachineplus
    src/main.cpp
    src/sqlite_helper.cpp
    src/service_run.cpp
    src/util.cpp
    src/api_server.cpp
    src/stdio_server.cpp)

# Find OpenSSL for MD5 hashing
# list(APPEND CMAKE_PREFIX_PATH "%userprofile%/.conda/envs/scratch/Library")
# find_package(OpenSSL REQUIRED)
# if(OpenSSL_FOUND)
#     target_include_directories(timemachineplus PRIVATE ${OPENSSL_INCLUDE_DIR})
#     if(LINUX)
#         target_link_libraries(timemachineplus "libcrypto.a" SQLiteCpp)
#     elseif(WIN32)
#         target_link_libraries(timemachineplus "${PROJECT_SOURCE_DIR}/thirdparty/OpenSSL/libcrypto.lib" SQLiteCpp)
#     endif()
# endif()

target_include_directories(timemachineplus PRIVATE ${OPENSSL_INCLUDE_DIR})
if(LINUX)
    target_link_libraries(timemachineplus "${PROJECT_SOURCE_DIR}/thirdparty/OpenSSL/lib/libcrypto.a" SQLiteCpp)
elseif(WIN32)
    target_link_libraries(timemachineplus
        "${PROJECT_SOURCE_DIR}/thirdparty/OpenSSL/lib/libcrypto.lib"
        SQLiteCpp
        ws2_32 mswsock)
endif()


# Link SQLiteCpp_example1 with SQLiteCpp
# target_link_libraries(timemachineplus SQLiteCpp)
# if(LINUX)
#     set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
#     set_target_properties(timemachineplus PROPERTIES INSTALL_RPATH "$ORIGIN")
# endif()
install(TARGETS timemachineplus DESTINATION bin)

# ---- QML UI（可选）：本机存在 Qt5.15+ 时构建 ----
# 关闭方式：cmake -DTIMEMACHINEPLUS_BUILD_QML_UI=OFF
option(TIMEMACHINEPLUS_BUILD_QML_UI "Build the QML (Qt Quick) desktop UI" ON)
if(TIMEMACHINEPLUS_BUILD_QML_UI)
    find_package(Qt5 5.15 QUIET COMPONENTS Quick Qml Gui QuickControls2 Concurrent)
    if(Qt5_FOUND)
        message(STATUS "Qt5 found (${Qt5_VERSION}), QML UI will be built")
        add_subdirectory(qml-ui)
    else()
        message(STATUS "Qt5 not found, skipping QML UI (CLI/stdio backend still builds)")
    endif()
endif()

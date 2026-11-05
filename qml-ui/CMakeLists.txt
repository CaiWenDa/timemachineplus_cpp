# QML UI 子工程：Qt Quick 界面的 Time Machine+
# 由根 CMakeLists.txt 通过 add_subdirectory(qml-ui) 引入（Qt5 存在时）
cmake_minimum_required(VERSION 3.16)

find_package(Qt5 5.15 REQUIRED COMPONENTS Quick Qml Gui QuickControls2 Concurrent)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# 复用主项目后端源码（同进程直接调用 ServiceRun，不经 stdio/HTTP）
set(TMP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_executable(timemachineplus_qml WIN32
    src/main.cpp
    src/BackendBridge.h
    src/BackendBridge.cpp
    src/app.manifest
    ${TMP_ROOT}/src/service_run.cpp
    ${TMP_ROOT}/src/sqlite_helper.cpp
    ${TMP_ROOT}/src/util.cpp
    qml/qml.qrc)

target_include_directories(timemachineplus_qml PRIVATE
    ${TMP_ROOT}/include
    ${TMP_ROOT}/thirdparty/OpenSSL
    ${TMP_ROOT}/thirdparty/nlohmann)

target_compile_definitions(timemachineplus_qml PRIVATE _UNICODE UNICODE)
if(MSVC)
    target_compile_options(timemachineplus_qml PRIVATE /utf-8)
endif()

target_link_libraries(timemachineplus_qml PRIVATE
    Qt5::Quick Qt5::Qml Qt5::Gui Qt5::QuickControls2 Qt5::Concurrent
    SQLiteCpp)

if(WIN32)
    target_link_libraries(timemachineplus_qml PRIVATE
        "${TMP_ROOT}/thirdparty/OpenSSL/lib/libcrypto.lib"
        ws2_32 mswsock)
elseif(LINUX)
    target_link_libraries(timemachineplus_qml PRIVATE
        "${TMP_ROOT}/thirdparty/OpenSSL/lib/libcrypto.a")
endif()

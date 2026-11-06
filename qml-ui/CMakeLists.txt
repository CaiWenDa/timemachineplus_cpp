# QML UI 子工程：Qt Quick 界面的 Time Machine+
# 由根 CMakeLists.txt 通过 add_subdirectory(qml-ui) 引入（Qt5 存在时）
cmake_minimum_required(VERSION 3.16)

find_package(Qt5 5.15 REQUIRED COMPONENTS Quick Qml Gui QuickControls2 Concurrent)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# 复用主项目后端源码（同进程直接调用 ServiceRun，不经 stdio/HTTP）
set(TMP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# WIN32 子系统 / Windows 专属资源文件仅在 Windows 下生效
set(QML_EXTRA_SOURCES
    src/app.manifest
    src/app.rc)
set(QML_WIN32_ARG "")
if(NOT WIN32)
    set(QML_EXTRA_SOURCES "")
else()
    set(QML_WIN32_ARG "WIN32")
endif()

add_executable(timemachineplus_qml ${QML_WIN32_ARG}
    src/main.cpp
    src/BackendBridge.h
    src/BackendBridge.cpp
    ${QML_EXTRA_SOURCES}
    ${TMP_ROOT}/src/service_run.cpp
    ${TMP_ROOT}/src/sqlite_helper.cpp
    ${TMP_ROOT}/src/util.cpp
    qml/qml.qrc)

target_include_directories(timemachineplus_qml PRIVATE
    ${TMP_ROOT}/include
    ${TMP_ROOT}/thirdparty/nlohmann)

# OpenSSL 头文件：Windows 用 thirdparty/OpenSSL，Linux/macOS 用系统路径
if(WIN32)
    target_include_directories(timemachineplus_qml PRIVATE
        ${TMP_ROOT}/thirdparty/OpenSSL)
endif()

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
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(OPENSSL REQUIRED openssl)
        target_include_directories(timemachineplus_qml PRIVATE ${OPENSSL_INCLUDE_DIRS})
        target_link_directories(timemachineplus_qml PRIVATE ${OPENSSL_LIBRARY_DIRS})
        target_link_libraries(timemachineplus_qml PRIVATE ${OPENSSL_LIBRARIES})
    else()
        find_package(OpenSSL REQUIRED)
        target_include_directories(timemachineplus_qml PRIVATE ${OPENSSL_INCLUDE_DIR})
        target_link_libraries(timemachineplus_qml PRIVATE OpenSSL::Crypto)
    endif()
    # Qt Quick 在 Linux 上需要 X11/dl/pthread
    target_link_libraries(timemachineplus_qml PRIVATE dl pthread)
endif()

# ---- 安装规则 ----
# schema 必须随包分发：main.cpp 以"exe 同目录存在 timemachine.sql"判定安装态
install(TARGETS timemachineplus_qml RUNTIME DESTINATION . COMPONENT qmlui)
install(FILES "${TMP_ROOT}/timemachine.sql" DESTINATION . COMPONENT qmlui)

# ---- 自动部署 ----
option(TIMEMACHINEPLUS_QML_AUTODEPLOY "构建后自动执行部署工具" OFF)
if(TIMEMACHINEPLUS_QML_AUTODEPLOY)
    get_target_property(_qmake_loc Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_loc}" DIRECTORY)
    if(WIN32)
        find_program(DEPLOY_EXE windeployqt HINTS "${_qt_bin_dir}")
        if(DEPLOY_EXE)
            add_custom_command(TARGET timemachineplus_qml POST_BUILD
                COMMAND "${DEPLOY_EXE}" --compiler-runtime
                        --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                        "$<TARGET_FILE:timemachineplus_qml>"
                COMMENT "windeployqt: 部署 Qt 运行依赖")
        else()
            message(WARNING "未找到 windeployqt，跳过自动部署")
        endif()
    elseif(UNIX AND NOT APPLE)
        find_program(DEPLOY_EXE linuxdeployqt HINTS "${_qt_bin_dir}/../libexec" "/usr/bin")
        if(DEPLOY_EXE)
            add_custom_command(TARGET timemachineplus_qml POST_BUILD
                COMMAND "${DEPLOY_EXE}"
                        "$<TARGET_FILE:timemachineplus_qml>"
                        "-appdir=$<TARGET_FILE_DIR:timemachineplus_qml>"
                COMMENT "linuxdeployqt: 部署 Qt 运行依赖")
        else()
            message(STATUS "未找到 linuxdeployqt（可选），跳过自动部署")
        endif()
    endif()
endif()

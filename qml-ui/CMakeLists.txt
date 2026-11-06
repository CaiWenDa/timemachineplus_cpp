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
    src/app.rc
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

# ---- 安装规则（供 packaging/package.ps1 或 CPack 使用）----
# schema 必须随包分发：main.cpp 以"exe 同目录存在 timemachine.sql"判定安装态
install(TARGETS timemachineplus_qml RUNTIME DESTINATION . COMPONENT qmlui)
install(FILES "${TMP_ROOT}/timemachine.sql" DESTINATION . COMPONENT qmlui)

# 可选：构建后自动部署 Qt 运行时依赖（打包时开启，日常 Debug 构建保持关闭）
option(TIMEMACHINEPLUS_QML_AUTODEPLOY "构建后自动执行 windeployqt 部署依赖" OFF)
if(TIMEMACHINEPLUS_QML_AUTODEPLOY AND WIN32)
    get_target_property(_qmake_loc Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_loc}" DIRECTORY)
    find_program(WINDEPLOYQT_EXE windeployqt HINTS "${_qt_bin_dir}")
    if(WINDEPLOYQT_EXE)
        add_custom_command(TARGET timemachineplus_qml POST_BUILD
            COMMAND "${WINDEPLOYQT_EXE}" --compiler-runtime
                    --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                    "$<TARGET_FILE:timemachineplus_qml>"
            COMMENT "windeployqt: 部署 Qt 运行依赖到输出目录")
    else()
        message(WARNING "未找到 windeployqt，跳过自动部署")
    endif()
endif()

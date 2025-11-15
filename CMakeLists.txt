cmake_minimum_required(VERSION 3.10)
project(timemachineplus_cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_compile_definitions(_UNICODE)

include_directories(${PROJECT_SOURCE_DIR}/include)

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
    src/util.cpp)

# Find OpenSSL for MD5 hashing
find_package(OpenSSL REQUIRED)
if(OpenSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
    target_link_libraries(timemachineplus PRIVATE ${OPENSSL_LIBRARIES} SQLiteCpp)
endif()

# Link SQLiteCpp_example1 with SQLiteCpp
# target_link_libraries(timemachineplus SQLiteCpp)
if(LINUX)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set_target_properties(timemachineplus PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()
install(TARGETS timemachineplus DESTINATION bin)

cmake_minimum_required(VERSION 3.20)
project(MyProject VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(FETCHCONTENT_QUIET FALSE)

# ----------------------------------------------------------------

set(SLANG_VERSION "2026.19" CACHE STRING "Slang compiler release version")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SLANG_OS "linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(SLANG_OS "windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(SLANG_OS "macos")
else()
    message(FATAL_ERROR "Unsupported OS: ${CMAKE_SYSTEM_NAME}")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(SLANG_ARCH "x86_64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(SLANG_ARCH "aarch64")
else()
    message(FATAL_ERROR "Unsupported Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Make url
set(SLANG_URL "https://github.com/shader-slang/slang/releases\
/download/v${SLANG_VERSION}\
/slang-${SLANG_VERSION}-${SLANG_OS}-${SLANG_ARCH}.tar.gz")

message(STATUS "[Slang] Target URL: ${SLANG_URL}")

# ----------------------------------------------------------------

include(FetchContent)

FetchContent_Declare(
    slang_release
    URL "${SLANG_URL}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE 

    DOWNLOAD_NO_PROGRESS FALSE
    USES_TERMINAL_DOWNLOAD TRUE
)

FetchContent_MakeAvailable(slang_release)

find_package(slang REQUIRED CONFIG 
    PATHS 
        "${slang_release_SOURCE_DIR}"
        "${slang_release_SOURCE_DIR}/lib/cmake/slang"
    NO_DEFAULT_PATH
)

# ----------------------------------------------------------------

# Create library
add_library(shadertool STATIC src/shadertool.cpp)

# Include public headers
target_include_directories(shadertool PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Fetch slang libraries
target_link_libraries(shadertool PUBLIC slang::slang)

# Test executable
add_executable(test_app test.cpp)
target_link_libraries(test_app PRIVATE shadertool)

# Set RPATH
set_target_properties(test_app PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "${slang_release_SOURCE_DIR}/lib"
)

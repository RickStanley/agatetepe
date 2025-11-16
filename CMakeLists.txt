cmake_minimum_required(VERSION 4.0)
project(agatetepe LANGUAGES CXX)

find_package(CURL REQUIRED)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(agatetepe http_5.cc)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else ()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif ()

if(WIN32)
  target_sources(agatetepe PRIVATE MmapReader.win32.cc)
else()
  target_sources(agatetepe PRIVATE MmapReader.unix.cc)
endif()

target_link_libraries(agatetepe PRIVATE CURL::libcurl)
target_sources(agatetepe PRIVATE MmapReader.hpp)

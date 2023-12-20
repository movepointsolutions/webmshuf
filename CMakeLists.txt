cmake_minimum_required (VERSION 2.8.12)
project (webmshuf)

set (SOURCES
	main.c++
)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MODULE_PATH CMake-Modules)
find_package(PkgConfig REQUIRED)
pkg_search_module(GSTREAMERMM REQUIRED gstreamermm-1.0)

#include (CTest)
#enable_testing()
include_directories(webmshuf ${GSTREAMERMM_INCLUDE_DIRS})
add_executable (webmshuf ${SOURCES})
target_link_libraries (webmshuf PRIVATE ${GSTREAMERMM_LIBRARIES})

cmake_minimum_required(VERSION 3.8)
project(httpserver_processpool)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++14")
set(CMAKE_CXX_COMPLIER "clang++")


# Add the source in project root directory
aux_source_directory(. DIRSRCS)
# Target
add_executable(httpserver_processpool ${DIRSRCS})
target_link_libraries(httpserver_processpool pthread)
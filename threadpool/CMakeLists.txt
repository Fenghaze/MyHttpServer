cmake_minimum_required(VERSION 3.8)
project(httpserver_threadpool)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++14")
set(CMAKE_CXX_COMPLIER "clang++")


# Add the source in project root directory
aux_source_directory(. DIRSRCS)

include_directories(../log)
# Add block directories
add_subdirectory(../log log)

# Target
add_executable(httpserver_threadpool ${DIRSRCS})
target_link_libraries(httpserver_threadpool log pthread)
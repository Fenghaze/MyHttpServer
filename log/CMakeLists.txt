cmake_minimum_required(VERSION 3.10)

project (log)

# SRC_LIST源文件目录为 src
aux_source_directory (src SRC_LIST)

# 依赖的头文件目录为 include
include_directories (include)

# 生成目标文件 main
add_executable (log_test ${SRC_LIST})

# -Wall -g 编译选项
add_definitions("-Wall -g")
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -g -std=c++11 -Wall")

# 目标文件输出目录为 bin
set (EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
# 指定 CMake 最低版本（3.10 足够覆盖大部分常用功能）
cmake_minimum_required(VERSION 3.10)

# 项目名称（可自定义）
project(MiniShell)

# 设置 C++ 标准为 C++17（可根据需要改成 11/14/20）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加可执行文件（将 main.cpp 替换成你的源文件名）
add_executable(MiniShell)
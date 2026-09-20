cmake_minimum_required(VERSION 3.10)

project(MiniShell)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(MiniShell
        buildin.cpp
        buildin.h
        database.h
        Shell.h
        Shell.cpp
        main.cpp
)

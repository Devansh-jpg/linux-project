cmake_minimum_required(VERSION 3.16)
project(ForkCage VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ForkCage
    src/main.cpp
    src/process_launcher.cpp
    src/fs_isolator.cpp
)

target_include_directories(ForkCage PRIVATE include)
target_link_libraries(ForkCage PRIVATE pthread)

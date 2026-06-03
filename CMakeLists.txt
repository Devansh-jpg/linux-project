cmake_minimum_required(VERSION 3.16)
project(sandbox VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(sandbox
    src/main.cpp
    src/process_launcher.cpp
)

target_include_directories(sandbox PRIVATE include)
target_link_libraries(sandbox PRIVATE pthread)

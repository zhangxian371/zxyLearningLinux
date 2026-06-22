cmake_minimum_required(VERSION 3.10)
project(day03-fd-copy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(copy    src/copy.cpp)
add_executable(dup_demo src/dup_demo.cpp)

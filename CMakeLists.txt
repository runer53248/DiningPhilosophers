cmake_minimum_required(VERSION 3.0)
project(philosophers)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED true)

add_compile_options(
    -Wall
    -Wextra
    -Wconversion
    -pedantic
    -Werror
)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} -lpthread)

cmake_minimum_required(VERSION 3.16.3)

project(kilo-cpp
  VERSION 0.1
  DESCRIPTION "Text editor built with C++"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(DEFAULT_BUILD_TYPE "Debug")

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FAITAL_ERROR "In-source builds not allowed. Please make a new directory (called a buld directory) and run CMake from there. You may need to remove CMakeCache.txt.")
endif()

add_executable(${PROJECT_NAME} src/main.cpp)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)

---
layout: page
---

## Chapter 1: Project Setup

Let's start by setting up our project structure and build system.

### Project Structure

Kilo++ uses a standard C++ project layout:

```bash
kilo++/
  ├── include/
  │   └── kilo++/
  │       ├── Editor.hpp
  │       └── EditorUtils.hpp
  ├── src/
  │   ├── Editor.cpp
  │   ├── EditorUtils.cpp
  │   └── kilo.cpp
  ├── CMakeLists.txt
  └── README.md
```

### Setting up CMake

First, create the root `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16.3)

project(kilo++
  VERSION 0.1
  DESCRIPTION "Text editor built with C++"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(src)

add_executable(${PROJECT_NAME} src/kilo.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE libkilo++)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
```

Go the next chapter, [Chapter 2: The Editor Class](02_editor_class).

[<- top page](/)

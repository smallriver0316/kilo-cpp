---
layout: page
---

# Chapter 1: Setup

Welcome to the first step in building kilo++, our modern C++ text editor.
The beginning of a new project, a clean slate upon which we'll build our text editor.
In the world of programming, however, this first step can sometimes be a bit tricky.
We need to ensure our environment is properly set up for C++ development.

Fortunately, our project doesn't depend on any external libraries beyond the C++ standard library.
You'll need a C++ compiler and CMake.
To check if you have a C++ compiler installed, try running `g++ --version` at the command line.
For CMake, run `cmake --version`.

## How to install a C++ compiler and CMake

### ...in Windows

For Windows users, we recommend using either:

1. **Windows Subsystem for Linux (WSL)**: Only available on Windows 10 and later. Follow the [installation guide](https://docs.microsoft.com/en-us/windows/wsl/install-win10). Once installed, run `sudo apt-get install g++ cmake` in the WSL terminal.

2. **MinGW-w64**: Download and install from [here](https://sourceforge.net/projects/mingw-w64/). Make sure to add the bin directory to your PATH.

For CMake on Windows, download and install from the [official website](https://cmake.org/download/).

### ...in macOS

On macOS, you can install the necessary tools using Homebrew:

```bash
brew install gcc cmake
```

If you don't have Homebrew, you can install it from [brew.sh](https://brew.sh/).

### ...in Linux

On Ubuntu or Debian-based distributions:

```bash
sudo apt-get install g++ cmake
```

For other distributions, use the appropriate package manager to install g++ and cmake.

## Project Structure

Let's set up our project structure. Create a new directory for your project and navigate into it:

```bash
mkdir kilo++
cd kilo++
```

Now, create the following directory structure:

```bash
kilo++/
  ├── include/
  │ └── kilo++/
  ├── src/
  └── CMakeLists.txt
```

## The Main Function

Create a new file named `kilo.cpp` in the `src` directory and give it a `main()` function:

```cpp
// src/kilo.cpp
#include <iostream>

int main() {
    std::cout << "Hello, Kilo++!" << std::endl;
    return 0;
}
```

## CMakeLists.txt

Create a CMakeLists.txt file in the root of your project with the following content:

```cmake
cmake_minimum_required(VERSION 3.10)
project(kilo++)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(kilo++ src/kilo.cpp)

target_include_directories(kilo++ PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
```

This CMakeLists.txt file sets up our project to use C++17 and includes our include directory.

## Building the Project

To build the project, create a build directory, navigate into it, and run CMake:

```bash
mkdir build
cd build
cmake ..
make
```

If everything is set up correctly, you should now have an executable named kilo++ in your build directory.
Run it with:

```bash
./kilo++
```

You should see "Hello, Kilo++!" printed to the console.

In the [next chapter](02_basic_structure) , we'll start building the basic structure of our text editor.

[<- top page](/)

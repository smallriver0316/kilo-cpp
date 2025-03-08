---
layout: page
---

# Chapter 2: Basic Structure

Now that we have our development environment set up, let's establish the foundation of our text editor. In this chapter, we'll create the basic structure of our program using modern C++ practices.

## The Editor Class

We'll start by defining a simple `Editor` class that will be the core of our text editor. This class will manage the screen display and handle user input.

Create a new file `include/kilo++/Editor.hpp`:

```cpp
// include/kilo++/Editor.hpp
#pragma once

#include <string>
#include <vector>

class Editor {
public:
    Editor();
    void run();
    void processKeypress();
    void refreshScreen();

private:
    // Member variables
    std::vector<std::string> m_rows;
    int m_screenrows = 0;
    int m_screencols = 0;
};
```

Let's examine the components of this class:

1. **Public Methods**:
   - `Editor()`: Constructor that initializes our editor
   - `run()`: Main loop that refreshes the screen and processes keypresses
   - `processKeypress()`: Handles user input
   - `refreshScreen()`: Updates the display

2. **Member Variables**:
   - `m_rows`: Stores the text content
   - `m_screenrows` and `m_screencols`: Hold the terminal dimensions

## Implementing the Editor

Now, let's implement the Editor class in `src/Editor.cpp`. We'll go through each function one by one.

Create a new file `src/Editor.cpp`:

```cpp
// src/Editor.cpp
#include "kilo++/Editor.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/ioctl.h>
```

### Constructor

First, let's implement the constructor:

```cpp
Editor::Editor() {
    // Get terminal size
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1) {
        m_screenrows = ws.ws_row;
        m_screencols = ws.ws_col;
    } else {
        // Default size if we can't get the actual size
        m_screenrows = 24;
        m_screencols = 80;
    }
}
```

The constructor's purpose is to initialize the editor by determining the terminal size:

1. It creates a `winsize` structure that will hold the terminal dimensions
2. It calls `ioctl()` with the `TIOCGWINSZ` command to query the terminal size
3. If successful, it stores the actual dimensions in our member variables
4. If the call fails, it falls back to a standard 24x80 terminal size

### Main Loop

Next, let's implement the main loop:

```cpp
void Editor::run() {
    while (true) {
        refreshScreen();
        processKeypress();
    }
}
```

The `run()` method creates the main control flow of our editor:

1. It establishes an infinite loop
2. In each iteration, it first refreshes the screen display
3. Then it processes a single keypress from the user
4. This continues until the program is explicitly terminated

### Processing Input

Now, let's implement the method to handle user input:

```cpp
void Editor::processKeypress() {
    // Read a character from standard input
    char c;
    std::cin.get(c);

    // Quit if 'q' is pressed
    if (c == 'q') {
        // Clear the screen before exiting
        std::cout << "\x1b[2J";
        std::cout << "\x1b[H";
        exit(0);
    }
}
```

The `processKeypress()` method handles keyboard input:

1. It reads a single character from standard input using `std::cin.get()`
2. It checks if the character is 'q' (our quit command)
3. If it is 'q', it:
   - Clears the screen with the escape sequence `\x1b[2J`
   - Moves the cursor to the top-left corner with `\x1b[H`
   - Exits the program with `exit(0)`
4. If it's any other character, it does nothing (for now)

### Refreshing the Screen

Finally, let's implement the screen refresh method:

```cpp
void Editor::refreshScreen() {
    // Clear the screen
    std::cout << "\x1b[2J";  // Clear the entire screen
    std::cout << "\x1b[H";   // Move cursor to top-left corner

    // Draw a tilde at the beginning of each line (like vim)
    for (int y = 0; y < m_screenrows; y++) {
        std::cout << "~\r\n";
    }

    // Move cursor back to top-left
    std::cout << "\x1b[H";

    // Make sure output is displayed
    std::cout.flush();
}
```

The `refreshScreen()` method updates the terminal display:

1. It clears the entire screen using the escape sequence `\x1b[2J`
2. It moves the cursor to the top-left corner with `\x1b[H`
3. It draws a tilde (`~`) at the beginning of each line to indicate empty lines:
   - Loops through each row of the screen
   - Outputs a tilde followed by a carriage return and newline (`\r\n`)
4. It moves the cursor back to the top-left corner
5. It flushes the output stream to ensure everything is displayed immediately

## Putting It All Together

Here's the complete implementation of `src/Editor.cpp`:

```cpp
// src/Editor.cpp
#include "kilo++/Editor.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/ioctl.h>

Editor::Editor() {
    // Get terminal size
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1) {
        m_screenrows = ws.ws_row;
        m_screencols = ws.ws_col;
    } else {
        // Default size if we can't get the actual size
        m_screenrows = 24;
        m_screencols = 80;
    }
}

void Editor::run() {
    while (true) {
        refreshScreen();
        processKeypress();
    }
}

void Editor::processKeypress() {
    // Read a character from standard input
    char c;
    std::cin.get(c);

    // Quit if 'q' is pressed
    if (c == 'q') {
        // Clear the screen before exiting
        std::cout << "\x1b[2J";
        std::cout << "\x1b[H";
        exit(0);
    }
}

void Editor::refreshScreen() {
    // Clear the screen
    std::cout << "\x1b[2J";  // Clear the entire screen
    std::cout << "\x1b[H";   // Move cursor to top-left corner

    // Draw a tilde at the beginning of each line (like vim)
    for (int y = 0; y < m_screenrows; y++) {
        std::cout << "~\r\n";
    }

    // Move cursor back to top-left
    std::cout << "\x1b[H";

    // Make sure output is displayed
    std::cout.flush();
}
```

## Updating Main Function

Now let's update our main function to use the Editor class. Edit `src/kilo.cpp`:

```cpp
// src/kilo.cpp
#include "kilo++/Editor.hpp"

int main() {
    Editor editor;
    editor.run();
    return 0;
}
```

This main function:

1. Creates an instance of our Editor class
2. Calls the `run()` method to start the editor
3. Returns 0 when the editor exits (though this won't happen with our current implementation)

## Updating CMakeLists.txt

We need to update our CMakeLists.txt to include our new Editor.cpp file:

```cmake
cmake_minimum_required(VERSION 3.10)
project(kilo++)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(kilo++
    src/kilo.cpp
    src/Editor.cpp
)

target_include_directories(kilo++ PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
```

This CMakeLists.txt:

1. Sets the minimum CMake version to 3.10
2. Names our project "kilo++"
3. Sets the C++ standard to C++17
4. Creates an executable from our source files
5. Adds our include directory to the include path

## Understanding Terminal Control Sequences

The escape sequences we're using are:

- `\x1b[2J`: Clears the entire screen
- `\x1b[H`: Positions the cursor at the top-left corner

These are part of the ANSI escape sequence standard used by most terminal emulators.

## Building and Running

Let's build our project:

```bash
cd build
cmake ..
make
```

Now run the program:

```bash
./kilo++
```

When you run the program, you should see:

1. A screen filled with tildes (`~`) at the beginning of each line
2. Press 'q' to quit the editor

## What We've Accomplished

In this chapter, we've:

1. Created the basic `Editor` class structure
2. Implemented methods to:
   - Initialize the editor and get terminal dimensions
   - Process basic keyboard input
   - Refresh the screen display
3. Set up the main loop structure
4. Learned about basic terminal control sequences

In the [next chapter](03_raw_input_output), we'll enhance our editor by:

- Implementing raw mode for better input handling
- Adding a welcome message
- Implementing more sophisticated screen drawing functions

But for now, we have a simple foundation to build upon.

[<- previous chapter](01_setup)

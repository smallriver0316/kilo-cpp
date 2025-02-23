---
# new `_layouts/default.html` for backwards-compatibility when multiple
# layouts have been customized.

layout: home
---

# Build Your Own Text Editor in Modern C++

## Introduction

Kilo++ is a text editor written in roughly 1000 lines of C++ code with no external dependencies.
It implements all the basic features you'd expect from a minimal editor:

- Basic text editing functionality
- Cursor movement
- File I/O
- Search functionality
- Syntax highlighting
- Status bar and message display

The key differences from the original kilo implementation include:

- Use of C++17 features
- RAII-based resource management
- Type-safe enums
- Standard library containers and algorithms
- Object-oriented design with classes
- Modern memory management with smart pointers
- More robust string handling
- CMake-based build system

## Prerequisites

To follow this tutorial, you'll need:

- A C++17 compliant compiler (g++ 9.4.0 or higher recommended)
- CMake 3.16.3 or higher
- Basic understanding of C++ and terminal-based applications
- A Unix-like environment (Linux/macOS)

## Table of Contents

1. [Project Setup](01_project_setup)
2. [The Editor Class](02_editor_class)
3. [Terminal Handling](03_terminal_handling)
4. [Text Editing Operation](04_text_editing_operation)
5. [File I/O](05_file_io)
6. [Syntax Highlighting](06_syntax_highlighting)
7. [Search Functionality](07_search_functionality)
8. [Conclusion](08_conclusion)

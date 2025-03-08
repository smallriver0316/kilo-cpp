---
layout: page
---

# Chapter 3: Raw Input and Output

Now that we have our basic editor structure in place, let's improve how we handle terminal input and output. In this chapter, we'll implement raw mode input handling, support for special keys, and efficient screen rendering.

## Understanding Terminal Modes

Let's start by understanding the difference between terminal modes:

- **Canonical mode** (default): Input is processed line-by-line, only sent after pressing Enter
- **Raw mode**: Input is processed character-by-character immediately, giving us full control

For a text editor, raw mode is essential because we need to:

- Process each keystroke immediately
- Handle special keys (arrows, Home, End, etc.)
- Control exactly what gets displayed on the screen

## Setting Up Key Definitions

First, let's define constants for special keys. Create or modify the file `include/kilo++/EditorUtils.hpp`:

```cpp
// include/kilo++/EditorUtils.hpp
#pragma once

enum class EditorKey
{
  BACKSPACE = 127,
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

namespace terminal_manager
{
  void die(const char *s);
  void disableRawMode();
  void enableRawMode();
  int getWindowSize(int *rows, int *cols);
  int readKey();
}
```

We use an enum class (a C++11 feature) to define our special keys:

- `BACKSPACE` is assigned its ASCII value (127)
- The arrow keys and other special keys start at 1000 to avoid conflicts with ASCII characters
- Each subsequent key gets automatically assigned the next integer value

## Implementing Terminal Management

Now let's implement these functions in `src/EditorUtils.cpp`:

```cpp
// src/EditorUtils.cpp
#include "kilo++/EditorUtils.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace terminal_manager
{
  // Store the original terminal settings
  termios orig_termios;
```

### Error Handling Function

First, let's implement a utility function to handle errors:

```cpp
  void die(const char *s)
  {
    // Clear screen before displaying error
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
  }
```

This function:

1. Clears the screen using ANSI escape sequences
2. Positions the cursor at the top-left corner
3. Calls `perror()` to display the error message with system context
4. Exits the program with an error code

### Raw Mode Management

Next, let's implement the functions to enable and disable raw mode:

```cpp
  void disableRawMode()
  {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
      die("tcsetattr");
  }

  void enableRawMode()
  {
    // Get current terminal attributes
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
      die("tcgetattr");

    // Register disableRawMode to be called at exit
    atexit(disableRawMode);

    // Modify terminal attributes for raw mode
    struct termios raw = orig_termios;

    // Input flags: disable break signal, CR to NL conversion,
    // stripping of 8th bit, and software flow control
    raw.c_iflag &= ~(BRKINT | ICRNL | ISTRIP | IXON);

    // Output flags: disable post-processing
    raw.c_oflag &= ~(OPOST);

    // Control flags: set 8-bit characters
    raw.c_cflag |= (CS8);

    // Local flags: disable echo, canonical mode, special keys, and signals
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    // Control characters: set read timeout
    raw.c_cc[VMIN] = 0;  // Return immediately, even if no input is available
    raw.c_cc[VTIME] = 1; // Wait up to 1/10 second for input

    // Apply the modified attributes
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
      die("tcsetattr");
  }
```

Let's break down what we're doing in `enableRawMode()`:

1. First, we save the original terminal attributes so we can restore them later
2. We register our `disableRawMode()` function to run when the program exits
3. We create a copy of the original attributes to modify
4. We turn off several terminal features by using the bitwise NOT (`~`) and bitwise AND (`&=`) operators:
   - `BRKINT`: Disable break signals
   - `ICRNL`: Disable carriage return to newline translation
   - `ISTRIP`: Disable stripping of the 8th bit
   - `IXON`: Disable software flow control (Ctrl+S, Ctrl+Q)
   - `OPOST`: Disable output processing
   - `ECHO`: Disable character echo
   - `ICANON`: Enable raw mode (process input byte-by-byte)
   - `IEXTEN`: Disable extended input processing
   - `ISIG`: Disable signal processing (Ctrl+C, Ctrl+Z)
5. We set `CS8` (control flag) to use 8 bits per byte
6. We set `VMIN` to 0 and `VTIME` to 1 for a 100ms timeout on input
7. Finally, we apply these settings using `tcsetattr()`

### Getting Terminal Size

Next, let's implement a function to get the terminal size:

```cpp
  int getWindowSize(int *rows, int *cols)
  {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
      return -1;

    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
```

This function:

1. Creates a `winsize` structure to hold the terminal dimensions
2. Uses `ioctl()` with `TIOCGWINSZ` command to query the terminal size
3. If successful, it sets the values through the pointers provided
4. Returns 0 on success or -1 on failure

### Reading Input with Support for Special Keys

Finally, let's implement the key reading function, which is the most complex part:

```cpp
  int readKey()
  {
    int nread;
    char c;

    // Keep trying until we read a character or get an error
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
      if (nread == -1 && errno != EAGAIN)
        die("read");
    }

    // If we read an escape sequence
    if (c == '\x1b')
    {
      char seq[3];

      // Try to read the next two bytes with a short timeout
      if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
      if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

      // Check if it's an escape sequence we recognize
      if (seq[0] == '[')
      {
        if (seq[1] >= '0' && seq[1] <= '9')
        {
          // Extended escape sequence, read one more byte
          if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';

          if (seq[2] == '~')
          {
            // Map to our special keys
            switch (seq[1])
            {
              case '1': return static_cast<int>(EditorKey::HOME_KEY);
              case '3': return static_cast<int>(EditorKey::DEL_KEY);
              case '4': return static_cast<int>(EditorKey::END_KEY);
              case '5': return static_cast<int>(EditorKey::PAGE_UP);
              case '6': return static_cast<int>(EditorKey::PAGE_DOWN);
              case '7': return static_cast<int>(EditorKey::HOME_KEY);
              case '8': return static_cast<int>(EditorKey::END_KEY);
            }
          }
        }
        else
        {
          // Simple escape sequence for arrow keys and others
          switch (seq[1])
          {
            case 'A': return static_cast<int>(EditorKey::ARROW_UP);
            case 'B': return static_cast<int>(EditorKey::ARROW_DOWN);
            case 'C': return static_cast<int>(EditorKey::ARROW_RIGHT);
            case 'D': return static_cast<int>(EditorKey::ARROW_LEFT);
            case 'H': return static_cast<int>(EditorKey::HOME_KEY);
            case 'F': return static_cast<int>(EditorKey::END_KEY);
          }
        }
      }
      else if (seq[0] == 'O')
      {
        // Another format for Home and End keys
        switch (seq[1])
        {
          case 'H': return static_cast<int>(EditorKey::HOME_KEY);
          case 'F': return static_cast<int>(EditorKey::END_KEY);
        }
      }

      // Unrecognized escape sequence
      return '\x1b';
    }
    else
    {
      // Regular character
      return c;
    }
  }

} // namespace terminal_manager
```

Let's examine this function:

1. It reads a single byte from standard input
2. If the byte is an escape character (`\x1b`), it tries to read the next two bytes with a short timeout
3. It looks for recognized escape sequences that match arrow keys, Home, End, Page Up, Page Down, and Delete
4. If it recognizes a sequence, it returns the corresponding `EditorKey` value
5. If it doesn't recognize the sequence, it returns the escape character
6. For regular characters, it returns the character itself

## Updating the Editor Class

Now we need to update our `Editor` class to use these new functions. Let's modify `include/kilo++/Editor.hpp`:

```cpp
// include/kilo++/Editor.hpp
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <time.h>

class Editor {
public:
    Editor();
    void run(int argc, char *argv[]);
    void processKeypress();
    void refreshScreen();

private:
    // Input methods
    void moveCursor(int key);

    // Output methods
    void drawRows(std::string &s);
    void scroll();
    void setStatusMessage(const char *fmt, ...);

    // Member variables
    std::vector<std::string> m_rows;
    int m_cx = 0, m_cy = 0;   // Cursor position
    int m_rx = 0;             // Render position (for tab rendering)
    int m_rowoff = 0, m_coloff = 0;  // Scroll offsets
    int m_screenrows, m_screencols;  // Terminal dimensions
    std::string m_statusmsg = "";    // Status message
    time_t m_statusmsg_time = 0;     // When the status message was set
};
```

## Implementing Enhanced Editor Functionality

Now let's update the `src/Editor.cpp` file:

```cpp
// src/Editor.cpp
#include "kilo++/Editor.hpp"
#include "kilo++/EditorUtils.hpp"

#include <cstdarg>
#include <cstring>
#include <unistd.h>

#define CTRL_KEY(k) ((k) & 0x1f)
```

First, let's define the `CTRL_KEY` macro, which will help us work with control keys. This macro masks out all but the lower 5 bits of a key, which is what happens when you press Ctrl plus a key.

### Constructor

```cpp
Editor::Editor()
{
    // Initialize terminal in raw mode
    terminal_manager::enableRawMode();

    // Get terminal size
    if (terminal_manager::getWindowSize(&m_screenrows, &m_screencols) == -1)
        terminal_manager::die("getWindowSize");

    // Reserve space for status bar and message line
    m_screenrows -= 2;
}
```

The constructor:

1. Enables raw mode using our terminal manager
2. Gets the terminal dimensions
3. Reserves two rows for the status bar and message line

### Main Run Method

```cpp
void Editor::run(int argc, char *argv[])
{
    // Display welcome message
    setStatusMessage("HELP: Ctrl-Q = quit");

    // Main program loop
    while (true) {
        refreshScreen();
        processKeypress();
    }
}
```

The `run()` method:

1. Sets an initial status message
2. Enters the main loop, refreshing the screen and processing keypresses

### Processing Input

```cpp
void Editor::processKeypress()
{
    // Read a key
    int c = terminal_manager::readKey();

    // Handle the key
    switch (c) {
        case CTRL_KEY('q'):
            // Clear screen and exit
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;

        case static_cast<int>(EditorKey::ARROW_UP):
        case static_cast<int>(EditorKey::ARROW_DOWN):
        case static_cast<int>(EditorKey::ARROW_LEFT):
        case static_cast<int>(EditorKey::ARROW_RIGHT):
            // Move cursor
            moveCursor(c);
            break;

        case static_cast<int>(EditorKey::PAGE_UP):
        case static_cast<int>(EditorKey::PAGE_DOWN):
            {
                // Move cursor by screenful
                int times = m_screenrows;
                while (times--)
                    moveCursor(c == static_cast<int>(EditorKey::PAGE_UP) ?
                             static_cast<int>(EditorKey::ARROW_UP) :
                             static_cast<int>(EditorKey::ARROW_DOWN));
            }
            break;

        case static_cast<int>(EditorKey::HOME_KEY):
            // Move to beginning of line
            m_cx = 0;
            break;

        case static_cast<int>(EditorKey::END_KEY):
            // Move to end of line
            if (m_cy < m_rows.size())
                m_cx = m_rows[m_cy].size();
            break;
    }
}
```

This method:

1. Reads a key using our `readKey()` function
2. Handles different key presses:
   - Ctrl+Q quits the program
   - Arrow keys move the cursor
   - Page Up/Down moves by a screenful
   - Home/End move to the beginning/end of the line

### Moving the Cursor

```cpp
void Editor::moveCursor(int key)
{
    // Handle cursor movement based on key
    switch (key) {
        case static_cast<int>(EditorKey::ARROW_LEFT):
            if (m_cx > 0) {
                m_cx--;
            } else if (m_cy > 0) {
                // Move to end of previous line
                m_cy--;
                m_cx = m_rows.empty() || m_cy >= m_rows.size() ? 0 : m_rows[m_cy].size();
            }
            break;

        case static_cast<int>(EditorKey::ARROW_RIGHT):
            if (!m_rows.empty() && m_cy < m_rows.size()) {
                if (m_cx < m_rows[m_cy].size()) {
                    m_cx++;
                } else if (m_cx == m_rows[m_cy].size() && m_cy < m_rows.size() - 1) {
                    // Move to beginning of next line
                    m_cy++;
                    m_cx = 0;
                }
            }
            break;

        case static_cast<int>(EditorKey::ARROW_UP):
            if (m_cy > 0) m_cy--;
            break;

        case static_cast<int>(EditorKey::ARROW_DOWN):
            if (!m_rows.empty() && m_cy < m_rows.size() - 1) m_cy++;
            break;
    }

    // Ensure cursor position is valid
    if (!m_rows.empty() && m_cy < m_rows.size()) {
        if (m_cx > m_rows[m_cy].size())
            m_cx = m_rows[m_cy].size();
    }
}
```

This method:

1. Handles the four arrow keys
2. For left/right, allows moving between lines
3. For up/down, ensures we don't go past the first or last line
4. Ensures the cursor position is valid for the current line

### Scrolling Logic

```cpp
void Editor::scroll()
{
    // Update render position
    m_rx = m_cx;

    // Vertical scrolling
    if (m_cy < m_rowoff) {
        // Scrolling up
        m_rowoff = m_cy;
    }
    if (m_cy >= m_rowoff + m_screenrows) {
        // Scrolling down
        m_rowoff = m_cy - m_screenrows + 1;
    }

    // Horizontal scrolling
    if (m_rx < m_coloff) {
        // Scrolling left
        m_coloff = m_rx;
    }
    if (m_rx >= m_coloff + m_screencols) {
        // Scrolling right
        m_coloff = m_rx - m_screencols + 1;
    }
}
```

This method:

1. Updates the render position (`m_rx`)
2. Handles vertical scrolling when the cursor moves outside the visible area
3. Handles horizontal scrolling when the cursor moves outside the visible area

### Drawing Rows

```cpp
void Editor::drawRows(std::string &s)
{
    // Draw each row
    for (int y = 0; y < m_screenrows; y++) {
        // Calculate file row
        int filerow = y + m_rowoff;

        if (filerow >= static_cast<int>(m_rows.size())) {
            // Display welcome message if no file is open
            if (m_rows.empty() && y == m_screenrows / 3) {
                // Create welcome message
                std::string welcome = "Kilo++ editor -- version 0.0.1";
                if (welcome.size() > static_cast<size_t>(m_screencols))
                    welcome.resize(m_screencols);

                // Center the welcome message
                int padding = (m_screencols - welcome.size()) / 2;
                if (padding) {
                    s += "~";
                    padding--;
                }
                while (padding--) s += " ";

                s += welcome;
            } else {
                // Empty line
                s += "~";
            }
        } else if (filerow < static_cast<int>(m_rows.size())) {
            // Display file content
            int len = m_rows[filerow].size() - m_coloff;
            if (len < 0) len = 0;
            if (len > m_screencols) len = m_screencols;

            if (len > 0)
                s += m_rows[filerow].substr(m_coloff, len);
        }

        // Clear to end of line and add newline
        s += "\x1b[K";
        s += "\r\n";
    }
}
```

This method:

1. Loops through each row of the screen
2. If we're past the end of the file, displays a welcome message or a tilde
3. If we're within the file, displays the file content with proper scrolling
4. Clears to the end of each line and adds a newline

### Status Message

```cpp
void Editor::setStatusMessage(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    char buf[80];
    vsnprintf(buf, sizeof(buf), fmt, ap);

    va_end(ap);

    m_statusmsg = buf;
    m_statusmsg_time = time(NULL);
}
```

This method:

1. Takes a format string and variable arguments (like `printf`)
2. Formats the message into a buffer
3. Stores the message and the current time

### Refreshing the Screen

```cpp
void Editor::refreshScreen()
{
    // Handle scrolling
    scroll();

    // Prepare output string
    std::string buffer;

    // Hide cursor during redraw
    buffer += "\x1b[?25l";

    // Position cursor at top-left
    buffer += "\x1b[H";

    // Draw rows
    drawRows(buffer);

    // Display status bar
    buffer += "\x1b[7m";  // Inverted colors

    std::string status = "New File";
    if (status.size() > static_cast<size_t>(m_screencols))
        status.resize(m_screencols);

    std::string rstatus = std::to_string(m_cy + 1) + "," + std::to_string(m_cx + 1);

    buffer += status;
    buffer.append(m_screencols - status.size() - rstatus.size(), ' ');
    buffer += rstatus;

    buffer += "\x1b[m";  // Normal colors
    buffer += "\r\n";

    // Display status message
    buffer += "\x1b[K";
    if (!m_statusmsg.empty() && time(NULL) - m_statusmsg_time < 5) {
        int msglen = m_statusmsg.size();
        if (msglen > m_screencols) msglen = m_screencols;
        buffer += m_statusmsg.substr(0, msglen);
    }

    // Position cursor
    buffer += "\x1b[" + std::to_string((m_cy - m_rowoff) + 1) + ";"
           + std::to_string((m_rx - m_coloff) + 1) + "H";

    // Show cursor
    buffer += "\x1b[?25h";

    // Write to terminal
    write(STDOUT_FILENO, buffer.c_str(), buffer.size());
}
```

This method:

1. Handles scrolling
2. Builds a string containing all output
3. Hides the cursor during redraw
4. Draws rows, status bar, and message line
5. Positions the cursor at the current position
6. Shows the cursor
7. Writes everything to the terminal in one go

## Updating the Main Function

Finally, let's update the main function in `src/kilo.cpp`:

```cpp
// src/kilo.cpp
#include "kilo++/Editor.hpp"

int main(int argc, char **argv)
{
    Editor editor;
    editor.run(argc, argv);
    return 0;
}
```

## Building and Running

Now let's build and run our improved editor:

```bash
cd build
cmake ..
make
./kilo++
```

When you run the program, you should now be able to:

1. See a welcome message
2. Move the cursor with arrow keys
3. Jump to the beginning/end of lines with Home/End
4. Scroll pages with Page Up/Down
5. Quit with Ctrl+Q

## Understanding ANSI Escape Sequences

Throughout this chapter, we've used several ANSI escape sequences:

- `\x1b[2J`: Clears the entire screen
- `\x1b[H`: Positions the cursor at the top-left corner
- `\x1b[K`: Clears from cursor to the end of line
- `\x1b[7m`: Enables inverted colors
- `\x1b[m`: Resets text formatting
- `\x1b[?25l`: Hides the cursor
- `\x1b[?25h`: Shows the cursor
- `\x1b[<row>;<col>H`: Positions the cursor at the specified row and column

## What We've Accomplished

In this chapter, we've:

1. Implemented raw mode for immediate character-by-character input
2. Added support for special keys like arrows, Home, End, Page Up/Down
3. Created a scrolling mechanism for navigating through content
4. Added a status bar and message line
5. Improved the screen rendering with a buffer to prevent flicker

In the [next chapter](04_text_viewer), we'll add file I/O capabilities to open and display text files.

## Further Enhancements

If you want to experiment further:

- Try adding support for more key combinations
- Implement a cursor that stays visible on screen (highlight the current character)
- Add line numbers to the display
- Experiment with different color schemes for the status bar

Remember, each of these improvements builds on the solid foundation we've established for handling raw terminal input and output.

[<- previous chapter](02_basic_structure)

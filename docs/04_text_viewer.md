---
layout: page
---

# Chapter 4: A Text Viewer

In this chapter, we will evolve our kilo++ into a simple text viewer. We'll implement functionality to read and display text files.

## Creating a Data Structure for Text Lines

First, let's define a data structure to store text data in memory. We'll leverage C++'s powerful features to design an efficient data structure.

```cpp
// Define EditorRow structure

// Add to editorConfig class in include/kilo++/Editor.hpp
struct EditorRow {
  EditorRow(int index) : idx(index), row({}), rendered({}) {};

  int idx;
  std::string row;       // Actual content from the file (raw text)
  std::string rendered;  // Processed text for display (expanded tabs, etc.)
};

// Add to Editor class private members
private:
  /*** members ***/
  std::vector<EditorRow> m_rows;
  // Other member variables...
```

The `EditorRow` structure represents a single line in the file. `row` contains the actual text read from the file, while `rendered` contains display-ready text with expanded tabs. We use `std::vector<EditorRow>` to manage all lines in the file.

We also initialize this data structure when initializing the editor:

```cpp
// Add to Editor::Editor() constructor in src/Editor.cpp
Editor::Editor()
{
  terminal_manager::enableRawMode();

  if (terminal_manager::getWindowSize(&m_screenrows, &m_screencols) == -1)
    terminal_manager::die("getWindowSize");

  m_screenrows -= 2;  // Reserve 2 rows for status bar and message bar
  m_cx = 0;
  m_cy = 0;
  m_rx = 0;
  m_rowoff = 0;
  m_coloff = 0;
  m_dirty = 0;
  // m_rows is initialized by default constructor
}
```

## Reading Files

Next, let's add functionality to open and read file contents. We'll start with adding a single line:

```cpp
// Add to src/Editor.cpp

void Editor::appendRow(const std::string& s) {
  m_rows.push_back(EditorRow(m_rows.size()));
  auto& row = m_rows.back();
  row.row = s;

  // Generate rendered version of the row (convert tabs to spaces, etc.)
  updateRow(row);
}

void Editor::updateRow(EditorRow& row) {
  // Render the row for display (expand tabs to spaces, etc.)
  std::string render;
  for (const auto& c : row.row) {
    if (c == '\t') {
      render += " ";
      while (render.size() % KILO_TAB_STOP != 0)
        render += " ";
    } else {
      render += c;
    }
  }
  row.rendered = render;
}

void Editor::open(const char* filename) {
  m_filename = std::string(filename);

  std::ifstream file(filename);
  if (!file.is_open())
    terminal_manager::die("fopen");

  std::string line;
  while (std::getline(file, line)) {
    // Strip trailing \r\n or \n
    while (line.size() && (line.back() == '\n' || line.back() == '\r'))
      line.pop_back();

    appendRow(line);
  }

  file.close();
  m_dirty = 0;  // Reset to unmodified state
}
```

## Vertical Scrolling

To display an entire file, we need to scroll through portions that don't fit on the screen. Let's first implement vertical scrolling:

```cpp
// Add to Editor class private members in include/kilo++/Editor.hpp
private:
  int m_rowoff = 0;  // Vertical scroll offset

// Add to src/Editor.cpp
void Editor::scroll() {
  m_rx = m_cx;
  if (m_cy < static_cast<int>(m_rows.size()))
    convertRowCxToRx(m_rows[m_cy]);

  // Vertical scrolling
  if (m_cy < m_rowoff) {
    m_rowoff = m_cy;
  }
  if (m_cy >= m_rowoff + m_screenrows) {
    m_rowoff = m_cy - m_screenrows + 1;
  }

  // Horizontal scrolling (will implement later)
}
```

`m_rowoff` is the index of the row displayed at the top of the screen. The `scroll()` function adjusts scrolling when the cursor moves outside the visible screen.

Next, let's update the `editorDrawRows()` function to display the appropriate rows:

```cpp
void Editor::drawRows(std::string& s) {
  for (int y = 0; y < m_screenrows; y++) {
    int filerow = y + m_rowoff;  // Index of the row to display
    if (filerow >= static_cast<int>(m_rows.size())) {
      // Beyond end of file, display welcome message or ~
      if (m_rows.empty() && y == m_screenrows / 3) {
        std::string welcome = "Kilo++ editor -- version " + std::string(KILO_VERSION);
        if (welcome.size() > static_cast<std::size_t>(m_screencols))
          welcome.resize(m_screencols);

        auto padding = (static_cast<std::size_t>(m_screencols) - welcome.size()) / 2;
        if (padding) {
          s += "~";
          padding--;
        }

        while (padding--)
          s += " ";

        s += welcome;
      } else {
        s += "~";
      }
    } else {
      // Display row content (truncate if too long)
      int len = static_cast<int>(m_rows[filerow].rendered.size()) - m_coloff;
      if (len < 0) len = 0;
      if (len > m_screencols) len = m_screencols;
      s += m_rows[filerow].rendered.substr(m_coloff, len);
    }

    s += "\x1b[K";  // Clear to end of line
    if (y < m_screenrows - 1) {
      s += "\r\n";
    }
  }
}
```

Then, we enable scrolling by calling `scroll()` at the beginning of `refreshScreen()`:

```cpp
void Editor::refreshScreen() {
  scroll();

  // Rest of the code...
}
```

## Horizontal Scrolling

Let's also add functionality to scroll horizontally for long lines:

```cpp
// Add to Editor class private members in include/kilo++/Editor.hpp
private:
  int m_coloff = 0;  // Horizontal scroll offset

// Extend the scroll() function in src/Editor.cpp
void Editor::scroll() {
  m_rx = m_cx;
  if (m_cy < static_cast<int>(m_rows.size()))
    convertRowCxToRx(m_rows[m_cy]);

  // Vertical scrolling
  if (m_cy < m_rowoff) {
    m_rowoff = m_cy;
  }
  if (m_cy >= m_rowoff + m_screenrows) {
    m_rowoff = m_cy - m_screenrows + 1;
  }

  // Horizontal scrolling
  if (m_rx < m_coloff) {
    m_coloff = m_rx;
  }
  if (m_rx >= m_coloff + m_screencols) {
    m_coloff = m_rx - m_screencols + 1;
  }
}
```

## Rendering Tabs

To display tab characters nicely, we'll also implement conversion between character indices and screen positions:

```cpp
// Add to src/Editor.cpp

void Editor::convertRowCxToRx(EditorRow& erow) {
  int rx = 0;
  for (int i = 0; i < m_cx; ++i) {
    if (erow.row[i] == '\t')
      rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
    rx++;
  }
  m_rx = rx;
}

int Editor::convertRowRxToCx(EditorRow& erow, int rx) {
  int cur_rx = 0, cx = 0;
  for (cx = 0; cx < static_cast<int>(erow.row.size()); ++cx) {
    if (erow.row[cx] == '\t')
      cur_rx += (KILO_TAB_STOP - 1) - (cur_rx % KILO_TAB_STOP);

    cur_rx++;

    if (cur_rx > rx)
      return cx;
  }

  return cx;
}
```

In this code, `m_rx` represents the cursor's actual position on screen, while `m_cx` represents its position in the text buffer. These values differ when tab characters are present.

## Implementing a Status Bar

Let's add a status bar to display information like filename and line count:

```cpp
// Add to src/Editor.cpp

void Editor::drawStatusBar(std::string& s) {
  s += "\x1b[7m";  // Inverted colors (swap background and foreground)

  std::stringstream ss, rss;
  ss << (m_filename.empty()
         ? "[No Name]"
         : m_filename.substr(0, std::min(static_cast<int>(m_filename.size()), FILENAME_DISPLAY_LEN)))
     << " - "
     << m_rows.size()
     << " lines"
     << (m_dirty ? "(modified)" : "");
  int len = std::min(static_cast<int>(ss.str().size()), m_screencols);

  rss << "| "
      << m_cy + 1
      << "/"
      << m_rows.size();
  int rlen = rss.str().size();

  s += ss.str().substr(0, len);
  while (len < m_screencols) {
    if (m_screencols - len == rlen) {
      s += rss.str();
      break;
    }
    s += " ";
    len++;
  }

  s += "\x1b[m";  // Reset formatting
  s += "\r\n";
}
```

Then, update `refreshScreen()` to display the status bar:

```cpp
void Editor::refreshScreen() {
  scroll();

  std::string s;

  s += "\x1b[?25l";  // Hide cursor
  s += "\x1b[H";     // Move cursor to home position

  drawRows(s);
  drawStatusBar(s);

  // Set cursor position
  std::stringstream ss;
  ss << "\x1b[" << (m_cy - m_rowoff) + 1 << ";" << (m_rx - m_coloff) + 1 << "H";
  s += ss.str();

  s += "\x1b[?25h";  // Show cursor

  write(STDOUT_FILENO, s.c_str(), s.size());
}
```

## Displaying Status Messages

Finally, let's add functionality to display temporary status messages:

```cpp
// Add to Editor class private members in include/kilo++/Editor.hpp
private:
  std::string m_statusmsg = "";
  time_t m_statusmsg_time = 0;

// Add to src/Editor.cpp
void Editor::drawMessageBar(std::string& s) {
  s += "\x1b[K";  // Clear line
  int msglen = static_cast<int>(m_statusmsg.size());
  if (msglen > m_screencols) msglen = m_screencols;
  if (msglen && time(NULL) - m_statusmsg_time < 5)
    s += m_statusmsg.substr(0, msglen);
}

void Editor::setStatusMessage(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  int size;
  if ((size = std::vsnprintf(nullptr, 0, fmt, ap) + 1) < 0) {
    va_end(ap);
    return;
  }

  if (size <= 1) {
    va_end(ap);
    return;
  }

  std::vector<char> buf(size);
  va_start(ap, fmt);
  vsnprintf(buf.data(), m_screencols, fmt, ap);
  va_end(ap);

  m_statusmsg = std::string(buf.data());
  m_statusmsg_time = time(NULL);
}
```

And update `refreshScreen()` again to display the message bar:

```cpp
void Editor::refreshScreen() {
  scroll();

  std::string s;

  s += "\x1b[?25l";  // Hide cursor
  s += "\x1b[H";     // Move cursor to home position

  drawRows(s);
  drawStatusBar(s);
  drawMessageBar(s);

  // Set cursor position
  std::stringstream ss;
  ss << "\x1b[" << (m_cy - m_rowoff) + 1 << ";" << (m_rx - m_coloff) + 1 << "H";
  s += ss.str();

  s += "\x1b[?25h";  // Show cursor

  write(STDOUT_FILENO, s.c_str(), s.size());
}
```

## Updating the Main Loop

Finally, let's update `run` function to open files from command line arguments:

```cpp
// src/Editor.cpp

void Editor::run(int argc, char *argv[]) {
  if (argc >= 2)
    open(argv[1]);

  setStatusMessage("HELP: Ctrl-Q = quit | Ctrl-S = save");

  while (1) {
    refreshScreen();
    processKeypress();
  }
}
```

## Summary

We've now completed the basic functionality of a text viewer. You can open and display files, move the cursor around, and scroll through long files and long lines. We've also added a status bar and message bar to provide information to the user.

In the [next chapter](05_text_editor), we'll add text editing features to evolve our program into a full-fledged text editor.

Additional exercises:

1. Improve error handling when reading files
2. Display cursor position (line, column) in the status bar
3. Add support for Page Up and Page Down keys
4. Add support for Home and End keys

[<- previous chapter](03_raw_input_output)

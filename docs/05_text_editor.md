---
layout: page
---

# Chapter 5: A Text Editor

In this chapter, we'll transform our text viewer into a full-featured text editor by implementing text editing capabilities. We'll add features to insert and delete characters, handle special keys like Enter and Backspace, and implement file saving.

## Insert Ordinary Characters

Let's begin by writing a function that inserts a single character into an `EditorRow` at a given position.

```cpp
void Editor::insertCharIntoRow(EditorRow& erow, int at, int c) {
  if (at < 0 || at > static_cast<int>(erow.row.size()))
    at = erow.row.size();

  erow.row.insert(at, 1, c);
  updateRow(erow);
  m_dirty++;
}
```

We first validate the `at` index, which is the position where we want to insert the character. We allow `at` to be one past the end of the string, in which case the character will be inserted at the end of the row.

Next, we insert the character into the string at the specified position using C++'s `std::string::insert()`. We then call `updateRow()` to update the `rendered` field with the updated row content and increment the `m_dirty` flag to indicate that the file has been modified.

Now let's create a new section called `/*** editor operations ***/`. This section will contain functions that we'll call from `processKeypress()` when mapping keypresses to various text editing operations. Let's add a function to this section called `insertChar()` which will use `insertCharIntoRow()` to insert a character at the cursor's position:

```cpp
void Editor::insertChar(int c) {
  if (m_cy == static_cast<int>(m_rows.size()))
    insertRow(m_rows.size(), "");

  insertCharIntoRow(m_rows[m_cy], m_cx, c);
  m_cx++;
}
```

If the cursor is on the line after the end of the file, we first add a new empty row before inserting the character. This allows the user to insert text at the end of the file. After inserting the character, we move the cursor forward so that the next character the user types will go after the one just inserted.

Now let's modify the `default:` case in our `processKeypress()` function to call `insertChar()`:

```cpp
void Editor::processKeypress() {
  static int quit_times = KILO_QUIT_TIMES;
  int c = terminal_manager::readKey();

  switch (c) {
  // ... other cases ...

  default:
    insertChar(c);
    break;
  }

  quit_times = KILO_QUIT_TIMES;
}
```

With this change, we've officially upgraded our text viewer to a text editor!

## Prevent Inserting Special Characters

Currently, if you press keys like Backspace or Enter, those characters would be inserted directly into the text, which we certainly don't want. Let's handle special keys in `processKeypress()` so they don't fall through to the `default:` case.

First, let's add some key definitions to our `EditorKey` enum in `EditorUtils.hpp`:

```cpp
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
```

The Backspace key has ASCII value 127. We'll assign it as a constant in the enum for clarity.

Now let's update `processKeypress()` to handle Backspace and similar keys:

```cpp
void Editor::processKeypress() {
  static int quit_times = KILO_QUIT_TIMES;
  int c = terminal_manager::readKey();

  switch (c) {
  case '\r':
    // TODO: Handle Enter key
    break;

  case CTRL_KEY('q'):
    // ... existing code for Ctrl-Q ...
    break;

  case static_cast<int>(EditorKey::BACKSPACE):
  case CTRL_KEY('h'):
  case static_cast<int>(EditorKey::DEL_KEY):
    // TODO: Handle backspace and delete
    break;

  // ... other cases ...

  default:
    insertChar(c);
    break;
  }

  quit_times = KILO_QUIT_TIMES;
}
```

We handle three keys here: Backspace (ASCII 127), Ctrl-H (which sends the ASCII code 8), and Delete. Backspace and Ctrl-H will both delete the character to the left of the cursor, while Delete will delete the character to the right of the cursor.

We're also handling the Enter key (`\r`), but we'll implement its functionality later.

## Save to Disk

Now that we have editing capabilities, let's implement saving to disk. First, we'll create a function that converts our array of `EditorRow` structs into a single string ready to be written to a file:

```cpp
std::string Editor::convertRowsToString() {
  std::string s{};
  for (const auto& row : m_rows)
    s += row.row + '\n';

  return s;
}
```

This function iterates through all rows, appending each row's content followed by a newline character to a string, and then returns the resulting string.

Now let's implement the `save()` function:

```cpp
void Editor::save() {
  if (m_filename.empty())
    return;

  std::string s = convertRowsToString();

  std::ofstream file(m_filename);
  if (!file.is_open()) {
    setStatusMessage("Can't save! I/O error: %s", std::strerror(errno));
    return;
  }

  file << s;
  file.close();
  m_dirty = 0;
  setStatusMessage("%d bytes written to disk", s.size());
}
```

If the file doesn't have a name (which happens when the user starts the editor without specifying a filename), we simply return. Later, we'll implement a "Save As" feature to handle this case.

We get a string containing the entire file content using `convertRowsToString()`, then open the file for writing using `std::ofstream`. If the file can't be opened, we display an error message. Otherwise, we write the string to the file, close it, reset the `m_dirty` flag to indicate that the file has been saved, and display a success message.

Now let's add a keyboard shortcut to save the file. We'll use Ctrl-S:

```cpp
void Editor::processKeypress() {
  static int quit_times = KILO_QUIT_TIMES;
  int c = terminal_manager::readKey();

  switch (c) {
  // ... other cases ...

  case CTRL_KEY('s'):
    save();
    break;

  // ... other cases ...
  }

  quit_times = KILO_QUIT_TIMES;
}
```

## Dirty Flag

We'd like to keep track of whether the text loaded in our editor differs from what's in the file. This way, we can warn the user if they try to quit without saving changes. Let's add a `m_dirty` variable to the global editor state and initialize it to 0:

```cpp
class Editor {
public:
  // ... other public members ...

private:
  // ... other private members ...
  uint8_t m_dirty = 0;
};
```

The `m_dirty` flag will be incremented whenever we make a change to the text, and reset to 0 when we save the file or open a new file. We've already modified our code to increment `m_dirty` when inserting a character and to reset it when saving.

Let's also update `open()` to reset `m_dirty` when a file is opened:

```cpp
void Editor::open(const char* filename) {
  // ... existing code ...

  file.close();
  m_dirty = 0;
}
```

Now let's show the state of `m_dirty` in the status bar by displaying "(modified)" after the filename if the file has been modified:

```cpp
void Editor::drawStatusBar(std::string& s) {
  s += "\x1b[7m";  // Inverted colors

  std::stringstream ss, rss;
  ss << (m_filename.empty()
         ? "[No Name]"
         : m_filename.substr(0, std::min(static_cast<int>(m_filename.size()), FILENAME_DISPLAY_LEN)))
     << " - "
     << m_rows.size()
     << " lines"
     << (m_dirty ? " (modified)" : "");
  int len = std::min(static_cast<int>(ss.str().size()), m_screencols);

  // ... rest of the function ...
}
```

## Quit Confirmation

Now we're ready to warn the user about unsaved changes when they try to quit. Let's modify the Ctrl-Q case in `processKeypress()`:

```cpp
void Editor::processKeypress() {
  static int quit_times = KILO_QUIT_TIMES;
  int c = terminal_manager::readKey();

  switch (c) {
  // ... other cases ...

  case CTRL_KEY('q'):
    if (m_dirty && quit_times > 0) {
      setStatusMessage(
          "WARNING!!! File has unsaved changes. Press Ctrl-Q %d more times to quit.",
          quit_times);
      quit_times--;
      return;
    }
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;

  // ... other cases ...
  }

  quit_times = KILO_QUIT_TIMES;
}
```

We've defined a constant `KILO_QUIT_TIMES` (let's set it to 3). If the file has unsaved changes and the user presses Ctrl-Q, we'll display a warning message and decrement `quit_times`. The user will need to press Ctrl-Q three more times in succession to quit without saving. If the user does anything else, `quit_times` will be reset back to 3 at the end of the function.

## Simple Backspacing

Let's implement backspacing by first creating a function that deletes a character in an `EditorRow`:

```cpp
void Editor::deleteCharFromRow(EditorRow& erow, int at) {
  if (at < 0 || at >= static_cast<int>(erow.row.size()))
    return;

  erow.row.erase(at, 1);
  updateRow(erow);
  m_dirty++;
}
```

This function is like the inverse of `insertCharIntoRow()`. It validates the `at` index, deletes the character at that position using C++'s `std::string::erase()`, updates the row, and increments the `m_dirty` flag.

Now let's implement the `deleteChar()` function that will be called when the user presses Backspace:

```cpp
void Editor::deleteChar() {
  if (m_cy == static_cast<int>(m_rows.size()))
    return;

  if (m_cx == 0 && m_cy == 0)
    return;

  auto& erow = m_rows[m_cy];
  if (m_cx > 0) {
    deleteCharFromRow(erow, m_cx - 1);
    m_cx--;
  }
}
```

This function does nothing if the cursor is past the end of the file or at the beginning of the first line. Otherwise, it deletes the character to the left of the cursor and moves the cursor one position to the left.

Now let's update the Backspace case in `processKeypress()`:

```cpp
void Editor::processKeypress() {
  static int quit_times = KILO_QUIT_TIMES;
  int c = terminal_manager::readKey();

  switch (c) {
  // ... other cases ...

  case static_cast<int>(EditorKey::BACKSPACE):
  case CTRL_KEY('h'):
  case static_cast<int>(EditorKey::DEL_KEY):
    if (c == static_cast<int>(EditorKey::DEL_KEY))
      moveCursor(static_cast<int>(EditorKey::ARROW_RIGHT));
    deleteChar();
    break;

  // ... other cases ...
  }

  quit_times = KILO_QUIT_TIMES;
}
```

If the Delete key is pressed, we first move the cursor one position to the right, so that when we call `deleteChar()`, it will delete the character that was under the cursor.

## Backspacing at the Start of a Line

Currently, `deleteChar()` does nothing when the cursor is at the beginning of a line. Let's modify it to backspace by joining the current line with the previous line:

```cpp
void Editor::deleteChar() {
  if (m_cy == static_cast<int>(m_rows.size()))
    return;

  if (m_cx == 0 && m_cy == 0)
    return;

  auto& erow = m_rows[m_cy];
  if (m_cx > 0) {
    deleteCharFromRow(erow, m_cx - 1);
    m_cx--;
  } else {
    m_cx = static_cast<int>(m_rows[m_cy - 1].row.size());
    appendStringToRow(m_rows[m_cy - 1], erow.row);
    deleteRow(m_cy);
    m_cy--;
  }
}
```

We need to add two new functions:

```cpp
void Editor::appendStringToRow(EditorRow& erow, const std::string& s) {
  erow.row += s;
  updateRow(erow);
  m_dirty++;
}

void Editor::deleteRow(int at) {
  if (at < 0 || at >= static_cast<int>(m_rows.size()))
    return;

  m_rows.erase(m_rows.begin() + at);
  std::for_each(m_rows.begin() + at, m_rows.end(), [](auto& erow)
                { erow.idx--; });
  m_dirty++;
}
```

`appendStringToRow()` appends a string to the end of an existing row, and `deleteRow()` deletes a row at a given index. Both functions update the `m_dirty` flag.

Now when the user backspaces at the beginning of a line, the line will be joined with the previous line.

## The Enter Key

The last editor operation we need to implement is the Enter key, which should insert a new line. Let's add an `insertNewline()` function:

```cpp
void Editor::insertNewline() {
  if (m_cx == 0) {
    insertRow(m_cy, "");
  } else {
    // Note:
    // when inserting newline with insertRow(), the vector of m_row will reallocate its memory,
    // then the reference to m_rows will become invalid.
    // In order to avoid this, we need to reserve memory accounting for the new row.
    m_rows.reserve(m_rows.size() + 1);

    auto& erow = m_rows[m_cy];
    if (insertRow(m_cy + 1, erow.row.substr(m_cx))) {
      erow.row = erow.row.substr(0, m_cx);
      updateRow(erow);
    }
  }

  m_cy++;
  m_cx = 0;
}
```

This function handles two cases:

1. When the cursor is at the beginning of a line, it simply inserts a new, empty row before the current line.
2. When the cursor is in the middle of a line, it splits the line into two: the part before the cursor stays on the current line, and the part after the cursor goes to a new line below.

We need to be careful with memory management here. When we insert a new row, the `m_rows` vector might need to reallocate its memory, which would invalidate any references to its elements. To avoid this, we reserve enough memory for the new row before manipulating the row that might be affected.

Now let's add a function to insert a row at a specified index:

```cpp
bool Editor::insertRow(int at, const std::string& s) {
  if (at < 0 || at > static_cast<int>(m_rows.size()))
    return false;

  m_rows.insert(m_rows.begin() + at, EditorRow(at));
  m_rows[at].row = s;
  m_rows[at].idx = at;
  std::for_each(m_rows.begin() + at + 1, m_rows.end(), [](auto& erow)
                { erow.idx++; });

  updateRow(m_rows[at]);
  m_dirty++;

  return true;
}
```

This function creates a new `EditorRow` at the specified index, sets its content to the given string, and updates the `idx` fields of all following rows.

Finally, let's update the Enter case in `processKeypress()`:

```cpp
void Editor::processKeypress() {
  static int quit_times = KILO_QUIT_TIMES;
  int c = terminal_manager::readKey();

  switch (c) {
  case '\r':
    insertNewline();
    break;

  // ... other cases ...
  }

  quit_times = KILO_QUIT_TIMES;
}
```

Now when the user presses Enter, a new line will be inserted.

## Save As

Currently, when the user tries to save a file without a name, we simply return. Let's implement a "Save As" feature by prompting the user for a filename.

First, let's create a function that displays a prompt in the status bar and lets the user input a line of text:

```cpp
std::string Editor::fromPrompt(
    std::string prompt, std::function<void(std::string&, int)> callback) {
  std::string s = "\0";
  while (1) {
    setStatusMessage(prompt.c_str(), s.c_str());
    refreshScreen();

    int c = terminal_manager::readKey();
    if (c == static_cast<int>(EditorKey::DEL_KEY) ||
        c == CTRL_KEY('h') ||
        c == static_cast<int>(EditorKey::BACKSPACE)) {
      if (s.size())
        s.pop_back();
    } else if (c == '\x1b') {
      setStatusMessage("");
      if (callback)
        callback(s, c);
      return "";
    } else if (c == '\r') {
      if (s.size()) {
        setStatusMessage("");
        if (callback)
          callback(s, c);
        return s;
      }
    } else if (!iscntrl(c) && c < 128) {
      s += c;
    }

    if (callback)
      callback(s, c);
  }
}
```

This function takes a prompt string and an optional callback function. It displays the prompt with the current input, waits for a keypress, and handles it appropriately. If the user presses Escape, it returns an empty string. If the user presses Enter and the input is not empty, it returns the input. If the user presses Backspace, it removes the last character from the input. Otherwise, it appends the key to the input.

Now let's update the `save()` function to use `fromPrompt()` when the filename is empty:

```cpp
void Editor::save() {
  if (m_filename.empty()) {
    m_filename = fromPrompt("Save as: %s (ESC to cancel)");
    if (m_filename.empty()) {
      setStatusMessage("Save aborted");
      return;
    }
  }

  // ... rest of the function ...
}
```

With this change, when the user presses Ctrl-S on a file without a name, they will be prompted to enter a filename.

## Summary

We've transformed our text viewer into a full-featured text editor. The user can now:

1. Insert and delete characters
2. Move the cursor using arrow keys
3. Insert new lines with Enter
4. Join lines by backspacing at the beginning of a line
5. Save the file with Ctrl-S
6. Be warned about unsaved changes when quitting with Ctrl-Q
7. Save a file under a new name

In the [next chapter](06_search), we'll add a search feature to our editor, making it even more useful.

[<- previous chapter](04_text_viewer)

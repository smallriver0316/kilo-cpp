---
layout: page
---

# Chapter 6: Search

In this chapter, we'll implement a search feature for our text editor. The ability to search through text is a fundamental feature of any text editor. We'll implement several search capabilities:

- Basic text search
- Incremental search (search updates as you type)
- Forward and backward navigation between search results
- Restoring cursor position when canceling a search

Let's start by implementing a minimal search feature and then gradually enhance it.

## Basic Search Feature

First, we need to add a function to handle the search functionality. We'll use the `fromPrompt()` method that we already have to get the search query from the user.

### The `find()` Method

Let's start by adding a simple `find()` method to the `Editor` class. This method will prompt the user for a search string and then search for that string in the editor's rows.

```cpp
void Editor::find() {
  char *query = fromPrompt("Search: %s (ESC to cancel)");
  if (query == NULL) return;

  int i;
  for (i = 0; i < E.numrows; i++) {
    erow *row = &E.row[i];
    char *match = strstr(row->render, query);
    if (match) {
      E.cy = i;
      E.cx = convertRowRxToCx(row, match - row->render);
      E.rowoff = E.numrows;
      break;
    }
  }

  free(query);
}
```

In our C++ implementation, we need to adapt this to use `std::string` instead of C-style strings, and our class member variables instead of the global `E` struct:

```cpp
void Editor::find() {
  std::string query = fromPrompt("Search: %s (ESC to cancel)");
  if (query.empty()) return;

  int i;
  for (i = 0; i < static_cast<int>(m_rows.size()); i++) {
    EditorRow &row = m_rows[i];
    auto match_start_offset = row.rendered.find(query);
    if (match_start_offset != std::string::npos) {
      m_cy = i;
      m_cx = convertRowRxToCx(row, static_cast<int>(match_start_offset));
      m_rowoff = m_rows.size();
      break;
    }
  }
}
```

There's a problem here: we're using `convertRowRxToCx()` to convert a render index to a chars index, but we haven't implemented that function yet. Let's add it.

### Converting Render Index to Chars Index

We already have a `convertRowCxToRx()` method that converts a chars index to a render index. Now we need to do the reverse:

```cpp
int Editor::convertRowRxToCx(EditorRow &erow, int rx) {
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

This function iterates through the `chars` string, keeping track of the corresponding `render` index (`cur_rx`). Once `cur_rx` reaches or exceeds the given `rx` value, we return the corresponding `chars` index.

### Using the Search Feature

Now we need to map a key to the `find()` method. In `processKeypress()`, we'll map Ctrl-F to our search function:

```cpp
case CTRL_KEY('f'):
  find();
  break;
```

Also, let's update the help message in `main()` to include the search command:

```cpp
editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");
```

## Incremental Search with Callbacks

Now, let's enhance our search to be incremental, updating the search results as the user types. We'll also add the ability to navigate between search results using the arrow keys.

To do this, we need to modify the `fromPrompt()` method to accept a callback function that will be called for each keypress during the prompt.

### Adding a Callback to `fromPrompt()`

First, let's update the `fromPrompt()` method to accept a callback function:

```cpp
std::string Editor::fromPrompt(
    std::string prompt,
    std::function<void(std::string &, int)> callback) {
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

We've enhanced `fromPrompt()` to accept a `std::function` callback that takes a string reference and an integer. The callback is called after each keypress, allowing us to perform searches incrementally as the user types.

### The `findCallback` Method

Now let's implement the callback function for searching:

```cpp
void Editor::findCallback(std::string &query, int key) {
  static int last_match = -1;
  static int direction = 1;

  if (key == '\r' || key == '\x1b') {
    last_match = -1;
    direction = 1;
    return;
  } else if (key == static_cast<int>(EditorKey::ARROW_RIGHT) ||
             key == static_cast<int>(EditorKey::ARROW_DOWN)) {
    direction = 1;
  } else if (key == static_cast<int>(EditorKey::ARROW_LEFT) ||
             key == static_cast<int>(EditorKey::ARROW_UP)) {
    direction = -1;
  } else {
    last_match = -1;
    direction = 1;
  }

  if (last_match == -1) direction = 1;

  int current = last_match;

  for (auto &row : m_rows) {
    current += direction;
    if (current == -1)
      current = m_rows.size() - 1;
    else if (current == static_cast<int>(m_rows.size()))
      current = 0;

    const auto &render = row.rendered;
    auto match_start_offset = render.find(query);
    if (match_start_offset != std::string::npos) {
      last_match = current;
      m_cy = current;
      m_cx = convertRowRxToCx(m_rows[current], static_cast<int>(match_start_offset));
      m_rowoff = m_rows.size();
      break;
    }
  }
}
```

This callback function keeps track of the last match found and the direction to search. When arrow keys are pressed, it changes the search direction. When Enter or Escape is pressed, it resets the search.

### Updating the `find()` Method

Now we need to update the `find()` method to use our new callback:

```cpp
void Editor::find() {
  int saved_cx = m_cx;
  int saved_cy = m_cy;
  int saved_coloff = m_coloff;
  int saved_rowoff = m_rowoff;

  std::string query = fromPrompt("Search: %s (Use ESC/Arrows/Enter)",
                                 [this](std::string &query, int key) {
                                   findCallback(query, key);
                                 });

  if (query.empty()) {
    m_cx = saved_cx;
    m_cy = saved_cy;
    m_coloff = saved_coloff;
    m_rowoff = saved_rowoff;
  }
}
```

We save the current cursor position and scroll position before starting the search. If the search is canceled (query is empty), we restore these positions.

## Highlighting Search Results

Let's add one more feature: highlighting the current search match. This will give users a visual indicator of where the search term is found.

We'll modify the `EditorHighlight` enum to include a `MATCH` value:

```cpp
enum class EditorHighlight : unsigned char
{
  NORMAL = 0,
  COMMENT,
  ML_COMMENT,
  KEYWORD1,
  KEYWORD2,
  STRING,
  NUMBER,
  MATCH
};
```

And then modify the `convertSyntaxToColor()` method to handle the `MATCH` case:

```cpp
int Editor::convertSyntaxToColor(EditorHighlight hl)
{
  switch (hl)
  {
  case EditorHighlight::COMMENT:
  case EditorHighlight::ML_COMMENT:
    return 36;
  case EditorHighlight::KEYWORD1:
    return 33;
  case EditorHighlight::KEYWORD2:
    return 32;
  case EditorHighlight::STRING:
    return 35;
  case EditorHighlight::NUMBER:
    return 31;
  case EditorHighlight::MATCH:
    return 34;
  }

  // default: NORMAL
  return 37;
}
```

Now we need to modify the `findCallback()` to highlight the match:

```cpp
void Editor::findCallback(std::string &query, int key) {
  static int last_match = -1;
  static int direction = 1;

  static int saved_hl_line;
  static std::vector<EditorHighlight> saved_hl;

  if (!saved_hl.empty()) {
    std::copy(saved_hl.begin(), saved_hl.end(), m_rows[saved_hl_line].hl.begin());
    saved_hl.clear();
  }

  if (key == '\r' || key == '\x1b') {
    last_match = -1;
    direction = 1;
    return;
  } else if (key == static_cast<int>(EditorKey::ARROW_RIGHT) ||
             key == static_cast<int>(EditorKey::ARROW_DOWN)) {
    direction = 1;
  } else if (key == static_cast<int>(EditorKey::ARROW_LEFT) ||
             key == static_cast<int>(EditorKey::ARROW_UP)) {
    direction = -1;
  } else {
    last_match = -1;
    direction = 1;
  }

  if (last_match == -1) direction = 1;

  int current = last_match;

  for (auto &row : m_rows) {
    current += direction;
    if (current == -1)
      current = m_rows.size() - 1;
    else if (current == static_cast<int>(m_rows.size()))
      current = 0;

    const auto &render = row.rendered;
    auto match_start_offset = render.find(query);
    if (match_start_offset != std::string::npos) {
      last_match = current;
      m_cy = current;
      m_cx = convertRowRxToCx(m_rows[current], static_cast<int>(match_start_offset));
      m_rowoff = m_rows.size();

      saved_hl_line = current;
      saved_hl.reserve(row.rendered.size());
      std::copy(row.hl.begin(), row.hl.end(), std::back_inserter(saved_hl));

      const auto match_end_offset = row.hl.begin() + match_start_offset + query.size() > row.hl.end()
                                        ? row.hl.end() - row.hl.begin()
                                        : match_start_offset + query.size();
      std::fill(row.hl.begin() + match_start_offset,
                row.hl.begin() + match_end_offset,
                EditorHighlight::MATCH);
      return;
    }
  }
}
```

We've added code to save the original highlighting of the row and then restore it when a new search is performed. We've also added code to highlight the matched text in the row.

## Final Code

Let's put all of these pieces together into the Editor.hpp header and the Editor.cpp implementation.

### Adding Declarations to Editor.hpp

```cpp
/*** find ***/
void findCallback(std::string &query, int key);
void find();
```

Make sure these are added to the private section of the `Editor` class.

### Complete Implementation in Editor.cpp

The complete implementation of the search feature is now available in the `Editor.cpp` file.

## Conclusion

We've successfully implemented a search feature for our text editor. This search feature includes:

1. Basic text search
2. Incremental search (search updates as you type)
3. Forward and backward navigation between search results using arrow keys
4. Highlighting of search matches
5. Restoring cursor position when canceling a search

In the [next chapter](07_syntax_highlighting), we'll implement syntax highlighting to make our editor more usable for programmers.

[<- previous chapter](05_text_editor)

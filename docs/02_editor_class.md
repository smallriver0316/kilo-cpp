---
layout: page
---

# Chapter 2: The Editor Class

The heart of kilo++ is the `Editor` class. Unlike the original C implementation which used global variables, we'll encapsulate all editor state in this class.

## Basic Class Structure

```cpp
class Editor {
public:
    Editor();
    void run(int argc, char *argv[]);

private:
    std::vector<EditorRow> m_rows;
    int m_cx = 0, m_cy = 0;  // Cursor position
    int m_rx = 0;            // Render position
    int m_rowoff = 0, m_coloff = 0;  // Scroll offset
    int m_screenrows, m_screencols;
    uint8_t m_dirty = 0;
    std::string m_filename = "";
    std::string m_statusmsg = "\0";
    time_t m_statusmsg_time = 0;
    std::shared_ptr<EditorSyntax> m_syntax = nullptr;
};
```

## Editor Row Structure

Instead of C structs, we use a proper class to represent each line of text:

```cpp
struct EditorRow {
    EditorRow(int index) : idx(index), row({}), rendered({}), hl({}) {
        hl_open_comment = false;
    };

    int idx;
    std::string row;         // Actual content
    std::string rendered;    // Rendered content (with tabs expanded)
    std::vector<EditorHighlight> hl;  // Syntax highlighting
    bool hl_open_comment;
};
```

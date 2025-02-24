---
layout: page
---

## Chapter 4: Text Editing Operations

In kilo++, text editing operations are implemented as member functions of the Editor class.

### Inserting Text

```cpp
void Editor::insertChar(int c) {
    if (m_cy == static_cast<int>(m_rows.size()))
        insertRow(m_cy, "");

    insertCharIntoRow(m_rows[m_cy], m_cx, c);
    m_cx++;
}

void Editor::insertCharIntoRow(EditorRow &erow, int yindex, int c) {
    if (yindex < 0 || yindex > static_cast<int>(erow.row.size()))
        yindex = erow.row.size();

    erow.row.insert(yindex, 1, c);
    updateRow(erow);
    m_dirty++;
}
```

Go the next chapter, [Chapter 5: File I/O](05_file_io).

[<- previous chapter](03_terminal_handling)

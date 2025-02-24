---
layout: page
---

## Chapter 7: Search Functionality

The search implementation uses C++ standard library features and callbacks.

```cpp
void Editor::find() {
    int saved_cx = m_cx;
    int saved_cy = m_cy;
    int saved_coloff = m_coloff;
    int saved_rowoff = m_rowoff;

    std::string query = fromPrompt(
        "Search: %s (Use ESC/Arrows/Enter)",
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

Go the final chapter, [Chapter 8: Conclusion](08_conclusion).

[<- previous chapter](06_syntax_highlighting)

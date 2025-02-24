---
layout: page
---

## Chapter 5: File I/O

Kilo++ uses modern C++ file handling with RAII principles.

### File Opening

```cpp
void Editor::open(const char *filename) {
    if (!std::filesystem::exists(filename)) {
        std::ofstream file(filename);
        if (!file)
            terminal_manager::die("create a new file");
    }

    m_filename = std::string(filename);
    selectSyntaxHighlight();

    std::ifstream file(filename);
    if (!file.is_open())
        terminal_manager::die("fopen");

    std::string line;
    while (std::getline(file, line)) {
        while (line.size() && (line[line.size() - 1] == '\n' ||
                              line[line.size() - 1] == '\r'))
            line.erase(line.size() - 1);

        insertRow(m_rows.size(), line);
    }

    file.close();
    m_dirty = 0;
}
```

Go the next chapter, [Chapter 6: Syntax Highlighting](06_syntax_highlighting).

[<- previous chapter](04_text_editing_operation)

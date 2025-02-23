---
layout: page
---

# Chapter 6: Syntax Highlighting

Kilo++ implements syntax highlighting using modern C++ features like enum classes and vectors.

## Syntax Highlighting Types

```cpp
enum class EditorHighlight : unsigned char {
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

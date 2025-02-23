---
layout: page
---

# Building and Running

To build kilo++:

```bash
mkdir build && cd build
cmake ..
make
```

To run:

```bash
./kilo++ [filename]
```

## Key Bindings

- Ctrl-S: Save
- Ctrl-Q: Quit
- Ctrl-F: Find
- Arrow keys: Move cursor
- Page Up/Down: Scroll
- Home/End: Move to start/end of line

## Conclusion

This tutorial has shown how to build a text editor using modern C++ features. The complete source code is available on GitHub, and you can extend it further by:

- Adding more syntax highlighting rules
- Implementing undo/redo
- Adding line numbers
- Supporting multiple buffers
- Implementing auto-indent

## Resources

- [Original kilo tutorial](https://viewsourcecode.org/snaptoken/kilo/)
- [Modern C++ resources](https://www.modernescpp.com/)
- [Terminal programming guide](https://vt100.net/docs/vt100-ug/chapter3.html)

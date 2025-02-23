---
layout: page
---

# Chapter 3: Terminal Handling

One major improvement in kilo++ is the separation of terminal-related code into its own namespace.

## Raw Mode Management

```cpp
namespace terminal_manager {
    void enableRawMode() {
        if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
            die("tcgetattr");

        atexit(disableRawMode);

        struct termios raw = orig_termios;
        raw.c_iflag &= ~(BRKINT | ICRNL | ISTRIP | IXON);
        raw.c_oflag &= ~(OPOST);
        raw.c_cflag |= (CS8);
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 1;

        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
            die("tcsetattr");
    }
}
```

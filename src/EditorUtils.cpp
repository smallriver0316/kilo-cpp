#include "kilo++/EditorUtils.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace terminal_manager
{

  termios orig_termios;

  void die(const char *s)
  {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
  }

  void disableRawMode()
  {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
      die("tcsetattr");
  }

  void enableRawMode()
  {
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

  int getCursorPosition(int *rows, int *cols)
  {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
      return -1;

    while (i < sizeof(buf) - 1)
    {
      if (read(STDIN_FILENO, &buf[i], 1) != 1)
        break;

      if (buf[i] == 'R')
        break;

      i++;
    }

    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[')
      return -1;

    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
      return -1;

    return 0;
  }

  int getWindowSize(int *rows, int *cols)
  {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
      if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
        return -1;

      return getCursorPosition(rows, cols);
    }
    else
    {
      *rows = ws.ws_row;
      *cols = ws.ws_col;
      return 0;
    }
  }

  int readKey()
  {
    int nread;
    char c;

    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
      if (nread == -1 && errno != EAGAIN)
        die("read");
    }

    if (c == '\x1b')
    {
      char seq[3];

      if (read(STDIN_FILENO, &seq[0], 1) != 1)
        return '\x1b';

      if (read(STDIN_FILENO, &seq[1], 1) != 1)
        return '\x1b';

      if (seq[0] == '[')
      {
        if (seq[1] >= '0' && seq[1] <= '9')
        {
          if (read(STDIN_FILENO, &seq[2], 1) != 1)
            return '\x1b';

          if (seq[2] == '~')
          {
            switch (seq[1])
            {
            case '1':
              return static_cast<int>(EditorKey::HOME_KEY);
            case '3':
              return static_cast<int>(EditorKey::DEL_KEY);
            case '4':
              return static_cast<int>(EditorKey::END_KEY);
            case '5':
              return static_cast<int>(EditorKey::PAGE_UP);
            case '6':
              return static_cast<int>(EditorKey::PAGE_DOWN);
            case '7':
              return static_cast<int>(EditorKey::HOME_KEY);
            case '8':
              return static_cast<int>(EditorKey::END_KEY);
            }
          }
        }
        else
        {
          switch (seq[1])
          {
          case 'A':
            return static_cast<int>(EditorKey::ARROW_UP);
          case 'B':
            return static_cast<int>(EditorKey::ARROW_DOWN);
          case 'C':
            return static_cast<int>(EditorKey::ARROW_RIGHT);
          case 'D':
            return static_cast<int>(EditorKey::ARROW_LEFT);
          case 'H':
            return static_cast<int>(EditorKey::HOME_KEY);
          case 'F':
            return static_cast<int>(EditorKey::END_KEY);
          }
        }
      }
      else if (seq[0] == 'O')
      {
        switch (seq[1])
        {
        case 'H':
          return static_cast<int>(EditorKey::HOME_KEY);
        case 'F':
          return static_cast<int>(EditorKey::END_KEY);
        }
      }

      return '\x1b';
    }

    return c;
  }

} // namespace terminal_manager

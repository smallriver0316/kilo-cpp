#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define KILO_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)

enum class EditorKey
{
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

/*** data ***/

struct EditorRow
{
  int size;
  char *chars;
};

struct EditorConfig
{
  int cx, cy;
  int screenrows;
  int screencols;
  int numrows;
  EditorRow row;
  termios orig_termios;
} E;

/*** terminal ***/

void die(const char *s)
{
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

void disableRawMode()
{
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
  {
    die("tcsetattr");
  }
}

void enableRawMode()
{
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
  {
    die("tcgetattr");
  }

  atexit(disableRawMode);

  struct termios raw = E.orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
  {
    die("tcsetattr");
  }
}

int editorReadKey()
{
  int nread;
  char c;

  while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
  {
    if (nread == -1 && errno != EAGAIN)
    {
      die("read");
    }
  }

  if (c == '\x1b')
  {
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1)
    {
      return '\x1b';
    }
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
    {
      return '\x1b';
    }

    if (seq[0] == '[')
    {
      if (seq[1] >= '0' && seq[1] <= '9')
      {
        if (read(STDIN_FILENO, &seq[2], 1) != 1)
        {
          return '\x1b';
        }
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

int getCursorPosition(int *rows, int *cols)
{
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
  {
    return -1;
  }

  while (i < sizeof(buf) - 1)
  {
    if (read(STDIN_FILENO, &buf[i], 1) != 1)
    {
      break;
    }

    if (buf[i] == 'R')
    {
      break;
    }

    i++;
  }

  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[')
  {
    return -1;
  }

  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
  {
    return -1;
  }

  return 0;
}

int getWindowSize(int *rows, int *cols)
{
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
  {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
    {
      return -1;
    }

    return getCursorPosition(rows, cols);
  }
  else
  {
    *rows = ws.ws_row;
    *cols = ws.ws_col;
    return 0;
  }
}

/*** output ***/

void editorDrawRows(std::string &s)
{
  int y;
  for (y = 0; y < E.screenrows; y++)
  {
    if (y == E.screenrows / 3)
    {
      std::string welcomemsg = "Kilo editor -- version " + std::string(KILO_VERSION);
      if (welcomemsg.size() > (std::size_t)E.screencols)
      {
        welcomemsg.resize(E.screencols);
      }

      int padding = ((std::size_t)E.screencols - welcomemsg.size()) / 2;
      if (padding)
      {
        s += "~";
        padding--;
      }

      while (padding--)
      {
        s += " ";
      }

      s += welcomemsg;
    }
    else
    {
      s += "~";
    }
    s += "\x1b[K";

    if (y < E.screenrows - 1)
    {
      s += "\r\n";
    }
  }
}

void editorRefreshScreen()
{
  std::string s;

  s += "\x1b[?25l";
  s += "\x1b[H";

  editorDrawRows(s);

  std::stringstream ss;
  ss << "\x1b[" << E.cy + 1 << ";" << E.cx + 1 << "H" << "\x1b[?25h";
  s += ss.str();

  write(STDOUT_FILENO, s.c_str(), s.size());
  s = "";
}

/** input ***/

void editorMoveCursor(int key)
{
  switch (key)
  {
  case static_cast<int>(EditorKey::ARROW_LEFT):
    if (E.cx != 0)
    {
      E.cx--;
    }
    break;
  case static_cast<int>(EditorKey::ARROW_RIGHT):
    if (E.cx != E.screencols - 1)
    {
      E.cx++;
    }
    break;
  case static_cast<int>(EditorKey::ARROW_UP):
    if (E.cy != 0)
    {
      E.cy--;
    }
    break;
  case static_cast<int>(EditorKey::ARROW_DOWN):
    if (E.cy != E.screenrows - 1)
    {
      E.cy++;
    }
    break;
  }
}

void editorProcessKeypress()
{
  int c = editorReadKey();

  switch (c)
  {
  case CTRL_KEY('q'):
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;
  case static_cast<int>(EditorKey::HOME_KEY):
    E.cx = 0;
    break;
  case static_cast<int>(EditorKey::END_KEY):
    E.cx = E.screencols - 1;
    break;
  case static_cast<int>(EditorKey::PAGE_UP):
  case static_cast<int>(EditorKey::PAGE_DOWN):
  {
    int times = E.screenrows;
    while (times--)
    {
      editorMoveCursor(c == static_cast<int>(EditorKey::PAGE_UP) ? static_cast<int>(EditorKey::ARROW_UP) : static_cast<int>(EditorKey::ARROW_DOWN));
    }
  }
  break;
  case static_cast<int>(EditorKey::ARROW_UP):
  case static_cast<int>(EditorKey::ARROW_DOWN):
  case static_cast<int>(EditorKey::ARROW_LEFT):
  case static_cast<int>(EditorKey::ARROW_RIGHT):
    editorMoveCursor(c);
    break;
  }
}

/*** init ***/

void initEditor()
{
  E.cx = 0;
  E.cy = 0;
  E.numrows = 0;
  if (getWindowSize(&E.screenrows, &E.screencols) == -1)
  {
    die("getWindowSize");
  }
}

int main()
{
  enableRawMode();
  initEditor();

  while (1)
  {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}

/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE

#include <cctype>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <vector>

/*** defines ***/

#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 8
#define KILO_QUIT_TIMES 3
#define FILENAME_DISPLAY_LEN 20

#define CTRL_KEY(k) ((k) & 0x1f)

enum class EditorKey
{
  BACKSPACE = 127,
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

struct EditorConfig
{
  int cx, cy;
  int rx;
  int rowoff;
  int coloff;
  int screenrows;
  int screencols;
  std::vector<std::string> rows;
  std::vector<std::string> renders;
  int dirty;
  std::string filename;
  std::string statusmsg;
  time_t statusmsg_time;
  termios orig_termios;
} E;

/*** prototypes ***/
void editorSetStatusMessage(const char *fmt, ...);

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

/*** row operations ***/

int editorRowCxToRx(std::string &s, int cx)
{
  int rx = 0;
  for (int i = 0; i < cx; i++)
  {
    if (s[i] == '\t')
    {
      rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
    }
    rx++;
  }
  return rx;
}

void editorUpdateRow(std::string &s, bool is_editing = false)
{
  int tabs = 0;
  for (int i = 0; i < (int)s.size(); i++)
  {
    if (s[i] == '\t')
    {
      tabs++;
    }
  }

  std::string render = "";
  for (int i = 0; i < (int)s.size(); i++)
  {
    if (s[i] == '\t')
    {
      render += " ";
      while (render.size() % KILO_TAB_STOP != 0)
      {
        render += " ";
      }
    }
    else
    {
      render += s[i];
    }
  }

  render += '\0';
  if (is_editing)
  {
    E.renders[E.cy] = render;
  }
  else
  {
    E.renders.push_back(render);
  }
}

void editorAppendRow(std::string &s)
{
  E.rows.push_back(s + '\0');
  editorUpdateRow(s);
  E.dirty++;
}

void editorDelRow(int at)
{
  if (at < 0 || at >= (int)E.rows.size())
  {
    return;
  }
  E.rows.erase(E.rows.begin() + at);
  E.renders.erase(E.renders.begin() + at);
  E.dirty++;
}

void editorRowInsertChar(std::string &s, int at, int c)
{
  if (at < 0 || at > (int)s.size())
  {
    at = s.size();
  }
  s.insert(at, 1, c);
  editorUpdateRow(s, true);
  E.dirty++;
}

void editorRowAppendString(std::string &s, std::string &append)
{
  s += append;
  editorUpdateRow(s, true);
  E.dirty++;
}

void editorRowDelChar(std::string &s, int at)
{
  if (at < 0 || at >= (int)s.size())
  {
    return;
  }
  s.erase(at, 1);
  editorUpdateRow(s, true);
  E.dirty++;
}

/*** editor operations */

void editorInsertChar(int c)
{
  if (E.cy == (int)E.rows.size())
  {
    std::string s = "";
    editorAppendRow(s);
  }
  editorRowInsertChar(E.rows[E.cy], E.cx, c);
  E.cx++;
}

void editorDelChar()
{
  if (E.cy == (int)E.rows.size())
  {
    return;
  }
  if (E.cx == 0 && E.cy == 0)
  {
    return;
  }

  if (E.cx > 0)
  {
    editorRowDelChar(E.rows[E.cy], E.cx - 1);
    E.cx--;
  }
  else
  {
    E.cy--;
    E.cx = E.rows[E.cy].size() - 1;
    editorRowAppendString(E.rows[E.cy], E.rows[E.cy + 1]);
    editorDelRow(E.cy + 1);
  }
}

/*** file i/o ***/

std::string editorRowsToString()
{
  std::string s = "";
  for (const auto &row : E.rows)
  {
    s += row.substr(0, row.size() - 1) + '\n';
  }
  return s;
}

void editorOpen(const char *filename)
{
  E.filename = std::string(filename);

  std::ifstream file(filename);
  if (!file.is_open())
  {
    die("fopen");
  }

  std::string line;
  while (std::getline(file, line))
  {
    if (line[line.size() - 1] == '\n')
    {
      line.erase(line.size() - 1);
    }
    if (line[line.size() - 1] == '\r')
    {
      line.erase(line.size() - 1);
    }
    editorAppendRow(line);
  }

  file.close();
  E.dirty = 0;
}

void editorSave()
{
  if (E.filename.empty())
  {
    return;
  }

  std::ofstream file(E.filename);
  if (!file.is_open())
  {
    editorSetStatusMessage("Can't save! I/O error: %s", std::strerror(errno));
    die("fopen");
  }

  std::string s = editorRowsToString();
  file << s;
  file.close();
  E.dirty = 0;
  editorSetStatusMessage("%d bytes written to disk", s.size());
}

/*** output ***/

void editorScroll()
{
  E.rx = E.cx;
  if (E.cy < (int)E.rows.size())
  {
    E.rx = editorRowCxToRx(E.rows[E.cy], E.cx);
  }
  if (E.cy < E.rowoff)
  {
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows)
  {
    E.rowoff = E.cy - E.screenrows + 1;
  }
  if (E.rx < E.coloff)
  {
    E.coloff = E.rx;
  }
  if (E.rx >= E.coloff + E.screencols)
  {
    E.coloff = E.rx - E.screencols + 1;
  }
}

void editorDrawRows(std::string &s)
{
  int y;
  for (y = 0; y < E.screenrows; y++)
  {
    int filerow = y + E.rowoff;
    if (filerow >= (int)E.rows.size())
    {
      if (E.rows.size() == 0 && y == E.screenrows / 3)
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
    }
    else
    {
      int len = (int)E.renders[filerow].size() - E.coloff;
      if (len < 0)
      {
        len = 0;
      }
      s += E.renders[filerow].substr(E.coloff, std::min(len, E.screencols));
    }

    s += "\x1b[K";
    s += "\r\n";
  }
}

void editorDrawStatusBar(std::string &s)
{
  s += "\x1b[7m";

  std::stringstream ss, rss;
  ss << (E.filename.empty() ? "[No Name]" : E.filename.substr(0, std::min((int)E.filename.size(), FILENAME_DISPLAY_LEN)))
     << " - "
     << E.rows.size()
     << " lines"
     << (E.dirty ? "(modified)" : "");
  int len = std::min((int)ss.str().size(), E.screencols);

  rss << E.cy + 1 << "/" << E.rows.size();
  int rlen = rss.str().size();

  s += ss.str().substr(0, len);
  while (len < E.screencols)
  {
    if (E.screencols - len == rlen)
    {
      s += rss.str();
      break;
    }
    s += " ";
    len++;
  }
  s += "\x1b[m";
  s += "\r\n";
}

void editorDrawMessageBar(std::string &s)
{
  s += "\x1b[K";
  int msglen = (int)E.statusmsg.size();
  if (msglen > E.screencols)
  {
    msglen = E.screencols;
  }
  if (msglen && time(NULL) - E.statusmsg_time < 5)
  {
    s += E.statusmsg.substr(0, msglen);
  }
}

void editorRefreshScreen()
{
  editorScroll();

  std::string s;

  s += "\x1b[?25l";
  s += "\x1b[H";

  editorDrawRows(s);
  editorDrawStatusBar(s);
  editorDrawMessageBar(s);

  std::stringstream ss;
  ss << "\x1b[" << (E.cy - E.rowoff) + 1 << ";" << (E.rx - E.coloff) + 1 << "H" << "\x1b[?25h";
  s += ss.str();

  write(STDOUT_FILENO, s.c_str(), s.size());
  s = "";
}

void editorSetStatusMessage(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  int size;
  if ((size = std::vsnprintf(nullptr, 0, fmt, args) + 1) < 0)
  {
    va_end(args);
    return;
  }

  if (size <= 1)
  {
    va_end(args);
    return;
  }

  std::vector<char> buf(size);

  va_start(args, fmt);
  std::vsnprintf(buf.data(), size, fmt, args);
  va_end(args);

  E.statusmsg = std::string(buf.data());
  E.statusmsg_time = time(NULL);
}

/** input ***/

void editorMoveCursor(int key)
{
  auto rowitr = (E.cy >= (int)E.rows.size()) ? E.rows.end() : E.rows.begin() + E.cy;

  switch (key)
  {
  case static_cast<int>(EditorKey::ARROW_LEFT):
    if (E.cx != 0)
    {
      E.cx--;
    }
    break;
  case static_cast<int>(EditorKey::ARROW_RIGHT):
    if (rowitr != E.rows.end() && E.cx < (int)rowitr->size() - 1)
    {
      E.cx++;
    }
    else if (rowitr != E.rows.end() && E.cx == (int)rowitr->size() - 1)
    {
      E.cy++;
      E.cx = 0;
    }
    break;
  case static_cast<int>(EditorKey::ARROW_UP):
    if (E.cy != 0)
    {
      E.cy--;
    }
    break;
  case static_cast<int>(EditorKey::ARROW_DOWN):
    if (E.cy < (int)E.rows.size())
    {
      E.cy++;
    }
    break;
  }

  rowitr = (E.cy >= (int)E.rows.size()) ? E.rows.end() : E.rows.begin() + E.cy;
  int rowlen = rowitr != E.rows.end() ? (int)rowitr->size() : 0;
  if (E.cx > rowlen)
  {
    E.cx = rowlen;
  }
}

void editorProcessKeypress()
{
  static int quit_times = KILO_QUIT_TIMES;
  int c = editorReadKey();

  switch (c)
  {
  case '\r':
    /* TODO */
    break;
  case CTRL_KEY('q'):
    if (E.dirty && quit_times > 0)
    {
      editorSetStatusMessage("WARNING!!! File has unsaved changes. Press Ctrl-Q %d more times to quit.", quit_times);
      quit_times--;
      return;
    }
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;
  case CTRL_KEY('s'):
    editorSave();
    break;
  case static_cast<int>(EditorKey::HOME_KEY):
    E.cx = 0;
    break;
  case static_cast<int>(EditorKey::END_KEY):
    if (E.cy < (int)E.rows.size())
    {
      E.cx = E.rows[E.cy].size();
    }
    break;
  case static_cast<int>(EditorKey::BACKSPACE):
  case CTRL_KEY('h'):
  case static_cast<int>(EditorKey::DEL_KEY):
    if (c == static_cast<int>(EditorKey::DEL_KEY))
    {
      editorMoveCursor(static_cast<int>(EditorKey::ARROW_RIGHT));
    }
    editorDelChar();
    break;
  case static_cast<int>(EditorKey::PAGE_UP):
  case static_cast<int>(EditorKey::PAGE_DOWN):
  {
    if (c == static_cast<int>(EditorKey::PAGE_UP))
    {
      E.cy = E.rowoff;
    }
    else
    {
      E.cy = E.rowoff + E.screenrows - 1;
      if (E.cy > (int)E.rows.size())
      {
        E.cy = E.rows.size();
      }
    }

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
  case CTRL_KEY('l'):
  case '\x1b':
    break;
  default:
    editorInsertChar(c);
    break;
  }

  quit_times = KILO_QUIT_TIMES;
}

/*** init ***/

void initEditor()
{
  E.cx = 0;
  E.cy = 0;
  E.rx = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.rows = {};
  E.renders = {};
  E.dirty = 0;
  E.filename = "";
  E.statusmsg = "\0";
  E.statusmsg_time = 0;

  if (getWindowSize(&E.screenrows, &E.screencols) == -1)
  {
    die("getWindowSize");
  }
  E.screenrows -= 2;
}

int main(int argc, char **argv)
{
  enableRawMode();
  initEditor();
  if (argc >= 2)
  {
    editorOpen(argv[1]);
  }

  editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit");

  while (1)
  {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}

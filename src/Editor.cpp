/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE

#include "Editor.hpp"
#include "EditorUtils.hpp"

#include <cctype>
#include <cstdarg>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>

/*** defines ***/

#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 8
#define KILO_QUIT_TIMES 3
#define FILENAME_DISPLAY_LEN 20

#define CTRL_KEY(k) ((k) & 0x1f)

Editor::Editor()
{
  terminal_manager::enableRawMode();

  if (terminal_manager::getWindowSize(&m_screenrows, &m_screencols) == -1)
    terminal_manager::die("getWindowSize");

  m_screenrows -= 2;
}

/*** row operations ***/

void Editor::convertRowCxToRx(EditorRow &erow)
{
  int rx = 0;
  for (int i = 0; i < m_cx; ++i)
  {
    if (erow.row[i] == '\t')
      rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
    rx++;
  }
  m_rx = rx;
}

void Editor::convertRowRxToCx(EditorRow &erow)
{
  int cur_rx = 0, cx = 0;
  for (cx = 0; cx < static_cast<int>(erow.row.size()); ++cx)
  {
    if (erow.row[cx] == '\t')
      cur_rx += (KILO_TAB_STOP - 1) - (cur_rx % KILO_TAB_STOP);

    cur_rx++;

    if (cur_rx > m_rx)
      m_cx = cx;
  }

  m_cx = cx;
}

void Editor::updateRow(EditorRow &erow)
{
  if (erow.row.empty())
    return;

  std::string render = "";
  for (int i = 0; i < static_cast<int>(erow.row.size()); ++i)
  {
    if (erow.row[i] == '\t')
    {
      render += " ";
      while (render.size() % KILO_TAB_STOP != 0)
        render += " ";
    }
    else
    {
      render += erow.row[i];
    }
  }

  erow.rendered = render;
}

bool Editor::insertRow(int yindex, const std::string &s)
{
  if (yindex < 0 || yindex > static_cast<int>(m_rows.size()))
    return false;

  m_rows.insert(m_rows.begin() + yindex, EditorRow());
  m_rows[yindex].row = s;
  updateRow(m_rows[yindex]);
  m_dirty++;

  return true;
}

void Editor::deleteRow(int yindex)
{
  if (yindex < 0 || yindex >= static_cast<int>(m_rows.size()))
    return;

  m_rows.erase(m_rows.begin() + yindex);
  m_dirty++;
}

void Editor::insertCharIntoRow(EditorRow &erow, int yindex, int c)
{
  if (yindex < 0 || yindex > static_cast<int>(erow.row.size()))
    yindex = erow.row.size();

  erow.row.insert(yindex, 1, c);
  updateRow(erow);
  m_dirty++;
}

void Editor::appendStringToRow(EditorRow &erow, const std::string &s)
{
  erow.row += s;
  updateRow(erow);
  m_dirty++;
}

void Editor::deleteCharFromRow(EditorRow &erow, int xindex)
{
  if (xindex < 0 || xindex >= static_cast<int>(erow.row.size()))
    return;

  erow.row.erase(xindex, 1);
  updateRow(erow);
  m_dirty++;
}

/*** editor operations ***/

void Editor::insertChar(int c)
{
  if (m_cy == static_cast<int>(m_rows.size()))
    insertRow(m_cy, "");

  insertCharIntoRow(m_rows[m_cy], m_cx, c);
  m_cx++;
}

void Editor::insertNewline()
{
  if (m_cx == 0)
    insertRow(m_cy, "");
  else
  {
    // Note:
    // when inserting newline with insertRow(), the vector of m_row will reallocate its memory,
    // then the reference to m_rows will become invalid.
    // In order to avoid this, we need to reserve memory accounting for the new row.
    m_rows.reserve(m_rows.size() + 1);

    auto &erow = m_rows[m_cy];
    if (insertRow(m_cy + 1, erow.row.substr(m_cx)))
    {
      erow.row = erow.row.substr(0, m_cx);
      updateRow(erow);
    }
  }

  m_cy++;
  m_cx = 0;
}

void Editor::deleteChar()
{
  if (m_cy == static_cast<int>(m_rows.size()))
    return;

  if (m_cx == 0 && m_cy == 0)
    return;

  auto &row = m_rows[m_cy];
  if (m_cx > 0)
  {
    deleteCharFromRow(row, m_cx - 1);
    m_cx--;
  }
  else
  {
    m_cx = static_cast<int>(m_rows[m_cy - 1].row.size());
    appendStringToRow(m_rows[m_cy - 1], row.row);
    deleteRow(m_cy);
    m_cy--;
  }
}

/*** file i/o ***/

std::string Editor::convertRowsToString()
{
  std::string s{};
  for (const auto &row : m_rows)
    s += row.row + '\n';

  return s;
}

void Editor::open(const char *filename)
{
  m_filename = std::string(filename);

  std::ifstream file(filename);
  if (!file.is_open())
    terminal_manager::die("fopen");

  std::string line;
  while (std::getline(file, line))
  {
    while (line.size() && (line[line.size() - 1] == '\n' || line[line.size() - 1] == '\r'))
      line.erase(line.size() - 1);

    insertRow(m_rows.size(), line);
  }

  file.close();
  m_dirty = 0;
}

void Editor::save()
{
  if (m_filename.empty())
  {
    m_filename = fromPrompt("Save as: %s (ESC to cancel)");
    if (m_filename.empty())
    {
      setStatusMessage("Save aborted");
      return;
    }
  }

  std::ofstream file(m_filename);
  if (!file.is_open())
  {
    setStatusMessage("Can't save! I/O error: %s", std::strerror(errno));
    terminal_manager::die("fopen");
  }

  std::string s = convertRowsToString();
  file << s;
  file.close();
  m_dirty = 0;
  setStatusMessage("%d bytes written to disk", s.size());
}

/*** find ***/

void Editor::findCallback(std::string &query, int key)
{
  static int last_match = -1;
  static int direction = 1;

  if (key == '\r' || key == '\x1b')
  {
    last_match = -1;
    direction = 1;
    return;
  }
  else if (key == static_cast<int>(EditorKey::ARROW_RIGHT) ||
           key == static_cast<int>(EditorKey::ARROW_DOWN))
  {
    direction = 1;
  }
  else if (key == static_cast<int>(EditorKey::ARROW_LEFT) ||
           key == static_cast<int>(EditorKey::ARROW_UP))
  {
    direction = -1;
  }
  else
  {
    last_match = -1;
    direction = 1;
  }

  if (last_match == -1)
    direction = 1;

  int current = last_match;

  for (const auto &row : m_rows)
  {
    current += direction;
    if (current == -1)
      current = m_rows.size() - 1;
    else if (current == static_cast<int>(m_rows.size()))
      current = 0;

    const auto &render = row.rendered;
    auto found = render.find(query);
    if (found != std::string::npos)
    {
      last_match = current;
      m_cy = current;
      m_rx = found;
      m_cx = 0;
      return;
    }
  }
}

void Editor::find()
{
  int saved_cx = m_cx;
  int saved_cy = m_cy;
  int saved_coloff = m_coloff;
  int saved_rowoff = m_rowoff;

  std::string query = fromPrompt("Search: %s (Use ESC/Arrows/Enter)", [this](std::string &query, int key)
                                 { findCallback(query, key); });

  if (query.empty())
  {
    m_cx = saved_cx;
    m_cy = saved_cy;
    m_coloff = saved_coloff;
    m_rowoff = saved_rowoff;
  }
}

/*** output ***/

void Editor::scroll()
{
  m_rx = m_cx;
  if (m_cy < static_cast<int>(m_rows.size()))
    convertRowCxToRx(m_rows[m_cy]);

  if (m_cy < m_rowoff)
    m_rowoff = m_cy;

  if (m_cy >= m_rowoff + m_screenrows)
    m_rowoff = m_cy - m_screenrows + 1;

  if (m_rx < m_coloff)
    m_coloff = m_rx;

  if (m_rx >= m_coloff + m_screencols)
    m_coloff = m_rx - m_screencols + 1;
}

void Editor::drawRows(std::string &s)
{
  for (int y = 0; y < m_screenrows; ++y)
  {
    auto filerow = y + m_rowoff;
    if (filerow >= static_cast<int>(m_rows.size()))
    {
      if (m_rows.empty() && y == m_screenrows / 3)
      {
        std::string welcome = "Kilo++ editor -- version " + std::string(KILO_VERSION);
        if (welcome.size() > static_cast<std::size_t>(m_screencols))
          welcome.resize(m_screencols);

        auto padding = (static_cast<std::size_t>(m_screencols) - welcome.size()) / 2;
        if (padding)
        {
          s += "~";
          padding--;
        }

        while (padding--)
          s += " ";

        s += welcome;
      }
      else
      {
        s += "~";
      }
    }
    else
    {
      int len = static_cast<int>(m_rows[filerow].rendered.size()) - m_coloff;
      if (len < 0)
        len = 0;

      s += m_rows[filerow].rendered.substr(m_coloff, std::min(len, m_screencols));
    }

    s += "\x1b[K\r\n";
  }
}

void Editor::drawStatusBar(std::string &s)
{
  s += "\x1b[7m";

  std::stringstream ss, rss;
  ss << (m_filename.empty()
             ? "[No Name]"
             : m_filename.substr(0, std::min(static_cast<int>(m_filename.size()), FILENAME_DISPLAY_LEN)))
     << " - "
     << m_rows.size()
     << " lines"
     << (m_dirty ? "(modified)" : "");
  int len = std::min(static_cast<int>(ss.str().size()), m_screencols);

  rss << m_cy + 1 << "/" << m_rows.size();
  int rlen = rss.str().size();

  s += ss.str().substr(0, len);
  while (len < m_screencols)
  {
    if (m_screencols - len == rlen)
    {
      s += rss.str();
      break;
    }
    s += " ";
    len++;
  }

  s += "\x1b[m\r\n";
}

void Editor::drawMessageBar(std::string &s)
{
  s += "\x1b[K";
  int msglen = static_cast<int>(m_statusmsg.size());
  if (msglen > m_screencols)
    msglen = m_screencols;

  if (msglen && time(NULL) - m_statusmsg_time < 5)
    s += m_statusmsg.substr(0, msglen);
}

void Editor::refreshScreen()
{
  scroll();

  std::string s;

  s += "\x1b[?25l";
  s += "\x1b[H";

  drawRows(s);
  drawStatusBar(s);
  drawMessageBar(s);

  std::stringstream ss;
  ss << "\x1b[" << (m_cy - m_rowoff) + 1 << ";" << (m_rx - m_coloff) + 1 << "H" << "\x1b[?25h";
  s += ss.str();

  write(STDOUT_FILENO, s.c_str(), s.size());
}

void Editor::setStatusMessage(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  int size;

  if ((size = std::vsnprintf(nullptr, 0, fmt, ap) + 1) < 0)
  {
    va_end(ap);
    return;
  }

  if (size <= 1)
  {
    va_end(ap);
    return;
  }

  std::vector<char> buf(size);
  va_start(ap, fmt);
  vsnprintf(buf.data(), m_screencols, fmt, ap);
  va_end(ap);

  m_statusmsg = std::string(buf.data());
  m_statusmsg_time = time(NULL);
}

/*** input ***/

std::string Editor::fromPrompt(std::string prompt, std::function<void(std::string &, int)> callback)
{
  std::string s = "\0";
  while (1)
  {
    setStatusMessage(prompt.c_str(), s.c_str());
    refreshScreen();

    int c = terminal_manager::readKey();
    if (c == static_cast<int>(EditorKey::DEL_KEY) ||
        c == CTRL_KEY('h') ||
        c == static_cast<int>(EditorKey::BACKSPACE))
    {
      if (s.size())
        s.pop_back();
    }
    else if (c == '\x1b')
    {
      setStatusMessage("");
      if (callback)
        callback(s, c);
      return "";
    }
    else if (c == '\r')
    {
      if (s.size())
      {
        setStatusMessage("");
        if (callback)
          callback(s, c);
        return s;
      }
    }
    else if (!iscntrl(c) && c < 128)
    {
      s += c;
    }

    if (callback)
      callback(s, c);
  }
}

void Editor::moveCursor(int key)
{
  switch (key)
  {
  case static_cast<int>(EditorKey::ARROW_UP):
    if (m_cy > 0)
      m_cy--;
    break;
  case static_cast<int>(EditorKey::ARROW_DOWN):
    if (m_cy < static_cast<int>(m_rows.size()))
      m_cy++;
    break;
  case static_cast<int>(EditorKey::ARROW_LEFT):
    if (m_cx > 0)
    {
      m_cx--;
    }
    else if (m_cy > 0)
    {
      m_cy--;
      m_cx = static_cast<int>(m_rows[m_cy].row.size());
    }
    break;
  case static_cast<int>(EditorKey::ARROW_RIGHT):
    if (m_cx < static_cast<int>(m_rows[m_cy].row.size()))
    {
      m_cx++;
    }
    else if (m_cy < static_cast<int>(m_rows.size()))
    {
      m_cy++;
      m_cx = 0;
    }
    break;
  }

  if (m_cx > static_cast<int>(m_rows[m_cy].row.size()))
    m_cx = static_cast<int>(m_rows[m_cy].row.size());
}

void Editor::processKeypress()
{
  static int quit_times = KILO_QUIT_TIMES;
  int c = terminal_manager::readKey();

  switch (c)
  {
  case '\r':
    insertNewline();
    break;
  case CTRL_KEY('q'):
    if (m_dirty && quit_times > 0)
    {
      setStatusMessage(
          "WARNING!!! File has unsaved changes. Press Ctrl-Q %d more times to quit.",
          quit_times);
      quit_times--;
      return;
    }
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;
  case CTRL_KEY('s'):
    save();
    break;
  case static_cast<int>(EditorKey::HOME_KEY):
    m_cx = 0;
    break;
  case static_cast<int>(EditorKey::END_KEY):
    if (m_cy < static_cast<int>(m_rows.size()))
      m_cx = m_rows[m_cy].row.size();
    break;
  case CTRL_KEY('f'):
    find();
    break;
  case static_cast<int>(EditorKey::BACKSPACE):
  case CTRL_KEY('h'):
  case static_cast<int>(EditorKey::DEL_KEY):
    if (c == static_cast<int>(EditorKey::DEL_KEY))
      moveCursor(static_cast<int>(EditorKey::ARROW_RIGHT));
    deleteChar();
    break;
  case static_cast<int>(EditorKey::PAGE_UP):
  case static_cast<int>(EditorKey::PAGE_DOWN):
  {
    if (c == static_cast<int>(EditorKey::PAGE_UP))
    {
      m_cy = m_rowoff;
    }
    else
    {
      m_cy = m_rowoff + m_screenrows - 1;
      if (m_cy > static_cast<int>(m_rows.size()))
        m_cy = m_rows.size();
    }

    int times = m_screenrows;
    while (times--)
    {
      moveCursor(
          c == static_cast<int>(EditorKey::PAGE_UP)
              ? static_cast<int>(EditorKey::ARROW_UP)
              : static_cast<int>(EditorKey::ARROW_DOWN));
    }
  }
  break;
  case static_cast<int>(EditorKey::ARROW_UP):
  case static_cast<int>(EditorKey::ARROW_DOWN):
  case static_cast<int>(EditorKey::ARROW_LEFT):
  case static_cast<int>(EditorKey::ARROW_RIGHT):
    moveCursor(c);
    break;
  case CTRL_KEY('l'):
  case '\x1b':
    break;
  default:
    insertChar(c);
    break;
  }

  quit_times = KILO_QUIT_TIMES;
}

void Editor::run(int argc, char *argv[])
{
  if (argc >= 2)
    open(argv[1]);

  setStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

  while (1)
  {
    refreshScreen();
    processKeypress();
  }
}

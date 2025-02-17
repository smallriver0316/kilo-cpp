/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE

#include "kilo++/Editor.hpp"
#include "kilo++/EditorUtils.hpp"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstring>
#include <filesystem>
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

#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)

/*** filetypes ***/

const std::vector<std::string_view> C_HL_EXTENSIONS = {".c", ".h", ".cpp", ".hpp"};
const std::vector<std::string_view> C_HL_KEYWORDS = {
    "switch", "if", "while", "for", "break", "continue", "return", "else", "struct", "union", "typedef", "static", "const",
    "enum", "class", "case", "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|", "auto|", "void|"};

const EditorSyntax HLDB[] = {
    {"c",
     C_HL_EXTENSIONS,
     C_HL_KEYWORDS,
     "//", "/*", "*/",
     HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS},
};

/*** constructor ***/

Editor::Editor()
{
  terminal_manager::enableRawMode();

  if (terminal_manager::getWindowSize(&m_screenrows, &m_screencols) == -1)
    terminal_manager::die("getWindowSize");

  m_screenrows -= 2;
}

/*** syntax highlighting ***/

bool isSeparator(int c)
{
  return std::isspace(c) || c == '\0' || std::strchr(",.()+-/*=~%<>[];", c) != NULL;
}

bool isSLCommentStarted(EditorRow &erow, const std::string &comment_start_kw, std::size_t pos)
{
  if (!erow.rendered.compare(pos, comment_start_kw.size(), comment_start_kw))
  {
    std::fill(
        erow.hl.begin() + pos,
        erow.hl.end(),
        EditorHighlight::COMMENT);
    return true;
  }
  return false;
}

bool isMLCommentStarted(EditorRow &erow, const std::string &comment_start_kw, std::size_t pos)
{
  if (!erow.rendered.compare(pos, comment_start_kw.size(), comment_start_kw))
  {
    std::fill(
        erow.hl.begin() + pos,
        erow.hl.begin() + pos + comment_start_kw.size(),
        EditorHighlight::ML_COMMENT);
    return true;
  }
  return false;
}

bool isMLCommentEnded(EditorRow &erow, const std::string &comment_end_kw, std::size_t pos)
{
  if (!erow.rendered.compare(pos, comment_end_kw.size(), comment_end_kw))
  {
    std::fill(
        erow.hl.begin() + pos,
        erow.hl.begin() + pos + comment_end_kw.size(),
        EditorHighlight::ML_COMMENT);
    return true;
  }
  return false;
}

void Editor::updateSyntax(EditorRow &erow)
{
  erow.hl.resize(erow.rendered.size(), EditorHighlight::NORMAL);

  if (!m_syntax)
    return;

  const auto &keywords = m_syntax->keywords;
  const auto &scs = m_syntax->singleline_comment_start;
  const auto &mcs = m_syntax->multiline_comment_start;
  const auto &mce = m_syntax->multiline_comment_end;

  bool prev_sep = true;
  int in_string = 0;
  bool in_comment = (erow.idx > 0 && m_rows[erow.idx - 1].hl_open_comment);

  std::size_t i = 0;
  while (i < erow.rendered.size())
  {
    const auto c = erow.rendered[i];
    EditorHighlight prev_hl = i > 0 ? erow.hl[i - 1] : EditorHighlight::NORMAL;

    if (!scs.empty() && !in_string && !in_comment)
    {
      if (isSLCommentStarted(erow, scs, i))
        break;
    }

    if (!mcs.empty() && !mce.empty() && !in_string)
    {
      if (in_comment)
      {
        erow.hl[i] = EditorHighlight::ML_COMMENT;
        if (isMLCommentEnded(erow, mce, i))
        {
          i += mce.size();
          in_comment = false;
          prev_sep = true;
        }
        else
        {
          i++;
        }
        continue;
      }
      else if (isMLCommentStarted(erow, mcs, i))
      {
        i += mcs.size();
        in_comment = true;
        continue;
      }
    }

    if (m_syntax->flags & HL_HIGHLIGHT_STRINGS)
    {
      if (in_string)
      {
        erow.hl[i] = EditorHighlight::STRING;

        if (c == '\\' && i + 1 < erow.rendered.size())
        {
          erow.hl[i + 1] = EditorHighlight::STRING;
          i += 2;
          continue;
        }

        if (c == in_string)
          in_string = 0;

        prev_sep = true;
        i++;
        continue;
      }
      else
      {
        if (c == '"' || c == '\'')
        {
          in_string = c;
          erow.hl[i] = EditorHighlight::STRING;
          i++;
          continue;
        }
      }
    }

    if (m_syntax->flags & HL_HIGHLIGHT_NUMBERS)
    {
      if ((std::isdigit(c) && (prev_sep || prev_hl == EditorHighlight::NUMBER)) ||
          (c == '.' && prev_hl == EditorHighlight::NUMBER))
      {
        erow.hl[i] = EditorHighlight::NUMBER;
        prev_sep = false;
        i++;
        continue;
      }
    }

    if (prev_sep)
    {
      auto kwitr = keywords.begin();
      for (; kwitr != keywords.end(); ++kwitr)
      {
        auto kw = *kwitr;
        int klen = kw.size();
        bool is_kw2 = kw[klen - 1] == '|';
        if (is_kw2)
          kw = kw.substr(0, --klen);

        if (!erow.rendered.compare(i, klen, kw) &&
            isSeparator(erow.rendered[i + klen]))
        {
          std::fill(
              erow.hl.begin() + i,
              erow.hl.begin() + i + klen,
              is_kw2 ? EditorHighlight::KEYWORD2 : EditorHighlight::KEYWORD1);
          i += klen;
          break;
        }
      }
      if (kwitr != keywords.end())
      {
        prev_sep = false;
        continue;
      }
    }

    prev_sep = isSeparator(c);
    i++;
  }

  bool is_changed = erow.hl_open_comment != in_comment;
  erow.hl_open_comment = in_comment;
  if (is_changed && erow.idx + 1 < static_cast<int>(m_rows.size()))
    updateSyntax(m_rows[erow.idx + 1]);
}

int Editor::convertSyntaxToColor(EditorHighlight hl)
{
  switch (hl)
  {
  case EditorHighlight::COMMENT:
  case EditorHighlight::ML_COMMENT:
    return 36;
  case EditorHighlight::KEYWORD1:
    return 33;
  case EditorHighlight::KEYWORD2:
    return 32;
  case EditorHighlight::STRING:
    return 35;
  case EditorHighlight::NUMBER:
    return 31;
  case EditorHighlight::MATCH:
    return 34;
  }

  // default: NORMAL
  return 37;
}

void Editor::selectSyntaxHighlight()
{
  m_syntax = nullptr;
  if (m_filename.empty())
    return;

  const auto ext = m_filename.rfind('.');

  for (const auto &syntax : HLDB)
  {
    for (const auto &fm : syntax.filematch)
    {
      int is_ext = (fm[0] == '.');
      if ((is_ext && ext != std::string::npos && !m_filename.compare(ext, fm.size(), fm)) ||
          (!is_ext && m_filename.find(fm) != std::string::npos))
      {
        m_syntax = std::make_shared<EditorSyntax>(syntax);

        for (auto &row : m_rows)
          updateSyntax(row);

        return;
      }
    }
  }
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

int Editor::convertRowRxToCx(EditorRow &erow, int rx)
{
  int cur_rx = 0, cx = 0;
  for (cx = 0; cx < static_cast<int>(erow.row.size()); ++cx)
  {
    if (erow.row[cx] == '\t')
      cur_rx += (KILO_TAB_STOP - 1) - (cur_rx % KILO_TAB_STOP);

    cur_rx++;

    if (cur_rx > rx)
      return cx;
  }

  return cx;
}

void Editor::updateRow(EditorRow &erow)
{
  std::string render = "";
  for (const auto &c : erow.row)
  {
    if (c == '\t')
    {
      render += " ";
      while (render.size() % KILO_TAB_STOP != 0)
        render += " ";
    }
    else
    {
      render += c;
    }
  }

  render += "\0";
  erow.rendered = render;
  updateSyntax(erow);
}

bool Editor::insertRow(int yindex, const std::string &s)
{
  if (yindex < 0 || yindex > static_cast<int>(m_rows.size()))
    return false;

  m_rows.insert(m_rows.begin() + yindex, EditorRow(yindex));
  m_rows[yindex].row = s;
  m_rows[yindex].idx = yindex;
  std::for_each(m_rows.begin() + yindex + 1, m_rows.end(), [](auto &erow)
                { erow.idx++; });

  updateRow(m_rows[yindex]);
  m_dirty++;

  return true;
}

void Editor::deleteRow(int yindex)
{
  if (yindex < 0 || yindex >= static_cast<int>(m_rows.size()))
    return;

  m_rows.erase(m_rows.begin() + yindex);
  std::for_each(m_rows.begin() + yindex, m_rows.end(), [](auto &erow)
                { erow.idx--; });

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

  auto &erow = m_rows[m_cy];
  if (m_cx > 0)
  {
    deleteCharFromRow(erow, m_cx - 1);
    m_cx--;
  }
  else
  {
    m_cx = static_cast<int>(m_rows[m_cy - 1].row.size());
    appendStringToRow(m_rows[m_cy - 1], erow.row);
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
  if (!std::filesystem::exists(filename))
  {
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
    selectSyntaxHighlight();
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

  static int saved_hl_line;
  static std::vector<EditorHighlight> saved_hl;

  if (!saved_hl.empty())
  {
    std::copy(saved_hl.begin(), saved_hl.end(), m_rows[saved_hl_line].hl.begin());
    saved_hl.clear();
  }

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

  for (auto &row : m_rows)
  {
    current += direction;
    if (current == -1)
      current = m_rows.size() - 1;
    else if (current == static_cast<int>(m_rows.size()))
      current = 0;

    const auto &render = row.rendered;
    auto match_start_offset = render.find(query);
    if (match_start_offset != std::string::npos)
    {
      last_match = current;
      m_cy = current;
      m_cx = convertRowRxToCx(m_rows[current], static_cast<int>(match_start_offset));
      m_rowoff = m_rows.size();

      saved_hl_line = current;
      saved_hl.reserve(row.rendered.size());
      std::copy(row.hl.begin(), row.hl.end(), std::back_inserter(saved_hl));

      const auto match_end_offset = row.hl.begin() + match_start_offset + query.size() > row.hl.end()
                                        ? row.hl.end() - row.hl.begin()
                                        : match_start_offset + query.size();
      std::fill(row.hl.begin() + match_start_offset,
                row.hl.begin() + match_end_offset,
                EditorHighlight::MATCH);
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

      int current_color = -1;
      for (int i = 0; i < len; ++i)
      {
        const auto &c = m_rows[filerow].rendered[m_coloff + i];
        const auto &hl = m_rows[filerow].hl[m_coloff + i];
        if (std::iscntrl(c))
        {
          char sym = c <= 26 ? '@' + c : '?';
          s += "\x1b[7m";
          s += std::string(1, sym);
          s += "\x1b[m";
          if (current_color != -1)
            s += "\x1b[" + std::to_string(current_color) + "m";
        }
        else if (hl == EditorHighlight::NORMAL)
        {
          if (current_color != -1)
          {
            s += "\x1b[39m";
            current_color = -1;
          }
          s += std::string(1, c);
        }
        else
        {
          const auto color = convertSyntaxToColor(hl);
          if (current_color != color)
          {
            current_color = color;
            std::vector<char> buf(16);
            snprintf(buf.data(), buf.size(), "\x1b[%dm", color);
            s += buf.data();
          }
          s += std::string(1, c);
        }
      }
      s += "\x1b[39m";
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

  rss << (m_syntax ? m_syntax->filetype : "no ft")
      << " | "
      << m_cy + 1
      << "/"
      << m_rows.size();
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

std::string Editor::fromPrompt(
    std::string prompt, std::function<void(std::string &, int)> callback)
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
    if (m_cy < static_cast<int>(m_rows.size()) - 1)
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
    else if (m_cy < static_cast<int>(m_rows.size()) - 1)
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

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <time.h>
#include <vector>

/*** data **/

enum class EditorHighlight : unsigned char
{
  NORMAL = 0,
  COMMENT,
  KEYWORD1,
  KEYWORD2,
  STRING,
  NUMBER,
  MATCH
};

struct EditorSyntax
{
  std::string filetype;
  std::vector<std::string_view> filematch;
  std::vector<std::string_view> keywords;
  std::string singleline_comment_start;
  int32_t flags;
};

struct EditorRow
{
  EditorRow() : row({}), rendered({}), hl({}) {};

  std::string row;
  std::string rendered;
  std::vector<EditorHighlight> hl;
};

/*** class definition */

class Editor
{
public:
  Editor();

  void run(int argc, char *argv[]);

  /*** syntax highlighting ***/

  void updateSyntax(EditorRow &erow);

  int convertSyntaxToColor(EditorHighlight hl);

  void selectSyntaxHighlight();

  /*** row operations ***/

  void convertRowCxToRx(EditorRow &erow);

  int convertRowRxToCx(EditorRow &erow, int rx);

  void updateRow(EditorRow &erow);

  bool insertRow(int yindex, const std::string &s);

  void deleteRow(int yindex);

  void insertCharIntoRow(EditorRow &erow, int yindex, int c);

  void appendStringToRow(EditorRow &erow, const std::string &s);

  void deleteCharFromRow(EditorRow &erow, int xindex);

  /*** editor operations ***/

  void insertChar(int c);

  void insertNewline();

  void deleteChar();

  /*** file i/o ***/

  std::string convertRowsToString();

  void open(const char *filename);

  void save();

  /*** find ***/

  void findCallback(std::string &query, int key);

  void find();

  /*** output ***/

  void scroll();

  void drawRows(std::string &s);

  void drawStatusBar(std::string &s);

  void drawMessageBar(std::string &s);

  void refreshScreen();

  void setStatusMessage(const char *fmt, ...);

  /*** input ***/

  std::string fromPrompt(std::string prompt, std::function<void(std::string &, int)> callback = nullptr);

  void moveCursor(int key);

  void processKeypress();

private:
  /*** members ***/

  std::vector<EditorRow> m_rows;
  int m_cx = 0, m_cy = 0;
  int m_rx = 0;
  int m_rowoff = 0, m_coloff = 0;
  int m_screenrows, m_screencols;
  uint8_t m_dirty = 0;
  std::string m_filename = "";
  std::string m_statusmsg = "\0";
  time_t m_statusmsg_time = 0;
  std::optional<EditorSyntax> m_syntax = std::nullopt;
};

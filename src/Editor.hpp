#pragma once

#include <functional>
#include <string>
#include <time.h>
#include <vector>

enum class EditorHighlight : unsigned char
{
  NORMAL = 0,
  NUMBER,
};

struct EditorRow
{
  EditorRow() : row({}), rendered({}), hl({}) {};

  std::string row;
  std::string rendered;
  std::vector<unsigned char> hl;
};

class Editor
{
public:
  Editor();

  void open(const char *filename);

  void refreshScreen();

  void setStatusMessage(const char *fmt, ...);

  void processKeypress();

  void run(int argc, char *argv[]);

private:
  /*** row operations ***/
  void convertRowCxToRx(EditorRow &erow);

  void convertRowRxToCx(EditorRow &erow);

  void updateRow(EditorRow &erow);

  void insertRow(int at, const std::string &s);

  void deleteRow(int at);

  void insertCharIntoRow(EditorRow &erow, int at, int c);

  void appendStringToRow(EditorRow &erow, const std::string &s);

  void deleteCharFromRow(EditorRow &erow, int at);

  /*** editor operations ***/
  void insertChar(int c);

  void insertNewline();

  void deleteChar();

  /*** file i/o ***/
  std::string convertRowsToString();

  void save();

  /*** find ***/
  void findCallback(std::string &query, int key);

  void find();

  /*** output ***/
  void scroll();

  void drawRows(std::string &s);

  void drawStatusBar(std::string &s);

  void drawMessageBar(std::string &s);

  /*** input ***/
  std::string fromPrompt(std::string prompt, std::function<void(std::string &, int)> callback = nullptr);

  void moveCursor(int key);

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
};

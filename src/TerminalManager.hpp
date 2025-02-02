#pragma once

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

class TerminalManager
{
public:
  static void die(const char *s);

  static void disableRawMode();

  static void enableRawMode();

  static int getWindowSize(int *rows, int *cols);

  static int readKey();

private:
  static int getCursorPosition(int *rows, int *cols);
};

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

namespace terminal_manager
{
  void die(const char *s);

  void disableRawMode();

  void enableRawMode();

  int getWindowSize(int *rows, int *cols);

  int readKey();
}

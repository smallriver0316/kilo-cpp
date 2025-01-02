#include <cctype>
#include <iostream>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios orig_termios;

void disableRawMode()
{
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode()
{
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disableRawMode);

  struct termios raw = orig_termios;

  tcgetattr(STDIN_FILENO, &raw);
  raw.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main()
{
  enableRawMode();

  char c;
  while (std::cin.get(c) && c != 'q')
  {
    if (iscntrl(c))
    {
      std::cout << (int)c << std::endl;
    }
    else
    {
      std::cout << (int)c << " ('" << c << "')" << std::endl;
    }
  }

  return 0;
}

#define WIN32_LEAN_AND_MEAN
#include "TerminalInput.hpp"
#include <conio.h>
#include <windows.h>

class TerminalInputWin32 : public TerminalInput {
public:
  explicit TerminalInputWin32() {
    // Windows setup
    hStdin = GetStdHandle(STD_INPUT_HANDLE);

    // Get the current console mode
    GetConsoleMode(hStdin, &fdwOldMode);

    // Save the old mode to restore later
    fdwMode = fdwOldMode;

    // Disable line input and echo input
    fdwMode &= ~ENABLE_LINE_INPUT;
    fdwMode &= ~ENABLE_ECHO_INPUT;
    fdwMode &= ~ENABLE_PROCESSED_INPUT;

    // Set the new mode
    SetConsoleMode(hStdin, fdwMode);
  }

  ~TerminalInputWin32() override {
    // Restore old console mode
    SetConsoleMode(hStdin, fdwOldMode);
  }

  // Get a key press, handling special sequences like arrow keys
  int getKey() const override {
    // Windows implementation
    int ch = _getch();

    // Check if it's a special key (like arrow keys)
    if (ch == 0 || ch == 224) {
      // Get the next character to determine which special key
      switch (_getch()) {
      case 72:
        return 1; // Up arrow
      case 80:
        return 2; // Down arrow
      case 77:
        return 3; // Right arrow
      case 75:
        return 4; // Left arrow
      default:
        return 0; // Unknown special key
      }
    }

    return ch; // Regular character
  }

private:
  HANDLE hStdin;
  DWORD fdwMode, fdwOldMode;
};

std::unique_ptr<TerminalInput> create_terminal_input() {
  return std::make_unique<TerminalInputWin32>();
}

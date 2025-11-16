#include "TerminalInput.hpp"
#include <memory>
#include <termios.h>

class TerminalInputUnix : public TerminalInput {
public:
  explicit TerminalInputUnix() {
    // Get terminal settings
    tcgetattr(STDIN_FILENO, &old_tio);

    // Copy old settings to new settings
    new_tio = old_tio;

    // Disable canonical mode and echo
    new_tio.c_lflag &= ~(ICANON | ECHO);

    // Apply new settings
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
  }

  ~TerminalInputUnix() override {
    // Restore old settings
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
  }

  // Get a key press, handling special sequences like arrow keys
  int getKey() const override {
    int ch = getchar();

    // Check if it's an escape sequence (like arrow keys)
    if (ch == '\033') {
      // Check the next character
      if (getchar() == '[') {
        // Get the final character of the sequence
        switch (getchar()) {
        case 'A':
          return 1; // Up arrow
        case 'B':
          return 2; // Down arrow
        case 'C':
          return 3; // Right arrow
        case 'D':
          return 4; // Left arrow
        default:
          return 0; // Unknown sequence
        }
      }
    }

    return ch; // Regular character
  }

private:
  struct termios old_tio, new_tio;
};

std::unique_ptr<TerminalInput> create_terminal_input() {
  return std::make_unique<TerminalInputUnix>();
}

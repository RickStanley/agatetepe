#include "TerminalInput.hpp"
#include <fcntl.h>
#include <memory>
#include <termios.h>
#include <thread>

class TerminalInputUnix : public TerminalInput {
public:
  explicit TerminalInputUnix() {
    // Open the controlling terminal for interactive input
    // This works even if stdin is redirected.
    m_tty_fd = open("/dev/tty", O_RDONLY);
    if (m_tty_fd == -1) {
      // Fallback for systems without /dev/tty or if we can't open it
      // We'll just have to operate non-interactively.
      m_is_interactive = false;
      return;
    }

    // Check if the opened file is actually a terminal
    if (!isatty(m_tty_fd)) {
      m_is_interactive = false;
      close(m_tty_fd);
      m_tty_fd = -1;
      return;
    }

    m_is_interactive = true;

    // Get terminal settings using the file descriptor
    tcgetattr(m_tty_fd, &m_old_tio);

    // Copy old settings to new settings
    m_new_tio = m_old_tio;

    // Disable canonical mode and echo
    m_new_tio.c_lflag &= ~(ICANON | ECHO);

    // Apply new settings
    tcsetattr(m_tty_fd, TCSANOW, &m_new_tio);
  }

  ~TerminalInputUnix() override {
    // Restore old settings and close the file descriptor only if we were
    // interactive
    if (m_is_interactive && m_tty_fd != -1) {
      tcsetattr(m_tty_fd, TCSANOW, &m_old_tio);
      close(m_tty_fd);
    }
  }

  int getKey() const override {
    if (!m_is_interactive) {
      // If not interactive, we can't get keys. Return a quit command
      // or simply sleep to prevent a tight loop.
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      return 'q';
    }

    // Read a single character from the TTY device
    char ch;
    if (read(m_tty_fd, &ch, 1) <= 0) {
      return 0; // Error or EOF
    }

    // Check if it's an escape sequence (like arrow keys)
    if (ch == '\033') {
      char seq[2];
      if (read(m_tty_fd, &seq[0], 1) <= 0)
        return ch;
      if (read(m_tty_fd, &seq[1], 1) <= 0)
        return ch;

      if (seq[0] == '[') {
        switch (seq[1]) {
        case 'A':
          return 1; // Up arrow
        case 'B':
          return 2; // Down arrow
        case 'C':
          return 3; // Right arrow
        case 'D':
          return 4; // Left arrow
        }
      }
    }

    return ch; // Regular character
  }

private:
  struct termios m_old_tio, m_new_tio;
  int m_tty_fd = -1;
  bool m_is_interactive = false;
};

std::unique_ptr<TerminalInput> create_terminal_input() {
  return std::make_unique<TerminalInputUnix>();
}

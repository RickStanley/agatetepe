#define WIN32_LEAN_AND_MEAN
#include "TerminalInput.hpp"
#include <conio.h>
#include <fcntl.h> // For _O_TEXT
#include <io.h>    // For _isatty
#include <windows.h>

class TerminalInputWin32 : public TerminalInput {
public:
  explicit TerminalInputWin32() {
    // First, check if the standard input is actually a console.
    // This is a good indicator of whether we're in an interactive session.
    // _isatty checks if a file descriptor is associated with a character device
    // (like a console).
    m_is_interactive = _isatty(_fileno(stdin));

    if (!m_is_interactive) {
      // If stdin is not a console (e.g., redirected from a file),
      // we need to explicitly open a handle to the console.
      // "CONIN$" is a special reserved file name for the console input buffer.
      hStdin = CreateFile("CONIN$", GENERIC_READ, FILE_SHARE_READ, NULL,
                          OPEN_EXISTING, 0, NULL);
      if (hStdin == INVALID_HANDLE_VALUE) {
        // If we can't even open the console, we're truly non-interactive.
        m_is_interactive = false;
        return;
      }
    } else {
      // If stdin is already a console, just use its handle.
      hStdin = GetStdHandle(STD_INPUT_HANDLE);
    }

    // Now that we have a handle to the console, we can configure it.
    // Get the current console mode
    if (!GetConsoleMode(hStdin, &fdwOldMode)) {
      // If we can't get the mode, something is wrong.
      m_is_interactive = false;
      if (!m_is_interactive)
        CloseHandle(hStdin); // Close the handle we opened
      return;
    }

    // Save the old mode to restore later
    fdwMode = fdwOldMode;

    // Disable line input and echo input for immediate key presses
    fdwMode &= ~ENABLE_LINE_INPUT;
    fdwMode &= ~ENABLE_ECHO_INPUT;
    fdwMode &= ~ENABLE_PROCESSED_INPUT; // Disable CTRL+C handling etc.

    // Set the new mode
    SetConsoleMode(hStdin, fdwMode);
  }

  ~TerminalInputWin32() override {
    // Restore old console mode and close the handle if we opened it
    if (m_is_interactive) {
      SetConsoleMode(hStdin, fdwOldMode);
      // Only close the handle if we opened it with CreateFile("CONIN$", ...)
      if (!_isatty(_fileno(stdin))) {
        CloseHandle(hStdin);
      }
    }
  }

  int get_key() const override {
    if (!m_is_interactive) {
      // If not interactive, we can't get keys. Return a quit command
      // to prevent a tight loop.
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      return 'q';
    }

    // Wait for a key press event on the console
    INPUT_RECORD irInBuf[128];
    DWORD cNumRead;

    // Wait for an event
    WaitForSingleObject(hStdin, INFINITE);

    // Read the console input buffer
    ReadConsoleInput(hStdin, irInBuf, 128, &cNumRead);

    // Process the events
    for (DWORD i = 0; i < cNumRead; i++) {
      if (irInBuf[i].EventType == KEY_EVENT &&
          irInBuf[i].Event.KeyEvent.bKeyDown) {
        WORD keyCode = irInBuf[i].Event.KeyEvent.wVirtualKeyCode;
        CHAR ch = irInBuf[i].Event.KeyEvent.uChar.AsciiChar;

        // Map virtual key codes to our application's codes
        switch (keyCode) {
        case VK_UP:
          return 1;
        case VK_DOWN:
          return 2;
        case VK_RIGHT:
          return 3;
        case VK_LEFT:
          return 4;
        case VK_RETURN:
          return '\n';
        case 'Q':
          return 'q';
        case 'D':
          return 'd';
        default:
          if (ch != 0)
            return ch; // Return the ASCII character if it exists
          break;
        }
      }
    }

    // If no key press was processed, loop again (or return 0)
    return get_key(); // Recursive call to wait for a valid key
  }

private:
  HANDLE hStdin;
  DWORD fdwMode, fdwOldMode;
  bool m_is_interactive;
};

std::unique_ptr<TerminalInput> create_terminal_input() {
  return std::make_unique<TerminalInputWin32>();
}

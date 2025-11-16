#pragma once

#include <memory>

class TerminalInput {
public:
  virtual ~TerminalInput() = default;

  virtual int getKey() const = 0;
};

std::unique_ptr<TerminalInput> create_terminal_input();

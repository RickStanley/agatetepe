#pragma once

#include <cstddef>
#include <iterator>
#include <algorithm>
#include <memory>

class MmapReader {
public:
  virtual ~MmapReader() = default;

  // Raw mapped data access
  virtual const char* get_data() const = 0;
  virtual size_t get_size() const = 0;
  virtual bool is_open() const = 0;

  class LineIterator {
  public:
    using iterator_category = std::input_iterator_tag;
    using value_type = std::string_view;
    using difference_type = std::ptrdiff_t;
    using pointer = const std::string_view*;
    using reference = const std::string_view&;

    LineIterator(const char* ptr, const char* end) : current_pos(ptr), end_pos(end) {}

    // dereference operator: returns a string_view of the current line
    reference operator*() const {
      const char* newline_pos = std::find(current_pos, end_pos, '\n');
      cached_view = std::string_view(current_pos, newline_pos - current_pos);
      return cached_view;
    }

    // pre-increment operator: moves to the next line
    LineIterator &operator++() {
      const char * newline_pos = std::find(current_pos, end_pos, '\n');
      current_pos = (newline_pos == end_pos) ? end_pos : newline_pos + 1;
      return *this;
    }

    bool operator!=(const LineIterator &other) const {
      return current_pos != other.current_pos;
    }

    private:
      const char* current_pos;
      const char* end_pos;
      mutable std::string_view cached_view;
  };

  LineIterator begin() const {
    return LineIterator(get_data(), get_data() + get_size());
  }

  LineIterator end() const {
    return LineIterator(get_data() + get_size(), get_data() + get_size());
  }
};

std::unique_ptr<MmapReader> create_mmap_reader(const std::string& filename);

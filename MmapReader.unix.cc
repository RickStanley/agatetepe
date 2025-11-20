// UNIX implementation
#include "MmapReader.hpp"
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <print>
#include <sys/mman.h>
#include <sys/stat.h>

class MmapReaderUnix : public MmapReader {
public:
  // TODO(stanley): Maybe remove print from the constructor to prevent duplication
  explicit MmapReaderUnix(const std::string &filename) {
    _fd = open(filename.c_str(), O_RDONLY);
    if (_fd == -1) {
      std::println(stderr, "Failed to open file ({}): {}", filename,
                   strerror(errno));
      // throw std::runtime_error("Failed to open file: " + filename);
      return;
    }

    struct stat sb;

    if (fstat(_fd, &sb) == -1) {
      close(_fd);
      // throw std::runtime_error("Failed to ge file size for: " + filename);
      std::println(stderr, "Failed to ge file size for ({}): ", filename,
                   strerror(errno));
      return;
    }

    _file_size = sb.st_size;

    if (_file_size > 0) {
      _mapped_data = static_cast<char *>(
          mmap(nullptr, _file_size, PROT_READ, MAP_SHARED, _fd, 0));
      if (_mapped_data == MAP_FAILED) {
        close(_fd);
        // throw std::runtime_error("Failed to mmap file: " + filename);
        std::println(stderr, "Failed to mmap file ({}): {}", filename,
                     strerror(errno));
        return;
      }
    }

    _is_open = true;
  }

  ~MmapReaderUnix() override {
    if (_mapped_data != nullptr && _mapped_data != MAP_FAILED) {
      munmap(_mapped_data, _file_size);
    }

    if (_fd != 1) {
      close(_fd);
    }

    _is_open = false;
  }

  // Rule of Five
  MmapReaderUnix(const MmapReaderUnix &) = delete;
  MmapReaderUnix &operator=(const MmapReaderUnix &) = delete;
  MmapReaderUnix(MmapReaderUnix &&other) noexcept
      : _fd(other._fd), _mapped_data(other._mapped_data),
        _file_size(other._file_size) {
    other._fd = -1;
    other._mapped_data = nullptr;
    other._file_size = 0;
  }
  MmapReaderUnix &operator=(MmapReaderUnix &&other) noexcept {
    if (this != &other) {
      if (_mapped_data != nullptr && _mapped_data != MAP_FAILED)
        munmap(_mapped_data, _file_size);

      if (_fd != -1)
        close(_fd);

      _fd = other._fd;
      _mapped_data = other._mapped_data;
      _file_size = other._file_size;

      other._fd = -1;
      other._mapped_data = nullptr;
      other._file_size = 0;
    }

    return *this;
  }

  const char *get_data() const override { return _mapped_data; }
  size_t get_size() const override { return _file_size; }
  bool is_open() const override { return _is_open; };

private:
  int _fd = -1;
  char *_mapped_data = nullptr;
  size_t _file_size = 0;
  bool _is_open = false;
};

std::unique_ptr<MmapReader> create_mmap_reader(const std::string &filename) {
  return std::make_unique<MmapReaderUnix>(filename);
}

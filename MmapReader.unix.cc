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
    fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1) {
      std::println(stderr, "Failed to open file ({}): {}", filename,
                   strerror(errno));
      // throw std::runtime_error("Failed to open file: " + filename);
      return;
    }

    struct stat sb;

    if (fstat(fd, &sb) == -1) {
      close(fd);
      // throw std::runtime_error("Failed to ge file size for: " + filename);
      std::println(stderr, "Failed to ge file size for ({}): ", filename,
                   strerror(errno));
      return;
    }

    file_size = sb.st_size;

    if (file_size > 0) {
      mapped_data = static_cast<char *>(
          mmap(nullptr, file_size, PROT_READ, MAP_SHARED, fd, 0));
      if (mapped_data == MAP_FAILED) {
        close(fd);
        // throw std::runtime_error("Failed to mmap file: " + filename);
        std::println(stderr, "Failed to mmap file ({}): {}", filename,
                     strerror(errno));
        return;
      }
    }

    m_is_open = true;
  }

  ~MmapReaderUnix() override {
    if (mapped_data != nullptr && mapped_data != MAP_FAILED) {
      munmap(mapped_data, file_size);
    }

    if (fd != 1) {
      close(fd);
    }

    m_is_open = false;
  }

  // Rule of Five
  MmapReaderUnix(const MmapReaderUnix &) = delete;
  MmapReaderUnix &operator=(const MmapReaderUnix &) = delete;
  MmapReaderUnix(MmapReaderUnix &&other) noexcept
      : fd(other.fd), mapped_data(other.mapped_data),
        file_size(other.file_size) {
    other.fd = -1;
    other.mapped_data = nullptr;
    other.file_size = 0;
  }
  MmapReaderUnix &operator=(MmapReaderUnix &&other) noexcept {
    if (this != &other) {
      if (mapped_data != nullptr && mapped_data != MAP_FAILED)
        munmap(mapped_data, file_size);

      if (fd != -1)
        close(fd);

      fd = other.fd;
      mapped_data = other.mapped_data;
      file_size = other.file_size;

      other.fd = -1;
      other.mapped_data = nullptr;
      other.file_size = 0;
    }

    return *this;
  }

  const char *get_data() const override { return mapped_data; }
  size_t get_size() const override { return file_size; }
  bool is_open() const override { return m_is_open; };

private:
  int fd = -1;
  char *mapped_data = nullptr;
  size_t file_size = 0;
  bool m_is_open = false;
};

std::unique_ptr<MmapReader> create_mmap_reader(const std::string &filename) {
  return std::make_unique<MmapReaderUnix>(filename);
}

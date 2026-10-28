#ifndef B11A1D54_2D7C_4AFE_B984_CC8E08CDA8A7
#define B11A1D54_2D7C_4AFE_B984_CC8E08CDA8A7

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <filesystem>
#include <span>
#include <system_error>
#include <utility>

namespace util {
// Owns a file descriptor and closes it on scope exit, so every early throw
// closes the fd without an explicit close().
class RAIIFileHandle {
 public:
  explicit RAIIFileHandle(int fd) noexcept : fd_{fd} {}
  ~RAIIFileHandle() {
    if (fd_ != -1) ::close(fd_);
  }

  RAIIFileHandle(const RAIIFileHandle&) = delete;
  RAIIFileHandle& operator=(const RAIIFileHandle&) = delete;

  [[nodiscard]] int get() const noexcept { return fd_; }

 private:
  int fd_;
};
}  // namespace util

class MappedFile {
 public:
  // Construct a mapped file. Only reads the file lazily, using mmap on Linux
  explicit MappedFile(const std::filesystem::path& filepath)
      : data{nullptr}, size{0} {
    const util::RAIIFileHandle fd{
        ::open(filepath.c_str(), O_RDONLY | O_CLOEXEC)};
    if (fd.get() == -1)
      throw std::system_error(errno, std::generic_category(),
                              "File open failed " + filepath.string());
    struct stat st;
    if (::fstat(fd.get(), &st) == -1)
      throw std::system_error(
          errno, std::generic_category(),
          "File size read (stat) failed " + filepath.string());
    else if (!S_ISREG(st.st_mode))
      throw std::system_error(
          std::make_error_code(std::errc::invalid_argument),
          "Path must point to a regular file " + filepath.string());
    else if (st.st_size == 0)
      throw std::system_error(std::make_error_code(std::errc::invalid_argument),
                              "File must not be empty " + filepath.string());
    size = static_cast<size_t>(st.st_size);
    void* ret = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd.get(), 0);
    if (ret == MAP_FAILED)
      throw std::system_error(errno, std::generic_category(),
                              "File mmap failed " + filepath.string());

    data = static_cast<std::byte*>(ret);

    ::madvise(data, size, MADV_SEQUENTIAL);  // read ahead aggressively
  }

  [[nodiscard]] std::span<const std::byte> get_bytes() const {
    return std::span<const std::byte>{data, size};
  }

  ~MappedFile() {
    if (data) ::munmap(data, size);
  }

  MappedFile(MappedFile&& src) noexcept : data{src.data}, size{src.size} {
    src.data = nullptr;
    src.size = 0;
  }

  // Swap: src takes our old mapping and unmaps it when it is destroyed.
  MappedFile& operator=(MappedFile&& src) noexcept {
    std::swap(data, src.data);
    std::swap(size, src.size);
    return *this;
  }

  // File handle is unique-- don't copy it
  MappedFile(const MappedFile&) = delete;
  MappedFile& operator=(const MappedFile&) = delete;

 private:
  std::byte* data;
  size_t size;
};

#endif /* B11A1D54_2D7C_4AFE_B984_CC8E08CDA8A7 */

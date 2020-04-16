#ifndef VGM_GZ_FILE_H_
#define VGM_GZ_FILE_H_

#include <vector>

#include <libaudcore/vfs.h>
#include <zconf.h>
#include <zlib.h>

class GzFile {
 private:
  z_stream_s zbuf_{};
  VFSFile *file_;

  std::vector<unsigned char> buffer_;

  enum Mode { MODE_AUTO, MODE_UNGZ, MODE_RAW_DEFLATE } mode_;

  void reset();
  bool is_deflate_ = false;

  int64_t pos_{}, skip_{}, start_{};

 public:
  explicit GzFile(VFSFile *file, Mode mode = MODE_AUTO)
      : file_(file), buffer_(16384), mode_(mode) {
    reset();
  }

  bool open();
  void close();
  int64_t read(void *buffer, uint64_t n_bytes);
  bool eof() const { return !zbuf_.avail_in && file_->feof(); }
  // bool seek(int64_t offset, unsigned whence);
};

#endif  // VGM_GZ_FILE_H_

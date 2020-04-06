#include "vfsloader.h"

#include <libaudcore/audstrings.h>
#include <libaudcore/runtime.h>

#include <cstdlib>
#include <memory>

#include "gzfile.h"
#include "log.h"
#include "stdtype.h"
#include "utils/DataLoader.h"

namespace {

using audvgm::log_err;

class VFSLoaderOps {
 public:
  explicit VFSLoaderOps(VFSFile *file) : file_(file) {}

  VFSLoaderOps(const VFSLoaderOps &) = delete;
  VFSLoaderOps &operator=(const VFSLoaderOps &) = delete;
  VFSLoaderOps(VFSLoaderOps &&) = default;
  VFSLoaderOps &operator=(VFSLoaderOps &&) = default;

  virtual UINT32 read(UINT8 *buffer, UINT32 numBytes) {
    if (!file_) return 0;
    return file_->fread(buffer, 1, numBytes);
  }
  virtual UINT8 seek(UINT32 offset, UINT8 whence) {
    if (!file_) return 1;
    return (UINT8)file_->fseek(offset, to_vfs_seek_type(whence));
  }
  virtual UINT8 close() {
    file_ = nullptr;
    return 0;
  }
  virtual INT32 tell() {
    if (!file_) return -1;
    return file_->ftell();
  }
  virtual UINT8 eof() {
    if (!file_) return 1;
    return static_cast<UINT8>(file_->feof());
  }

 protected:
  VFSFile *file_;
};

class GZLoaderOps : public VFSLoaderOps {
 public:
  explicit GZLoaderOps(VFSFile *file) : VFSLoaderOps(file), gz_file_(file) {}

  bool open() { return gz_file_.open(); }

  UINT32 read(UINT8 *buffer, UINT32 numBytes) override {
    if (!file_) return 0;
    return gz_file_.read(buffer, numBytes);
  }
  UINT8 seek(UINT32 offset, UINT8 whence) override {
    log_err("FIXME: VFSLoaderImpl unexpected seek() called from libvgm.");
    return 1;
  }
  INT32 tell() override {
    log_err("FIXME: VFSLoaderImpl unexpected seek() called from libvgm.");
    return -1;
  }

 private:
  GzFile gz_file_;
};

class VFSLoader {
 public:
  explicit VFSLoader(VFSFile &file) : file_(file), bytes_total_(0) {}
  VFSLoader(const VFSLoaderOps &) = delete;
  VFSLoader &operator=(const VFSLoaderOps &) = delete;

  static UINT8 s_open(void *context);
  static UINT32 s_read(void *context, UINT8 *buffer, UINT32 numBytes);
  static UINT8 s_seek(void *context, UINT32 offset, UINT8 whence);
  static UINT8 s_close(void *context);
  static INT32 s_tell(void *context);
  static UINT32 s_length(void *context);
  static UINT8 s_eof(void *context);

 private:
  VFSFile &file_;
  std::unique_ptr<VFSLoaderOps> ops_;
  uint32_t bytes_total_;
};

#pragma clang diagnostic push
#pragma ide diagnostic ignored "hicpp-signed-bitwise"
inline uint32_t readLE32(const unsigned char *data) {
  return data[0x03] << 24 | data[0x02] << 16 | data[0x01] << 8 | data[0x00] << 0;
}
#pragma clang diagnostic pop

UINT8 VFSLoader::s_open(void *context) {
  auto self = reinterpret_cast<VFSLoader *>(context);

  VFSFile &file = self->file_;

  UINT8 file_hdr[4];
  int64_t read_bytes = file.fread(file_hdr, 0x01, 4);
  if (read_bytes < 0) return 1;

  if (read_bytes == 4 && file_hdr[0] == 31 && file_hdr[1] == 139) {  // check for gz
    uint8_t size_buffer[4];
    if (file.fseek(-4, VFS_SEEK_END) < 0) {
      return 1;
    }
    if (file.fread(size_buffer, 4, 1) < 1) {
      return 1;
    }

    self->bytes_total_ = readLE32(size_buffer);
    if (self->bytes_total_ < file.ftell() / 2) self->bytes_total_ = 0;

    if (file.fseek(0, VFS_SEEK_SET) < 0) {
      return 1;
    }

    std::unique_ptr<GZLoaderOps> ops = std::make_unique<GZLoaderOps>(&self->file_);
    if (!ops->open()) return 1;
    self->ops_ = std::move(ops);
    return 0;
  }

  if (file.fseek(0, VFS_SEEK_SET) < 0) {
    return 1;
  }
  int64_t fsize = file.fsize();
  self->bytes_total_ = fsize >= 0 ? fsize : 0;
  self->ops_ = std::make_unique<VFSLoaderOps>(&self->file_);

  return 0;
}

UINT32 VFSLoader::s_read(void *context, UINT8 *buffer, UINT32 numBytes) {
  auto self = reinterpret_cast<VFSLoader *>(context);
  if (!self->ops_) return 0;
  return self->ops_->read(buffer, numBytes);
}

UINT8 VFSLoader::s_seek(void *context, UINT32 offset, UINT8 whence) {
  auto self = reinterpret_cast<VFSLoader *>(context);
  if (!self->ops_) return 1;
  return self->ops_->seek(offset, whence);
}

UINT8 VFSLoader::s_close(void *context) {
  auto self = reinterpret_cast<VFSLoader *>(context);
  if (!self->ops_) return 0;
  return self->ops_->close();
}

INT32 VFSLoader::s_tell(void *context) {
  auto self = reinterpret_cast<VFSLoader *>(context);
  if (!self->ops_) return -1;
  return self->ops_->tell();
}

UINT32 VFSLoader::s_length(void *context) {
  auto self = reinterpret_cast<VFSLoader *>(context);
  return self->bytes_total_;
}

UINT8 VFSLoader::s_eof(void *context) {
  auto self = reinterpret_cast<VFSLoader *>(context);
  if (!self->ops_) return 1;
  return self->ops_->eof();
}

const DATA_LOADER_CALLBACKS vfs_loader_callbacks = {
    0x56465346,  // "VFSF"
    "VFS Loader",       VFSLoader::s_open, VFSLoader::s_read,   VFSLoader::s_seek,
    VFSLoader::s_close, VFSLoader::s_tell, VFSLoader::s_length, VFSLoader::s_eof};

}  // namespace

namespace audvgm {

DATA_LOADER *VFSLoader_Init(VFSFile &file) {
  DATA_LOADER *data_loader;

  data_loader = (DATA_LOADER *)calloc(1, sizeof(DATA_LOADER));
  if (!data_loader) return nullptr;

  DataLoader_Setup(data_loader, &vfs_loader_callbacks, new VFSLoader(file));
  return data_loader;
}

}  // namespace audvgm

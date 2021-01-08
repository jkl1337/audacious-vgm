#include "vfsloader.h"

#include <cstdlib>
#include <cstring>
#include <memory>

#define WANT_VFS_STDIO_COMPAT
#include <libaudcore/audstrings.h>
#include <libaudcore/runtime.h>
#include <libaudcore/vfs.h>

#include "gzfile.h"
#include "log.h"
#include <libvgm/player/vgmplayer.hpp>
#include <libvgm/stdtype.h>
#include <libvgm/utils/DataLoader.h>

using namespace vgmtk;

class vgmtk::VFSLoaderOps {
 public:
  explicit VFSLoaderOps(VFSFile *file) : file_(file) {}

  VFSLoaderOps(const VFSLoaderOps &) = delete;
  VFSLoaderOps &operator=(const VFSLoaderOps &) = delete;
  VFSLoaderOps(VFSLoaderOps &&) = default;
  VFSLoaderOps &operator=(VFSLoaderOps &&) = default;

  virtual ~VFSLoaderOps() = default;

  virtual UINT32 read(UINT8 *buffer, UINT32 numBytes) {
    if (!file_) return 0;
    return file_->fread(buffer, 1, numBytes);
  }
  virtual UINT8 seek(UINT32 offset, UINT8 whence) {
    if (!file_) return -1;
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
  virtual UINT8 eof() const {
    if (!file_) return 1;
    return static_cast<UINT8>(file_->feof());
  }
  virtual UINT32 length() const {
    if (!file_) return -1;
    return file_->fsize();
  }

 protected:
  VFSFile *file_;
};

class GZLoaderOps : public VFSLoaderOps {
 public:
  explicit GZLoaderOps(VFSFile *file) : VFSLoaderOps(file), gz_file_(file), length_(-1) {}

  bool open() { return gz_file_.open(); }

  UINT32 read(UINT8 *buffer, UINT32 numBytes) override {
    if (!file_) return 0;
    return gz_file_.read(buffer, numBytes);
  }
  UINT8 seek(UINT32 offset, UINT8 whence) override { return gz_file_.seek(offset, whence); }
  INT32 tell() override { return gz_file_.tell(); }
  UINT8 eof() const override { return gz_file_.eof(); }

  UINT32 length() const override {
    if (length_ != -1) return length_;

    auto *file = gz_file_.file();
    if (!file) return -1;

    auto pos = file->ftell();
    if (pos < 0) return -1;
    if (file->fseek(-4, VFS_SEEK_END) < 0) return -1;
    uint8_t buf[4];
    auto bytes_read = file->fread(buf, 0x01, 4);
    auto file_length = file->ftell();
    if (file->fseek(pos, VFS_SEEK_SET) < 0) return -1;
    if (bytes_read != 4) return -1;

    auto length = unpack_le32(buf);
    if (file_length == -1 || length < file_length / 2) return -1;
    return length_ = length;
  }

 private:
  static inline uint32_t unpack_le32(const unsigned char *data) {
    return data[0x03] << 24 | data[0x02] << 16 | data[0x01] << 8 | data[0x00] << 0;
  }

  GzFile gz_file_;
  mutable uint32_t length_;
};

UINT8 VFSLoader::open() {
  VFSFile &file = file_;

  uint8_t file_hdr[4];
  int64_t read_bytes = file.fread(file_hdr, 0x01, 4);
  if (read_bytes < 0) return 1;

  if (read_bytes == 4 && file_hdr[0] == 31 && file_hdr[1] == 139) {  // check for gz
    if (file.fseek(0, VFS_SEEK_SET) < 0) {
      return 1;
    }

    std::unique_ptr<GZLoaderOps> ops = std::make_unique<GZLoaderOps>(&file_);
    if (!ops->open()) return 1;
    ops_ = std::move(ops);
    return 0;
  }

  if (file.fseek(0, VFS_SEEK_SET) < 0) {
    return 1;
  }
  ops_ = std::make_unique<VFSLoaderOps>(&file_);
  return 0;
}

UINT32 VFSLoader::read(UINT8 *buffer, UINT32 numBytes) {
  if (!ops_) return 0;
  return ops_->read(buffer, numBytes);
}

UINT8 VFSLoader::seek(UINT32 offset, UINT8 whence) {
  if (!ops_) return -1;
  return ops_->seek(offset, whence);
}

UINT8 VFSLoader::close() {
  if (!ops_) return 0;
  return ops_->close();
}

INT32 VFSLoader::tell() {
  if (!ops_) return -1;
  return ops_->tell();
}

UINT32 VFSLoader::length() const {
  if (!ops_) return -1;
  return ops_->length();
}

UINT8 VFSLoader::eof() const {
  if (!ops_) return 1;
  return ops_->eof();
}

constexpr const DATA_LOADER_CALLBACKS VFSLoader::callbacks = {
    0x56465346,  // "VFSF"
    "VFS Loader",
    [](void *ctx) { return (*static_cast<VFSLoader **>(ctx))->open(); },
    [](void *ctx, UINT8 *buf, UINT32 nb) {
      return (*static_cast<VFSLoader **>(ctx))->read(buf, nb);
    },
    [](void *ctx, UINT32 ofs, UINT8 whence) {
      return (*static_cast<VFSLoader **>(ctx))->seek(ofs, whence);
    },
    [](void *ctx) { return (*static_cast<VFSLoader **>(ctx))->close(); },
    [](void *ctx) { return (*static_cast<VFSLoader **>(ctx))->tell(); },
    [](void *ctx) { return (*static_cast<VFSLoader **>(ctx))->length(); },
    [](void *ctx) { return (*static_cast<VFSLoader **>(ctx))->eof(); }};

VFSLoader::VFSLoader(VFSFile &file) : file_(file), libvgm_loader_(nullptr) {
  auto *loader = static_cast<DATA_LOADER *>(calloc(1, sizeof(DATA_LOADER)));
  if (!loader) throw std::bad_alloc();
  // damn library - it will free the ctx
  auto *ctx = static_cast<VFSLoader **>(malloc(sizeof(VFSLoader **)));
  if (!ctx) {
    free(loader);
    throw std::bad_alloc();
  }
  libvgm_loader_ = loader;
  *ctx = this;
  DataLoader_Setup(loader, &VFSLoader::callbacks, ctx);
}

VFSLoader::~VFSLoader() { DataLoader_Deinit(libvgm_loader_); }

FileHandler::FileHandler(VFSFile& fd)
    : loader_(fd) {
  DataLoader_SetPreloadBytes(loader_.libvgm_loader(), 0x100);
}

bool FileHandler::load() {
  if (DataLoader_Load(loader_.libvgm_loader())) {
    DataLoader_CancelLoading(loader_.libvgm_loader());
    return false;
  }

  if (VGMPlayer::PlayerCanLoadFile(loader_.libvgm_loader()) == 0) {
    player_ = std::make_unique<VGMPlayer>();
  } else {
    log_err("Unsupported file type\n");
  }

  if (player_->LoadFile(loader_.libvgm_loader())) {
    log_err("Failed to load file");
    return false;
  }
  return true;
}

VGMPlayer* FileHandler::vgm_player() const {
  return (player_->GetPlayerType() == FCC_VGM) ? (VGMPlayer*)player_.get() : nullptr;
}

FileHandler::FileType FileHandler::probe_file() {
  loader_.open();
  auto offset = loader_.tell();
  if (offset != 0)
    return kTypeNone;

  const UINT8 vgm_magic[5] = "Vgm ";
  UINT8 buffer[4];
  UINT32 bytes_read = loader_.read(buffer, 4);
  loader_.seek(0, SEEK_SET);
  if (bytes_read != sizeof buffer)
    return kTypeNone;
  if (!std::memcmp(buffer, vgm_magic, sizeof buffer))
    return kTypeVGM;
  return kTypeNone;
}

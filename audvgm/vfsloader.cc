#include "vfsloader.h"

#include <cstdlib>
#include <cstring>
#include <memory>

#define WANT_VFS_STDIO_COMPAT
#include <libaudcore/audstrings.h>
#include <libaudcore/runtime.h>
#include <libaudcore/vfs.h>

#include <libvgm/player/vgmplayer.hpp>

#include "gzfile.h"
#include "log.h"

using namespace vgmtk;

namespace vgmtk::impl {

class VFSLoaderOps {
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
    return static_cast<UINT8>(file_->fseek(offset, to_vfs_seek_type(whence)));
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
  explicit GZLoaderOps(VFSFile *file) : VFSLoaderOps(file), gz_file_(file) {}

  bool open() { return gz_file_.open(); }

  UINT32 read(UINT8 *buffer, UINT32 numBytes) override {
    if (!file_) return 0;
    return gz_file_.read(buffer, numBytes);
  }
  UINT8 seek(UINT32 offset, UINT8 whence) override { return gz_file_.seek(offset, whence); }
  INT32 tell() override { return gz_file_.tell(); }
  UINT8 eof() const override { return gz_file_.eof(); }

  UINT32 length() const override {
    if (length_ != static_cast<uint32_t>(-1)) return length_;

    auto *file = gz_file_.file();
    if (!file) return -1;

    const auto pos = file->ftell();
    if (pos < 0) return -1;
    if (file->fseek(-4, VFS_SEEK_END) < 0) return -1;
    uint8_t buf[4];
    const auto bytes_read = file->fread(buf, 0x01, 4);
    const auto file_length = file->ftell();
    if (file->fseek(pos, VFS_SEEK_SET) < 0) return -1;
    if (bytes_read != 4) return -1;

    const auto length = unpack_le32(buf);
    if (file_length == -1 || length < file_length / 2) return -1;
    return length_ = length;
  }

 private:
  static uint32_t unpack_le32(const unsigned char *data) {
    return data[0x03] << 24 | data[0x02] << 16 | data[0x01] << 8 | data[0x00] << 0;
  }

  GzFile gz_file_;
  mutable uint32_t length_{UINT32_MAX};
};

class VFSLoader : public DATA_LOADER {
 public:
  explicit VFSLoader(VFSFile &file) : _data_loader(), file_(file) {
    DataLoader_Setup(this, &callbacks, this);
  }
  explicit VFSLoader(VFSFile &&file_own)
      : _data_loader(), file_(file_own_), file_own_(std::move(file_own)) {
    DataLoader_Setup(this, &callbacks, this);
  }

  VFSLoader(const VFSLoader &) = delete;
  VFSLoader &operator=(const VFSLoader &) = delete;

  UINT8 open();
  UINT32 read(UINT8 *buffer, UINT32 numBytes) const {
    if (!ops_) return 0;
    return ops_->read(buffer, numBytes);
  }
  UINT8 seek(UINT32 offset, UINT8 whence) const {
    if (!ops_) return -1;
    return ops_->seek(offset, whence);
  }
  UINT8 close() const {
    if (!ops_) return 0;
    return ops_->close();
  }
  INT32 tell() const {
    if (!ops_) return -1;
    return ops_->tell();
  }
  UINT8 eof() const {
    if (!ops_) return 1;
    return ops_->eof();
  }
  UINT32 length() const {
    if (!ops_) return -1;
    return ops_->length();
  }

 private:
  VFSFile &file_;
  VFSFile file_own_;
  std::unique_ptr<VFSLoaderOps> ops_;

  static const DATA_LOADER_CALLBACKS callbacks;
};

constexpr DATA_LOADER_CALLBACKS VFSLoader::callbacks = {
    0x56465346,  // "VFSF"
    "VFS Loader",
    [](void *ctx) { return (static_cast<VFSLoader *>(ctx))->open(); },
    [](void *ctx, UINT8 *buf, UINT32 nbytes) {
      return (static_cast<VFSLoader *>(ctx))->read(buf, nbytes);
    },
    [](void *ctx, UINT32 ofs, UINT8 whence) {
      return (static_cast<VFSLoader *>(ctx))->seek(ofs, whence);
    },
    [](void *ctx) { return (static_cast<VFSLoader *>(ctx))->close(); },
    [](void *ctx) { return (static_cast<VFSLoader *>(ctx))->tell(); },
    [](void *ctx) { return (static_cast<VFSLoader *>(ctx))->length(); },
    [](void *ctx) { return (static_cast<VFSLoader *>(ctx))->eof(); },
    [](void *ctx) { static_cast<VFSLoader *>(ctx)->~VFSLoader(); }};

UINT8 VFSLoader::open() {
  uint8_t file_hdr[4];
  const auto read_bytes = file_.fread(file_hdr, 0x01, 4);
  if (read_bytes < 0) return 1;

  if (read_bytes == 4 && file_hdr[0] == 31 && file_hdr[1] == 139) {  // check for gz
    if (file_.fseek(0, VFS_SEEK_SET) < 0) {
      return 1;
    }

    std::unique_ptr<GZLoaderOps> ops = std::make_unique<GZLoaderOps>(&file_);
    if (!ops->open()) return 1;
    ops_ = std::move(ops);
    return 0;
  }

  if (file_.fseek(0, VFS_SEEK_SET) < 0) {
    return 1;
  }
  ops_ = std::make_unique<VFSLoaderOps>(&file_);
  return 0;
}

}  // namespace vgmtk::impl

FileHandler::FileHandler(VFSFile &file)
    : loader_(nullptr, [](impl::VFSLoader *p) { DataLoader_Deinit(p); }) {
  auto *mem = malloc(sizeof(impl::VFSLoader));
  if (!mem) throw std::bad_alloc();
  loader_.reset(new (mem) impl::VFSLoader(file));
  DataLoader_SetPreloadBytes(loader_.get(), 0x100);
}

bool FileHandler::load() {
  if (DataLoader_Load(loader_.get())) {
    DataLoader_CancelLoading(loader_.get());
    return false;
  }

  if (VGMPlayer::PlayerCanLoadFile(loader_.get()) == 0) {
    player_ = std::make_unique<VGMPlayer>();
    player_->SetFileReqCallback(
        [](void *, PlayerBase *, const char *fileName) -> DATA_LOADER * {
          auto path = filename_build({aud_get_path(AudPath::DataDir), fileName});
          if (!VFSFile::test_file(path, VFS_EXISTS)) return nullptr;

          auto *mem = malloc(sizeof(impl::VFSLoader));
          if (!mem) throw std::bad_alloc();
          auto *loader = new (mem) impl::VFSLoader(VFSFile(path, "rb"));
          DataLoader_Load(loader);
          return loader;
        },
        nullptr);
  } else {
    log_err("Unsupported file type\n");
  }

  if (player_->LoadFile(loader_.get())) {
    log_err("Failed to load file");
    return false;
  }
  return true;
}

VGMPlayer *FileHandler::vgm_player() const {
  return (player_->GetPlayerType() == FCC_VGM) ? static_cast<VGMPlayer *>(player_.get()) : nullptr;
}

FileHandler::FileType FileHandler::probe_file() const {
  loader_->open();
  auto offset = loader_->tell();
  if (offset != 0) return FileType::none;

  constexpr UINT8 vgm_magic[5] = "Vgm ";
  UINT8 buffer[4];
  UINT32 bytes_read = loader_->read(buffer, 4);
  // ReSharper disable once CppExpressionWithoutSideEffects
  loader_->seek(0, SEEK_SET);
  if (bytes_read != sizeof buffer) return FileType::none;
  if (!std::memcmp(buffer, vgm_magic, sizeof buffer)) return FileType::VGM;
  return FileType::none;
}

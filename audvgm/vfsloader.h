#ifndef AUDVGM__VFS_LOADER_H_
#define AUDVGM__VFS_LOADER_H_

#include <memory>

#include <libvgm/utils/DataLoader.h>

class VFSFile;

class PlayerBase;
class VGMPlayer;

namespace vgmtk {

class VFSLoaderOps;

class VFSLoader {
 public:
  explicit VFSLoader(VFSFile &file);
  ~VFSLoader();
  VFSLoader(const VFSLoader &) = delete;
  VFSLoader &operator=(const VFSLoader &) = delete;

  UINT8 open();
  UINT32 read(UINT8 *buffer, UINT32 numBytes);
  UINT8 seek(UINT32 offset, UINT8 whence);
  UINT8 close();
  INT32 tell();
  UINT8 eof() const;
  UINT32 length() const;

  DATA_LOADER *libvgm_loader() { return libvgm_loader_; }

 private:
  VFSFile &file_;
  DATA_LOADER *libvgm_loader_;
  std::unique_ptr<VFSLoaderOps> ops_;
  static const DATA_LOADER_CALLBACKS callbacks;
};

class FileHandler {
 public:
  enum FileType {
    kTypeNone = 0,
    kTypeVGM,
    kTypeDRO,
    kTypeS98
  };

  bool load();

  explicit FileHandler(VFSFile &fd);

  PlayerBase *player() const { return player_.get(); }
  VGMPlayer *vgm_player() const;

  FileType probe_file();
 private:
  VFSLoader loader_;
  std::unique_ptr<PlayerBase> player_;
};

}  // namespace vgm

#endif  // AUDVGM__VFS_LOADER_H_

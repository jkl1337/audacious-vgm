#ifndef AUDVGM_VFS_LOADER_H_
#define AUDVGM_VFS_LOADER_H_

#include <memory>

#include <libvgm/utils/DataLoader.h>

class VFSFile;

class PlayerBase;
class VGMPlayer;

namespace vgmtk {

namespace impl {
class VFSLoader;
}

class FileHandler {
 public:
  enum FileType : uint8_t {
    kTypeNone = 0,
    kTypeVGM,
    kTypeDRO,
    kTypeS98
  };

  bool load();

  explicit FileHandler(VFSFile &file);

  PlayerBase *player() const { return player_.get(); }
  VGMPlayer *vgm_player() const;

  FileType probe_file();
 private:
  std::unique_ptr<impl::VFSLoader, void(*)(impl::VFSLoader*)> loader_;
  std::unique_ptr<PlayerBase> player_;
};

}  // namespace vgm

#endif  // AUDVGM_VFS_LOADER_H_

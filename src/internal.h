#ifndef AUDACIOUS_VGM_INTERNAL_H
#define AUDACIOUS_VGM_INTERNAL_H

#include <libaudcore/audstrings.h>
#include <libaudcore/vfs.h>

#include <functional>
#include <memory>

class PlayerBase;
class VGMPlayer;

extern "C" {
typedef struct _data_loader DATA_LOADER;
}

namespace vgm {

class FileHandler {
 public:
  enum FileType {
    kTypeNone = 0,
    kTypeVGM,
    kTypeDRO,
    kTypeS98
  };
  String path_;

  bool load();

  explicit FileHandler(VFSFile &fd);

  PlayerBase *player() const { return player_.get(); }
  VGMPlayer *vgm_player() const;

  FileType probe_file() const;
 private:
  std::unique_ptr<DATA_LOADER, void(*)(DATA_LOADER*)> loader_;
  std::unique_ptr<PlayerBase> player_;
};

}

#endif  // AUDACIOUS_VGM_INTERNAL_H

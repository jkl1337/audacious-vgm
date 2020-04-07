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
void DataLoader_Deinit(DATA_LOADER *loader);
}

namespace audvgm {

class FileHandler {
 public:
  String path_;

  bool load(int sample_rate = -1);

  explicit FileHandler(VFSFile &fd);

  PlayerBase *player() const { return player_.get(); }
  VGMPlayer *vgm_player() const;

 private:
  std::unique_ptr<DATA_LOADER, std::function<void(DATA_LOADER*)>> loader_;
  std::unique_ptr<PlayerBase> player_;
};

}

#endif  // AUDACIOUS_VGM_INTERNAL_H

#include "internal.h"

#include <player/vgmplayer.hpp>

#include "log.h"
#include "vfsloader.h"

namespace audvgm {

FileHandler::FileHandler(VFSFile& fd) {
  loader_ = std::unique_ptr<DATA_LOADER, std::function<void(DATA_LOADER*)>>(
      audvgm::VFSLoader_Init(fd), [](DATA_LOADER* l) { DataLoader_Deinit(l); });

  if (!loader_) return;
  // TODO: multiple track support
  // uri_parse(path, nullptr, nullptr, &sub, &m_track);
  auto loader = loader_.get();

  DataLoader_SetPreloadBytes(loader, 0x100);
  if (DataLoader_Load(loader)) {
    DataLoader_CancelLoading(loader);
    loader_.reset();
    return;
  }

  if (VGMPlayer::IsMyFile(loader) == 0) {
    player_ = std::make_unique<VGMPlayer>();
  } else {
    audvgm::log_err("Unsupported file type\n");
  }
}

bool FileHandler::load(int sample_rate) {
  if (!player_ || !loader_) {
    return false;
  }

  if (player_->LoadFile(loader_.get())) {
    audvgm::log_err("Failed to load file");
    return false;
  }

  return true;
}

VGMPlayer* FileHandler::vgm_player() const {
  return (player_->GetPlayerType() == FCC_VGM) ? (VGMPlayer*)player_.get() : nullptr;
}

}  // namespace audvgm
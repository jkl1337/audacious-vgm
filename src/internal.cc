#include "internal.h"

#include <player/droplayer.hpp>
#include <player/s98player.hpp>
#include <player/vgmplayer.hpp>

#include "log.h"
#include "vfsloader.h"

namespace vgm {

FileHandler::FileHandler(VFSFile& fd)
    : loader_(vgm::VFSLoader_Init(fd), [](DATA_LOADER * l) { DataLoader_Deinit(l); }) {

  if (!loader_) return;
  auto *loader = loader_.get();

  DataLoader_SetPreloadBytes(loader, 0x100);
  if (DataLoader_Load(loader)) {
    DataLoader_CancelLoading(loader);
    loader_.reset();
    return;
  }

}

bool FileHandler::load() {
  if (!loader_)
    return false;

  if (VGMPlayer::IsMyFile(loader_.get()) == 0) {
    player_ = std::make_unique<VGMPlayer>();
  } else {
    vgm::log_err("Unsupported file type\n");
  }

  if (player_->LoadFile(loader_.get())) {
    vgm::log_err("Failed to load file");
    return false;
  }
  return true;
}

VGMPlayer* FileHandler::vgm_player() const {
  return (player_->GetPlayerType() == FCC_VGM) ? (VGMPlayer*)player_.get() : nullptr;
}

FileHandler::FileType FileHandler::probe_file() const {
  if (!loader_)
    return kTypeNone;

  if (VGMPlayer::IsMyFile(loader_.get()) == 0) {
    return kTypeVGM;
  } else if (S98Player::IsMyFile(loader_.get()) == 0) {
    return kTypeS98;
  } else if (DROPlayer::IsMyFile(loader_.get()) == 0) {
    return kTypeDRO;
  }
  return kTypeNone;
}

}  // namespace audvgm
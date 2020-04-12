//
// Created by luebsj on 4/11/20.
//

#include "libvgm-int.h"

#include <player/playerbase.hpp>
#include <player/vgmplayer.hpp>

#include "libvgm-dev-meta.h"

using namespace audvgm;

PlayerSettings PlayerSettings::init_lib_defaults() {
  PlayerSettings settings;
  VGMPlayer player;

  settings.devices.resize(vgm::lib_devices.len);

  for (const auto [dev, dev_meta] : settings.devices_with_meta()) {

    if (dev_meta.has_options()) {
      PLR_DEV_OPTS opts{};
      if (!player.GetDeviceOptions(PLR_DEV_ID(dev_meta.devid, 0), opts)) {
        dev.core_opts = opts.coreOpts;
      }
    }

    int panning_count = 0;
    if (dev_meta.has_panning()) {
      panning_count += dev_meta.num_channels();
    }
    const auto *linked_dev_meta = dev_meta.linked_config();
    if (linked_dev_meta && linked_dev_meta->has_panning()) {
      panning_count += linked_dev_meta->num_channels();
    }
    if (panning_count) {
      for (auto &chip : dev.chips) {
        chip.panning.resize(panning_count);
      }
    }
  }
  return settings;
}

#include "config.h"

#include <corelib/device-config.h>
#include <corelib/player-settings.h>
#include <corelib/tagging.h>
#include <libaudcore/runtime.h>

#include <libvgm/player/vgmplayer.hpp>
#include <string>

using namespace vgmtk;

namespace {

std::string dev_name_to_key(const char* label) {
  std::string k(label);

  for (auto& c : k) {
    if (c >= 'A' && c <= 'Z')
      c = c - ('Z' - 'z');
    else if (c == ' ')
      c = '-';
  }
  return k;
}

std::string core_to_str(uint32_t core) {
  if (core == 0) {
    return "0";
  }
  std::string str;
  str.push_back((core >> 24) & 0xFF);
  str.push_back((core >> 16) & 0xFF);
  str.push_back((core >> 8) & 0xFF);
  str.push_back(core & 0xFF);
  return str;
}

uint32_t str_to_core(const char* str) {
  if (!std::strcmp("0", str)) return 0;

  uint32_t core = 0;
  for (char c = *str; c; c = *++str) {
    core = (core << 8) | (c & 0xFF);
  }
  return core;
}

}  // namespace

void vgmtk::set_defaults() {
  const auto settings = PlayerSettings::init_lib_defaults();

  for (const auto [dev, dev_info] : settings.devices_with_meta()) {
    std::string dev_name = dev_name_to_key(dev_info.label);
    std::string opt_val, pan_val;

    aud_config_set_defaults(VGM_CFGID,
                            (std::array<const char*, 4>{(dev_name + ".core").c_str(), "0"}).data());

    if (dev_info.has_options()) {
      std::string key = dev_name + ".opts";
      std::string opt_val = n2hexstr(dev.core_opts);
      aud_config_set_defaults(VGM_CFGID,
                              (std::array<const char*, 4>{key.c_str(), opt_val.c_str()}).data());
    }
  }

  static constexpr const char* defaults[] = {cfg::kChipSrMode,
                                             "native",
                                             cfg::kChipSampleRate,
                                             "0",
                                             cfg::kSampleRate,
                                             "44100",
                                             cfg::kResampleQuality,
                                             "high",
                                             cfg::kPlaybackRate,
                                             "0",
                                             cfg::kLoopCount,
                                             "2",
                                             cfg::kFadeLength,
                                             "6000",
                                             cfg::kGapLooping,
                                             "0",
                                             cfg::kGapNonLooping,
                                             "0",
                                             cfg::kHardStopOld,
                                             "FALSE",
                                             cfg::kSSG2XVol,
                                             "TRUE",
                                             cfg::kSurround,
                                             "FALSE",
                                             cfg::kGain,
                                             "1.0",
                                             cfg::kPreferJPNTag,
                                             "FALSE",
                                             cfg::kGuessTrackNum,
                                             "FALSE",
                                             nullptr};
  aud_config_set_defaults(VGM_CFGID, defaults);
}

void vgmtk::save_player_settings(const PlayerSettings& settings) {
  for (const auto [dev, dev_info] : settings.devices_with_meta()) {
    std::string dev_name = dev_name_to_key(dev_info.label);
    aud_set_str(VGM_CFGID, (dev_name + ".core").c_str(), core_to_str(dev.core).c_str());

    if (dev_info.has_options()) {
      aud_set_str(VGM_CFGID, (dev_name + ".opts").c_str(), n2hexstr(dev.core_opts).c_str());
    }
  }
}

void vgmtk::load_player_settings(PlayerSettings& settings) {
  for (const auto [dev, dev_info] : settings.devices_with_meta()) {
    std::string dev_name = dev_name_to_key(dev_info.label);
    dev.core = str_to_core(aud_get_str(VGM_CFGID, (dev_name + ".core").c_str()));

    if (dev_info.has_options()) {
      dev.core_opts = hexstr2n<uint32_t>(aud_get_str(VGM_CFGID, (dev_name + ".opts").c_str()));
    }
  }
}

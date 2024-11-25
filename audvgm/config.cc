#include "config.h"

#include <corelib/device-config.h>
#include <corelib/player-settings.h>
#include <corelib/tagging.h>
#include <libaudcore/runtime.h>

#include <libvgm/player/vgmplayer.hpp>
#include <string>

using namespace vgmtk;

static std::string dev_name_to_key(const char* label) {
  std::string k(label);

  for (auto& c : k) {
    if (c >= 'A' && c <= 'Z')
      c = c - ('Z' - 'z');
    else if (c == ' ')
      c = '-';
  }
  return k;
}

static std::string make_all_devices_list() {
  std::string value;
  value.reserve(lib_devices.size() * 9);

  const char* sep = "";
  for (const auto& d : lib_devices) {
    value += sep;
    value += dev_name_to_key(d.label);
    sep = ",";
  }
  return value;
}

static std::string make_zeros_list(size_t n) {
  std::string str = "0,";
  str.reserve(n * 2);

  std::size_t i = 2;
  for (; i < n; i *= 2) {
    str += str;
  }
  str.append(str.data(), (n - (i / 2)) * 2);
  str.pop_back();
  return str;
}

void vgmtk::set_defaults() {
  std::vector<const char*> vec;

  VGMPlayer player;
  auto settings = PlayerSettings::init_lib_defaults();

  for (const auto [dev, dev_info] : settings.devices_with_meta()) {
    std::string opt_val, pan_val;
    const char* values[] = {"disable", "0",     "mute_0", "0",     "mute_1", "0",
                            "solo_0",  "0",     "solo_1", "0",     nullptr,  nullptr,
                            nullptr,   nullptr, nullptr,  nullptr, nullptr};
    const char** endp = values;
    while (*endp) endp++;

    if (dev_info.num_cores() > 1) {
      *endp++ = "core";
      *endp++ = "0";
    }

    if (dev_info.has_options()) {
      opt_val = n2hexstr(dev.core_opts);
      *endp++ = "opts";
      *endp++ = opt_val.c_str();
    }

    if (dev_info.has_panning()) {
      pan_val = make_zeros_list(dev.chips[0].panning.size());
      *endp++ = "pan";
      *endp = pan_val.c_str();
    }

    std::string section("vgm.");
    section.append(dev_name_to_key(dev_info.label));
    aud_config_set_defaults(section.c_str(), values);
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

void vgmtk::load_player_settings(const PlayerSettings& settings) {}

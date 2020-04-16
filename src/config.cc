#include "config.h"

#include <libaudcore/runtime.h>

#include <cstring>
#include <player/playerbase.hpp>
#include <player/vgmplayer.hpp>
#include <string>

#include "libvgm-dev-meta.h"

namespace vgm {
namespace {

inline std::string dev_name_to_key(const char* label) {
  std::string k(label);

  for (auto& c : k) {
    if (c >= 'A' && c <= 'Z')
      c = c - ('Z' - 'z');
    else if (c == ' ')
      c = '-';
  }
  return k;
}

std::string make_all_devices_list() {
  std::string value;
  value.reserve(vgm::lib_devices.len * 9);

  const char* sep = "";
  for (const auto& d : vgm::lib_devices) {
    value += sep;
    value += dev_name_to_key(d.label);
    sep = ",";
  }
  return value;
}

std::string make_zeros_list(size_t n) {
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

template <typename I>
std::string n2hexstr(I w) {
  static const char* digits = "0123456789abcdef";
  size_t len = 2;
  for (I k{w >> 8}; k; k >>= 8) {
    len += 2;
  }
  std::string rc(len + 2, '0');
  rc[1] = 'x';
  for (size_t i = 2, j = (len - 1) * 4; i < len + 2; ++i, j -= 4) {
    rc[i] = digits[(w >> j) & 0x0f];
  }
  return rc;
}

}  // namespace

void set_defaults() {
  std::vector<const char*> vec;

  VGMPlayer player;
  auto settings = PlayerSettings::init_lib_defaults();

  for (const auto [dev, dev_meta] : settings.devices_with_meta()) {
    std::string opt_val, pan_val;
    const char* values[] = {"disable", "0",     "mute_0", "0",     "mute_1", "0",
                            "solo_0",  "0",     "solo_1", "0",     nullptr,  nullptr,
                            nullptr,   nullptr, nullptr,  nullptr, nullptr};
    const char** endp = values;
    while (*endp) endp++;

    if (dev_meta.num_cores() > 1) {
      *endp++ = "core";
      *endp++ = "0";
    }

    if (dev_meta.has_options()) {
      opt_val = n2hexstr(dev.core_opts);
      *endp++ = "opts";
      *endp++ = opt_val.c_str();
    }

    if (dev_meta.has_panning()) {
      pan_val = make_zeros_list(dev.chips[0].panning.size());
      *endp++ = "pan";
      *endp = pan_val.c_str();
    }

    std::string section("vgm.");
    section.append(dev_name_to_key(dev_meta.label));
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

void load_player_settings(const PlayerSettings& settings) {}

}  // namespace audvgm
#ifndef AUDACIOUS_VGM_CONFIG_H
#define AUDACIOUS_VGM_CONFIG_H

#include <corelib/libvgm-iface.h>
#include <corelib/libvgm-rt.h>

namespace vgmtk {

// extern Config config;

constexpr const char *VGM_CFGID = "vgm";

namespace cfg {

constexpr const char *kChipSrMode = "chip_sr_mode";
constexpr const char *kChipSrModeNative = "native";
constexpr const char *kChipSrModeHighest = "highest";
constexpr const char *kChipSrModeCustom = "custom";

constexpr const char *kChipSampleRate = "chip_sample_rate";
constexpr const char *kLoopCount = "loop_count";
constexpr const char *kFadeLength = "fade_length";
constexpr const char *kSampleRate = "sample_rate";
constexpr const char *kResampleQuality = "resample_quality";
constexpr const char *kResampleQualityHQ = "high";
constexpr const char *kResampleQualityHQUp = "uphi_dnlw";
constexpr const char *kResampleQualityLQ = "low";

constexpr const char *kPlaybackRate = "playback_rate";
constexpr const char *kGapLooping = "gap_looping";
constexpr const char *kGapNonLooping = "gap_non_looping";
constexpr const char *kHardStopOld = "hard_stop_old";
constexpr const char *kSSG2XVol = "ssg_2x_vol";
constexpr const char *kSurround = "surround";
constexpr const char *kGain = "gain";
constexpr const char *kPreferJPNTag = "prefer_jpn_tag";
constexpr const char *kGuessTrackNum = "guess_track_num";

}  // namespace cfg

void set_defaults();
void load_player_settings(const PlayerSettings &settings);
void save_player_settings(const PlayerSettings &settings);
void save_player_settings_device(int dev_idx, const PlayerSettings::Device &dev_settings);

}

#endif  // AUDACIOUS_VGM_CONFIG_H

#ifndef AUDACIOUS_VGM_CONFIG_H
#define AUDACIOUS_VGM_CONFIG_H

namespace audvgm {

// extern Config config;

constexpr const char *VGM_CFGID = "vgm";

namespace cfg {

constexpr const char* CHIP_SR_MODE = "chip_sr_mode";
constexpr const char* CHIP_SR_MODE_NATIVE = "native";
constexpr const char* CHIP_SR_MODE_HIGHEST = "highest";
constexpr const char* CHIP_SR_MODE_CUSTOM = "custom";

constexpr const char* CHIP_SAMPLE_RATE = "chip_sample_rate";
constexpr const char *LOOP_COUNT = "loop_count";
constexpr const char *FADE_LENGTH = "fade_length";
constexpr const char *SAMPLE_RATE = "sample_rate";
constexpr const char *RESAMPLE_QUALITY = "resample_quality";
constexpr const char *RESAMPLE_QUALITY_HQ = "high";
constexpr const char *RESAMPLE_QUALITY_HQUP = "uphi_dnlw";
constexpr const char *RESAMPLE_QUALITY_LQ = "low";

constexpr const char *PLAYBACK_RATE = "playback_rate";
constexpr const char *GAP_LOOPING = "gap_looping";
constexpr const char *GAP_NON_LOOPING = "gap_non_looping";
constexpr const char *HARD_STOP_OLD = "hard_stop_old";
constexpr const char *SSG_2X_VOL = "ssg_2x_vol";
constexpr const char *SURROUND = "surround";
constexpr const char *GAIN = "gain";
constexpr const char *PREFER_JPN_TAG = "prefer_jpn_tag";
constexpr const char *GUESS_TRACK_NUM = "guess_track_num";

}

void set_defaults();

}

#endif  // AUDACIOUS_VGM_CONFIG_H

#include <libaudcore/runtime.h>
#include <libaudcore/tuple.h>

#include <cstring>
#include <player/playerbase.hpp>
#include <player/vgmplayer.hpp>

#include "config.h"
#include "internal.h"
#include "tags.h"
#include "vgm.h"

// FIXME: configuration
#if 0
typedef struct {
    std::array<std::atomic_uint_least32_t, 2> mute_masks;
} AudaciousVgmPlaybackSettings;

extern AudaciousVgmPlaybackSettings audvgm_playback_settings;

#endif

namespace audvgm {

namespace {

void dump_info(PlayerBase &player) {
  auto &vgm_player = reinterpret_cast<VGMPlayer &>(player);

  PLR_SONG_INFO song_info = {};
  player.GetSongInfo(song_info);

  std::vector<PLR_DEV_INFO> dev_info_list;
  player.GetSongDeviceInfo(dev_info_list);

  for (auto dev : dev_info_list) {
    int chip_id = dev.instance < 2 ? dev.instance : 0;

    auto chip_device = vgm_player.GetDevicePtr(dev.type, chip_id);
    auto base_dev = &chip_device->base;

    auto dev_info = &base_dev->defInf;
    auto dev_def = dev_info->devDef;

    printf(R"EOT(
name: %s
author: %s
)EOT",
           dev_def->name, dev_def->author);

    printf(R"EOT(srcSR: %u dstSR: %u linkDevCount: %u
)EOT",
           base_dev->resmpl.smpRateSrc, base_dev->resmpl.smpRateDst, dev_info->linkDevCount);
    if (dev_info->linkDevCount) {
      printf("link ID: %d\n", dev_info->linkDevs[0].devID);
    }
    printf(R"(ID: %u
Type: 0x%x
Instance: 0x%x
Volume: %u
Core: 0x%x
Clock: %u
Params: 0x%x
SR: %u


)",
           dev.id, dev.type, dev.instance, dev.volume, dev.core, dev.clock, dev.cParams,
           dev.smplRate);
    fflush(stdout);
  }
}

inline uint_fast32_t get_total_frames(const PlayerBase &player, uint_fast32_t sample_rate,
                                      uint_fast32_t loop_count, uint_fast32_t fade_length) {
  uint_fast64_t total_samples = player.Tick2Sample(player.GetTotalPlayTicks(loop_count));

  if (player.GetLoopTicks()) {
    total_samples += uint_fast64_t{fade_length} * sample_rate / 1000;
  }
  return std::min(total_samples, uint_fast64_t{UINT32_MAX});
}

struct Frame {
  int_fast32_t l;
  int_fast32_t r;
};

inline void pack_frame(int32_t *out, const Frame f) {
  out[0] = f.l;
  out[1] = f.r;
}

inline void pack_frame(int16_t *out, const Frame f) {
  out[0] = f.l / 256;
  out[1] = f.r / 256;
}

/**
 * Applies postprocessing of rendered audio, including gain, "surround" sound and
 * fading. Packs into buffer of type BufT.
 */
template <typename BufT, typename CntT, typename PosT = uint_fast32_t>
void process_frames(BufT *out, PosT frames_remaining, PosT frames_fade, CntT frame_count,
                    WAVE_32BS *data, int gain_256, bool surround) {
  constexpr long kSampMin = -(1 << 24) / 2;
  constexpr long kSampMax = (1 << 24) / 2 - 1;

  const int_fast64_t gains[2]{gain_256, surround ? -gain_256 : gain_256};
  int_fast64_t s[2];

  CntT fade_start_i = frame_count;
  if (frames_remaining > frames_fade) {
    PosT f = frames_remaining - frames_fade;
    if (f < frame_count) {
      fade_start_i = f;
    }
  } else {
    fade_start_i = 0;
  }

  for (CntT i = 0; i < frame_count; ++i) {
    s[0] = data[i].L;
    s[1] = data[i].R;

    if (i >= fade_start_i) {
      uint_fast64_t fade_volume = uint_fast64_t{frames_remaining - i} << 24;
      fade_volume /= frames_fade;
      fade_volume *= fade_volume;
      fade_volume >>= 24;

      s[0] = ((s[0] * static_cast<int_fast64_t>(fade_volume)) / (1 << 24));
      s[1] = ((s[1] * static_cast<int_fast64_t>(fade_volume)) / (1 << 24));
    }
    s[0] = (s[0] * gains[0]) / 256;
    s[1] = (s[1] * gains[1]) / 256;
    pack_frame(out, Frame{std::clamp<int_fast64_t>(s[0], kSampMin, kSampMax),
                          std::clamp<int_fast64_t>(s[1], kSampMin, kSampMax)});
    out += 2;
  }
}

}  // namespace

bool Plugin::play(const char *, VFSFile &file) {
  static const int buffer_n_frames = 4096;

  uint_fast32_t sample_rate =
      std::clamp<long>(aud_get_int(VGM_CFGID, cfg::kSampleRate), 0, UINT32_MAX);
  uint_fast32_t loop_count =
      std::clamp<long>(aud_get_int(VGM_CFGID, cfg::kLoopCount), 0, UINT32_MAX);
  uint_fast32_t fade_length =
      std::clamp<long>(aud_get_int(VGM_CFGID, cfg::kFadeLength), 0, UINT32_MAX);

  FileHandler fh(file);
  if (!fh.load(sample_rate)) return false;

  PlayerBase &driver = *fh.player();

  VGMPlayer *vgm_driver = fh.vgm_player();
  if (vgm_driver) {
    VGM_PLAY_OPTIONS play_opts = {
        .playbackHz = static_cast<UINT32>(std::min(aud_get_int(VGM_CFGID, cfg::kPlaybackRate), 0)),
        .hardStopOld = aud_get_bool(VGM_CFGID, cfg::kHardStopOld)};
    vgm_driver->SetPlayerOptions(play_opts);
  }

  Tuple tuple = get_playback_tuple();
  TagHandler::get_tags(driver, tuple);
  set_playback_tuple(tuple.ref());

  driver.SetSampleRate(sample_rate);
  driver.Start();

  dump_info(driver);

  std::vector<WAVE_32BS> render_buffer(buffer_n_frames);
  std::vector<int16_t> output_buffer(buffer_n_frames * 2);

  uint_fast32_t total_frames = get_total_frames(driver, sample_rate, loop_count, fade_length);
  uint_fast32_t frame_position = 0;
  uint_fast32_t frames_fade = 0;

  if (driver.GetLoopTicks()) {
    frames_fade =
        std::min(uint_fast64_t{fade_length} * sample_rate / 1000, uint_fast64_t{UINT32_MAX});
  }

#if 0
  std::array<uint_least32_t, 2> last_mute_mask = {audvgm_playback_settings.mute_masks[0],
                                                  audvgm_playback_settings.mute_masks[1]};
  for (int i = 0; i < 2; ++i) {
    PLR_MUTE_OPTS mute_opts = {.chnMute = {last_mute_mask[i]}};
    player->SetDeviceMuting(i, mute_opts);
  }
#endif

  open_audio(FMT_S16_NE, sample_rate, 2);

  while (!check_stop()) {
    int seek_value = check_seek();
    if (seek_value >= 0) {
      uint32_t new_pos = static_cast<uint_fast64_t>(seek_value) * sample_rate / 1000;
      driver.Seek(PLAYPOS_SAMPLE, new_pos);
      frame_position = new_pos;
    }

#if 0
    for (int i = 0; i < 2; ++i) {
      unsigned current_mute_mask = audvgm_playback_settings.mute_masks[i];
      if (current_mute_mask != last_mute_mask[i]) {
        PLR_MUTE_OPTS mute_opts = {.chnMute = {current_mute_mask}};
        player->SetDeviceMuting(i, mute_opts);
        last_mute_mask[i] = current_mute_mask;
      }
    }
#endif

    // RealTimeConfig current_config = realtime_config.load();
    int gain_256 = std::clamp<int>(aud_get_double(VGM_CFGID, cfg::kGain) * 256.0, 0, 1024);
    bool surround = aud_get_bool(VGM_CFGID, cfg::kSurround);

    uint_fast32_t frames_remaining = total_frames - frame_position;
    int n_frames = std::min(render_buffer.size(), frames_remaining);

    std::memset(render_buffer.data(), 0, render_buffer.size() * sizeof render_buffer[0]);
    n_frames = driver.Render(n_frames, render_buffer.data());

    process_frames(output_buffer.data(), total_frames - frame_position, frames_fade, n_frames,
                   render_buffer.data(), gain_256, surround);

    write_audio(output_buffer.data(),
                n_frames * 2 * int{sizeof(decltype(output_buffer)::value_type)});

    frame_position += n_frames;

    if (frame_position >= total_frames) break;
  }
  return true;
}

}  // namespace audvgm
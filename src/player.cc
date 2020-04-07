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
           base_dev->resmpl.smpRateSrc, base_dev->resmpl.smpRateDst,
           dev_info->linkDevCount);
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

unsigned long get_total_frames(const PlayerBase &player, unsigned sample_rate) {
  UINT32 ticks = player.GetTotalPlayTicks(aud_get_int(VGM_CFGID, cfg::LOOP_COUNT));
  UINT32 total_samples = player.Tick2Sample(ticks);

  if (player.GetLoopTicks()) {
    total_samples +=
        int64_t(aud_get_int(VGM_CFGID, cfg::FADE_LENGTH)) * sample_rate / 1000;
  }

  return total_samples;
}

constexpr long kSampMin = -(1 << 24) / 2;
constexpr long kSampMax = (1 << 24) / 2 - 1;

void pack_frames(int16_t *buf, unsigned int frame_count, const WAVE_32BS *data) {
  while (frame_count--) {
    WAVE_32BS s = *data++;

    if (s.L < kSampMin)
      s.L = kSampMin;
    else if (s.L > kSampMax)
      s.L = kSampMax;

    if (s.R < kSampMin)
      s.R = kSampMin;
    else if (s.R > kSampMax)
      s.R = kSampMax;

    *buf++ = static_cast<unsigned>(s.L) >> 8;
    *buf++ = static_cast<unsigned>(s.R) >> 8;
  }
}

/* apply a fade to a buffer of frames */
/* frames_rem - remaining total frames, includes fade frames */
/* frames_fade - total number of fade samples */
/* frame_count - number of frames being rendered right now */
void fade_frames(unsigned int frames_rem, unsigned int frames_fade,
                 unsigned int frame_count, WAVE_32BS *data) {
  unsigned int i = 0;
  unsigned int f = frames_fade;
  UINT64 fade_vol;

  if (frames_rem - frame_count > frames_fade) return;
  if (frames_rem > frames_fade) {
    i = frames_rem - frames_fade;
    f += i;
  } else {
    f = frames_rem;
  }
  while (i < frame_count) {
    fade_vol = (UINT64)((f - i)) * (1 << 16);
    fade_vol /= frames_fade;
    fade_vol *= fade_vol;
    fade_vol >>= 16;

    // FIXME: This conversion is suspect and also need to take into account minimum length
    data[i].L = (INT32)(((UINT64)data[i].L * fade_vol) >> 16);
    data[i].R = (INT32)(((UINT64)data[i].R * fade_vol) >> 16);
    i++;
  }
}

}  // namespace

bool Plugin::play(const char *, VFSFile &file) {
  Tuple tuple = get_playback_tuple();

  int sample_rate = aud_get_int(VGM_CFGID, cfg::SAMPLE_RATE);

  FileHandler fh(file);
  if (!fh.load(sample_rate)) return false;

  PlayerBase &driver = *fh.player();

  TagHandler::get_tags(driver, tuple);
  set_playback_tuple(tuple.ref());

  VGMPlayer *vgm_driver = fh.vgm_player();
  if (vgm_driver) {
    VGM_PLAY_OPTIONS play_opts = {
        .playbackHz = static_cast<UINT32>(aud_get_int(VGM_CFGID, "playback_rate")),
        .hardStopOld = aud_get_bool(VGM_CFGID, "hard_stop_old")};
    vgm_driver->SetPlayerOptions(play_opts);
  }

  driver.SetSampleRate(sample_rate);
  driver.Start();

  dump_info(driver);

  open_audio(FMT_S16_LE, sample_rate, 2);

  unsigned long total_frames = get_total_frames(driver, sample_rate);
  unsigned long frame_pos = 0;
  auto frames_fade = 0;
  if (driver.GetLoopTicks()) {
    frames_fade = int64_t(aud_get_int(VGM_CFGID, cfg::FADE_LENGTH)) * sample_rate / 1000;
  }
  
  const int buf_size = 4096;

  std::vector<WAVE_32BS> buf(buf_size);
  std::vector<int16_t> buf_packed(buf_size * 2);

#if 0
  std::array<uint_least32_t, 2> last_mute_mask = {audvgm_playback_settings.mute_masks[0],
                                                  audvgm_playback_settings.mute_masks[1]};
  for (int i = 0; i < 2; ++i) {
    PLR_MUTE_OPTS mute_opts = {.chnMute = {last_mute_mask[i]}};
    player->SetDeviceMuting(i, mute_opts);
  }
#endif

  while (!check_stop()) {
    int seek_value = check_seek();
    if (seek_value >= 0) {
      unsigned long new_pos = int64_t(seek_value) * sample_rate / 1000;

      driver.Seek(PLAYPOS_SAMPLE, new_pos);
      frame_pos = new_pos;
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

    std::memset(buf.data(), 0, buf.size() * sizeof buf[0]);

    unsigned long frames_remaining = total_frames - frame_pos;

    int n_frames = buf.size() > frames_remaining ? static_cast<int>(frames_remaining)
                                                 : static_cast<int>(buf.size());
    n_frames = driver.Render(n_frames, buf.data());

    fade_frames(total_frames - frame_pos, frames_fade, n_frames, buf.data());
    pack_frames(buf_packed.data(), n_frames, buf.data());
    write_audio(buf_packed.data(), n_frames * 4);

    frame_pos += n_frames;

    if (frame_pos >= total_frames) break;
  }
  return true;
}

}  // namespace audvgm
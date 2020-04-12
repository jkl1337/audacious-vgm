#include <libaudcore/runtime.h>
#include <libaudcore/tuple.h>

#include <cstring>
#include <player/playerbase.hpp>
#include <player/vgmplayer.hpp>

#include "config.h"
#include "internal.h"
#include "plugin.h"
#include "tags.h"

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
  auto &vgm_player = static_cast<VGMPlayer &>(player);

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
void process_frames(BufT *out, PosT remaining_frames, PosT fade_frames, CntT num_frames,
                    WAVE_32BS *data, int gain_256, bool surround) {
  constexpr long kSampMin = -(1 << 24) / 2;
  constexpr long kSampMax = (1 << 24) / 2 - 1;

  const int_fast64_t gains[2]{gain_256, surround ? -gain_256 : gain_256};
  int_fast64_t s[2];

  CntT fade_start_i = num_frames;
  if (remaining_frames > fade_frames) {
    PosT f = remaining_frames - fade_frames;
    if (f < num_frames) {
      fade_start_i = f;
    }
  } else {
    fade_start_i = 0;
  }

  for (CntT i = 0; i < num_frames; ++i) {
    s[0] = data[i].L;
    s[1] = data[i].R;

    if (i >= fade_start_i) {
      uint_fast64_t fade_volume = uint_fast64_t{remaining_frames - i} << 24;
      fade_volume /= fade_frames;
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
  static constexpr const int kNChannels = 2;
  static constexpr const int kBufferSize = 4096;

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

  // FIXME: Break out

  const auto *settings = backend_settings().acquire(true);

  int gain_256 = std::clamp<int>(settings->gain() * 256.0, 0, 1024);
  bool surround = settings->surround();

  struct SongDevice {
    int idx;
    uint32_t type;
    uint8_t instance;
    const vgm::DeviceConfig &dev_info;
    SongDevice(int idx, uint32_t type, uint8_t instance, const vgm::DeviceConfig &dev_info)
        : idx(idx), type(type), instance(instance), dev_info(dev_info) {}
  };

  std::vector<PLR_DEV_INFO> dev_info_list;
  driver.GetSongDeviceInfo(dev_info_list);

  std::vector<SongDevice> devices;
  // FIXME: break out
  for (const auto &d : dev_info_list) {
    int lib_id = 0; // bug in library
    for (int idx = 0; idx < vgm::lib_devices.size(); ++idx) {
      const auto &di = vgm::lib_devices[idx];
      if (d.type == di.devid) {
        devices.emplace_back(idx, d.type, d.instance, di);
      }
    }
  }

  for (const auto &d : devices) {
    PLR_DEV_OPTS opts{};
    driver.GetDeviceOptions(PLR_DEV_ID(d.type, d.instance), opts);
    settings->load_dev_opts(d.idx, d.instance, opts);
    driver.SetDeviceOptions(PLR_DEV_ID(d.type, d.instance), opts);
  }


  driver.SetSampleRate(sample_rate);
  driver.Start();

  // GetSongDeviceInfo has distinct function depending on whether driver is "PLAYING", ie after
  // start has been called.
  driver.GetSongDeviceInfo(dev_info_list);
  playback_status_->set_device_info(std::move(dev_info_list));

  dump_info(driver);

  std::vector<WAVE_32BS> render_buffer(kBufferSize);
  std::vector<int16_t> output_buffer(kBufferSize * kNChannels);

  uint_fast32_t total_frames = get_total_frames(driver, sample_rate, loop_count, fade_length);
  uint_fast32_t frame_position = 0;

  bool is_looping = driver.GetLoopTicks();

  uint_fast32_t gap_length = std::clamp<long>(
      aud_get_int(VGM_CFGID, is_looping ? cfg::kGapLooping : cfg::kGapNonLooping), 0, UINT32_MAX);
  uint_fast32_t gap_frames =
      std::min(uint_fast64_t{gap_length} * sample_rate / 1000, uint_fast64_t{UINT32_MAX});

  uint_fast32_t fade_frames = 0;
  if (is_looping) {
    fade_frames =
        std::min(uint_fast64_t{fade_length} * sample_rate / 1000, uint_fast64_t{UINT32_MAX});
  };
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

    settings = backend_settings().acquire();
    if (settings) {
      for (const auto &d : devices) {
        PLR_MUTE_OPTS opts{};
        settings->load_mute_opts(d.idx, d.instance, opts);
        driver.SetDeviceMuting(PLR_DEV_ID(d.type, d.instance), opts);
      }
      gain_256 = std::clamp<int>(settings->gain() * 256.0, 0, 1024);
      surround = settings->surround();
    }

    int n_frames = 0;
    if (frame_position < total_frames) {

      uint_fast32_t remaining_frames = total_frames - frame_position;
      n_frames = std::min(render_buffer.size(), remaining_frames);

      std::memset(render_buffer.data(), 0, render_buffer.size() * sizeof render_buffer[0]);
      n_frames = driver.Render(n_frames, render_buffer.data());

      process_frames(output_buffer.data(), total_frames - frame_position, fade_frames, n_frames,
                     render_buffer.data(), gain_256, surround);
    } else if (frame_position - total_frames < gap_frames) {
      n_frames =
          std::min(output_buffer.size() / kNChannels, gap_frames - (frame_position - total_frames));
      std::memset(output_buffer.data(), 0, n_frames * kNChannels * sizeof output_buffer[0]);
    } else {
      break;
    }

    write_audio(output_buffer.data(),
                n_frames * kNChannels * int{sizeof(decltype(output_buffer)::value_type)});

    frame_position += n_frames;
  }
  return true;
}

}  // namespace audvgm
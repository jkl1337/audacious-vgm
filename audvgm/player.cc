#include <cstring>
#include <algorithm>

#include <libaudcore/runtime.h>
#include <libvgm/emu/EmuCores.h>
#include <libvgm/emu/SoundDevs.h>
#include <libvgm/emu/cores/2612intf.h>

#include <libvgm/player/playerbase.hpp>
#include <libvgm/player/vgmplayer.hpp>

#include "config.h"
#include "plugin.h"
#include "tags.h"
#include "vfsloader.h"

namespace vgmtk {

namespace {

constexpr const int kNChannels = 2;
constexpr const int kBufferSizeSamples = 4096;
constexpr const int kRenderBlockSizeSamples = 1024;

void dump_info(PlayerBase &player) {
  auto &vgm_player = static_cast<VGMPlayer &>(player);

  PLR_SONG_INFO song_info = {};
  player.GetSongInfo(song_info);

  std::vector<PLR_DEV_INFO> dev_info_list;
  player.GetSongDeviceInfo(dev_info_list);

  for (const auto &dev : dev_info_list) {
    int chip_id = dev.instance < 2 ? dev.instance : 0;

    // FIXME: look into this.
    UINT8 messed_up = dev.type == DEVID_RF5C68 ? 0x10 : dev.type;
    auto *chip_device = vgm_player.GetDevicePtr(messed_up, chip_id);
    if (!chip_device) chip_device = vgm_player.GetDevicePtr(DEVID_RF5C68, chip_id);
    auto *base_dev = &chip_device->base;

    auto *dev_info = &base_dev->defInf;
    const auto *dev_def = dev_info->devDef;

    const auto *dt = lib_devices.find(dev.type);

    printf(R"EOT(
AccuChip!!: %s
name: %s
author: %s
)EOT",
           vgmtk::accurate_chip_name(dev), dev_def->name, dev_def->author);

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
Core: %s
Clock: %u
Params: 0x%x
SR: %u


)",
           dev.id, dev.type, dev.instance, dev.volume, dt->core_name(dev.core), dev.clock,
           dev.cParams, dev.smplRate);
    fflush(stdout);
  }
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

template <typename PosT, typename CntT>
static CntT fade_start_index(PosT remaining_frames, PosT fade_frames, CntT num_frames) {
  CntT index = num_frames;
  if (remaining_frames > fade_frames) {
    PosT f = remaining_frames - fade_frames;
    if (f < num_frames) {
      index = f;
    }
  } else {
    index = 0;
  }
  return index;
}

/**
 * Applies postprocessing of rendered audio, including gain, "surround" sound and
 * fading. Packs into buffer of type BufT.
 */
template <typename BufT, typename CntT, typename PosT = uint_fast32_t>
void process_frames(BufT *out, PosT remaining_frames, PosT fade_frames, CntT num_frames,
                    WAVE_32BS *data, float gain, bool surround) {
  constexpr long kSampMin = -(1 << 24) / 2;
  constexpr long kSampMax = (1 << 24) / 2 - 1;

  int_fast64_t gain_fp = gain * 256;
  const int_fast64_t gains[2]{gain_fp, surround ? -gain_fp : gain_fp};
  int_fast64_t s[2];

  CntT fade_start_i = fade_start_index(remaining_frames, fade_frames, num_frames);

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
    pack_frame(out, Frame{static_cast<int_fast32_t>(std::clamp<int_fast64_t>(s[0], kSampMin, kSampMax)),
                          static_cast<int_fast32_t>(std::clamp<int_fast64_t>(s[1], kSampMin, kSampMax))});
    out += 2;
  }
}

template <typename CntT, typename PosT = uint_fast32_t>
void process_frames(float *out, PosT remaining_frames, PosT fade_frames, CntT num_frames,
                    WAVE_32BS *data, float gain, bool surround) {
  const float gains[2]{gain, surround ? -gain : gain};
  double s[2];

  CntT fade_start_i = fade_start_index(remaining_frames, fade_frames, num_frames);

  for (CntT i = 0; i < num_frames; ++i) {
    s[0] = data[i].L;
    s[1] = data[i].R;

    if (i >= fade_start_i) {
      float fade_volume = static_cast<float>(remaining_frames - i) / fade_frames;
      fade_volume *= fade_volume;

      s[0] = s[0] * fade_volume;
      s[1] = s[1] * fade_volume;
    }
    s[0] = (s[0] * gains[0]) / (1 << 23);
    s[1] = (s[1] * gains[1]) / (1 << 23);

    *out++ = std::clamp(s[0], -1.0, 1.0);
    *out++ = std::clamp(s[1], -1.0, 1.0);
  }
}

class PlaybackHandler {
 public:
  explicit PlaybackHandler(PlayerBase &backend_player, SettingsManager &settings_manager,
                           PlaybackStatus &playback_status,
                           DeviceConfigList info_list = vgmtk::lib_devices)
      : backend_(backend_player),
        n_frames_(0),
        gain_(1.0),
        surround_(false),
        sample_rate_(std::clamp<long>(aud_get_int(VGM_CFGID, cfg::kSampleRate), 0, UINT32_MAX)),
        loop_count_(std::clamp<long>(aud_get_int(VGM_CFGID, cfg::kLoopCount), 0, UINT32_MAX)),
        fade_length_(std::clamp<long>(aud_get_int(VGM_CFGID, cfg::kFadeLength), 0, UINT32_MAX)),
        settings_manager_(settings_manager),
        playback_status_(playback_status),
        info_list_(info_list) {
    setup_vgm_player();
    init_chips();
    init_settings();
    init_device_options();

    backend_.SetSampleRate(sample_rate_);
    backend_.Start();

    handle_loaded_device_info();

    render_buffer_.resize(kBufferSizeSamples);
    output_buffer_.resize(kBufferSizeSamples * kNChannels);
    total_frames_ = calc_total_frames();
    fade_frames_ = calc_fade_frames();
    gap_frames_ = calc_gap_frames();
    frame_position_ = 0;
  }

  ~PlaybackHandler() { playback_status_.set_device_info(std::vector<PLR_DEV_INFO>()); }

  void seek(int seek_value) {
    uint32_t new_pos = static_cast<uint_fast64_t>(seek_value) * sample_rate_ / 1000;
    backend_.Seek(PLAYPOS_SAMPLE, new_pos);
    frame_position_ = new_pos;
  }

  bool fill_buffer() {
    int n_frames = 0;
    n_frames_ = n_frames;

    if (frame_position_ < total_frames_) {
      uint_fast32_t remaining_frames = total_frames_ - frame_position_;
      n_frames = std::min<int>(render_buffer_.size(), remaining_frames);

      std::memset(render_buffer_.data(), 0, render_buffer_.size() * sizeof render_buffer_[0]);

      for (int i = 0; i < n_frames;) {
        update_settings();
        int n_block = std::min(kRenderBlockSizeSamples, n_frames - i);
        n_block = backend_.Render(n_block, &render_buffer_[i]);

        process_frames(&output_buffer_[i * 2], total_frames_ - frame_position_, fade_frames_,
                       n_block, &render_buffer_[i], gain_, surround_);
        frame_position_ += n_block;
        i += n_block;
      }
    } else if (frame_position_ - total_frames_ < gap_frames_) {
      n_frames = std::min<int>(output_buffer_.size() / kNChannels,
                          gap_frames_ - (frame_position_ - total_frames_));
      std::memset(output_buffer_.data(), 0, n_frames * kNChannels * sizeof output_buffer_[0]);
      frame_position_ += n_frames;
    } else {
      return false;
    }
    n_frames_ = n_frames;
    return true;
  }
  const void *output_ptr() const { return static_cast<const void *>(output_buffer_.data()); }
  int output_len() const { return n_frames_ * kNChannels * int{sizeof output_buffer_[0]}; }

  uint_fast32_t calc_gap_frames() const {
    bool is_looping = backend_.GetLoopTicks();
    uint_fast32_t gap_length = std::clamp<long>(
        aud_get_int(VGM_CFGID, is_looping ? cfg::kGapLooping : cfg::kGapNonLooping), 0, UINT32_MAX);
    return std::min(uint_fast64_t{gap_length} * sample_rate_ / 1000, uint_fast64_t{UINT32_MAX});
  }

  uint_fast32_t calc_fade_frames() const {
    if (!is_looping()) return 0;
    return std::min(uint_fast64_t{fade_length_} * sample_rate_ / 1000, uint_fast64_t{UINT32_MAX});
  }

  uint_fast32_t calc_total_frames() const {
    uint_fast64_t total_samples = backend_.Tick2Sample(backend_.GetTotalPlayTicks(loop_count_));

    if (backend_.GetLoopTicks()) {
      total_samples += uint_fast64_t{fade_length_} * sample_rate_ / 1000;
    }
    return std::min(total_samples, uint_fast64_t{UINT32_MAX});
  }

  bool is_looping() const { return backend_.GetLoopTicks(); }
  uint_fast32_t sample_rate() const { return sample_rate_; }

 private:
  void setup_vgm_player() {
    if (backend_.GetPlayerType() != FCC_VGM) return;
    auto &vgm_player = static_cast<VGMPlayer &>(backend_);

    VGM_PLAY_OPTIONS play_opts = {
        .playbackHz = static_cast<UINT32>(std::min(aud_get_int(VGM_CFGID, cfg::kPlaybackRate), 0)),
        .hardStopOld = aud_get_bool(VGM_CFGID, cfg::kHardStopOld)};
    vgm_player.SetPlayerOptions(play_opts);
  }

  void init_chips() {
    std::vector<PLR_DEV_INFO> dev_info_list;
    backend_.GetSongDeviceInfo(dev_info_list);

    chips_.clear();
    chips_.reserve(dev_info_list.size());
    for (const auto &d : dev_info_list) {
      int dev_idx = info_list_.find_index(d.type);
      if (dev_idx < 0) continue;
      chips_.push_back({dev_idx, d.instance, d.type});
    }
    mute_opts_.resize(chips_.size());
  }

  void init_settings() {
    auto settings_ctx = settings_manager_.read_always();
    gain_ = std::clamp(settings_ctx->gain, 0.0, 4.0);
    surround_ = settings_ctx->surround;
  }

  void init_device_options() {
    std::vector<PLR_DEV_OPTS> dev_opts(chips_.size());

    for (int i = 0; i < chips_.size(); ++i) {
      const auto &c = chips_[i];
      backend_.GetDeviceOptions(PLR_DEV_ID(c.vgm_type, c.instance), dev_opts[i]);
    }

    {
      auto settings_ctx = settings_manager_.read_always();
      settings_ctx.load_dev_opts(chips_, dev_opts);
      settings_ctx.load_mute_opts(chips_, mute_opts_);
      // FIXME: initial panning options
    }

    for (int i = 0; i < chips_.size(); ++i) {
      const auto &c = chips_[i];
      dev_opts[i].muteOpts = mute_opts_[i];
      if (c.vgm_type == DEVID_YM2612 || c.vgm_type == DEVID_YMF262 || c.vgm_type == DEVID_YM2413 ||
          c.vgm_type == DEVID_YM3812) {
        auto &dopt = dev_opts[i];
        dopt.emuCore = FCC_NUKE;
        if (c.vgm_type == DEVID_YM2612) dopt.coreOpts |= OPT_YM2612_TYPE_OPN2C_ASIC;
      } else if (c.vgm_type == DEVID_SN76496) {
        auto &dopt = dev_opts[i];
        dopt.emuCore = FCC_MAXM;
      }

      backend_.SetDeviceOptions(PLR_DEV_ID(info_list_[c.dev_idx].devid, c.instance), dev_opts[i]);
    }
  }

  void handle_loaded_device_info() {
    std::vector<PLR_DEV_INFO> dev_info_list;
    backend_.GetSongDeviceInfo(dev_info_list);

    chips_pannable_.clear();
    for (const auto &d : dev_info_list) {
      int dev_idx = info_list_.find_index(d.type);
      if (dev_idx < 0) continue;
      if (info_list_[dev_idx].has_panning(d.core))
        chips_pannable_.push_back({dev_idx, d.instance, d.type});
    }

    playback_status_.set_device_info(std::move(dev_info_list));
    dump_info(backend_);
  }

  void update_settings() {
    {
      auto settings_ctx = settings_manager_.read();
      if (!settings_ctx) return;
      const auto &reader = settings_ctx.value();
      reader.load_mute_opts(chips_, mute_opts_);

      for (const auto &c : chips_pannable_) {
        const auto &pan = reader.get_panning_opt(c);
        backend_.SetDevicePanning(PLR_DEV_ID(c.vgm_type, c.instance), pan.data(), pan.size());
      }

      gain_ = std::clamp(reader->gain, 0.0, 4.0);
      surround_ = reader->surround;
    }

    auto it = mute_opts_.begin();
    for (const auto &c : chips_) {
      backend_.SetDeviceMuting(PLR_DEV_ID(c.vgm_type, c.instance), *it++);
    }
  }

  PlayerBase &backend_;
  std::vector<WAVE_32BS> render_buffer_;
  std::vector<float> output_buffer_;
  int n_frames_;
  uint_fast32_t frame_position_;
  uint_fast32_t total_frames_;
  uint_fast32_t fade_frames_;
  float gain_;
  bool surround_;
  uint_fast32_t gap_frames_;

  uint_fast32_t sample_rate_;
  uint_fast32_t loop_count_;
  uint_fast32_t fade_length_;

  SettingsManager &settings_manager_;
  std::vector<InstanceId> chips_, chips_pannable_;
  std::vector<PLR_MUTE_OPTS> mute_opts_;

  PlaybackStatus &playback_status_;
  DeviceConfigList info_list_;
};

}  // namespace

bool Plugin::play(const char *, VFSFile &file) {
  FileHandler fh(file);
  if (!fh.load()) return false;

  PlaybackHandler handler(*fh.player(), settings_manager(), playback_status());

  Tuple t = get_playback_tuple();
  TagHandler::get_tags(*fh.player(), t);
  set_playback_tuple(std::move(t));

  open_audio(FMT_FLOAT, handler.sample_rate(), 2);

  while (!check_stop()) {
    int seek_value = check_seek();
    if (seek_value >= 0) handler.seek(seek_value);

    if (!handler.fill_buffer()) break;

    write_audio(handler.output_ptr(), handler.output_len());
  }

  return true;
}

}  // namespace vgmtk

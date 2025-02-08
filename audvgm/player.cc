#include <libaudcore/runtime.h>
#include <libvgm/emu/EmuCores.h>
#include <libvgm/emu/SoundDevs.h>
#include <libvgm/emu/cores/2612intf.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <libvgm/player/playerbase.hpp>
#include <libvgm/player/vgmplayer.hpp>

#include "config.h"
#include "plugin.h"
#include "tags.h"
#include "vfsloader.h"

namespace vgmtk {

namespace {
constexpr int kNChannels = 2;
constexpr int kBufferSizeSamples = 4096;
constexpr int kRenderBlockSizeSamples = 1024;

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
  }
}

template <typename PosT, typename CntT>
CntT fade_start_index(PosT remaining_frames, PosT fade_frames, CntT num_frames) {
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

template <typename CntT, typename PosT = uint_fast32_t>
void process_frames(float *out, PosT remaining_frames, PosT fade_frames, CntT num_frames,
                    WAVE_32BS *data, float gain, bool surround) {
  const float gains[2]{gain / (1 << 23), surround ? -gain / (1 << 23) : gain / (1 << 23)};

  CntT i_fade_start = fade_start_index(remaining_frames, fade_frames, num_frames);

  for (CntT i = 0; i < num_frames; ++i) {
    float volume[2] = {gains[0], gains[1]};

    if (i >= i_fade_start) {
      float fade_volume = static_cast<float>(remaining_frames - i) / fade_frames;
      fade_volume *= fade_volume;

      volume[0] *= fade_volume;
      volume[1] *= fade_volume;
    }

    *out++ = std::clamp(static_cast<double>(data[i].L) * volume[0], -1.0, 1.0);
    *out++ = std::clamp(static_cast<double>(data[i].R) * volume[1], -1.0, 1.0);
  }
}

class PlaybackHandler {
 public:
  explicit PlaybackHandler(PlayerBase &backend_player, SettingsManager &settings_manager,
                           PlaybackStatus &playback_status,
                           DeviceConfigList info_list = vgmtk::lib_devices)
      : backend_(backend_player),
        n_frames_(0),
        frame_position_(0),
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
      const uint_fast32_t remaining_frames = total_frames_ - frame_position_;
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
    auto settings_ctx = settings_manager_.read();
    gain_ = std::clamp(settings_ctx->gain, 0.0, 4.0);
    surround_ = settings_ctx->surround;
  }

  void init_device_options() {
    std::vector<PLR_DEV_OPTS> dev_opts(chips_.size());

    {
      auto settings_ctx = settings_manager_.read();
      settings_ctx.load_dev_opts(chips_, dev_opts);
      settings_ctx.load_mute_opts(chips_, mute_opts_);
      // FIXME: initial panning options
    }
    for (size_t i = 0; i < chips_.size(); ++i) {
      const auto &c = chips_[i];
      dev_opts[i].muteOpts = mute_opts_[i];
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
      auto settings_ctx = settings_manager_.try_read();
      if (!settings_ctx) return;
      const auto &reader = settings_ctx.value();
      reader.load_mute_opts(chips_, mute_opts_);

      for (const auto &c : chips_pannable_) {
        const auto &pan = reader.get_panning_opt(c);
        PLR_DEV_OPTS dev_opts{};
        backend_.GetDeviceOptions(PLR_DEV_ID(c.vgm_type, c.instance), dev_opts);
        std::copy(pan.begin(), pan.end(), &dev_opts.panOpts.chnPan[0][0]);
        backend_.SetDeviceOptions(PLR_DEV_ID(c.vgm_type, c.instance), dev_opts);
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

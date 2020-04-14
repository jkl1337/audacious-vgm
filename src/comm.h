#ifndef AUDACIOUS_VGM_COMM_H
#define AUDACIOUS_VGM_COMM_H

#include <atomic>
#include <cstring>
#include <functional>
#include <mutex>
#include <player/playerbase.hpp>
#include <shared_mutex>
#include <vector>

#include "libvgm-int.h"

namespace audvgm {

class SettingsManager {
 public:
  class WriteContext {
   public:
    WriteContext(PlayerSettings &backend,
        std::atomic_flag &flag, std::shared_mutex &m) : backend_(backend), flag_(flag), lock_(m) {}

    bool set_mute(int dev_idx, int instance, bool level, PlayerSettings::ChannelMask mask);
    bool set_solo(int dev_idx, int instance, bool level, PlayerSettings::ChannelMask mask);

    bool set_disable(int dev_idx, int instance, bool level, PlayerSettings::DisableMask mask);
    bool set_disable(int dev_idx, bool level, PlayerSettings::DisableMask mask);

    bool set_core(int dev_idx, uint32_t core);
    bool set_core_opts(int dev_idx, uint32_t mask, uint32_t level);

    bool set_panning(int dev_idx, int instance, int channel, int16_t pan);

    bool set_gain(double gain);
    bool set_surround(bool surround);

   private:
    using holder = std::unique_lock<std::shared_mutex>;
    PlayerSettings &backend_;
    std::atomic_flag &flag_;
    holder lock_;

    template <typename T>
    bool write_value(T& dest, const T& src);

    template <typename Func>
    bool store_to_device(int dev_idx, Func f);

    template <typename Func>
    bool store_to_instance(int dev_idx, int instance, Func f);

    template <typename T, typename C>
    bool store_in_range(T &dest, const T &src, const C &lo, const C &hi);
  };

  class ReadContext {
   public:
    const PlayerSettings* operator->() const { return &settings_; }

    void load_dev_opts(int idx, int instance, PLR_DEV_OPTS &opts) const {
      const auto &dev_info = vgm::lib_devices[idx];
      const auto &dcb = settings_.devices[idx];
      opts.emuCore = dcb.core;
      opts.coreOpts = dcb.core_opts;
      load_mute_opts(idx, instance, opts.muteOpts);
    }

    void load_mute_opts(int idx, int instance, PLR_MUTE_OPTS &opts) const {
      const auto &dev_info = vgm::lib_devices[idx];
      const auto &ccb = settings_.devices[idx].chips[instance];
      opts.disable = ccb.disable.to_ulong();

      PlayerSettings::ChannelMask mute_mask = ccb.channel_mute;
      if (ccb.channel_solo.any()) {
        mute_mask |= ~ccb.channel_solo;
      }
      uint_fast32_t mute_bits = mute_mask.to_ulong();

      if (dev_info.has_linked()) {
        unsigned shift = dev_info.num_channels();
        uint_fast32_t mask = (1 << shift) - 1;
        opts.chnMute[0] = mute_bits & mask;
        opts.chnMute[1] = (mute_bits & ~mask) >> shift;
      } else {
        opts.chnMute[0] = mute_bits;
        opts.chnMute[1] = 0;
      }
    }

   private:
    ReadContext(const PlayerSettings &settings, std::shared_mutex &m)
        : settings_(settings), lock_(m) {}
    friend class SettingsManager;
    using holder = std::shared_lock<std::shared_mutex>;
    const PlayerSettings &settings_;
    holder lock_;
  };

  using OptReadContext = std::optional<ReadContext>;

  WriteContext writer() { return WriteContext(current_, notify_, mutex_); }
  OptReadContext read(bool always = false) const {
    if (!notify_.test_and_set(std::memory_order_relaxed) || always) {
      return ReadContext(current_, mutex_);
    }
    return OptReadContext();
  }

  explicit SettingsManager(PlayerSettings ps) : notify_(false), current_(std::move(ps)) {}

  // FIXME: fix the locking mess
  const PlayerSettings &current() const { return current_; }

 private:
  mutable std::atomic_flag notify_;
  mutable std::shared_mutex mutex_;
  PlayerSettings current_;
};

class PlaybackStatus {
 public:
  typedef void (*NotifyFunc)(const PlaybackStatus &, void *);

  ~PlaybackStatus();

  void set_device_info(std::vector<PLR_DEV_INFO> &&dev_info_list);
  std::vector<PLR_DEV_INFO> get_device_info() const;

  void subscribe(NotifyFunc func, void *ctx = nullptr);
  void unsubscribe(NotifyFunc func, void *ctx = nullptr);

 private:
  mutable std::mutex dev_info_lock_;
  std::vector<PLR_DEV_INFO> dev_info_list_;

  static void hook_cb(void *, void *);
  void call_hooks();
  struct HookInfo {
    PlaybackStatus::NotifyFunc func;
    void *ctx;
  };
  std::mutex hook_lock_;
  std::vector<HookInfo> hooks_;
};

}  // namespace audvgm

#endif  // AUDACIOUS_VGM_COMM_H

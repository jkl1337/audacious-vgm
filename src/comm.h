//
// Created by luebsj on 4/9/20.
//

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

class SettingsManager;

class PlayerBackendSettings {
 public:
  explicit PlayerBackendSettings(const PlayerSettings &);

  void commit() {
    settings_update_.clear(std::memory_order_release);
  }

  const PlayerBackendSettings *acquire(bool force=false) const {
    if (!settings_update_.test_and_set(std::memory_order_acquire) || force)
      return this;
    return nullptr;
  }

  void load_dev_opts(int idx, int instance, PLR_DEV_OPTS &opts) const {
    const auto &dev_info = vgm::lib_devices[idx];
    const DeviceControlInternal &cb = device_control_vec_[idx*vgm::kMaxChips + instance];
    opts.emuCore = cb.core.load(std::memory_order_relaxed);
    opts.coreOpts = cb.core.load(std::memory_order_relaxed);
    load_mute_opts(idx, instance, opts.muteOpts);
  }

  void load_mute_opts(int idx, int instance, PLR_MUTE_OPTS &opts) const {
    const auto &dev_info = vgm::lib_devices[idx];
    const DeviceControlInternal &cb = device_control_vec_[idx*vgm::kMaxChips + instance];

    opts.disable = cb.disable.load(std::memory_order_relaxed);
    uint_fast32_t mute = cb.channel_mute.load(std::memory_order_relaxed);

    if (dev_info.has_linked()) {
      unsigned shift = dev_info.num_channels();
      uint_fast32_t mask = (1 << shift) - 1;
      opts.chnMute[0] = mute & mask;
      opts.chnMute[1] = (mute & ~mask) >> shift;
    } else {
      opts.chnMute[0] = mute;
      opts.chnMute[1] = 0;
    }
  }

  float gain() const { return gain_.load(std::memory_order_relaxed); }
  bool surround() const { return surround_.load(std::memory_order_relaxed); }

 private:
#pragma clang diagnostic push
#pragma ide diagnostic ignored "cppcoreguidelines-pro-type-member-init"
#pragma ide diagnostic ignored "hicpp-member-init"
  struct DeviceControlInternal {
    std::atomic<uint_fast32_t> core;
    std::atomic<uint_fast32_t> core_opts;

    static_assert(vgm::kMaxLinked < 8, "Maximum number of linked devices assumed exceeded");
    static_assert(vgm::kMaxChannels <= 32, "Maximum number of device channels assumed exceeded");

    std::atomic<uint_fast8_t> disable;
    std::atomic<uint_fast32_t> channel_mute;
    std::vector<std::atomic<int>> panning;

    DeviceControlInternal(uint32_t core, uint32_t core_opts, uint32_t disable,
                          uint32_t channel_mute, int panning_size)
        : core(core),
          core_opts(core_opts),
          disable(disable),
          channel_mute(channel_mute),
          panning(panning_size) {}
    DeviceControlInternal() = default;

    // We use a reserve and emplace to initialize the atomics array. The reserve requires
    // a copy constructor, but since the array is fresh, it should never be called.
    DeviceControlInternal(const DeviceControlInternal &d) {
      throw std::logic_error("This internal function should never get called.");
    }
  };
#pragma clang diagnostic pop

  std::vector<DeviceControlInternal> device_control_vec_;

  std::atomic<float> gain_;
  std::atomic<bool> surround_;
  mutable std::atomic_flag settings_update_;

  friend class SettingsManager;
};

class SettingsManager {
 public:
  explicit SettingsManager(PlayerSettings ps) : current_(std::move(ps)), backend_(current_) {}

  // FIXME: fix the locking mess
  const PlayerSettings &current() const { return current_; }
  const PlayerBackendSettings &backend() const { return backend_; }

  class Mutator {
   public:
    Mutator(SettingsManager &container, std::shared_mutex &mutex)
        : container_(container), lock_(mutex) {}
    // Mutator(Mutator &&m) noexcept : container_(m.container_), ctx_lock_(std::move(m.ctx_lock_))
    // {}

    bool set_muting(int dev_idx, int chip, PlayerSettings::ChannelMask solo,
                    PlayerSettings::ChannelMask mute);
    bool clear_muting(int dev_idx, int chip, PlayerSettings::ChannelMask solo,
                      PlayerSettings::ChannelMask mute);
    bool set_disable(int dev_idx, int chip, PlayerSettings::DisableMask mask);
    bool clear_disable(int dev_idx, int chip, PlayerSettings::DisableMask mask);

    bool set_core(int dev_idx, uint32_t core);
    bool set_core_opts(int dev_idx, uint32_t core_opts);
    bool clear_core_opts(int dev_idx, uint32_t core_opts);

    bool set_panning(int dev_idx, int chip, int channel, int16_t pan);

    bool set_gain(float gain);
    bool set_surround(bool surround);

    void commit() { lock_.unlock(); container_.commit(); }

   private:
    PlayerSettings::Device *settings_block(PlayerSettings &p, int dev_idx) {
      auto &dev = p.devices;
      if (dev_idx < 0 || dev_idx >= dev.size()) {
        return nullptr;
      }
      return &dev[dev_idx];
    }

    PlayerSettings::Chip *settings_block(PlayerSettings &p, int dev_idx, int chip) {
      auto &dev = p.devices;
      if (dev_idx < 0 || dev_idx >= dev.size() || chip < 0 ||
          chip >= std::tuple_size_v<decltype(PlayerSettings::Device::chips)>) {
        return nullptr;
      }
      return &dev[dev_idx].chips[chip];
    }
    PlayerBackendSettings::DeviceControlInternal *backend_control_block(int dev_idx, int chip = -1) {
      auto &vec = container_.backend_.device_control_vec_;
      if (chip < 0)
        chip = 0;
      int idx =
          dev_idx * static_cast<int>(std::tuple_size_v<decltype(PlayerSettings::Device::chips)>) +
          chip;
      return idx < vec.size() && idx >= 0 ? &vec[idx] : nullptr;
    }

    SettingsManager &container_;
    //FIXME: Remove PlayerSettings &copy_;
    std::unique_lock<std::shared_mutex> lock_;
  };

  Mutator mutate() { return Mutator(*this, mutex_); }

 protected:
  void commit() {
    backend_.commit();
  }

 private:
  PlayerSettings current_;
  PlayerBackendSettings backend_;
  mutable std::shared_mutex mutex_;
};

class PlaybackStatus {
 public:
  typedef void (*NotifyFunc)(const PlaybackStatus &, void *);

  ~PlaybackStatus();

  void set_device_info(std::vector<PLR_DEV_INFO> &&dev_info_list);
  std::vector<PLR_DEV_INFO> device_info() const;

  void hook(NotifyFunc func, void *ctx = nullptr);
  void unhook(NotifyFunc func, void *ctx = nullptr);

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

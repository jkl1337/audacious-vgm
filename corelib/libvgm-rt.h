#ifndef AUDACIOUS_VGM_COMM_H
#define AUDACIOUS_VGM_COMM_H

#include <atomic>
#include <cstring>
#include <functional>
#include <libvgm/player/playerbase.hpp>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <vector>

#include "libvgm-iface.h"
#include "types.h"

namespace vgmtk {

class SettingsManager {
 public:
  class WriteContext {
   public:
    WriteContext(PlayerSettings &backend, std::atomic_bool &flag, std::shared_mutex &m)
        : backend_(backend), notify_(flag), lock_(m) {}

    ~WriteContext() {
      if (changed_) notify_.store(true, std::memory_order_release);
    }
    WriteContext(const WriteContext &) = delete;
    WriteContext &operator=(const WriteContext &) = delete;

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
    bool changed_{false};
    std::atomic_bool &notify_;
    holder lock_;

    template <typename T>
    bool write_value(T &dest, const T &src);

    template <typename Func>
    bool store_to_device(int dev_idx, Func &&f);

    template <typename Func>
    bool store_to_instance(int dev_idx, int instance, Func &&f);

    template <typename T, typename C>
    bool store_in_range(T &dest, const T &src, const C &lo, const C &hi);
  };

  class ReadContext {
   public:
    const PlayerSettings *operator->() const { return &settings_; }

    void load_dev_opts(const std::vector<InstanceId> &devs, std::vector<PLR_DEV_OPTS> &opts);

    void load_mute_opts(const std::vector<InstanceId> &devs,
                        std::vector<PLR_MUTE_OPTS> &opts) const;

    const std::vector<int16_t> &get_panning_opt(const InstanceId &i) const {
      return settings_.chip(i).panning;
    }
    ReadContext(const ReadContext &) = delete;
    ReadContext &operator=(const ReadContext &) = delete;
    ReadContext(const PlayerSettings &settings, std::shared_lock<std::shared_mutex> lock)
        : settings_(settings), lock_(std::move(lock)) {}

   private:
    using holder = std::shared_lock<std::shared_mutex>;
    const PlayerSettings &settings_;
    holder lock_;
  };

  using OptReadContext = std::optional<ReadContext>;

  WriteContext writer() { return {current_, notify_, mutex_}; }
  OptReadContext read() const {
    if (!notify_.load(std::memory_order_acquire)) return {};

    std::shared_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) return {};
    notify_.store(false, std::memory_order_relaxed);
    return OptReadContext(std::in_place, current_, std::move(lock));
  }
  ReadContext read_always() const {
    std::shared_lock lock(mutex_);
    notify_.store(false, std::memory_order_relaxed);
    return {current_, std::move(lock)};
  }

  explicit SettingsManager(PlayerSettings ps) : notify_(false), current_(std::move(ps)) {}

  const PlayerSettings &current() const { return current_; }

 private:
  mutable std::atomic_bool notify_;
  static_assert(std::atomic_bool::is_always_lock_free, "atomic_bool not lock free.");
  mutable std::shared_mutex mutex_;
  PlayerSettings current_;
};

}  // namespace vgmtk

#endif  // AUDACIOUS_VGM_COMM_H

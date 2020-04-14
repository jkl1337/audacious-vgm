//
// Created by luebsj on 4/9/20.
//

#include "comm.h"

#include <libaudcore/hook.h>

using namespace audvgm;

constexpr static const char *kUpdateEvent = "audvgm player update";

PlaybackStatus::~PlaybackStatus() {
  std::lock_guard lock(hook_lock_);
  if (!hooks_.empty()) hook_dissociate(kUpdateEvent, hook_cb, this);
}

void PlaybackStatus::set_device_info(std::vector<PLR_DEV_INFO> &&dev_info_list) {
  std::unique_lock lock(dev_info_lock_);
  std::swap(dev_info_list_, dev_info_list);
  lock.unlock();
  event_queue(kUpdateEvent, this);
}

std::vector<PLR_DEV_INFO> PlaybackStatus::get_device_info() const {
  std::lock_guard lock(dev_info_lock_);
  return dev_info_list_;
}

void PlaybackStatus::subscribe(PlaybackStatus::NotifyFunc func, void *ctx) {
  std::lock_guard lock(hook_lock_);
  if (hooks_.empty()) hook_associate(kUpdateEvent, hook_cb, this);
  hooks_.push_back(HookInfo{func, ctx});
}

void PlaybackStatus::unsubscribe(PlaybackStatus::NotifyFunc func, void *ctx) {
  std::lock_guard lock(hook_lock_);
  hooks_.erase(std::remove_if(hooks_.begin(), hooks_.end(),
                              [=](const HookInfo &info) {
                                return info.func == func && (ctx == info.ctx || !ctx);
                              }),
               hooks_.end());
  if (hooks_.empty()) hook_dissociate(kUpdateEvent, hook_cb, this);
}

void PlaybackStatus::hook_cb(void *data, void *ctx) {
  static_cast<PlaybackStatus *>(data)->call_hooks();
}

void PlaybackStatus::call_hooks() {
  std::unique_lock lock(hook_lock_);
  auto hooks = hooks_;
  lock.unlock();
  for (const auto &h : hooks) {
    h.func(*this, h.ctx);
  }
}

template <typename T>
bool SettingsManager::WriteContext::write_value(T &dest, const T &src) {
  if (src != dest) {
    dest = src;
    flag_.clear(std::memory_order_relaxed);
    return true;
  }
  return false;
}

template <typename Func>
bool SettingsManager::WriteContext::store_to_device(int dev_idx, Func f) {
  if (dev_idx < 0 || dev_idx >= backend_.devices.size()) {
    return false;
  }
  return f(backend_.devices[dev_idx]);
}

template <typename Func>
bool SettingsManager::WriteContext::store_to_instance(int dev_idx, int instance, Func f) {
  if (dev_idx < 0 || dev_idx >= backend_.devices.size() || instance < 0
      || instance >= vgm::kNumInstances) {
    return false;
  }
  return f(backend_.devices[dev_idx].chips[instance]);
}

template <typename T, typename C>
bool SettingsManager::WriteContext::store_in_range(T &dest, const T &src, const C &lo,
                                                   const C &hi) {
  if (src < lo || src > hi) return false;
  if (src != dest) {
    dest = src;
    flag_.clear();
    return true;
  }
  return false;
}

bool SettingsManager::WriteContext::set_mute(int dev_idx, int instance, bool level,
                                             PlayerSettings::ChannelMask mask) {
  return store_to_instance(dev_idx, instance, [=](PlayerSettings::Chip &c) {
    PlayerSettings::ChannelMask apply;
    if (level) apply = apply.set();
    return write_value(c.channel_mute, (c.channel_mute & ~mask) | (apply & mask));
  });
}

bool SettingsManager::WriteContext::set_solo(int dev_idx, int instance, bool level,
                                             PlayerSettings::ChannelMask mask) {
  return store_to_instance(dev_idx, instance, [=](PlayerSettings::Chip &c) {
    PlayerSettings::ChannelMask apply;
    if (level) apply = apply.set();
    return write_value(c.channel_solo, (c.channel_solo & ~mask) | (apply & mask));
  });
}

bool SettingsManager::WriteContext::set_disable(int dev_idx, int instance, bool level,
                                                PlayerSettings::DisableMask mask) {
  return store_to_instance(dev_idx, instance, [=](PlayerSettings::Chip &c) {
    PlayerSettings::DisableMask apply;
    if (level) apply = apply.set();
    return write_value(c.disable, (c.disable & ~mask) | (apply & mask));
  });
}

bool SettingsManager::WriteContext::set_disable(int dev_idx, bool level,
                                                PlayerSettings::DisableMask mask) {
  return store_to_device(dev_idx, [=](PlayerSettings::Device &d) {
    PlayerSettings::DisableMask apply;
    if (level) apply = apply.set();
    bool ret = false;
    for (auto &c : d.chips) {
      ret |= write_value(c.disable, (c.disable & ~mask) | (apply & mask));
    }
    return ret;
  });
}

bool SettingsManager::WriteContext::set_core(int dev_idx, uint32_t core) {
  return store_to_device(dev_idx, [=](auto &d) {
    return write_value(d.core, core);
  });
}

bool SettingsManager::WriteContext::set_core_opts(int dev_idx, uint32_t mask, uint32_t level) {
  return store_to_device(dev_idx, [=](auto &d) {
    return write_value(d.core_opts, (d.core_opts & ~mask) | (level & mask));
  });
}

bool SettingsManager::WriteContext::set_panning(int dev_idx, int instance, int channel,
                                                int16_t pan) {
  return store_to_instance(dev_idx, instance, [=](PlayerSettings::Chip &c) {
    if (channel < 0 || channel >= c.panning.size()) return false;
    return write_value(c.panning[channel], pan);
  });
}

bool SettingsManager::WriteContext::set_gain(double gain) {
  return write_value(backend_.gain, std::clamp(gain, 0.0, 4.0));
}

bool SettingsManager::WriteContext::set_surround(bool surround) {
  return write_value(backend_.surround, surround);
}

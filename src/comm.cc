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

std::vector<PLR_DEV_INFO> PlaybackStatus::device_info() const {
  std::lock_guard lock(dev_info_lock_);
  return dev_info_list_;
}

void PlaybackStatus::hook(PlaybackStatus::NotifyFunc func, void *ctx) {
  std::lock_guard lock(hook_lock_);
  if (hooks_.empty()) hook_associate(kUpdateEvent, hook_cb, this);
  hooks_.push_back(HookInfo{func, ctx});
}

void PlaybackStatus::unhook(PlaybackStatus::NotifyFunc func, void *ctx) {
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

PlayerBackendSettings::PlayerBackendSettings(const PlayerSettings &ps)
    : gain_(ps.gain), surround_(ps.surround), settings_update_(0) {
  device_control_vec_.reserve(ps.devices.size() *
                              std::tuple_size_v<decltype(PlayerSettings::Device::chips)>);

  int i = 0;
  for (const auto &src : ps.devices) {
    for (const auto &src_chip : src.chips) {
      ++i;
      auto mute_mask = src_chip.channel_mute.to_ulong();
      auto solo_mask = src_chip.channel_solo.to_ulong();
      if (solo_mask) {
        mute_mask |= ~solo_mask;
      }
      auto &d = device_control_vec_.emplace_back(
          src.core, src.core_opts, src_chip.disable.to_ulong(),
          mute_mask, src_chip.panning.size());
      for (int i = 0; i < src_chip.panning.size(); ++i) {
        d.panning[i].store(src_chip.panning[i], std::memory_order_relaxed);
      }
    }
  }
  commit();
}

bool SettingsManager::Mutator::set_muting(int dev_idx, int chip, PlayerSettings::ChannelMask solo,
                                            PlayerSettings::ChannelMask mute) {
  auto *sb = settings_block(container_.current_, dev_idx, chip);
  auto *be = backend_control_block(dev_idx, chip);
  if (!be || !sb) return false;

  sb->channel_solo |= solo;
  sb->channel_mute |= mute;

  be->channel_mute.store((~sb->channel_solo | sb->channel_mute).to_ulong(),
                         std::memory_order_relaxed);
  return true;
}

bool SettingsManager::Mutator::clear_muting(int dev_idx, int chip,
                                              PlayerSettings::ChannelMask solo,
                                              PlayerSettings::ChannelMask mute) {
  auto *sb = settings_block(container_.current_, dev_idx, chip);
  auto *be = backend_control_block(dev_idx, chip);
  if (!be || !sb) return false;

  sb->channel_solo &= ~solo;
  sb->channel_mute &= ~mute;

  be->channel_mute.store((~sb->channel_solo | sb->channel_mute).to_ulong(),
                         std::memory_order_relaxed);
  return true;
}

bool SettingsManager::Mutator::set_core(int dev_idx, uint32_t core) {
  auto *sb = settings_block(container_.current_, dev_idx);
  auto *be = backend_control_block(dev_idx);
  if (!be || !sb) return false;

  sb->core = core;
  be->core.store(sb->core, std::memory_order_relaxed);
  return true;
}

bool SettingsManager::Mutator::set_core_opts(int dev_idx, uint32_t core_opts) {
  auto *sb = settings_block(container_.current_, dev_idx);
  auto *be = backend_control_block(dev_idx);
  if (!be || !sb) return false;

  sb->core_opts |= core_opts;
  be->core_opts.store(sb->core_opts, std::memory_order_relaxed);
  return true;
}

bool SettingsManager::Mutator::clear_core_opts(int dev_idx, uint32_t core_opts) {
  auto *sb = settings_block(container_.current_, dev_idx);
  auto *be = backend_control_block(dev_idx);
  if (!be || !sb) return false;

  sb->core_opts &= ~core_opts;
  be->core_opts.store(sb->core_opts, std::memory_order_relaxed);
  return true;
}

bool SettingsManager::Mutator::set_gain(float gain) {
  container_.current_.gain = gain;
  container_.backend_.gain_.store(gain, std::memory_order_relaxed);
  return true;
}

bool SettingsManager::Mutator::set_surround(bool surround) {
  container_.current_.surround = surround;
  container_.backend_.surround_.store(surround, std::memory_order_relaxed);
  return true;
}

bool SettingsManager::Mutator::set_panning(int dev_idx, int chip, int channel, int16_t pan) {
  auto *sb = settings_block(container_.current_, dev_idx, chip);
  auto *be = backend_control_block(dev_idx, chip);
  if (!be || !sb || channel < 0 || channel > sb->panning.size()) return false;

  sb->panning[channel] = pan;
  be->panning[channel].store(pan, std::memory_order_relaxed);
  return true;
}

bool SettingsManager::Mutator::set_disable(int dev_idx, int chip, PlayerSettings::DisableMask mask) {
  auto *sb = settings_block(container_.current_, dev_idx, chip);
  auto *be = backend_control_block(dev_idx, chip);
  if (!be || !sb) return false;

  sb->disable |= mask;
  be->disable.store(sb->disable.to_ulong(), std::memory_order_relaxed);
  return true;
}

bool SettingsManager::Mutator::clear_disable(int dev_idx, int chip, PlayerSettings::DisableMask mask) {
  auto *sb = settings_block(container_.current_, dev_idx, chip);
  auto *be = backend_control_block(dev_idx, chip);
  if (!be || !sb) return false;

  sb->disable &= ~mask;
  be->disable.store(sb->disable.to_ulong(), std::memory_order_relaxed);
  return true;
}

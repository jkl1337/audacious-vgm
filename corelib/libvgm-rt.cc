#include "libvgm-rt.h"

using namespace vgmtk;

void SettingsManager::ReadContext::load_mute_opts(const std::vector<InstanceId> &devs,
                                                  std::vector<PLR_MUTE_OPTS> &opts) const {
  auto output = opts.begin();

  bool solo = std::any_of(devs.begin(), devs.end(), [this](const InstanceId &i){
    return settings_.chip(i).channel_solo.any();
  });
  for (const auto &i : devs) {
    const auto &dev_info = vgmtk::lib_devices[i.dev_idx];
    const auto &ccb = settings_.chip(i);

    output->disable = ccb.disable.to_ulong();

    PlayerSettings::ChannelMask mute_mask = ccb.channel_mute;
    if (solo) {
      mute_mask |= ~ccb.channel_solo;
    }
    uint_fast64_t mute_bits = mute_mask.to_ullong();

    if (dev_info.has_linked()) {
      unsigned shift = dev_info.num_channels();
      uint_fast64_t mask = (1ULL << shift) - 1;
      output->chnMute[0] = static_cast<UINT32>(mute_bits & mask);
      output->chnMute[1] = static_cast<UINT32>((mute_bits & ~mask) >> shift);
    } else {
      output->chnMute[0] = static_cast<UINT32>(mute_bits);
      output->chnMute[1] = 0;
    }
    ++output;
  }
}

void SettingsManager::ReadContext::load_dev_opts(const std::vector<InstanceId> &devs,
                                                 std::vector<PLR_DEV_OPTS> &opts) {

  auto output = opts.begin();
  for (const auto &i : devs) {
    const auto &dev_info = vgmtk::lib_devices[i.dev_idx];
    const auto &dcb = settings_.device(i);

    output->emuCore[0] = dcb.core;
    output->coreOpts = dcb.core_opts;
    ++output;
  }
}

template <typename T>
bool SettingsManager::WriteContext::write_value(T &dest, const T &src) {
  if (src != dest) {
    dest = src;
    changed_ = true;
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
      || instance >= vgmtk::kNumInstances) {
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
    changed_ = true;
    return true;
  }
  return false;
}

bool SettingsManager::WriteContext::set_mute(int dev_idx, int instance, bool level,
                                             PlayerSettings::ChannelMask mask) {
  // FIXME: thinking maybe this should only mess with the "valid" bits
  return store_to_instance(dev_idx, instance, [level, this, mask](PlayerSettings::Chip &c) {
    PlayerSettings::ChannelMask apply;
    if (level) apply = apply.set();
    return write_value(c.channel_mute, (c.channel_mute & ~mask) | (apply & mask));
  });
}

bool SettingsManager::WriteContext::set_solo(int dev_idx, int instance, bool level,
                                             PlayerSettings::ChannelMask mask) {
  return store_to_instance(dev_idx, instance, [level, this, mask](PlayerSettings::Chip &c) {
    PlayerSettings::ChannelMask apply;
    if (level) apply = apply.set();
    return write_value(c.channel_solo, (c.channel_solo & ~mask) | (apply & mask));
  });
}

bool SettingsManager::WriteContext::set_disable(int dev_idx, int instance, bool level,
                                                PlayerSettings::DisableMask mask) {
  return store_to_instance(dev_idx, instance, [level, this, mask](PlayerSettings::Chip &c) {
    PlayerSettings::DisableMask apply;
    if (level) apply = apply.set();
    return write_value(c.disable, (c.disable & ~mask) | (apply & mask));
  });
}

bool SettingsManager::WriteContext::set_disable(int dev_idx, bool level,
                                                PlayerSettings::DisableMask mask) {
  return store_to_device(dev_idx, [level, this, mask](PlayerSettings::Device &d) {
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
  return store_to_device(dev_idx, [this, core](auto &d) {
    return write_value(d.core, core);
  });
}

bool SettingsManager::WriteContext::set_core_opts(int dev_idx, uint32_t mask, uint32_t level) {
  return store_to_device(dev_idx, [this, mask, level](auto &d) {
    return write_value(d.core_opts, (d.core_opts & ~mask) | (level & mask));
  });
}

bool SettingsManager::WriteContext::set_panning(int dev_idx, int instance, int channel,
                                                int16_t pan) {
  return store_to_instance(dev_idx, instance, [channel, this, pan](PlayerSettings::Chip &c) {
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

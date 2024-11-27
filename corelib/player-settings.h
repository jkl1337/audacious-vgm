#ifndef AUDACIOUS_VGM_LIBVGM_INT_H
#define AUDACIOUS_VGM_LIBVGM_INT_H

#include <array>
#include <bitset>
#include <cstdint>
#include <vector>

#include "device-config.h"
#include "types.h"

namespace vgmtk {

struct PlayerSettings {
  using DisableMask = std::bitset<max_linked>;
  using ChannelMask = std::bitset<max_all_channels>;

  struct Chip {
    DisableMask disable = 0;
    ChannelMask channel_solo = 0;
    ChannelMask channel_mute = 0;
    std::vector<int16_t> panning;
  };

  struct Device {
    uint32_t core = 0;
    uint32_t core_opts = 0;
    std::array<Chip, num_instances> chips;
  };

  std::vector<Device> devices;

  double gain = 1.0;
  bool surround = false;

  static PlayerSettings init_lib_defaults();

  DeviceInfoRange<std::span<Device>> devices_with_meta() {
    return DeviceInfoRange(std::span(devices));
  }
  DeviceInfoRange<std::span<const Device>> devices_with_meta() const {
    return DeviceInfoRange(std::span(devices));
  }

  const Chip &chip(const InstanceId &i) const { return devices[i.dev_idx].chips[i.instance]; }
  const Device &device(const InstanceId &i) const { return devices[i.dev_idx]; }
  const Device &device(int dev_idx) const { return devices[dev_idx]; }
};

}  // namespace vgmtk

#endif  // AUDACIOUS_VGM_LIBVGM_INT_H

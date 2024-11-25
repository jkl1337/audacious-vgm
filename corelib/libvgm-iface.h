#ifndef AUDACIOUS_VGM_LIBVGM_INT_H
#define AUDACIOUS_VGM_LIBVGM_INT_H

#include <array>
#include <bitset>
#include <cstdint>
#include <mutex>
#include <vector>

#include "libvgm-dev-meta.h"
#include "types.h"

namespace vgmtk {

struct PlayerSettings {
  typedef std::bitset<kMaxLinked> DisableMask;
  typedef std::bitset<kMaxAllChannels> ChannelMask;

  struct Chip {
    DisableMask disable = 0;
    ChannelMask channel_solo = 0;
    ChannelMask channel_mute = 0;
    std::vector<int16_t> panning;
  };

  struct Device {
    uint32_t core = 0;
    uint32_t core_opts = 0;
    std::array<Chip, kNumInstances> chips;
  };

  std::vector<Device> devices;

  double gain = 1.0;
  bool surround = false;

  static PlayerSettings init_lib_defaults();

  DevInfoRange<std::vector<Device>> devices_with_meta() { return DevInfoRange(devices); }
  const Chip &chip(const InstanceId &i) const { return devices[i.dev_idx].chips[i.instance]; }
  const Device &device(const InstanceId &i) const { return devices[i.dev_idx]; }
};

class PlaybackStatus {
 public:
  typedef void (*NotifyFunc)(const PlaybackStatus &, void *);

  ~PlaybackStatus();

  void set_device_info(std::vector<PLR_DEV_INFO> dev_info_list);
  std::vector<PLR_DEV_INFO> get_device_info() const;

  void subscribe(NotifyFunc func, void *ctx = nullptr);
  void unsubscribe(NotifyFunc func, void *ctx = nullptr);

 private:
  mutable std::mutex dev_info_lock_;
  std::vector<PLR_DEV_INFO> dev_info_list_;

  static void hook_cb(void *, void *);
  void call_hooks();
  struct HookInfo {
    NotifyFunc func;
    void *ctx;
  };
  std::mutex hook_lock_;
  std::vector<HookInfo> hooks_;
};
}  // namespace vgmtk

#endif  // AUDACIOUS_VGM_LIBVGM_INT_H

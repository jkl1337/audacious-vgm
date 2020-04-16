//
// Created by luebsj on 4/11/20.
//

#ifndef AUDACIOUS_VGM_LIBVGM_INT_H
#define AUDACIOUS_VGM_LIBVGM_INT_H

#include <array>
#include <bitset>
#include <cstdint>
#include <vector>

#include "types.h"
#include "libvgm-dev-meta.h"

namespace vgm {

template <class C, class T = typename C::value_type>
struct DevInfoRange {
  C &v;
 public:
  template <class R>
  struct Tuple {
    R &dev;
    const vgm::DeviceConfig &dev_info;
  };

  template <class BaseIt, class V>
  class IterT {
    BaseIt it;
    typename C::difference_type idx;
   public:

    using iterator_category = std::forward_iterator_tag;
    using difference_type = long;
    using value_type = V;
    using pointer = void;
    using reference = void;

    explicit constexpr IterT(BaseIt it, typename C::difference_type idx = 0) : it(it), idx(idx) {}
    constexpr value_type operator*() const { return value_type{ *it, vgm::lib_devices[idx]}; }
    bool operator!=(const IterT & other) const { return it != other.it; }
    void operator++() { it++; idx++; }
  };
  using iterator = IterT<typename C::iterator, Tuple<T>>;
  using const_iterator = IterT<typename C::const_iterator, Tuple<const T>>;
  using const_reference = Tuple<T>;

  explicit DevInfoRange(C &v) : v(v) {}

  constexpr iterator begin() { return iterator{ v.begin() }; }
  constexpr iterator end() { return iterator{ v.end(), std::distance(v.begin(), v.end())};}
  constexpr const_iterator begin() const { return const_iterator{ v.cbegin() }; }
  constexpr const_iterator end() const { return const_iterator{ v.cend(), std::distance(v.cbegin(), v.cend())};}
  constexpr const DevInfoRange &crange() const noexcept { return *this; }
};

struct PlayerSettings {
 public:
  typedef std::bitset<vgm::kMaxLinked> DisableMask;
  typedef std::bitset<vgm::kMaxChannels> ChannelMask;

  struct Chip {
    DisableMask disable = 0;
    ChannelMask channel_solo = 0;
    ChannelMask channel_mute = 0;
    std::vector<int16_t> panning;
  };

  struct Device {
    uint32_t core = 0;
    uint32_t core_opts = 0;
    std::array<Chip, vgm::kNumInstances> chips;
  };

  std::vector<Device> devices;

  double gain = 1.0;
  bool surround = false;

  static PlayerSettings init_lib_defaults();

  DevInfoRange<std::vector<Device>> devices_with_meta() { return DevInfoRange(devices); }
  const Chip& chip(const InstanceId &i) const { return devices[i.dev_idx].chips[i.instance]; }
  const Device& device(const InstanceId &i) const { return devices[i.dev_idx]; }
};

}

#endif  // AUDACIOUS_VGM_LIBVGM_INT_H

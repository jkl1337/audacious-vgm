#ifndef VGM_DEV_META_H_
#define VGM_DEV_META_H_

#include <array>
#include <cstdint>
#include <vector>

#pragma clang diagnostic push

namespace vgm {

#pragma clang diagnostic push
#pragma ide diagnostic ignored "google-explicit-constructor"
template <class T>
struct ArrayRef {
  const T *data;
  int len;

  constexpr ArrayRef(decltype(nullptr) = nullptr) : data(nullptr), len(0) {}

  template <size_t N>
  constexpr ArrayRef(const T (&array)[N]) : data(array), len(N) {}

  template <size_t N>
  constexpr ArrayRef(const std::array<T, N> &a) : data(a.data()), len(N) {}

  constexpr ArrayRef(const T *data, int len) : data(data), len(len) {}

  constexpr const T& operator[](int pos) const { return data[pos]; }

  const T *begin() const { return data; }
  const T *end() const { return data + len; }
};
#pragma clang diagnostic pop

const uint_least32_t CoreInvalid = UINT32_MAX;

struct ChannelGroup {
  int serial = 0;
  const ArrayRef<const char *> named;
  const char group[8] = {};

  constexpr bool valid() const { return serial || named.len; }
};

struct SwitchOption {
  const char *label = nullptr;
  const char *doc = nullptr;
  uint_least32_t value = 0;

  constexpr bool valid() const { return value; }
};

struct ChoiceOption {
  const char *label = nullptr;
  const char *doc = nullptr;

  ArrayRef<SwitchOption> choices;
};

struct OptionSpec {
  enum Type { None, Switch, Choice };
  Type type;
  union {
    SwitchOption sw;
    ChoiceOption choice;
  };
  uint_least32_t core = CoreInvalid;

  constexpr OptionSpec(const char *label, const char *doc, uint_least32_t value,
                       uint_least32_t core = CoreInvalid)
      : type(Switch), sw{label, doc, value} {}

  constexpr OptionSpec(const char *label, const char *doc,
                       const ArrayRef<SwitchOption> choices,
                       uint_least32_t core = CoreInvalid)
      : type(Choice), choice{label, doc, choices} {}

  constexpr OptionSpec() : type(None), sw() {}
};

struct CoreSpec {
  uint_least32_t core = CoreInvalid;
  const char label[12] = {};
  bool panning = false;

  constexpr bool valid() const { return core != CoreInvalid; }
};

struct LinkSpec {
  uint_least32_t id = CoreInvalid;
  const char prefix[4] = {};

  constexpr bool valid() const { return id != CoreInvalid; }
};

struct DeviceConfig;

extern const DeviceConfig lib_devices[];
extern const size_t num_devices;
extern const ArrayRef<DeviceConfig> lib_devices_a;

struct DeviceConfig {
  uint_least32_t id = CoreInvalid;
  const char label[12] = {};

  ArrayRef<ChannelGroup> channels;
  ArrayRef<CoreSpec> cores;

  ArrayRef<OptionSpec> options;

  LinkSpec linked;

  constexpr bool has_linked() const { return linked.valid(); }
  constexpr bool valid() const { return id != CoreInvalid; }

  bool has_panning() const {
    for (const auto &core : cores) {
      if (core.panning) return true;
    }
    return false;
  }

  int num_channels() const {
    int count = 0;
    for (const auto &c : channels) {
      count += c.serial;
      count += c.named.len;
    }
    return count;
  }

  int num_channels_all() const {
    int count = num_channels();
    if (has_linked()) {
      for (const auto &d : lib_devices_a) {
        if (d.id == linked.id) {
          count += d.num_channels();
          break;
        }
      }
    }
    return count;
  }

};

}  // namespace vgm

#endif  // VGM_DEV_META_H_

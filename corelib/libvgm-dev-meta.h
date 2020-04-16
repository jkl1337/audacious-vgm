#ifndef VGM_DEV_META_H_
#define VGM_DEV_META_H_

#include <array>
#include <cstdint>
#include <vector>

#pragma clang diagnostic push

struct PLR_DEV_INFO;

namespace vgmtk {

constexpr const int kNumDevices = 41;
constexpr const int kNumInstances = 2;
constexpr const int kMaxLinked = 2;
constexpr const int kMaxChannels = 32;

constexpr const uint_least32_t kCoreInvalid = UINT32_MAX;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "google-explicit-constructor"
#pragma ide diagnostic ignored "hicpp-explicit-conversions"
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

  constexpr const T &operator[](int pos) const { return data[pos]; }
  constexpr int size() const { return len; }

  const T *begin() const { return data; }
  const T *end() const { return data + len; }
};
#pragma clang diagnostic pop

struct ChannelGroup {
  int serial = 0;
  const ArrayRef<const char *> named;
  const char group[8] = {};

  constexpr bool valid() const { return serial || named.len; }
  constexpr const char* name() const { return group[0] ? group : nullptr; }
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
  uint_least32_t core = kCoreInvalid;

  constexpr OptionSpec(const char *label, const char *doc, uint_least32_t value,
                       uint_least32_t core = kCoreInvalid)
      : type(Switch), sw{label, doc, value}, core(core) {}

  constexpr OptionSpec(const char *label, const char *doc, const ArrayRef<SwitchOption> choices,
                       uint_least32_t core = kCoreInvalid)
      : type(Choice), choice{label, doc, choices}, core(core) {}

  constexpr OptionSpec() : type(None), sw() {}
};

struct CoreSpec {
  uint_least32_t core = kCoreInvalid;
  const char label[12] = {};
  bool panning = false;

  constexpr bool valid() const { return core != kCoreInvalid; }
};

class DeviceConfig;

struct LinkSpec {
  const DeviceConfig *dev = nullptr;
  const char prefix[4] = {};

  constexpr bool valid() const { return dev != nullptr; }
};

const char *accurate_chip_name(const PLR_DEV_INFO &);
const char *core_name(uint32_t core_id);

class DeviceConfigList;
extern const size_t num_devices;
extern const DeviceConfigList lib_devices;

struct DeviceConfig {
  uint_least32_t devid = kCoreInvalid;
  const char label[12] = {};

  ArrayRef<ChannelGroup> channels;
  ArrayRef<CoreSpec> cores;

  ArrayRef<OptionSpec> options;

  LinkSpec linked;

  constexpr int num_cores() const { return cores.len; }
  constexpr bool has_options() const { return options.len; }
  constexpr bool has_linked() const { return linked.valid(); }
  constexpr bool valid() const { return devid != kCoreInvalid; }

  const DeviceConfig *linked_config() const { return linked.dev; }

  bool has_panning(uint32_t core = 0) const {
    for (const auto &c : cores) {
      if (c.panning && (!core || c.core == core)) return true;
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
    return num_channels() + (has_linked() ? linked_config()->num_channels() : 0);
  }

  const char *core_name(uint32_t core_id = 0) const {
    if (!core_id) core_id = cores[0].core;
    for (const auto &c : cores) {
      if (c.core == core_id) {
        if (c.label[0]) return c.label;
        break;
      }
    }
    return vgmtk::core_name(core_id);
  }
};

class DeviceConfigList : public ArrayRef<DeviceConfig> {
 public:
  constexpr DeviceConfigList(const DeviceConfig *data, int len)
      : ArrayRef<DeviceConfig>(data, len) {}

  const DeviceConfig *find(unsigned int devid) const {
    for (const auto *it = begin(); it != end(); ++it) {
      if (it->devid == devid) return it;
    }
    return nullptr;
  }

  int find_index(unsigned int devid) const {
    const auto *d = find(devid);
    return d ? d - begin() : -1;
  }
};

// Utility class for zipping over elements of container with
template <class Container, class Element = typename Container::value_type>
struct DevInfoRange {
  Container &v;
 public:
  template <class TElem>
  struct Tuple {
    TElem &dev;
    const vgmtk::DeviceConfig &dev_info;
  };

  template <class BaseIterator, class ValueType>
  class IterT {
    BaseIterator it;
    typename Container::difference_type idx;
   public:

    using iterator_category = std::forward_iterator_tag;
    using difference_type = long;
    using value_type = ValueType;
    using pointer = void;
    using reference = void;

    explicit constexpr IterT(BaseIterator it, typename Container::difference_type idx = 0) : it(it), idx(idx) {}
    constexpr value_type operator*() const { return value_type{ *it, vgmtk::lib_devices[idx]}; }
    bool operator!=(const IterT & other) const { return it != other.it; }
    void operator++() { it++; idx++; }
  };
  using iterator = IterT<typename Container::iterator, Tuple<Element>>;
  using const_iterator = IterT<typename Container::const_iterator, Tuple<const Element>>;
  using const_reference = Tuple<Element>;

  explicit DevInfoRange(Container &v) : v(v) {}

  constexpr iterator begin() { return iterator{ v.begin() }; }
  constexpr iterator end() { return iterator{ v.end(), std::distance(v.begin(), v.end())};}
  constexpr const_iterator begin() const { return const_iterator{ v.cbegin() }; }
  constexpr const_iterator end() const { return const_iterator{ v.cend(), std::distance(v.cbegin(), v.cend())};}
  constexpr const DevInfoRange &crange() const noexcept { return *this; }
};

}  // namespace vgm

#endif  // VGM_DEV_META_H_

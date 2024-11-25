#ifndef VGM_DEV_META_H_
#define VGM_DEV_META_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

struct PLR_DEV_INFO;

namespace vgmtk {

constexpr int num_instances = 2;
constexpr int max_linked = 2;
constexpr int max_channels = 32;
constexpr int max_all_channels = 64;

constexpr uint_least32_t core_invalid = std::numeric_limits<uint_least32_t>::max();

struct ChannelGroup {
  int serial = 0;
  std::span<const char *const> named;
  std::array<char, 8> group;

  constexpr bool valid() const { return serial || named.size(); }
  constexpr std::string_view name() const { return group[0] ? group.data() : std::string_view(); }
};

struct OnOffOption {
  const char *label = nullptr;
  const char *doc = nullptr;
  uint_least32_t value = 0;

  constexpr bool valid() const { return value; }
};

struct ChoiceOption {
  const char *label = nullptr;
  const char *doc = nullptr;

  const std::span<const OnOffOption> choices;
};

struct OptionSpec {
  enum class Type : uint8_t { none, onoff, choice };
  Type type;
  union {
    OnOffOption sw;
    ChoiceOption choice;
  };
  uint_least32_t core = core_invalid;

  constexpr OptionSpec(const char *label, const char *doc, uint_least32_t value,
                       uint_least32_t core = core_invalid)
      : type(Type::onoff), sw{label, doc, value}, core(core) {}

  constexpr OptionSpec(const char *label, const char *doc, std::span<const OnOffOption> choices,
                       uint_least32_t core = core_invalid)
      : type(Type::choice), choice{label, doc, choices}, core(core) {}

  constexpr OptionSpec() : type(Type::none), sw() {}
};

struct CoreSpec {
  uint_least32_t core = core_invalid;
  char label[12]{};
  bool panning = false;

  constexpr bool valid() const { return core != core_invalid; }
};

class DeviceConfig;

struct LinkSpec {
  const DeviceConfig *dev = nullptr;
  char prefix[4]{};

  constexpr bool valid() const { return dev != nullptr; }
};

const char *accurate_chip_name(const PLR_DEV_INFO &);
const char *core_name(uint32_t core_id);

class DeviceConfigList;
extern const std::size_t num_devices;
extern const DeviceConfigList lib_devices;

class DeviceConfig {
 public:
  uint_least32_t devid{core_invalid};
  char label[12]{};

  std::span<const ChannelGroup> channels;
  std::span<const CoreSpec> cores;
  std::span<const OptionSpec> options;

  LinkSpec linked;

  constexpr int num_cores() const { return cores.size(); }
  constexpr bool has_options() const { return options.size(); }
  constexpr bool has_linked() const { return linked.valid(); }
  constexpr bool valid() const { return devid != core_invalid; }

  const DeviceConfig *linked_config() const { return linked.dev; }

  bool has_panning(uint32_t core = 0) const {
    return std::any_of(cores.begin(), cores.end(),
                       [core](const auto &c) { return c.panning && (!core || c.core == core); });
  }

  int num_channels() const {
    int count = 0;
    for (const auto &[serial, named, group] : channels) {
      count += serial;
      count += named.size();
    }
    return count;
  }

  int num_channels_all() const {
    return num_channels() + (has_linked() ? linked_config()->num_channels() : 0);
  }

  const char *core_name(uint32_t core_id = 0) const {
    if (!core_id) core_id = cores[0].core;
    for (const auto &[core, label, panning] : cores) {
      if (core == core_id) {
        if (label[0]) return label;
        break;
      }
    }
    return vgmtk::core_name(core_id);
  }
};

class DeviceConfigList : public std::span<const DeviceConfig> {
 public:
  constexpr DeviceConfigList(const DeviceConfig *data, int len)
      : std::span<const DeviceConfig>(data, len) {}

  const DeviceConfig *find(unsigned int devid) const {
    for (auto it = begin(); it != end(); ++it) {
      if (it->devid == devid) return &*it;
    }
    return nullptr;
  }

  int find_index(unsigned int devid) const {
    const auto *d = find(devid);
    return d ? d - &*begin() : -1;
  }
};

// Utility class for zipping over elements of container with
template <class Container, class Element = typename Container::value_type>
struct DevInfoRange {
  Container &v;

  template <class TElem>
  struct Tuple {
    TElem &dev;
    const DeviceConfig &dev_info;
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

    explicit constexpr IterT(BaseIterator it, typename Container::difference_type idx = 0)
        : it(it), idx(idx) {}
    constexpr value_type operator*() const { return value_type{*it, lib_devices[idx]}; }
    bool operator!=(const IterT &other) const { return it != other.it; }
    void operator++() {
      ++it;
      ++idx;
    }
  };
  using iterator = IterT<typename Container::iterator, Tuple<Element>>;
  using const_iterator = IterT<typename Container::const_iterator, Tuple<const Element>>;
  using const_reference = Tuple<Element>;

  explicit DevInfoRange(Container &v) : v(v) {}
  DevInfoRange(const DevInfoRange &) = delete;

  constexpr iterator begin() { return iterator{v.begin()}; }
  constexpr iterator end() { return iterator{v.end(), std::distance(v.begin(), v.end())}; }
  constexpr const_iterator begin() const { return const_iterator{v.cbegin()}; }
  constexpr const_iterator end() const {
    return const_iterator{v.cend(), std::distance(v.cbegin(), v.cend())};
  }
  constexpr const DevInfoRange &crange() const noexcept { return *this; }
};

}  // namespace vgmtk

#endif  // VGM_DEV_META_H_

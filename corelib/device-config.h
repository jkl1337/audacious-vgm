#ifndef VGM_DEV_META_H_
#define VGM_DEV_META_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <span>
#include <string_view>

struct PLR_DEV_INFO;

namespace vgmtk {

constexpr int num_instances = 2;
constexpr int max_linked = 2;
constexpr int max_channels = 32;
constexpr int max_all_channels = 64;

constexpr uint_least32_t core_invalid = std::numeric_limits<uint_least32_t>::max();

const char *accurate_chip_name(const PLR_DEV_INFO &);
const char *core_name(uint32_t core_id);

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
  constexpr std::string_view name() const { return label[0] ? label : core_name(core); }
};

class DeviceConfig;

struct LinkSpec {
  const DeviceConfig *dev = nullptr;
  char prefix[4]{};

  constexpr bool valid() const { return dev != nullptr; }
};

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

  constexpr const DeviceConfig &get(size_t idx) const {
    if (idx > size()) throw std::out_of_range("DeviceConfigList::get");
    return (*this)[idx];
  }

  const DeviceConfig *find(uint32_t devid) const {
    for (auto it = begin(); it != end(); ++it) {
      if (it->devid == devid) return &*it;
    }
    return nullptr;
  }

  int find_index(uint32_t devid) const {
    const auto *d = find(devid);
    return d ? d - &*begin() : -1;
  }
};

template <typename ForwardIterator>
class DeviceInfoZipIterator {
  ForwardIterator it_;
  std::ptrdiff_t idx_;

 public:
  struct Value {
    std::iter_reference_t<ForwardIterator> dev;  // NOLINT
    const DeviceConfig &meta;                    // NOLINT
  };

  using iterator_category = std::forward_iterator_tag;
  using difference_type = std::iter_difference_t<ForwardIterator>;
  using value_type = Value;
  using pointer = void;
  using reference = value_type;
  using iterator_concept = std::forward_iterator_tag;

  explicit constexpr DeviceInfoZipIterator(ForwardIterator it, std::ptrdiff_t idx = 0)
      : it_(it), idx_(idx) {}

  value_type operator*() const { return {*it_, lib_devices[idx_]}; }
  DeviceInfoZipIterator &operator++() {
    ++it_;
    ++idx_;
    return *this;
  }

  DeviceInfoZipIterator operator++(int) {
    DeviceInfoZipIterator tmp = *this;
    ++*this;
    return tmp;
  }

  bool operator==(const DeviceInfoZipIterator &other) const { return it_ == other.it_; }
  bool operator!=(const DeviceInfoZipIterator &other) const { return !(*this == other); }
  difference_type operator-(const DeviceInfoZipIterator &other) const { return it_ - other.it_; }
};

// Utility class for zipping over elements of a container with device meta data
template <typename View>
class DeviceInfoRange {
  View view_;

 public:
  using iterator = DeviceInfoZipIterator<std::ranges::iterator_t<View>>;
  using const_iterator = DeviceInfoZipIterator<std::ranges::iterator_t<const View>>;

  explicit DeviceInfoRange(View v) : view_(std::move(v)) {}

  constexpr iterator begin() { return iterator{std::ranges::begin(view_)}; }
  constexpr iterator end() {
    return iterator{std::ranges::end(view_), std::ranges::distance(view_)};
  }
  constexpr const_iterator begin() const { return const_iterator{std::ranges::begin(view_)}; }
  constexpr const_iterator end() const {
    return const_iterator{std::ranges::end(view_), std::ranges::distance(view_)};
  }
};

}  // namespace vgmtk

#endif  // VGM_DEV_META_H_

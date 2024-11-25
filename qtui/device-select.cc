#include "device-select.h"

#define N_(String) (String)

#include <QtCore/QAbstractItemModel>

#include "../corelib/libvgm-rt.h"

using namespace vgmqt;

namespace {

constexpr unsigned width_to_mask(unsigned width) { return (1 << width) - 1; }

class NodeLocator {
 public:
  enum class NodeLevel : uint8_t { root = 0, device, instance, linked };

  friend constexpr NodeLevel& operator++(NodeLevel& l) {
    return l = (l == NodeLevel::linked) ? l
                                        : l = static_cast<NodeLevel>(static_cast<uint8_t>(l) + 1);
  }
  friend constexpr NodeLevel& operator--(NodeLevel& l) {
    return l = (l == NodeLevel::root) ? l : l = static_cast<NodeLevel>(static_cast<uint8_t>(l) - 1);
  }

 private:
  class NodeInternal {
    static constexpr unsigned kDevIdxStart = 0, kDevIdxWidth = 9, kInstanceStart = kDevIdxWidth,
                              kInstanceWidth = 5, kLevelStart = kInstanceStart + kInstanceWidth,
                              kLevelWidth = 2;
    static_assert(kDevIdxWidth + kInstanceWidth + kLevelWidth <= 16, "locator bitfield too wide");

    static constexpr unsigned kLevelMask = width_to_mask(kLevelWidth),
                              kDevIdxMask = width_to_mask(kDevIdxWidth),
                              kInstanceMask = width_to_mask(kInstanceWidth);

    uint_fast16_t loc_;

   public:
    explicit constexpr NodeInternal(uintptr_t v = 0) : loc_(v) {}
    constexpr NodeInternal(NodeLevel level, int dev_idx, int instance)
        : loc_(((static_cast<uint8_t>(level) & kLevelMask) << kLevelStart) |
               ((dev_idx & kDevIdxMask) << kDevIdxStart) |
               ((instance & kInstanceMask) << kInstanceStart)) {}

    constexpr operator uintptr_t() const { return loc_; }

    constexpr NodeLevel level() const {
      return static_cast<NodeLevel>((loc_ >> kLevelStart) & kLevelMask);
    }
    constexpr int instance() const { return (loc_ >> kInstanceStart) & kInstanceMask; }
    constexpr int dev_idx() const { return (loc_ >> kDevIdxStart) & kDevIdxMask; }
  };

 public:
  constexpr NodeLocator(int row, NodeInternal v) : row_(row), v_(v) {}
  explicit constexpr NodeLocator(const QModelIndex& mi)
      : row_(mi.isValid() ? mi.row() : 0), v_(mi.isValid() ? mi.internalId() : 0) {}
  constexpr static NodeLocator make_device(int dev_idx) {
    return {dev_idx, {NodeLevel::device, dev_idx, 0}};
  }
  constexpr static NodeLocator make_instance(int dev_idx, int instance) {
    return {instance, {NodeLevel::instance, dev_idx, instance}};
  }

  constexpr int instance() const { return v_.instance(); }
  constexpr int dev_idx() const { return v_.dev_idx(); }
  constexpr NodeLevel level() const { return v_.level(); }
  constexpr int row() const { return row_; }
  constexpr uintptr_t internal() const { return v_; }

  NodeLocator child(int row) const {
    switch (auto new_level = level(); ++new_level) {
      case NodeLevel::device:
        return {row, {new_level, row, 0}};
      case NodeLevel::instance:
        return {row, {new_level, dev_idx(), row}};
      default:
      case NodeLevel::linked:
        return {row, {new_level, dev_idx(), instance()}};
    }
  }

  NodeLocator parent() const {
    int row = 0;
    int p_dev_idx = 0, p_instance = 0;
    auto p_level = level();
    switch (--p_level) {
      case NodeLevel::device:
        p_dev_idx = row = dev_idx();
        break;
      case NodeLevel::instance:
        p_dev_idx = dev_idx();
        p_instance = row = instance();
        break;
      default:
        break;
    }
    return {row, NodeInternal(p_level, p_dev_idx, p_instance)};
  };

 private:
  int row_ = 0;
  NodeInternal v_;
};

int tree_child_count(const vgmtk::DeviceConfigList& info_list, const NodeLocator& loc) {
  const vgmtk::DeviceConfig& c = info_list[loc.dev_idx()];

  switch (loc.level()) {
    case NodeLocator::NodeLevel::root:
      return info_list.size();
    case NodeLocator::NodeLevel::device:
      return vgmtk::kNumInstances;
    case NodeLocator::NodeLevel::instance:
      return c.has_linked() ? 1 : 0;
    default:
    case NodeLocator::NodeLevel::linked:
      return 0;
  }
}

constexpr bool playing_device_comp(const PlayingDevice& lhs, const PlayingDevice& rhs) {
  return lhs.dev_idx < rhs.dev_idx || (lhs.dev_idx == rhs.dev_idx && lhs.instance < rhs.instance);
}

}  // namespace

DeviceTreeModel::DeviceTreeModel(vgmtk::SettingsManager& settings_manager,
                                 vgmtk::PlaybackStatus& playback_status, QObject* parent,
                                 const vgmtk::DeviceConfigList& info_list)
    : QAbstractItemModel(parent), settings_manager_(settings_manager), info_list_(info_list) {
  auto hook_func = [](const vgmtk::PlaybackStatus& ps, void* ctx) {
    auto* self = static_cast<DeviceTreeModel*>(ctx);
    const auto& info_list = self->info_list_;
    const auto& new_player_info = ps.get_device_info();

    PlayingDevices now_playing, changed;
    now_playing.reserve(new_player_info.size());
    changed.reserve(new_player_info.size());

    for (const auto& a : new_player_info) {
      const auto& dev_idx = info_list.find_index(a.type);
      if (dev_idx < 0) continue;
      now_playing.push_back({dev_idx, a.instance, a.core, vgmtk::accurate_chip_name(a)});
    }
    std::sort(now_playing.begin(), now_playing.end(), playing_device_comp);
    std::set_symmetric_difference(now_playing.begin(), now_playing.end(),
                                  self->playing_devices_.begin(), self->playing_devices_.end(),
                                  std::back_inserter(changed), playing_device_comp);

    if (changed.empty()) return;
    std::swap(self->playing_devices_, now_playing);

    const QVector<int> roles({PlayingDeviceRole});

    int changing_dev = -1;
    for (const auto& a : changed) {
      auto mi = self->createIndex(a.instance, 0,
                                  NodeLocator::make_instance(a.dev_idx, a.instance).internal());
      self->dataChanged(mi, mi, roles);
      if (changing_dev != a.dev_idx) {
        changing_dev = a.dev_idx;
        mi = self->createIndex(changing_dev, 0, NodeLocator::make_device(a.dev_idx).internal());
        self->dataChanged(mi, mi, roles);
      }
    }
  };

  hook_func(playback_status, this);

  playback_status.subscribe(hook_func, this);
  connect(this, &QObject::destroyed, this, [&]() { playback_status.unsubscribe(hook_func, this); }); // NOLINT(*-unused-return-value)
}

QModelIndex DeviceTreeModel::index(int row, int column, const QModelIndex& parent) const {
  if (!hasIndex(row, column, parent)) return {};
  return createIndex(row, column, NodeLocator(parent).child(row).internal());
}

QModelIndex DeviceTreeModel::parent(const QModelIndex& index) const {
  NodeLocator node{index};
  if (!index.isValid() || node.level() <= NodeLocator::NodeLevel::device) return {};

  node = node.parent();
  return createIndex(node.row(), 0, node.internal());
}

int DeviceTreeModel::rowCount(const QModelIndex& parent) const {
  if (parent.column() > 0) return 0;
  return tree_child_count(info_list_, NodeLocator(parent));
}

int DeviceTreeModel::columnCount(const QModelIndex&) const { return 1; }

bool DeviceTreeModel::setData(const QModelIndex& index, const QVariant& value, int role) {
  if (!index.isValid() || role != Qt::CheckStateRole || index.column() != 0) return false;
  const NodeLocator node{index};

  bool changed = false;
  const bool state = value.value<Qt::CheckState>() == Qt::Unchecked;

  switch (node.level()) {
    case NodeLocator::NodeLevel::device: {
      auto writer = settings_manager_.writer();
      changed =
          writer.set_disable(node.dev_idx(), state, vgmtk::PlayerSettings::DisableMask().set());
    } break;
    case NodeLocator::NodeLevel::instance: {
      auto writer = settings_manager_.writer();
      changed = writer.set_disable(node.dev_idx(), node.instance(), state,
                                   vgmtk::PlayerSettings::DisableMask().set());
    } break;
    case NodeLocator::NodeLevel::linked: {
      auto writer = settings_manager_.writer();
      changed = writer.set_disable(node.dev_idx(), node.instance(), state,
                                   vgmtk::PlayerSettings::DisableMask().set(1));
    } break;
    default:
      return false;
  }

  if (!changed) return true;

  const QVector<int> roles({Qt::CheckStateRole});
  QModelIndex ci = index;
  for (auto level = node.level(); level > NodeLocator::NodeLevel::root; --level) {
    dataChanged(ci, ci, roles);
    ci = ci.parent();
  }
  data_changed_helper(index, roles);

  return true;
}

void DeviceTreeModel::data_changed_helper(const QModelIndex& p, const QVector<int>& roles) { // NOLINT: misc-no-recursion
  const int row_count = rowCount(p);
  if (!row_count) return;

  const NodeLocator node{p};

  // notify all this node's children
  const QModelIndex start{createIndex(0, 0, node.child(0).internal())},
      end{createIndex(row_count - 1, 0, node.child(row_count - 1).internal())};
  dataChanged(start, end, roles);

  if (node.level() >= NodeLocator::NodeLevel::instance) return;

  for (int i = 0; i < row_count; ++i) {
    data_changed_helper(QModelIndex{createIndex(i, 0, node.child(i).internal())}, roles);
  }
}

static QString instance_row_display(const std::vector<PlayingDevice>& pd, const NodeLocator& node) { // NOLINT: misc-no-recursion
  const auto end = pd.end();
  const PlayingDevice key{.dev_idx = node.dev_idx(), .instance = node.row()};

  const auto it = std::lower_bound(pd.begin(), end, key, playing_device_comp);
  if ((it != end) && !playing_device_comp(key, *it)) {
    return QString(N_("#%1 (%2)")).arg(node.row() + 1).arg(it->accurate_name);
  }
  return QString(N_("#%1")).arg(node.row() + 1);
}

QVariant DeviceTreeModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid()) return {};

  const NodeLocator node{index};
  const int row = index.row();
  const vgmtk::DeviceConfig& dev_info = info_list_[node.dev_idx()];

  if (index.column() == 1) {
    if (role == Qt::DisplayRole && node.level() == NodeLocator::NodeLevel::device) {
      return dev_info.core_name();
    }
    return {};
  }

  switch (role) {
    case Qt::DisplayRole: {
      switch (node.level()) {
        case NodeLocator::NodeLevel::device:
          return dev_info.label;
        case NodeLocator::NodeLevel::instance:
          return instance_row_display(playing_devices_, node);
        case NodeLocator::NodeLevel::linked:
          return dev_info.linked.prefix;
        default:
          return {};
      }
    }

    case Qt::CheckStateRole: {
      auto dev_settings = settings_manager_.current().devices[node.dev_idx()];
      Qt::CheckState value = Qt::Checked;
      switch (node.level()) {
        case NodeLocator::NodeLevel::device: {
          int disable_count = 0;

          for (const auto& chip : dev_settings.chips) {
            const auto dis = chip.disable;

            if (dis[0]) {
              disable_count++;
              value = Qt::PartiallyChecked;
            } else if (dev_info.has_linked() && dis.any()) {
              value = Qt::PartiallyChecked;
              break;
            }
          }
          if (disable_count == dev_settings.chips.size()) {
            value = Qt::Unchecked;
          }
        } break;
        case NodeLocator::NodeLevel::instance: {
          const auto dis = dev_settings.chips[node.instance()].disable;
          if (dis[0]) {
            value = Qt::Unchecked;
          } else if (dev_info.has_linked() && dis.any()) {
            value = Qt::PartiallyChecked;
          }
        } break;
        case NodeLocator::NodeLevel::linked: {
          const auto dis = dev_settings.chips[node.instance()].disable;
          value = dis[1] ? Qt::Unchecked : Qt::Checked;
        } break;
        default:
          break;
      }
      return value;
    }
    case PlayingDeviceRole: {
      bool playing = false;
      switch (node.level()) {
        case NodeLocator::NodeLevel::device:
          playing = std::binary_search(playing_devices_.begin(), playing_devices_.end(),
                                       PlayingDevice{.dev_idx = row},
                                       [](const PlayingDevice& lhs, const PlayingDevice& rhs) {
                                         return lhs.dev_idx < rhs.dev_idx;
                                       });
          break;
        case NodeLocator::NodeLevel::instance:
          playing = std::binary_search(playing_devices_.begin(), playing_devices_.end(),
                                       PlayingDevice{.dev_idx = node.dev_idx(), .instance = row},
                                       playing_device_comp);
          break;
        default:
          break;
      }
      return playing;
    }
    default:
      return {};
  }
}

Qt::ItemFlags DeviceTreeModel::flags(const QModelIndex& index) const {
  if (!index.isValid()) return Qt::NoItemFlags;
  const NodeLocator node{index};
  const Qt::ItemFlags checkable =
      index.column() == 0 ? (Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate) : Qt::NoItemFlags;

  switch (node.level()) {
    case NodeLocator::NodeLevel::linked:
      return Qt::ItemIsEnabled | Qt::ItemNeverHasChildren | checkable;
    case NodeLocator::NodeLevel::instance:
      return Qt::ItemIsSelectable | checkable | Qt::ItemIsEnabled |
             (!info_list_[node.dev_idx()].has_linked() ? Qt::ItemNeverHasChildren
                                                       : Qt::NoItemFlags);
    default:
    case NodeLocator::NodeLevel::device:
      return Qt::ItemIsSelectable | checkable | Qt::ItemIsEnabled;
  }
}

QVariant DeviceTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
  static const std::array<const QString, 2> labels = {tr("Name"), tr("Core")};
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 &&
      static_cast<size_t>(section) < labels.size())
    return labels[section];

  return {};
}

bool DeviceTreeFiltered::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const {
  auto* source = sourceModel();
  auto index = source->index(source_row, 0, source_parent);
  return source->data(index, DeviceTreeModel::PlayingDeviceRole).toBool();
}

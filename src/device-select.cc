#include "device-select.h"

#define N_(String) (String)

#include <QtCore/QAbstractItemModel>

#include "comm.h"
#include "libvgm-dev-meta.h"

using namespace vgmqt;

static constexpr unsigned width_to_mask(unsigned width) { return (1 << width) - 1; }

namespace {
class NodeLocator {
 public:
  enum NodeLevel { kLevelRoot = 0, kLevelDevice, kLevelInstance, kLevelLinked };

 private:
  class NodeInternal {
    static constexpr const unsigned kDevIdxStart = 0, kDevIdxWidth = 9,
                                    kInstanceStart = kDevIdxWidth, kInstanceWidth = 5,
                                    kLevelStart = kInstanceStart + kInstanceWidth, kLevelWidth = 2;
    static_assert(kDevIdxWidth + kInstanceWidth + kLevelWidth <= 16, "locator bitfield too wide");

    static constexpr const unsigned kLevelMask = width_to_mask(kLevelWidth),
                                    kDevIdxMask = width_to_mask(kDevIdxWidth),
                                    kInstanceMask = width_to_mask(kInstanceWidth);

    uint_fast16_t loc_;

   public:
    explicit constexpr NodeInternal(uintptr_t v = 0) : loc_(v) {}
    constexpr NodeInternal(int level, int dev_idx, int instance)
        : loc_(((level & kLevelMask) << kLevelStart) | ((dev_idx & kDevIdxMask) << kDevIdxStart) |
               ((instance & kInstanceMask) << kInstanceStart)) {}

    constexpr operator uintptr_t() const { return loc_; }

    constexpr int level() const { return (loc_ >> kLevelStart) & kLevelMask; }
    constexpr int instance() const { return (loc_ >> kInstanceStart) & kInstanceMask; }
    constexpr int dev_idx() const { return (loc_ >> kDevIdxStart) & kDevIdxMask; }
  };

 public:
  constexpr NodeLocator(int row, NodeInternal v) : row_(row), v_(v) {}
  explicit constexpr NodeLocator(const QModelIndex& mi)
      : row_(mi.isValid() ? mi.row() : 0), v_(mi.isValid() ? mi.internalId() : 0) {}
  constexpr static NodeLocator make_device(int dev_idx) {
    return NodeLocator(dev_idx, NodeInternal(kLevelDevice, dev_idx, 0));
  }
  constexpr static NodeLocator make_instance(int dev_idx, int instance) {
    return NodeLocator(instance, NodeInternal(kLevelInstance, dev_idx, instance));
  }

  constexpr int instance() const { return v_.instance(); }
  constexpr int dev_idx() const { return v_.dev_idx(); }
  constexpr NodeLevel level() const { return static_cast<NodeLevel>(v_.level()); }
  constexpr int row() const { return row_; }
  constexpr uintptr_t internal() const { return v_; }

  NodeLocator child(int row) const {
    int new_level = std::min<int>(level() + 1, kLevelLinked);
    switch (new_level) {
      case kLevelDevice:
        return NodeLocator{row, NodeInternal{new_level, row, 0}};
      case kLevelInstance:
        return NodeLocator{row, NodeInternal{new_level, dev_idx(), row}};
      default:
      case kLevelLinked:
        return NodeLocator{row, NodeInternal{new_level, dev_idx(), instance()}};
    }
  }

  NodeLocator parent() const {
    int p_level = std::max(0, level() - 1), row = 0;
    int p_dev_idx = 0, p_instance = 0;
    switch (p_level) {
      case kLevelDevice:
        p_dev_idx = row = dev_idx();
        break;
      case kLevelInstance:
        p_dev_idx = dev_idx();
        p_instance = row = instance();
        break;
      default:
        break;
    }
    return NodeLocator(row, NodeInternal(p_level, p_dev_idx, p_instance));
  };

 private:
  int row_ = 0;
  NodeInternal v_;
};

int tree_child_count(const vgm::DeviceConfigList& info_list, const NodeLocator& loc) {
  const vgm::DeviceConfig& c = info_list[loc.dev_idx()];

  switch (loc.level()) {
    case NodeLocator::kLevelRoot:
      return info_list.size();
    case NodeLocator::kLevelDevice:
      return vgm::kNumInstances;
    case NodeLocator::kLevelInstance:
      return c.has_linked() ? 1 : 0;
    default:
    case NodeLocator::kLevelLinked:
      return 0;
  }
}

}  // namespace

DeviceTreeModel::DeviceTreeModel(audvgm::SettingsManager& settings_manager,
                                 audvgm::PlaybackStatus& playback_status, QObject* parent,
                                 const vgm::DeviceConfigList& info_list)
    : QAbstractItemModel(parent), settings_manager_(settings_manager), info_list_(info_list) {
  auto hook_func = [](const audvgm::PlaybackStatus& ps, void* ctx) {
    auto* self = static_cast<DeviceTreeModel*>(ctx);
    const auto& info_list = self->info_list_;
    PlayingDevices now_playing;

    for (const auto& a : ps.device_info()) {
      int dev_idx = info_list.find_index(a.type);
      if (dev_idx < 0) continue;
      now_playing[dev_idx * vgm::kNumInstances + a.instance] = true;
    }

    PlayingDevices changed = now_playing ^ self->playing_devices_old_;

    if (changed.none()) return;
    self->playing_devices_old_ = now_playing;

    QVector<int> roles({PlayingDeviceRole});

    for (int dev_idx = 0; dev_idx < info_list.size(); ++dev_idx) {
      bool device_changed = false;
      for (int inst = 0; inst < vgm::kNumInstances; ++inst) {
        if (changed[dev_idx * vgm::kNumInstances + inst]) {
          auto mi =
              self->createIndex(inst, 0, NodeLocator::make_instance(dev_idx, inst).internal());

          self->dataChanged(mi, mi, roles);
          device_changed = true;
        }
      }
      if (device_changed) {
        auto mi = self->createIndex(dev_idx, 0, NodeLocator::make_device(dev_idx).internal());
        self->dataChanged(mi, mi, roles);
      }
    }
  };

  hook_func(playback_status, this);

  playback_status.subscribe(hook_func, this);
  connect(this, &QObject::destroyed, this, [&]() { playback_status.unsubscribe(hook_func, this); });
}

QModelIndex DeviceTreeModel::index(int row, int column, const QModelIndex& parent) const {
  if (!hasIndex(row, column, parent))
    return QModelIndex();
  return createIndex(row, column, NodeLocator(parent).child(row).internal());
}

QModelIndex DeviceTreeModel::parent(const QModelIndex& index) const {
  if (!index.isValid()) return QModelIndex();

  NodeLocator node{index};
  if (node.level() <= 1)
    return QModelIndex();

  node = node.parent();
  return createIndex(node.row(), 0, node.internal());
}

int DeviceTreeModel::rowCount(const QModelIndex& parent) const {
  if (parent.column() > 0) return 0;
  return tree_child_count(info_list_, NodeLocator(parent));
}

int DeviceTreeModel::columnCount(const QModelIndex& parent) const { return 1; }

bool DeviceTreeModel::setData(const QModelIndex& index, const QVariant& value, int role) {
  if (!index.isValid() || role != Qt::CheckStateRole) return false;
  NodeLocator node{index};
  int row = index.row();

  bool changed = false;
  const vgm::DeviceConfig& dev_info = info_list_[node.dev_idx()];
  bool state = value.value<Qt::CheckState>() == Qt::Unchecked;

  switch (node.level()) {
    case NodeLocator::kLevelDevice: {
      auto writer = settings_manager_.writer();
      changed =
          writer.set_disable(node.dev_idx(), state, audvgm::PlayerSettings::DisableMask().set());
    } break;
    case NodeLocator::kLevelInstance: {
      auto writer = settings_manager_.writer();
      changed = writer.set_disable(node.dev_idx(), node.instance(), state,
                                   audvgm::PlayerSettings::DisableMask().set());
    } break;
    case NodeLocator::kLevelLinked: {
      auto writer = settings_manager_.writer();
      changed = writer.set_disable(node.dev_idx(), node.instance(), state,
                                   audvgm::PlayerSettings::DisableMask().set(1));
    } break;
    default:
      return false;
  }

  if (!changed) return true;

  QVector<int> roles({Qt::CheckStateRole});
  QModelIndex ci = index;
  for (int level = node.level(); level > NodeLocator::kLevelRoot; level--) {
    dataChanged(ci, ci, roles);
    ci = ci.parent();
  }
  data_changed_helper(index, roles);

  return true;
}

void DeviceTreeModel::data_changed_helper(const QModelIndex& p, const QVector<int>& roles) {
  int row_count = rowCount(p);
  if (!row_count) return;

  NodeLocator node{p};

  // notify all this nodes children
  QModelIndex start{createIndex(0, 0, node.child(0).internal())},
      end{createIndex(row_count - 1, 0, node.child(row_count - 1).internal())};
  dataChanged(start, end, roles);

  if (node.level() >= NodeLocator::kLevelInstance) return;

  for (int i = 0; i < row_count; ++i) {
    data_changed_helper(QModelIndex{createIndex(i, 0, node.child(i).internal())}, roles);
  }
}

QVariant DeviceTreeModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid()) return QVariant();

  NodeLocator node{index};
  const int row = index.row();
  const vgm::DeviceConfig& dev_info = info_list_[node.dev_idx()];

  switch (role) {
    case Qt::DisplayRole: {
      switch (node.level()) {
        case NodeLocator::kLevelDevice:
          return QVariant(dev_info.label);
        case NodeLocator::kLevelInstance:
          return QVariant(QString(N_("Chip %1")).arg(row + 1));
        case NodeLocator::kLevelLinked:
          return QVariant(dev_info.linked.prefix);
        default:
          return QVariant();
      }
    }

    case Qt::CheckStateRole: {
      auto dev_settings = settings_manager_.current().devices[node.dev_idx()];
      Qt::CheckState value = Qt::Checked;
      switch (node.level()) {
        case NodeLocator::kLevelDevice: {
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
        case NodeLocator::kLevelInstance: {
          const auto dis = dev_settings.chips[node.instance()].disable;
          if (dis[0]) {
            value = Qt::Unchecked;
          } else if (dev_info.has_linked() && dis.any()) {
            value = Qt::PartiallyChecked;
          }
        } break;
        case NodeLocator::kLevelLinked: {
          const auto dis = dev_settings.chips[node.instance()].disable;
          value = dis[1] ? Qt::Unchecked : Qt::Checked;
        } break;
        default:
          break;
      }
      return QVariant(value);
    }
    case PlayingDeviceRole: {
      bool playing = false;
      switch (node.level()) {
        case NodeLocator::kLevelDevice:
          for (int inst = 0; inst < vgm::kNumInstances; ++inst) {
            if (playing_devices_old_[row * vgm::kNumInstances + inst]) {
              playing = true;
              break;
            }
          }
          break;
        case NodeLocator::kLevelInstance:
          playing = playing_devices_old_[node.dev_idx() * vgm::kNumInstances + row];
          break;
        default:
          break;
      }
      return QVariant(playing);
    }
    default:
      return QVariant();
  }
}

Qt::ItemFlags DeviceTreeModel::flags(const QModelIndex& index) const {
  if (!index.isValid()) return Qt::NoItemFlags;
  NodeLocator node{index};
  if (node.level() == NodeLocator::kLevelLinked) {
    return Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren;
  } else {
    return Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate |
           Qt::ItemIsEnabled;
  }
}

QVariant DeviceTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
  return QAbstractItemModel::headerData(section, orientation, role);
}

bool DeviceTreeFiltered::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const {
  auto* source = sourceModel();
  auto index = source->index(source_row, 0, source_parent);
  return source->data(index, DeviceTreeModel::PlayingDeviceRole).toBool();
}


#include "channel-select.h"

#include "comm.h"

using namespace vgmqt;

namespace {

constexpr unsigned width_to_mask(unsigned width) { return (1 << width) - 1; }

class NodeLocator {
 public:
  enum NodeLevel { kLevelRoot = 0, kLevelDevice, kLevelChannel };

 private:
  class NodeInternal {
    static constexpr const unsigned kDevIdxStart = 0, kDevIdxWidth = 14, kLevelStart = kDevIdxWidth,
                                    kLevelWidth = 2;
    static_assert(kDevIdxWidth + kLevelWidth == 16, "locator bitfield incorrect size");

    static constexpr const unsigned kLevelMask = width_to_mask(kLevelWidth),
                                    kDevIdxMask = width_to_mask(kDevIdxWidth);

    uint_fast16_t loc_;

   public:
    explicit constexpr NodeInternal(uintptr_t v = 0) : loc_(v) {}
    constexpr NodeInternal(int level, int dev_idx)
        : loc_(((level & kLevelMask) << kLevelStart) | ((dev_idx & kDevIdxMask) << kDevIdxStart)) {}

    constexpr operator uintptr_t() const { return loc_; }

    constexpr int level() const { return (loc_ >> kLevelStart) & kLevelMask; }
    constexpr int dev_idx() const { return (loc_ >> kDevIdxStart) & kDevIdxMask; }
  };

 public:
  constexpr NodeLocator(int row, NodeInternal v) : row_(row), v_(v) {}
  explicit constexpr NodeLocator(const QModelIndex &mi)
      : row_(mi.isValid() ? mi.row() : 0), v_(mi.isValid() ? mi.internalId() : 0) {}
  constexpr static NodeLocator make_device(int dev_idx) {
    return NodeLocator(dev_idx, NodeInternal(kLevelDevice, dev_idx));
  }

  constexpr int dev_idx() const { return v_.dev_idx(); }
  constexpr NodeLevel level() const { return static_cast<NodeLevel>(v_.level()); }
  constexpr int row() const { return row_; }
  constexpr uintptr_t internal() const { return v_; }

  NodeLocator child(int row) const {
    int new_level = std::min<int>(level() + 1, kLevelChannel);
    switch (new_level) {
      case kLevelDevice:
        return NodeLocator{row, NodeInternal{new_level, row}};
      default:
      case kLevelChannel:
        return NodeLocator{row, NodeInternal{new_level, dev_idx()}};
    }
  }

  NodeLocator parent() const {
    int p_level = std::max(0, level() - 1), row = 0;
    int p_dev_idx = 0;
    if (p_level == kLevelDevice) {
      p_dev_idx = row = dev_idx();
    }
    return NodeLocator(row, NodeInternal(p_level, p_dev_idx));
  };

 private:
  int row_ = 0;
  NodeInternal v_;
};

}  // namespace
ChannelTreeModel::ChannelTreeModel(audvgm::SettingsManager &settings_manager,
                                   audvgm::PlaybackStatus *playback_status, QObject *parent,
                                   const vgm::DeviceConfigList &info_list)
    : settings_manager_(settings_manager), info_list_(info_list), show_all_(true) {
  auto hook_func = [](const audvgm::PlaybackStatus &ps, void *ctx) {
    auto *self = static_cast<ChannelTreeModel *>(ctx);
    const auto &info_list = self->info_list_;
    const auto &new_info = ps.get_device_info();

    std::vector<int> now_active;
    now_active.reserve(new_info.size());

    for (const auto &a : new_info) {
      int dev_idx = info_list.find_index(a.type);
      if (dev_idx < 0) continue;
      now_active.push_back(dev_idx);
    }
    std::sort(now_active.begin(), now_active.end());
    self->beginResetModel();
    std::swap(self->active_devices_, now_active);
    self->endResetModel();
  };

  if (playback_status) {
    hook_func(*playback_status, this);
    playback_status->subscribe(hook_func, this);
    connect(this, &QObject::destroyed, this,
            [=]() { playback_status->unsubscribe(hook_func, this); });
  }
}

QModelIndex ChannelTreeModel::index(int row, int column, const QModelIndex &parent) const {
  if (!hasIndex(row, column, parent)) return QModelIndex();
  return createIndex(row, column, NodeLocator(parent).child(row).internal());
}

QModelIndex ChannelTreeModel::parent(const QModelIndex &index) const {
  NodeLocator node{index};
  if (!index.isValid() || node.level() <= 1) return QModelIndex();

  node = node.parent();
  return createIndex(node.row(), 0, node.internal());
}

int ChannelTreeModel::rowCount(const QModelIndex &parent) const {
  if (parent.column() > 0) return 0;
  NodeLocator loc{parent};

  switch (loc.level()) {
    case NodeLocator::kLevelRoot:
      return active_devices_.empty() ? (show_all_ ? info_list_.size() : 0) : active_devices_.size();
    case NodeLocator::kLevelDevice:
      return active_devices_.empty()
             ? info_list_[loc.dev_idx()].num_channels_all()
             : info_list_[active_devices_[loc.dev_idx()]].num_channels_all();
    default:
      return 0;
  }
}

int ChannelTreeModel::columnCount(const QModelIndex &parent) const { return 5; }

bool ChannelTreeModel::setData(const QModelIndex &index, const QVariant &value, int role) {
  if (!index.isValid() || role != Qt::CheckStateRole || index.column() != 1 || index.column() != 2)
    return false;

  NodeLocator node{index};
  int row = index.row();
  bool changed = false;
  const vgm::DeviceConfig& dev_info = info_list_[node.dev_idx()];
  bool state = value.value<Qt::CheckState>() == Qt::Unchecked;
  const vgm::DeviceConfig &dev_info = active_devices_.empty()
                                      ? info_list_[node.dev_idx()]
                                      : info_list_[active_devices_[node.dev_idx()]];

}

static QString channel_name(const vgm::DeviceConfig &dev_cfg, int idx) {
  static const QString kLabelChannelNum(ChannelTreeModel::tr("Channel %1"));
  static const QString kLabelGroupedChannelNum(ChannelTreeModel::tr("%1 Channel %2"));
  static const QString kLabelGroupedChannelName(ChannelTreeModel::tr("%1 %2"));

  const char *group = nullptr;
  for (const auto *d = &dev_cfg; d;) {
    for (const auto &cs : d->channels) {
      if (!group) group = cs.name();
      if (idx < cs.serial) {
        return group ? kLabelGroupedChannelNum.arg(group).arg(idx + 1)
                     : kLabelChannelNum.arg(idx + 1);
      } else if (idx < cs.serial + cs.named.size()) {
        idx -= cs.serial;
        return group ? kLabelGroupedChannelName.arg(group).arg(cs.named[idx])
                     : ChannelTreeModel::tr(cs.named[idx]);
      }
      idx -= cs.serial + cs.named.size();
    }
    group = d->linked.prefix;
    d = d->linked_config();
  }
  return QString("");
}

QVariant ChannelTreeModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) return QVariant();

  NodeLocator node{index};
  const int row = index.row();
  const vgm::DeviceConfig &dev_info = active_devices_.empty()
                                          ? info_list_[node.dev_idx()]
                                          : info_list_[active_devices_[node.dev_idx()]];

  if (index.column() != 0 || role != Qt::DisplayRole) return QVariant();

  switch (role) {
    case Qt::DisplayRole:
      switch (node.level()) {
        case NodeLocator::kLevelDevice:
          return QVariant(dev_info.label);
        case NodeLocator::kLevelChannel:
          return QVariant(channel_name(dev_info, row));
        default:
          return QVariant();
      }
  }

  return QVariant();
}

Qt::ItemFlags ChannelTreeModel::flags(const QModelIndex &index) const {
  if (!index.isValid()) return Qt::NoItemFlags;

  NodeLocator node{index};

  Qt::ItemFlags flags = Qt::NoItemFlags;

  switch (node.level()) {
    case NodeLocator::kLevelChannel:
      flags = Qt::ItemIsEnabled | Qt::ItemNeverHasChildren;
      switch (index.column()) {
        case 1:
        case 2:
          flags |= Qt::ItemIsUserCheckable;
          break;
        case 3:
        case 4:
          flags |= Qt::ItemIsEditable;
          break;
        default:
        case 0:
          flags |= Qt::ItemIsSelectable;
          break;
      }
      break;
    case NodeLocator::kLevelDevice:
    default:
      flags = Qt::ItemIsEnabled;
      if (index.column() == 0) flags |= Qt::ItemIsSelectable;
      break;
  }
  return flags;
}

QVariant ChannelTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
  static const std::array<const QString, 5> labels = {tr("Channel"), tr("M"), tr("S"),
                                                      tr("Panning"), tr("Name")};
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 &&
      section < labels.size())
    return labels[section];

  return QVariant();
}

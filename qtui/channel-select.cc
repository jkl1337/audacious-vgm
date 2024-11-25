
#include "channel-select.h"

#include "../corelib/libvgm-rt.h"

using namespace vgmqt;

namespace {

constexpr unsigned width_to_mask(unsigned width) { return (1 << width) - 1; }

class NodeLocator {
 public:
  enum NodeLevel : uint8_t { kLevelRoot = 0, kLevelDevice, kLevelChannel };

 private:
  class NodeInternal {
    static constexpr const unsigned kCtxStart = 0, kCtxWidth = 14, kLevelStart = kCtxWidth,
                                    kLevelWidth = 2;
    static_assert(kCtxWidth + kLevelWidth == 16, "locator bitfield incorrect size");

    static constexpr const unsigned kLevelMask = width_to_mask(kLevelWidth),
                                    kCtxMask = width_to_mask(kCtxWidth);

    uint_fast16_t loc_;

   public:
    explicit constexpr NodeInternal(uintptr_t v = 0) : loc_(v) {}
    constexpr NodeInternal(int level, int ctx)
        : loc_(((level & kLevelMask) << kLevelStart) | ((ctx & kCtxMask) << kCtxStart)) {}

    constexpr operator uintptr_t() const { return loc_; }

    constexpr int level() const { return (loc_ >> kLevelStart) & kLevelMask; }
    constexpr int ctx() const { return (loc_ >> kCtxStart) & kCtxMask; }
  };

 public:
  constexpr NodeLocator(int row, NodeInternal v) : row_(row), v_(v) {}
  explicit constexpr NodeLocator(const QModelIndex &mi)
      : row_(mi.isValid() ? mi.row() : 0), v_(mi.isValid() ? mi.internalId() : 0) {}
  constexpr static NodeLocator make_device(int dev_idx) { return {dev_idx, {kLevelDevice, 0}}; }

  constexpr int ctx() const { return v_.ctx(); }
  constexpr NodeLevel level() const { return static_cast<NodeLevel>(v_.level()); }
  constexpr int row() const { return row_; }
  constexpr uintptr_t internal() const { return v_; }
  constexpr int top_idx() const { return level() == kLevelDevice ? row_ : ctx(); }

  NodeLocator child(int row) const {
    return {row, {std::min<int>(level() + 1, kLevelChannel), row_}};
  }

  NodeLocator parent() const { return {ctx(), {std::max(0, level() - 1), 0}}; };

 private:
  int row_ = 0;
  NodeInternal v_;
};

}  // namespace
ChannelTreeModel::ChannelTreeModel(vgmtk::SettingsManager &settings_manager,
                                   vgmtk::PlaybackStatus *playback_status, QObject *,
                                   const vgmtk::DeviceConfigList &info_list)
    : settings_manager_(settings_manager), info_list_(info_list) {
  auto hook_func = [](const vgmtk::PlaybackStatus &ps, void *ctx) {
    auto *self = static_cast<ChannelTreeModel *>(ctx);
    const auto &info_list = self->info_list_;
    const auto &new_info = ps.get_device_info();

    if (new_info.empty()) return;
    ActiveDevices now_active;
    now_active.reserve(new_info.size());

    for (const auto &a : new_info) {
      int dev_idx = info_list.find_index(a.type);
      if (dev_idx < 0) continue;
      now_active.push_back({dev_idx, a.instance, 0});
    }
    std::sort(now_active.begin(), now_active.end(), [](const auto &l, const auto &r) {
      return (l.dev_idx < r.dev_idx) || (l.dev_idx == r.dev_idx && l.instance < r.instance);
    });
    if (now_active == self->active_devices_) return;
    self->beginResetModel();
    std::swap(self->active_devices_, now_active);
    self->endResetModel();
  };

  if (playback_status) {
    hook_func(*playback_status, this);
    playback_status->subscribe(hook_func, this);
    connect(this, &QObject::destroyed, this, [playback_status, hook_func, this]() {
      playback_status->unsubscribe(hook_func, this);
    });
  }
}

QModelIndex ChannelTreeModel::index(int row, int column, const QModelIndex &parent) const {
  if (!hasIndex(row, column, parent)) return {};
  return createIndex(row, column, NodeLocator(parent).child(row).internal());
}

QModelIndex ChannelTreeModel::parent(const QModelIndex &index) const {
  NodeLocator node{index};
  if (!index.isValid() || node.level() <= 1) return {};

  node = node.parent();
  return createIndex(node.row(), 0, node.internal());
}

int ChannelTreeModel::rowCount(const QModelIndex &parent) const {
  if (parent.column() > 0) return 0;
  NodeLocator loc{parent};

  switch (loc.level()) {
    case NodeLocator::kLevelRoot:
      return active_devices_.size();
    case NodeLocator::kLevelDevice:
      return info_list_[active_devices_[loc.top_idx()].dev_idx].num_channels_all();
    default:
      return 0;
  }
}

int ChannelTreeModel::columnCount(const QModelIndex &) const { return 4; }

bool ChannelTreeModel::setData(const QModelIndex &index, const QVariant &value, int role) {
  const int column = index.column();
  if (!index.isValid()) return false;

  NodeLocator node{index};

  if (column == 3 && role == Qt::EditRole && node.level() == NodeLocator::kLevelChannel) {
    int pan = std::clamp(value.toInt(), -256, 256);
    auto writer = settings_manager_.writer();

    const auto &active = active_devices_[node.top_idx()];
    bool changed = writer.set_panning(active.dev_idx, active.instance, node.row(), pan);
    if (!changed) return true;
    dataChanged(index, index, {Qt::EditRole});
    return true;
  } else if ((column == 1 || column == 2) && role == Qt::CheckStateRole) {
    bool changed = false;
    const bool state = value.value<Qt::CheckState>() == Qt::Checked;
    const auto &active = active_devices_[node.top_idx()];

    auto writer = settings_manager_.writer();
    vgmtk::PlayerSettings::ChannelMask mask;
    if (node.level() == NodeLocator::kLevelChannel) {
      mask[index.row()] = true;
    } else {
      mask.set();
    }

    changed = column == 1 ? writer.set_mute(active.dev_idx, active.instance, state, mask)
                          : writer.set_solo(active.dev_idx, active.instance, state, mask);
    if (!changed) return true;

    QVector<int> roles({Qt::CheckStateRole});
    if (node.level() == NodeLocator::kLevelChannel) {
      dataChanged(index, index, roles);
      auto p = index.parent().siblingAtColumn(column);
      dataChanged(p, p, roles);
    } else {
      dataChanged(index, index, roles);
      QModelIndex top_left{createIndex(0, column, NodeLocator(index).child(0).internal())},
          bottom_right{
              createIndex(rowCount(index) - 1, column, NodeLocator(index).child(0).internal())};
      dataChanged(top_left, bottom_right, roles);
    }

    return true;
  }
  return false;
}

namespace {
QString channel_name(const vgmtk::DeviceConfig &dev_cfg, int idx) {
  static const QString kLabelChannelNum(ChannelTreeModel::tr("Channel %1"));
  static const QString kLabelGroupedChannelNum(ChannelTreeModel::tr("%1 Channel %2"));
  static const QString kLabelGroupedChannelName(ChannelTreeModel::tr("%1 %2"));

  const char *link_group = nullptr;
  for (const auto *d = &dev_cfg; d;) {
    for (const auto &cs : d->channels) {
      // FIXME: This isn't quite right
      const char *group = link_group ? link_group : cs.name();
      if (idx < cs.serial) {
        return group ? kLabelGroupedChannelNum.arg(group).arg(idx + 1)
                     : kLabelChannelNum.arg(idx + 1);
      } else if (idx < cs.serial + cs.named.size()) {
        idx -= cs.serial;
        return group ? kLabelGroupedChannelName.arg(group, cs.named[idx])
                     : ChannelTreeModel::tr(cs.named[idx]);
      }
      idx -= cs.serial + cs.named.size();
    }
    link_group = d->linked.prefix[0] ? d->linked.prefix : nullptr;
    d = d->linked_config();
  }
  return {""};
}
}  // namespace

QVariant ChannelTreeModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) return {};

  NodeLocator node{index};
  const int row = index.row(), column = index.column();
  const auto &active = active_devices_[node.top_idx()];
  const vgmtk::DeviceConfig &dev_info = info_list_[active.dev_idx];

  switch (role) {
    case Qt::EditRole:
      if (column != 3) return {};
      {
        const auto &chip = settings_manager_.current().chip(active);
        return {chip.panning[row]};
      }
    case Qt::DisplayRole:
      if (column != 0) return {};
      switch (node.level()) {
        case NodeLocator::kLevelDevice:
          return QString("%1 %2").arg(dev_info.label).arg(active.instance + 1);
        case NodeLocator::kLevelChannel:
          return channel_name(dev_info, row);
        default:
          return {};
      }
    case Qt::CheckStateRole: {
      if (column != 1 && column != 2) return {};
      auto dev_settings = settings_manager_.current().devices[active.dev_idx];
      const auto &chip = dev_settings.chips[active.instance];
      const auto &state = column == 1 ? chip.channel_mute : chip.channel_solo;

      if (node.level() == NodeLocator::kLevelDevice) {
        if (state.none()) return Qt::Unchecked;

        uint32_t mask{(uint32_t{1} << dev_info.num_channels_all()) - 1};
        uint32_t masked = state.to_ulong() & mask;

        return masked == mask ? Qt::Checked : Qt::PartiallyChecked;
      } else if (node.level() == NodeLocator::kLevelChannel) {
        return state[row] ? Qt::Checked : Qt::Unchecked;
      }
    }
    default:
      return {};
  }
}

Qt::ItemFlags ChannelTreeModel::flags(const QModelIndex &index) const {
  if (!index.isValid()) return Qt::NoItemFlags;

  NodeLocator node{index};
  const auto &dev_info = info_list_[active_devices_[node.top_idx()].dev_idx];

  Qt::ItemFlags flags;

  switch (node.level()) {
    case NodeLocator::kLevelChannel:
      flags = Qt::ItemIsEnabled | Qt::ItemNeverHasChildren;
      switch (index.column()) {
        case 1:
        case 2:
          flags |= Qt::ItemIsUserCheckable;
          break;
        case 3:
          // FIXME: want this based on the current core
          if (dev_info.has_panning()) flags |= Qt::ItemIsEditable;
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
      switch (index.column()) {
        case 1:
        case 2:
          flags |= Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate;
          break;
        default:
        case 0:
          flags |= Qt::ItemIsSelectable;
      }
      break;
  }
  return flags;
}

QVariant ChannelTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
  static const std::array<const QString, 4> labels = {tr("Channel"), tr("M"), tr("S"),
                                                      tr("Panning")};
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 &&
      static_cast<size_t>(section) < labels.size())
    return labels[section];

  return {};
}

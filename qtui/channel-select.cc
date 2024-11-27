
#include "channel-select.h"

#include "../corelib/playback-status.h"
#include "../corelib/settings-manager.h"

using namespace vgmqt;

namespace {

constexpr unsigned width_to_mask(unsigned width) { return (1 << width) - 1; }

class NodeLocator {
 public:
  enum class NodeLevel : uint8_t { root = 0, device, channel };

  friend constexpr NodeLevel &operator++(NodeLevel &l) {
    return l = (l == NodeLevel::channel) ? l
                                         : l = static_cast<NodeLevel>(static_cast<uint8_t>(l) + 1);
  }
  friend constexpr NodeLevel &operator--(NodeLevel &l) {
    return l = (l == NodeLevel::root) ? l : l = static_cast<NodeLevel>(static_cast<uint8_t>(l) - 1);
  }

 private:
  class NodeInternal {
    uint_fast16_t loc_;

    static constexpr unsigned ctx_bit_start = 0, ctx_bit_width = 14,
                              level_bit_start = ctx_bit_width, level_bit_width = 2;
    static_assert(ctx_bit_width + level_bit_width == 16, "locator bitfield incorrect size");

    static constexpr unsigned level_bit_mask = width_to_mask(level_bit_width),
                              ctx_bit_mask = width_to_mask(ctx_bit_width);

   public:
    explicit constexpr NodeInternal(uintptr_t v = 0) : loc_(v) {}
    constexpr NodeInternal(NodeLevel level, int ctx)
        : loc_(((static_cast<uint16_t>(level) & level_bit_mask) << level_bit_start) |
               ((ctx & ctx_bit_mask) << ctx_bit_start)) {}

    constexpr operator uintptr_t() const { return loc_; }

    constexpr NodeLevel level() const {
      return static_cast<NodeLevel>(loc_ >> level_bit_start & level_bit_mask);
    }
    constexpr int ctx() const { return (loc_ >> ctx_bit_start) & ctx_bit_mask; }
  };

 public:
  constexpr NodeLocator(int row, NodeInternal v) : row_(row), v_(v) {}
  explicit constexpr NodeLocator(const QModelIndex &mi)
      : row_(mi.isValid() ? mi.row() : 0), v_(mi.isValid() ? mi.internalId() : 0) {}

  constexpr int ctx() const { return v_.ctx(); }
  constexpr NodeLevel level() const { return v_.level(); }
  constexpr int row() const { return row_; }
  constexpr uintptr_t internal() const { return v_; }
  constexpr int top_idx() const { return level() == NodeLevel::device ? row_ : ctx(); }

  NodeLocator child(int row) const {
    auto l = level();
    return {row, {++l, row_}};
  }

  NodeLocator parent() const {
    auto l = level();
    return {ctx(), {--l, 0}};
  };

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
      const int dev_idx = info_list.find_index(a.type);
      if (dev_idx < 0) continue;
      now_active.push_back({dev_idx, a.instance, 0});
    }
    std::ranges::sort(now_active, [](const auto &l, const auto &r) {
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
    // NOLINTNEXTLINE(*-unused-return-value)
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
  if (!index.isValid() || node.level() <= NodeLocator::NodeLevel::device) return {};

  node = node.parent();
  return createIndex(node.row(), 0, node.internal());
}

int ChannelTreeModel::rowCount(const QModelIndex &parent) const {
  if (parent.column() > 0) return 0;
  const NodeLocator loc{parent};

  switch (loc.level()) {
    case NodeLocator::NodeLevel::root:
      return active_devices_.size();
    case NodeLocator::NodeLevel::device:
      return info_list_[active_devices_[loc.top_idx()].dev_idx].num_channels_all();
    default:
      return 0;
  }
}

int ChannelTreeModel::columnCount(const QModelIndex &) const { return 4; }

bool ChannelTreeModel::setData(const QModelIndex &index, const QVariant &value, int role) {
  const int column = index.column();
  if (!index.isValid()) return false;

  const NodeLocator node{index};

  if (column == 3 && role == Qt::EditRole && node.level() == NodeLocator::NodeLevel::channel) {
    const int pan = std::clamp(value.toInt(), -256, 256);
    auto writer = settings_manager_.writer();

    const auto &active = active_devices_[node.top_idx()];
    const bool changed = writer.set_panning(active.dev_idx, active.instance, node.row(), pan);
    if (!changed) return true;
    dataChanged(index, index, {Qt::EditRole});
    return true;
  } else if ((column == 1 || column == 2) && role == Qt::CheckStateRole) {
    bool changed = false;
    const bool state = value.value<Qt::CheckState>() == Qt::Checked;
    const auto &active = active_devices_[node.top_idx()];

    auto writer = settings_manager_.writer();
    vgmtk::PlayerSettings::ChannelMask mask;
    if (node.level() == NodeLocator::NodeLevel::channel) {
      mask[index.row()] = true;
    } else {
      mask.set();
    }

    changed = column == 1 ? writer.set_mute(active.dev_idx, active.instance, state, mask)
                          : writer.set_solo(active.dev_idx, active.instance, state, mask);
    if (!changed) return true;

    const QVector<int> roles({Qt::CheckStateRole});
    if (node.level() == NodeLocator::NodeLevel::channel) {
      dataChanged(index, index, roles);
      auto p = index.parent().siblingAtColumn(column);
      dataChanged(p, p, roles);
    } else {
      dataChanged(index, index, roles);
      const QModelIndex top_left{createIndex(0, column, NodeLocator(index).child(0).internal())},
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

  std::string_view link_group;
  for (const auto *d = &dev_cfg; d;) {
    for (const auto &cs : d->channels) {
      // FIXME: This isn't quite right
      const auto group = link_group.size() ? link_group : cs.name();
      if (idx < cs.serial) {
        return group.size()
                   ? kLabelGroupedChannelNum.arg(QString::fromUtf8(group.data(), group.size()))
                         .arg(idx + 1)
                   : kLabelChannelNum.arg(idx + 1);
      } else if (idx < cs.serial + static_cast<int>(cs.named.size())) {
        idx -= cs.serial;
        return group.size() ? kLabelGroupedChannelName.arg(
                                  QString::fromUtf8(group.data(), group.size()), cs.named[idx])
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

  const NodeLocator node{index};
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
        case NodeLocator::NodeLevel::device:
          return QString("%1 %2").arg(dev_info.label).arg(active.instance + 1);
        case NodeLocator::NodeLevel::channel:
          return channel_name(dev_info, row);
        default:
          return {};
      }
    case Qt::CheckStateRole: {
      if (column != 1 && column != 2) return {};
      auto dev_settings = settings_manager_.current().devices[active.dev_idx];
      const auto &chip = dev_settings.chips[active.instance];
      const auto &state = column == 1 ? chip.channel_mute : chip.channel_solo;

      if (node.level() == NodeLocator::NodeLevel::device) {
        if (state.none()) return Qt::Unchecked;

        const uint32_t mask{(uint32_t{1} << dev_info.num_channels_all()) - 1};
        const uint32_t masked = state.to_ulong() & mask;

        return masked == mask ? Qt::Checked : Qt::PartiallyChecked;
      } else if (node.level() == NodeLocator::NodeLevel::channel) {
        return state[row] ? Qt::Checked : Qt::Unchecked;
      }
    }
    default:
      return {};
  }
}

Qt::ItemFlags ChannelTreeModel::flags(const QModelIndex &index) const {
  if (!index.isValid()) return Qt::NoItemFlags;

  const NodeLocator node{index};
  const auto &dev_info = info_list_[active_devices_[node.top_idx()].dev_idx];

  Qt::ItemFlags flags;

  switch (node.level()) {
    case NodeLocator::NodeLevel::channel:
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
    case NodeLocator::NodeLevel::device:
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

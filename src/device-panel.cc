//
// Created by luebsj on 4/9/20.
//
#include "device-panel.h"

#include <libaudcore/i18n.h>
#include <libaudqt/libaudqt.h>

#include "libvgm-dev-meta.h"

namespace audvgm {
class ChannelTreeModel : public QAbstractItemModel {
 public:
  explicit ChannelTreeModel(QObject* parent = nullptr,
                            const vgm::DeviceConfigList& device_db = vgm::lib_devices);
  QModelIndex index(int row, int column, const QModelIndex& parent) const override;
  QModelIndex parent(const QModelIndex& child) const override;
  int rowCount(const QModelIndex& parent) const override;
  int columnCount(const QModelIndex& parent) const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role) override;
  QVariant data(const QModelIndex& index, int role) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

  void set_current_device(int idx);
 private:
  const vgm::DeviceConfigList& device_db_;
  std::vector<std::reference_wrapper<const vgm::DeviceConfig>> current_devices_;
};
}  // namespace audvgm

using namespace audvgm;

ChannelTreeModel::ChannelTreeModel(QObject* parent, const vgm::DeviceConfigList& device_db)
    : device_db_(device_db) {}

QVariant ChannelTreeModel::data(const QModelIndex& index, int role) const { return QVariant(); }

QModelIndex ChannelTreeModel::index(int row, int column, const QModelIndex& parent) const {
  return QModelIndex();
}

Qt::ItemFlags ChannelTreeModel::flags(const QModelIndex& index) const {
  return QAbstractItemModel::flags(index);
}

QModelIndex ChannelTreeModel::parent(const QModelIndex& child) const { return QModelIndex(); }

int ChannelTreeModel::rowCount(const QModelIndex& parent) const {
  int num_channels = current_devices_.size() > 1 ? static_cast<int>(current_devices_.size()) : 0;
  for (const vgm::DeviceConfig& d : current_devices_) {
    //num_channels += d.num_channels_all();
  }
  return num_channels;
}

int ChannelTreeModel::columnCount(const QModelIndex& parent) const {
  // Channel, Mute, Solo, Panning, [Editable area]
  return 0;
}

bool ChannelTreeModel::setData(const QModelIndex& index, const QVariant& value, int role) {
  if (!index.isValid())
    return false;

}

QVariant ChannelTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
  static constexpr std::array<const char*, 5> headers = {N_("Channel"), N_("M"), N_("S"),
                                                         N_("Panning"), N_("Comment")};
  if (role == Qt::DisplayRole) {
    if (section < 0 || section >= headers.size()) return QVariant();
    return tr(headers[section]);
  }
  return QAbstractItemModel::headerData(section, orientation, role);
}

void ChannelTreeModel::set_current_device(int idx) {
  beginResetModel();
#if 0
  if (idx >= 0 && idx < device_db_.len)
    cur_device_ = &device_db_[idx];
#endif
  endResetModel();
}

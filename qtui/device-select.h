#ifndef AUDACIOUS_VGM_DEVICE_SELECT_H
#define AUDACIOUS_VGM_DEVICE_SELECT_H

#include <QtCore/QSortFilterProxyModel>

#include <corelib/libvgm-dev-meta.h>

namespace vgmtk {
class SettingsManager;
class PlaybackStatus;
}

namespace vgmqt {

struct PlayingDevice {
  int dev_idx;
  int instance;
  uint32_t core;
  const char *accurate_name;
};

class DeviceTreeModel : public QAbstractItemModel {
 public:
  enum ItemDataRole { PlayingDeviceRole = Qt::UserRole };

  explicit DeviceTreeModel(vgmtk::SettingsManager& settings_manager,
                           vgmtk::PlaybackStatus& playback_status,
                           QObject* parent = nullptr,
                           const vgmtk::DeviceConfigList& info_list = vgmtk::lib_devices);
  QModelIndex index(int row, int column, const QModelIndex& parent) const override;
  QModelIndex parent(const QModelIndex& index) const override;
  int rowCount(const QModelIndex& parent) const override;
  int columnCount(const QModelIndex& parent) const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role) override;
  QVariant data(const QModelIndex& index, int role) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

 private:
  vgmtk::SettingsManager& settings_manager_;
  const vgmtk::DeviceConfigList &info_list_;

  void data_changed_helper(const QModelIndex& p, const QVector<int>& roles);

  using PlayingDevices = std::vector<PlayingDevice>;
  PlayingDevices playing_devices_;
};

class DeviceTreeFiltered : public QSortFilterProxyModel {
 protected:
  bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

 public:
  explicit DeviceTreeFiltered(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {
    //setFilterRole(DeviceTreeModel::PlayingDeviceRole);
    setDynamicSortFilter(true);
  }
};

}

#endif  // AUDACIOUS_VGM_DEVICE_SELECT_H

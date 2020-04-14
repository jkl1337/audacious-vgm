#ifndef AUDACIOUS_VGM_DEVICE_SELECT_H
#define AUDACIOUS_VGM_DEVICE_SELECT_H

#include <QtCore/QAbstractItemModel>
#include <QtCore/QSortFilterProxyModel>
#include <QtWidgets/QTreeView>
#include <bitset>

#include "libvgm-dev-meta.h"

namespace audvgm {
class SettingsManager;
class PlaybackStatus;
}

namespace vgmqt {

class DeviceTreeModel : public QAbstractItemModel {
 public:
  enum ItemDataRole { PlayingDeviceRole = Qt::UserRole };

  explicit DeviceTreeModel(audvgm::SettingsManager& settings_manager,
                           audvgm::PlaybackStatus& playback_status,
                           QObject* parent = nullptr,
                           const vgm::DeviceConfigList& info_list = vgm::lib_devices);
  QModelIndex index(int row, int column, const QModelIndex& parent) const override;
  QModelIndex parent(const QModelIndex& index) const override;
  int rowCount(const QModelIndex& parent) const override;
  int columnCount(const QModelIndex& parent) const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role) override;
  QVariant data(const QModelIndex& index, int role) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

 private:
  audvgm::SettingsManager& settings_manager_;
  const vgm::DeviceConfigList &info_list_;

  void data_changed_helper(const QModelIndex& p, const QVector<int>& roles);

  struct RunningDevice {
    int dev_idx;
    int instance;
    const char *accurate_name;

    bool operator <(const RunningDevice &rhs) const {
      return (dev_idx < rhs.dev_idx || (dev_idx == rhs.dev_idx && instance < rhs.instance));
    }
  };
  using PlayingDevices = std::bitset<vgm::kNumDevices * vgm::kNumInstances>;
  PlayingDevices playing_devices_old_;
  using PlayingDevices2 = std::vector<RunningDevice>;
  PlayingDevices2 playing_devices_3_;
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

#ifndef AUDACIOUS_VGM_CHANNEL_SELECT_H
#define AUDACIOUS_VGM_CHANNEL_SELECT_H

#include <QtCore/QAbstractItemModel>

#include "../corelib/device-config.h"
#include "../corelib/types.h"

namespace vgmtk {
class SettingsManager;
class PlaybackStatus;
}

namespace vgmqt {

class ChannelTreeModel : public QAbstractItemModel {
 public:
  explicit ChannelTreeModel(vgmtk::SettingsManager& l, vgmtk::PlaybackStatus* r = nullptr,
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

  using ActiveDevices = std::vector<vgmtk::InstanceId>;
  ActiveDevices active_devices_;
  std::vector<std::vector<std::string>> comments_;
};

}  // namespace vgmqt

#endif  // AUDACIOUS_VGM_CHANNEL_SELECT_H

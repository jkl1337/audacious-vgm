#ifndef AUDACIOUS_VGM_CHANNEL_SELECT_H
#define AUDACIOUS_VGM_CHANNEL_SELECT_H

#include <QtCore/QAbstractItemModel>

#include "libvgm-dev-meta.h"

namespace audvgm {
class SettingsManager;
class PlaybackStatus;
}

namespace vgmqt {

class ChannelTreeModel : public QAbstractItemModel {
 public:
  explicit ChannelTreeModel(audvgm::SettingsManager& settings_manager,
                            audvgm::PlaybackStatus* playback_status = nullptr,
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

  struct DeviceKey {
    int dev_idx;
    int instance;
  };
  std::vector<int> active_devices_;
  std::vector<std::vector<std::string>> comments_;
};

}  // namespace vgmqt

#endif  // AUDACIOUS_VGM_CHANNEL_SELECT_H

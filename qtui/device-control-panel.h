#ifndef VGMTK_DEVICE_CONTROL_PANEL_H
#define VGMTK_DEVICE_CONTROL_PANEL_H

#include <QtWidgets/QWidget>

namespace vgmtk {
class PlaybackStatus;
class SettingsManager;
}  // namespace vgmtk
namespace vgmqt {
class DeviceControlPanelPrivate;

class DeviceControlPanel : public QWidget {
  Q_OBJECT
  std::unique_ptr<DeviceControlPanelPrivate> const d_ptr;

  Q_DECLARE_PRIVATE(DeviceControlPanel)

 public:
  DeviceControlPanel(vgmtk::PlaybackStatus &playback_status,
                     vgmtk::SettingsManager &settings_manager, QWidget *parent = nullptr);
  ~DeviceControlPanel() override;
};

}  // namespace vgmqt

#endif  // VGMTK_DEVICE_CONTROL_PANEL_H

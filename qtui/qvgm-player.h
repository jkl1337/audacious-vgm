#ifndef VGMTK_QVGM_PLAYER_H
#define VGMTK_QVGM_PLAYER_H

#include <corelib/libvgm-iface.h>

#include <QtWidgets/QWidget>

namespace vgmtk {
class PlaybackStatus;
class SettingsManager;
}

namespace vgmqt {

class DeviceControlPanel : public QWidget {
   public:
    explicit DeviceControlPanel(vgmtk::PlaybackStatus &playback_status, vgmtk::SettingsManager &settings_manager,
                               QWidget *parent = nullptr);

   private:
  };

}  // namespace vgmqt

#endif  // VGMTK_QVGM_PLAYER_H

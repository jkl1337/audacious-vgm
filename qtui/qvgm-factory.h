#ifndef VGMTK_QVGM_FACTORY_H
#define VGMTK_QVGM_FACTORY_H

class QWidget;

namespace vgmtk {
class PlaybackStatus;
class SettingsManager;
}

namespace vgmqt {
QWidget *create_qvgm_control_panel(vgmtk::PlaybackStatus &, vgmtk::SettingsManager &,
                                   QWidget *parent = nullptr);
}

#endif  // VGMTK_QVGM_FACTORY_H

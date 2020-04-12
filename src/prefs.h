#ifndef AUDACIOUS_VGM_PREFS_H
#define AUDACIOUS_VGM_PREFS_H

#include "comm.h"
class PlaybackStatus;

namespace audvgm {

void* construct_chip_control_widget(PlaybackStatus& playback_status,
                                    audvgm::SettingsManager& settings_manager);

}

#endif //AUDACIOUS_VGM_PREFS_H

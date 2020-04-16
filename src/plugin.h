#ifndef AUDACIOUS_VGM_PLUGIN_H
#define AUDACIOUS_VGM_PLUGIN_H

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>

#include "comm.h"
#include "config.h"

namespace vgm {

static void ui_realtime_config_update();

class Plugin : public InputPlugin {
 public:
  static const char about[];
  static const char *const exts[];
  static const char *const defaults[];
  static const PluginPreferences prefs;

  static constexpr PluginInfo info = {N_("libvgm VGM Music Decoder"), "audacious-vgm",
                                      about, &prefs};

  constexpr Plugin()
      : InputPlugin(info, InputInfo().with_priority(1).with_exts(exts)) {}

  bool init() override;
  void cleanup() override;

  bool is_our_file(const char *filename, VFSFile &file) override;

  bool read_tag(const char *filename, VFSFile &file, Tuple &tuple,
                Index<char> *image) override;
  bool play(const char *filename, VFSFile &file) override;

  PlaybackStatus &playback_status() const { return *playback_status_; }
  SettingsManager &settings_manager() const { return *settings_container_; }
  static SettingsManager *settings_hack;

 private:
  PlaybackStatus *playback_status_ = nullptr;
  SettingsManager *settings_container_ = nullptr;
};

}

#endif  // AUDACIOUS_VGM_PLUGIN_H

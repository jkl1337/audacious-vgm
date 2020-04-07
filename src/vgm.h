#ifndef AUDACIOUS_VGM_VGM_H
#define AUDACIOUS_VGM_VGM_H

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>

namespace audvgm {


class Plugin : public InputPlugin {
 public:
  static const char about[];
  static const char *const exts[];
  static const char *const defaults[];
  static const PluginPreferences prefs;

  static constexpr PluginInfo info = {N_("libvgm VGM Music Decoder"), "audacious-vgm",
                                      about, &prefs};

  constexpr Plugin()
      : InputPlugin(info, InputInfo(FlagSubtunes).with_priority(1).with_exts(exts)) {}

  bool init() override;
  void cleanup() override;

  bool is_our_file(const char *filename, VFSFile &file) override { return false; }

  bool read_tag(const char *filename, VFSFile &file, Tuple &tuple,
                Index<char> *image) override;
  bool play(const char *filename, VFSFile &file) override;
};

}

#endif  // AUDACIOUS_VGM_VGM_H

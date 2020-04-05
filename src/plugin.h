#ifndef VGM_PLUGIN_H_
#define VGM_PLUGIN_H_

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>

class VgmPlugin : public InputPlugin {
public:
    static const char about[];
    static const char * const exts[];
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("libvgm VGM Music Decoder"),
        "audacious-vgm",
        about,
        & prefs
    };

    constexpr VgmPlugin () : InputPlugin (info, InputInfo (FlagSubtunes)
                                          .with_exts (exts)) {}

    bool init();
    void cleanup();

    bool is_our_file (const char *filename, VFSFile & file)
        { return false;  }

    bool read_tag(const char *filename, VFSFile & file, Tuple & tuple, Index<char> * image);
    bool play(const char *filename, VFSFile & file);
};

#endif // VGM_PLUGIN_H_

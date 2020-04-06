#include "configure.h"
#include "plugin.h"

#include <libaudcore/runtime.h>

#define VGM_CFGID "vgm"

AudaciousVgmConfig audcfg;

const char * const VgmPlugin::defaults[] = {
    "num_loops", "2",
    "fade_length", "6",
    nullptr
};

#include <libaudcore/plugins.h>
#include <libaudcore/interface.h>
#include <libaudqt/libaudqt.h>
#include "plugin.h"

extern VgmPlugin aud_plugin_instance;

static void vgm_configure()
{
    audqt::plugin_prefs(aud_plugin_by_header(&aud_plugin_instance));
}

bool VgmPlugin::init()
{
    aud_config_set_defaults(VGM_CFGID, defaults);

    audcfg.num_loops = aud_get_int(VGM_CFGID, "num_loops");
    audcfg.fade_length = aud_get_double(VGM_CFGID, "fade_length");

    aud_plugin_menu_add(AudMenuID::Main, vgm_configure, _("VGM Configure..."), 0);

    return true;
}

void VgmPlugin::cleanup()
{
    aud_set_int(VGM_CFGID, "num_loops", audcfg.num_loops);
    aud_set_int(VGM_CFGID, "fade_length", audcfg.fade_length);
}

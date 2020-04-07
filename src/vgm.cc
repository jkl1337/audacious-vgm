
#include "vgm.h"

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>
#include <libaudqt/libaudqt.h>

#include "config.h"

namespace audvgm {
void vgm_configure();

/* FIXME, atomic sync objects for config groups */
// Config config;

bool Plugin::init() {
  //  static constexpr const char *defaults[] = {"num_loops", "2", "fade_length", "6",
  //                                             nullptr};
  //  aud_config_set_defaults(VGM_CFGID, defaults);
  set_defaults();

  aud_plugin_menu_add(AudMenuID::Main, audvgm::vgm_configure, _("VGM Configure..."),
                      nullptr);

  return true;
}

void Plugin::cleanup() {}

const char Plugin::about[] =
    N_("VGM music decoder based on libvgm 1.0\n"
       "Supported formats: vgm\n\n"
       "Audacious plugin by:\n"
       "John Luebs <jkl@johnluebs.com>\n");

const char *const Plugin::exts[] = {"vgm", "vgz", nullptr};

int playback_rate_item_selected;

const ComboItem playback_rate_items[] = {
    {N_("Recorded Rate"), 0},
    {N_("60 Hz (NTSC)"), 60},
    {N_("50 Hz (PAL)"), 50},
    {N_("Other:"), -1},
};

static inline int playback_rate_preset(int r) {
  switch (r) {
    case 0:
    case 50:
    case 60:
      return r;
    default:
      return -1;
  }
}

static ArrayRef<ComboItem> playback_rate_combo_fill() {
  ArrayRef<ComboItem> items(playback_rate_items);
  playback_rate_item_selected =
      playback_rate_preset(aud_get_int(VGM_CFGID, cfg::PLAYBACK_RATE));
  if (playback_rate_item_selected != -1) items.len = 3;
  return items;
}

static void playback_rate_combo_change() {
  if (playback_rate_item_selected != -1)
    aud_set_int(VGM_CFGID, cfg::PLAYBACK_RATE, playback_rate_item_selected);
  hook_call("audvgm config playback_rate", nullptr);
}

static void playback_rate_entry_change() {
  playback_rate_item_selected =
      playback_rate_preset(aud_get_int(VGM_CFGID, cfg::PLAYBACK_RATE));
  hook_call("audvgm config playback_rate", nullptr);
}

const PreferencesWidget playback_rate_widgets[] = {
    WidgetCombo(N_("Playback Rate"),
                WidgetInt(playback_rate_item_selected, playback_rate_combo_change,
                          "audvgm config playback_rate"),
                {{}, playback_rate_combo_fill}),
    WidgetSpin(nullptr,
               WidgetInt(VGM_CFGID, cfg::PLAYBACK_RATE, playback_rate_entry_change,
                         "audvgm config playback_rate"),
               {0, 8000, 1, N_("Hz")}),
};

const ComboItem resampler_quality_items[] = {
    {N_("High"), cfg::RESAMPLE_QUALITY_HQ},
    {N_("HQ up, LQ down"), cfg::RESAMPLE_QUALITY_HQUP},
    {N_("Low"), cfg::RESAMPLE_QUALITY_LQ},
};

const ComboItem chip_sr_mode_items[] = {
    {N_("Native"), cfg::CHIP_SR_MODE_NATIVE},
    {N_("Highest"), cfg::CHIP_SR_MODE_HIGHEST},
    {N_("Custom"), cfg::CHIP_SR_MODE_CUSTOM},
};

static const PreferencesWidget length_widgets[] = {
    WidgetLabel(N_("<b>Length</b>")),
    WidgetSpin(N_("Loop Count:"), WidgetInt(VGM_CFGID, cfg::LOOP_COUNT),
               {1, 14400, 1, N_("times")}),
    WidgetSpin(N_("Fade time:"), WidgetInt(VGM_CFGID, cfg::FADE_LENGTH),
               {0, 360000, 200, N_("ms")}),
    WidgetCheck(N_("Hard stop old VGMs"), WidgetBool(VGM_CFGID, cfg::HARD_STOP_OLD)),
    WidgetLabel(N_("<b>Gap between tracks</b>")),
    WidgetSpin(N_("Looping:"), WidgetInt(VGM_CFGID, cfg::GAP_LOOPING),
               {0, 360000, 200, N_("ms")}),
    WidgetSpin(N_("Non-looping:"), WidgetInt(VGM_CFGID, cfg::GAP_NON_LOOPING),
               {0, 360000, 200, N_("ms")}),
    WidgetLabel(N_("<b>Tags</b>")),
    WidgetCheck(N_("Prefer Japanese Tags"), WidgetBool(VGM_CFGID, cfg::PREFER_JPN_TAG)),
    WidgetCheck(N_("Guess track number from file name"),
                WidgetBool(VGM_CFGID, cfg::GUESS_TRACK_NUM))};

static const PreferencesWidget playback_widgets[] = {
    WidgetLabel(N_("<b>Playback Quality</b>")),
    WidgetSpin(N_("Sample Rate"), WidgetInt(VGM_CFGID, cfg::SAMPLE_RATE),
               {8000, 96000, 50, N_("Hz")}),
    WidgetCombo(N_("Resample Quality"), WidgetString(VGM_CFGID, cfg::RESAMPLE_QUALITY),
                {{resampler_quality_items}}),
    WidgetBox({{playback_rate_widgets}, true}),
    WidgetCombo(N_("Chip Sample Mode"), WidgetString(VGM_CFGID, cfg::CHIP_SR_MODE),
                {{chip_sr_mode_items}}),
    WidgetSpin(N_("Chip Rate"), WidgetInt(VGM_CFGID, cfg::CHIP_SAMPLE_RATE),
               {0, 96000, 50, N_("Hz")}),
    WidgetLabel(N_("<b>Audio Output</b>")),
    WidgetSpin(N_("Gain"), WidgetFloat(VGM_CFGID, cfg::GAIN), {0.0, 8.0, 0.1}),
    WidgetCheck(N_("\"Surround\" Sound"), WidgetBool(VGM_CFGID, cfg::SURROUND)),
    WidgetCheck(N_("Double SSG volume"), WidgetBool(VGM_CFGID, cfg::SSG_2X_VOL)),
};

static const PreferencesWidget general_widgets_columns[] = {
    WidgetBox({length_widgets}),
    WidgetSeparator(),
    WidgetBox({playback_widgets}),
};

static const PreferencesWidget general_widgets[] = {
    WidgetBox({{general_widgets_columns}, true})};
static const NotebookTab notebook_tabs[] = {{N_("General"), {general_widgets}},
                                            {N_("Other Options"), {}}};

const PreferencesWidget widgets[] = {WidgetNotebook({{notebook_tabs}})};

const PluginPreferences Plugin::prefs = {widgets};

}  // namespace audvgm

LIBAUDCORE_PUBLIC audvgm::Plugin aud_plugin_instance;

void audvgm::vgm_configure() {
  audqt::plugin_prefs(aud_plugin_by_header(&aud_plugin_instance));
}

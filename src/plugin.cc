
#include "plugin.h"

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>
#include <libaudqt/libaudqt.h>

#include "config.h"
#include "prefs.h"

namespace audvgm {

void vgm_configure();

SettingsManager *Plugin::settings_hack = nullptr;

bool Plugin::init() {
  set_defaults();

  playback_status_ = new PlaybackStatus;
  PlayerSettings ps = PlayerSettings::init_lib_defaults();

  settings_container_ = new SettingsManager(ps);
  settings_hack = settings_container_;

  aud_plugin_menu_add(AudMenuID::Main, audvgm::vgm_configure, _("VGM Configure..."),
                      nullptr);

  return true;
}

void Plugin::cleanup() {
  delete playback_status_;
  delete settings_container_;
}

const char Plugin::about[] =
    N_("VGM music decoder based on libvgm 1.0\n"
       "Supported formats: vgm\n\n"
       "Audacious plugin by:\n"
       "John Luebs <jkl@johnluebs.com>\n");

const char *const Plugin::exts[] = {"vgm", "vgz", nullptr};

static void *populate_chip_prefs();

namespace {

int playback_rate_item_selected;

const ComboItem playback_rate_items[] = {
    {N_("Recorded Rate"), 0},
    {N_("60 Hz (NTSC)"), 60},
    {N_("50 Hz (PAL)"), 50},
    {N_("Other:"), -1},
};

inline int playback_rate_preset(int r) {
  switch (r) {
    case 0:
    case 50:
    case 60:
      return r;
    default:
      return -1;
  }
}

ArrayRef<ComboItem> playback_rate_combo_fill() {
  ArrayRef<ComboItem> items(playback_rate_items);
  playback_rate_item_selected =
      playback_rate_preset(aud_get_int(VGM_CFGID, cfg::kPlaybackRate));
  if (playback_rate_item_selected != -1) items.len = 3;
  return items;
}

void playback_rate_combo_change() {
  if (playback_rate_item_selected != -1)
    aud_set_int(VGM_CFGID, cfg::kPlaybackRate, playback_rate_item_selected);
  hook_call("audvgm config playback_rate", nullptr);
}

void playback_rate_entry_change() {
  playback_rate_item_selected =
      playback_rate_preset(aud_get_int(VGM_CFGID, cfg::kPlaybackRate));
  hook_call("audvgm config playback_rate", nullptr);
}

const PreferencesWidget playback_rate_widgets[] = {
    WidgetCombo(N_("Playback Rate"),
                WidgetInt(playback_rate_item_selected, playback_rate_combo_change,
                          "audvgm config playback_rate"),
                {{}, playback_rate_combo_fill}),
    WidgetSpin(nullptr,
               WidgetInt(VGM_CFGID, cfg::kPlaybackRate, playback_rate_entry_change,
                         "audvgm config playback_rate"),
               {0, 8000, 1, N_("Hz")}),
};

const ComboItem resampler_quality_items[] = {
    {N_("High"), cfg::kResampleQualityHQ},
    {N_("HQ up, LQ down"), cfg::kResampleQualityHQUp},
    {N_("Low"), cfg::kResampleQualityLQ},
};

const ComboItem chip_sr_mode_items[] = {
    {N_("Native"), cfg::kChipSrModeNative},
    {N_("Highest"), cfg::kChipSrModeHighest},
    {N_("Custom"), cfg::kChipSrModeCustom},
};

const PreferencesWidget length_widgets[] = {
    WidgetLabel(N_("<b>Length</b>")),
    WidgetSpin(N_("Loop Count:"), WidgetInt(VGM_CFGID, cfg::kLoopCount),
               {1, 14400, 1, N_("times")}),
    WidgetSpin(N_("Fade time:"), WidgetInt(VGM_CFGID, cfg::kFadeLength),
               {0, 360000, 200, N_("ms")}),
    WidgetCheck(N_("Hard stop old VGMs"), WidgetBool(VGM_CFGID, cfg::kHardStopOld)),
    WidgetLabel(N_("<b>Gap between tracks</b>")),
    WidgetSpin(N_("Looping:"), WidgetInt(VGM_CFGID, cfg::kGapLooping),
               {0, 360000, 200, N_("ms")}),
    WidgetSpin(N_("Non-looping:"), WidgetInt(VGM_CFGID, cfg::kGapNonLooping),
               {0, 360000, 200, N_("ms")}),
    WidgetLabel(N_("<b>Tags</b>")),
    WidgetCheck(N_("Prefer Japanese Tags"), WidgetBool(VGM_CFGID, cfg::kPreferJPNTag)),
    WidgetCheck(N_("Guess track number from file name"),
                WidgetBool(VGM_CFGID, cfg::kGuessTrackNum))};

static void test_callback() {
  auto gain = aud_get_double(VGM_CFGID, cfg::kGain);
  auto m = Plugin::settings_hack->mutate();
  m.set_gain(gain);
  m.commit();
}
const PreferencesWidget playback_widgets[] = {
    WidgetLabel(N_("<b>Playback Quality</b>")),
    WidgetSpin(N_("Sample Rate"), WidgetInt(VGM_CFGID, cfg::kSampleRate),
               {8000, 96000, 50, N_("Hz")}),
    WidgetCombo(N_("Resample Quality"), WidgetString(VGM_CFGID, cfg::kResampleQuality),
                {{resampler_quality_items}}),
    WidgetBox({{playback_rate_widgets}, true}),
    WidgetCombo(N_("Chip Sample Mode"), WidgetString(VGM_CFGID, cfg::kChipSrMode),
                {{chip_sr_mode_items}}),
    WidgetSpin(N_("Chip Rate"), WidgetInt(VGM_CFGID, cfg::kChipSampleRate),
               {0, 96000, 50, N_("Hz")}),
    WidgetLabel(N_("<b>Audio Post Process</b>")),
    WidgetSpin(N_("Gain"), WidgetFloat(VGM_CFGID, cfg::kGain, test_callback), {0.0, 4.0, 0.005}),
    WidgetCheck(N_("\"Surround\" Sound"), WidgetBool(VGM_CFGID, cfg::kSurround)),
    WidgetCheck(N_("Double SSG volume"), WidgetBool(VGM_CFGID, cfg::kSSG2XVol)),
};

const PreferencesWidget general_widgets_columns[] = {
    WidgetBox({length_widgets}),
    WidgetSeparator(),
    WidgetBox({playback_widgets}),
};

const PreferencesWidget general_widgets[] = {
    WidgetBox({{general_widgets_columns}, true})};


const PreferencesWidget chip_widgets[] = {
    WidgetCustomQt(audvgm::populate_chip_prefs)
};

const NotebookTab notebook_tabs[] = {{N_("General"), {general_widgets}},
                                            {N_("Other Options"), {chip_widgets}}};

const PreferencesWidget widgets[] = {WidgetNotebook({{notebook_tabs}})};

}  // namespace

const PluginPreferences Plugin::prefs = {widgets};

}  // namespace audvgm

LIBAUDCORE_PUBLIC audvgm::Plugin aud_plugin_instance;

static void *audvgm::populate_chip_prefs() {
  auto plugin = static_cast<Plugin*>(&aud_plugin_instance);
  return construct_chip_control_widget(plugin->playback_status(), plugin->settings_manager());
}

void audvgm::vgm_configure() {
  audqt::plugin_prefs(aud_plugin_by_header(&aud_plugin_instance));
}


#include "plugin.h"

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>
#include <qtui/qvgm-factory.h>

#include "config.h"

namespace audqt {
void plugin_prefs(PluginHandle *ph);  // NOLINT
}

LIBAUDCORE_PUBLIC vgmtk::Plugin aud_plugin_instance;  // NOLINT

namespace vgmtk {

SettingsManager *Plugin::global_settings = nullptr;

bool Plugin::init() {
  set_defaults();

  playback_status_ = new PlaybackStatus;
  const PlayerSettings ps = PlayerSettings::init_lib_defaults();

  settings_container_ = new SettingsManager(ps);
  global_settings = settings_container_;

  hook_associate(
      "audvgm init",
      [](void *, void *) { audqt::plugin_prefs(aud_plugin_by_header(&aud_plugin_instance)); },
      nullptr);
  event_queue("audvgm init", nullptr);
  // aud_plugin_menu_add(AudMenuID::Main, vgmtk::vgm_configure, _("VGM Configure..."), nullptr);

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

namespace {

#if 0
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

::ArrayRef<ComboItem> playback_rate_combo_fill() {
  ::ArrayRef items(playback_rate_items);
  playback_rate_item_selected = playback_rate_preset(aud_get_int(VGM_CFGID, cfg::kPlaybackRate));
  if (playback_rate_item_selected != -1) items.len = 3;
  return items;
}

void playback_rate_combo_change() {
  if (playback_rate_item_selected != -1)
    aud_set_int(VGM_CFGID, cfg::kPlaybackRate, playback_rate_item_selected);
  hook_call("audvgm config playback_rate", nullptr);
}

void playback_rate_entry_change() {
  playback_rate_item_selected = playback_rate_preset(aud_get_int(VGM_CFGID, cfg::kPlaybackRate));
  hook_call("audvgm config playback_rate", nullptr);
}

constexpr PreferencesWidget playback_rate_widgets[]{
    WidgetCombo(N_("Playback Rate"),
                WidgetInt(playback_rate_item_selected, playback_rate_combo_change,
                          "audvgm config playback_rate"),
                {.elems = {}, .fill = playback_rate_combo_fill}),
    WidgetSpin(nullptr,
               WidgetInt(VGM_CFGID, cfg::kPlaybackRate, playback_rate_entry_change,
                         "audvgm config playback_rate"),
               {.min = 0, .max = 8000, .step = 1, .right_label = N_("Hz")}),
};

constexpr ComboItem resampler_quality_items[]{
    {N_("High"), cfg::kResampleQualityHQ},
    {N_("HQ up, LQ down"), cfg::kResampleQualityHQUp},
    {N_("Low"), cfg::kResampleQualityLQ},
};

constexpr ComboItem chip_sr_mode_items[]{
    {N_("Native"), cfg::kChipSrModeNative},
    {N_("Highest"), cfg::kChipSrModeHighest},
    {N_("Custom"), cfg::kChipSrModeCustom},
};
#endif

constexpr PreferencesWidget length_widgets[]{
    WidgetLabel(N_("<b>Length</b>")),
    WidgetSpin(N_("Loop Count:"), WidgetInt(VGM_CFGID, cfg::kLoopCount),
               {1, 14400, 1, N_("times")}),
    WidgetSpin(N_("Fade time:"), WidgetInt(VGM_CFGID, cfg::kFadeLength),
               {0, 360000, 200, N_("ms")}),
    WidgetCheck(N_("Hard stop old VGMs"), WidgetBool(VGM_CFGID, cfg::kHardStopOld)),
    WidgetLabel(N_("<b>Gap between tracks</b>")),
    WidgetSpin(N_("Looping:"), WidgetInt(VGM_CFGID, cfg::kGapLooping),
               {.min = 0, .max = 360000, .step = 200, .right_label = N_("ms")}),
    WidgetSpin(N_("Non-looping:"), WidgetInt(VGM_CFGID, cfg::kGapNonLooping),
               {0, 360000, 200, N_("ms")}),
    WidgetLabel(N_("<b>Tags</b>")),
    WidgetCheck(N_("Prefer Japanese Tags"), WidgetBool(VGM_CFGID, cfg::kPreferJPNTag)),
    WidgetCheck(N_("Guess track number from file name"),
                WidgetBool(VGM_CFGID, cfg::kGuessTrackNum))};

void soundfx_callback() {
  auto gain = aud_get_double(VGM_CFGID, cfg::kGain);
  auto surround = aud_get_bool(VGM_CFGID, cfg::kSurround);
  auto m = Plugin::global_settings->writer();
  m.set_gain(gain);
  m.set_surround(surround);
}

constexpr PreferencesWidget playback_widgets[]{
    WidgetLabel(N_("<b>Playback Quality</b>")),
    WidgetSpin(N_("Sample Rate"), WidgetInt(VGM_CFGID, cfg::kSampleRate),
               {.min = 8000, .max = 96000, .step = 50, .right_label = N_("Hz")}),
#if 0
    WidgetCombo(N_("Resample Quality"), WidgetString(VGM_CFGID, cfg::kResampleQuality),
                {{resampler_quality_items}}),
    WidgetBox({{playback_rate_widgets}, true}),
    WidgetCombo(N_("Chip Sample Mode"), WidgetString(VGM_CFGID, cfg::kChipSrMode),
                {{chip_sr_mode_items}}),
    WidgetSpin(N_("Chip Rate"), WidgetInt(VGM_CFGID, cfg::kChipSampleRate),
               {0, 96000, 50, N_("Hz")}),
#endif
    WidgetLabel(N_("<b>Audio Post Process</b>")),
    WidgetSpin(N_("Gain"), WidgetFloat(VGM_CFGID, cfg::kGain, soundfx_callback),
               {.min = 0.0, .max = 4.0, .step = 0.005}),
    WidgetCheck(N_("\"Surround\" Sound"), WidgetBool(VGM_CFGID, cfg::kSurround, soundfx_callback)),
    WidgetCheck(N_("Double SSG volume"), WidgetBool(VGM_CFGID, cfg::kSSG2XVol)),
};

constexpr PreferencesWidget general_widgets_columns[]{
    WidgetBox({.widgets = length_widgets}),
    WidgetSeparator(),
    WidgetBox({.widgets = playback_widgets}),
};

constexpr PreferencesWidget general_widgets[]{
    WidgetBox({.widgets = {general_widgets_columns}, .horizontal = true})};

constexpr PreferencesWidget chip_widgets[]{WidgetCustomQt([]() -> void * {
  const auto &plugin = aud_plugin_instance;
  return vgmqt::create_qvgm_control_panel(plugin.playback_status(), plugin.settings_manager());
})};

constexpr NotebookTab notebook_tabs[]{{.name = N_("General"), .widgets = {general_widgets}},
                                      {.name = N_("Other Options"), .widgets = {chip_widgets}}};

constexpr PreferencesWidget widgets[]{WidgetNotebook({{notebook_tabs}})};

}  // namespace

constexpr PluginPreferences Plugin::prefs{.widgets = widgets};

}  // namespace vgmtk

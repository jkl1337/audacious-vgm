#include "device-config.h"

#include <libvgm/emu/EmuCores.h>
#include <libvgm/emu/SoundDevs.h>
#include <libvgm/emu/cores/2612intf.h>
#include <libvgm/emu/cores/c352.h>
#include <libvgm/emu/cores/gb.h>
#include <libvgm/emu/cores/iremga20.h>
#include <libvgm/emu/cores/nesintf.h>
#include <libvgm/emu/cores/okim6258.h>
#include <libvgm/emu/cores/scsp.h>

#include <libvgm/player/playerbase.hpp>

using namespace vgmtk;

namespace {

constexpr const char* sn76489_cname[]{"Noise"};
constexpr const char* ym2413_cname[]{"Bass Drum", "Snare Drum", "Tom Tom", "Cymbal", "Hi-Hat"};
constexpr const char* ym2612_cname[]{"DAC"};
constexpr const char* delta_t_cname[]{"Delta-T"};
constexpr const char* gb_cname[]{"Square 1", "Square 2", "Prog. Wave", "Noise"};
constexpr const char* nes_cname[]{"Square 1", "Square 2", "Triangle", "Noise", "DPCM", "FDS"};

using ChG = ChannelGroup;
using Cr = CoreSpec;

constexpr ChG sn76489_cg[]{{3, sn76489_cname}};
constexpr ChG ym2413_cg[]{{9, ym2413_cname}};
constexpr ChG ym2612_cg[]{{6, ym2612_cname}};
constexpr ChG s1ch_cg[]{{1}}, s3ch_cg[]{{3}}, s4ch_cg[]{{4}}, s5ch_cg[]{{5}}, s6ch_cg[]{{5}},
    s8ch_cg[]{{8}}, s12ch_cg[]{{12}}, s16ch_cg[]{{16}}, s24ch_cg[]{{24}}, s28ch_cg[]{{28}},
    s32ch_cg[]{{32}};
constexpr ChG ym22_cg[]{{3, {}, "FM"}};
constexpr ChG ym26_cg[]{{6, {}, "FM"}, {6, {}, "PCM"}, {0, delta_t_cname}};
constexpr ChG y8950_cg[]{{9, ym2413_cname}, {0, delta_t_cname}};
constexpr ChG ymf262_cg[]{{18, ym2413_cname}};
constexpr ChG gb_cg[]{{0, gb_cname}};
constexpr ChG nes_cg[]{{0, nes_cname}};

constexpr Cr mame_cs[]{{FCC_MAME}};
constexpr Cr sn76489_cs[]{{FCC_MAME}, {FCC_MAXM, "Maxim", true}};
constexpr Cr ym2413_cs[]{{FCC_EMU_, "EMU2413", true}, {FCC_MAME}, {FCC_NUKE, "Nuked OPLL"}};
constexpr Cr ym2612_cs[]{{FCC_GPGX, "GPGX", false}, {FCC_NUKE, "Nuked OPN2"}, {FCC_GENS, "GENS"}};
constexpr Cr rf5c68_cs[]{{FCC_MAME}, {FCC_GENS, "GENS"}};
constexpr Cr ym3812_cs[]{{FCC_MAME}, {FCC_ADLE, "AdLibEmu"}, {FCC_NUKE, "Nuked OPL"}};
constexpr Cr ymf278b_cs[]{{FCC_OMSX, "openMSX"}};
constexpr Cr gens_cs[]{{FCC_GENS, "Gens/GS"}};
constexpr Cr ay8910_cs[]{{FCC_EMU_, "EMU2149", true}};
constexpr Cr nes_cs[]{{FCC_NSFP, "NSFPlay"}, {FCC_MAME}};
constexpr Cr c6280_cs[]{{FCC_OOTK, "Ootake"}, {FCC_MAME}};
constexpr Cr qsound_cs[]{{FCC_CTR_, "ctr"}, {FCC_MAME}};
constexpr Cr wswan_cs[]{{0, "in_wsr"}};
constexpr Cr vboy_cs[]{{FCC_MEDN, "Mednafen"}};
constexpr Cr saa_cs[]{{FCC_MAME}, {FCC_VBEL, "Valley Bell"}};
constexpr Cr mikey_cs[]{{FCC_LAOO}};
constexpr Cr ym2151_cs[]{{FCC_MAME}, {FCC_NUKE, "Nuked OPM"}};

constexpr std::array<OnOffOption, 3> ym2612_chip_type_opts{
    {{.label = "YM2612", .doc = "Emulate original YM2612.", .value = OPT_YM2612_TYPE_OPN2},
     {.label = "YM3438 ASIC", .doc = "Emulate ASIC YM3438.", .value = OPT_YM2612_TYPE_OPN2C_ASIC},
     {.label = "YM3438 Discrete",
      .doc = "Emulate Discrete YM3438.",
      .value = OPT_YM2612_TYPE_OPN2C_DISC}}};

constexpr OptionSpec ym2612_opts[]{
    {"Pseudo Stereo",
     "Pseudo Stereo mode. Alternate between updating left and right "
     "speaker, adds some stereo effect to sharp and noisy sounds.",
     OPT_YM2612_PSEUDO_STEREO, FCC_GPGX},
    {"Legacy Mode",
     "Simulate behavior of older emulation core. Not recommended, but "
     "required for playing GYM files.",
     OPT_YM2612_LEGACY_MODE, FCC_GPGX},
    {"Chip Type", nullptr, ym2612_chip_type_opts, FCC_NUKE},
    {"DAC highpass", "Enable DAC highpass filter.", OPT_YM2612_DAC_HIGHPASS, FCC_GENS},
    {"SSG-EG", "Enable (broken) SSG-EG emulation.", OPT_YM2612_SSGEG, FCC_GENS}};

constexpr OptionSpec gb_opts[]{
    {"Boost Wave Channel", "Boost volume of wave channel by 6dB.", OPT_GB_DMG_BOOST_WAVECH},
    {"Disable WaveRAM corruption",
     "Non-GBC models overwrite parts of the WaveRAM when triggered while "
     "reading a sample.",
     OPT_GB_DMG_NO_WAVE_CORRUPT},
    {"Legacy Mode",
     "Simulate behavior of old MAME core. Required for playing older VGM "
     "files optimized with vgm_cmp.",
     OPT_GB_DMG_LEGACY_MODE}};

constexpr OptionSpec nes_opts[]{
    {"Unmute on reset", "Enable all channels by default after reset.", OPT_NES_UNMUTE_ON_RESET,
     FCC_NSFP},
    {"Non-linear Mixer", nullptr, OPT_NES_NONLINEAR_MIXER, FCC_NSFP},
    {"Phrase Refresh", nullptr, OPT_NES_PHASE_REFRESH, FCC_NSFP},
    {"Duty Swap", "Swap 25:75 and 50:50 duty cycles (behavior of some Famiclones)",
     OPT_NES_DUTY_SWAP, FCC_NSFP},
    {"Enable $4011", "Enable register 4011 writes", OPT_NES_ENABLE_4011, FCC_NSFP},
    {"Enable PNoise",
     "Enable \"loop noise\" (disable for early Famicon console + Arcade "
     "boards)",
     OPT_NES_ENABLE_PNOISE, FCC_NSFP},
    {"DPCM Anti Click", "Nullify register 4011 writes, but preserve nonlinearity",
     OPT_NES_DPCM_ANTI_CLICK, FCC_NSFP},
    {"Randomize Noise", "Randomize noise at device reset", OPT_NES_RANDOMIZE_NOISE, FCC_NSFP},
    {"Tri Mute",
     "Stops Triangle wave if set to freq = 0, processes it a very high rate "
     "otherwise.",
     OPT_NES_TRI_MUTE, FCC_NSFP},
    {"Tri Null",
     "Always make triangle return to null-level when stopping, prevents "
     "clicking (Valley Bell mod).",
     OPT_NES_TRI_NULL, FCC_NSFP},
    {"4085 Reset", "Reset modulation phase on 4085 writes.", OPT_NES_4085_RESET},
    {"FDS Disable", "Disable FDS", OPT_NES_FDS_DISABLE}};

constexpr OptionSpec m6258_opts[]{
    {"Force 12-bit", "Enforce 12-bit output precision", OPT_OKIM6258_FORCE_12BIT}};

constexpr OptionSpec scsp_opts[]{{"Bypass DSP",
                                  "skip all DSP calculations (huge speedup, DSP is broken anyway)",
                                  OPT_SCSP_BYPASS_DSP}};
constexpr OptionSpec c352_opts[]{
    {"Disable Rear", "Mute rear speakers regardless of configuration", OPT_C352_MUTE_REAR}};
constexpr OptionSpec ga20_opts[]{
    {"Linear interpolation", "Enable linear interpolation for samples.", OPT_GA20_INTERPOLATE}};

constexpr int index_AY8910 = 17;
constexpr int index_YMF262 = 12;

constexpr DeviceConfig lib_devices_p[42]{
    {.devid = DEVID_SN76496, .label = "SN76496", .channels = sn76489_cg, .cores = sn76489_cs},
    {.devid = DEVID_YM2413, .label = "YM2413", .channels = ym2413_cg, .cores = ym2413_cs},
    {.devid = DEVID_YM2612,
     .label = "YM2612",
     .channels = ym2612_cg,
     .cores = ym2612_cs,
     .options = ym2612_opts},
    {.devid = DEVID_YM2151, .label = "YM2151", .channels = s8ch_cg, .cores = ym2151_cs},
    {.devid = DEVID_SEGAPCM, .label = "Sega PCM", .channels = s16ch_cg, .cores = mame_cs},
    {.devid = DEVID_RF5C68, .label = "RF5C68", .channels = s8ch_cg, .cores = rf5c68_cs},
    {.devid = DEVID_YM2203,
     .label = "YM2203",
     .channels = ym22_cg,
     .cores = mame_cs,
     .options = {},
     .linked = LinkSpec{.dev = lib_devices_p + index_AY8910, .prefix = "PSG"}},
    {.devid = DEVID_YM2608,
     .label = "YM2608",
     .channels = ym26_cg,
     .cores = mame_cs,
     .options = {},
     .linked = LinkSpec{.dev = lib_devices_p + index_AY8910, .prefix = "PSG"}},
    {.devid = DEVID_YM2610,
     .label = "YM2610",
     .channels = ym26_cg,
     .cores = mame_cs,
     .options = {},
     .linked = LinkSpec{.dev = lib_devices_p + index_AY8910, .prefix = "PSG"}},
    {.devid = DEVID_YM3812, .label = "YM3812", .channels = ym2413_cg, .cores = ym3812_cs},
    {.devid = DEVID_YM3526, .label = "YM3526", .channels = ym2413_cg, .cores = mame_cs},
    {.devid = DEVID_Y8950, .label = "Y8950", .channels = y8950_cg, .cores = mame_cs},
    {.devid = DEVID_YMF262, .label = "YMF262", .channels = ymf262_cg, .cores = ym3812_cs},
    {.devid = DEVID_YMF278B,
     .label = "YMF278B",
     .channels = s24ch_cg,
     .cores = ymf278b_cs,
     .options = {},
     .linked = LinkSpec{lib_devices_p + index_YMF262, "FM"}},
    {.devid = DEVID_YMF271, .label = "YMF271", .channels = s12ch_cg, .cores = mame_cs},
    {.devid = DEVID_YMZ280B, .label = "YMZ280B", .channels = s8ch_cg, .cores = mame_cs},
    {.devid = DEVID_32X_PWM, .label = "PWM", .channels = s1ch_cg, .cores = gens_cs},
    {.devid = DEVID_AY8910, .label = "AY8910", .channels = s3ch_cg, .cores = ay8910_cs},
    {.devid = DEVID_GB_DMG,
     .label = "GameBoy DMG",
     .channels = gb_cg,
     .cores = mame_cs,
     .options = gb_opts},
    {.devid = DEVID_NES_APU,
     .label = "NES APU",
     .channels = nes_cg,
     .cores = nes_cs,
     .options = nes_opts},
    {.devid = DEVID_YMW258, .label = "YMW258", .channels = s28ch_cg, .cores = mame_cs},
    {.devid = DEVID_uPD7759, .label = "uPD7759", .channels = s1ch_cg, .cores = mame_cs},
    {.devid = DEVID_OKIM6258,
     .label = "MSM6258",
     .channels = s1ch_cg,
     .cores = mame_cs,
     .options = m6258_opts},
    {.devid = DEVID_OKIM6295, .label = "MSM6295", .channels = s4ch_cg, .cores = mame_cs},
    {.devid = DEVID_K051649, .label = "K051649", .channels = s5ch_cg, .cores = mame_cs},
    {.devid = DEVID_K054539, .label = "K054539", .channels = s8ch_cg, .cores = mame_cs},
    {.devid = DEVID_C6280, .label = "HuC6280", .channels = s6ch_cg, .cores = c6280_cs},
    {.devid = DEVID_C140, .label = "C140", .channels = s24ch_cg, .cores = mame_cs},
    {.devid = DEVID_C219, .label = "C219", .channels = s16ch_cg, .cores = mame_cs},
    {.devid = DEVID_K053260, .label = "K053260", .channels = s4ch_cg, .cores = mame_cs},
    {.devid = DEVID_POKEY, .label = "POKEY", .channels = s4ch_cg, .cores = mame_cs},
    {.devid = DEVID_QSOUND, .label = "QSound", .channels = s16ch_cg, .cores = qsound_cs},
    {.devid = DEVID_SCSP,
     .label = "SCSP",
     .channels = s32ch_cg,
     .cores = mame_cs,
     .options = scsp_opts},
    {.devid = DEVID_WSWAN, .label = "WonderSwan", .channels = s4ch_cg, .cores = wswan_cs},
    {.devid = DEVID_VBOY_VSU, .label = "VSU-VUE", .channels = s6ch_cg, .cores = vboy_cs},
    {.devid = DEVID_SAA1099, .label = "SAA1099", .channels = s6ch_cg, .cores = saa_cs},
    {.devid = DEVID_ES5503, .label = "ES5503", .channels = s32ch_cg, .cores = mame_cs},
    {.devid = DEVID_ES5506, .label = "ES5506", .channels = s32ch_cg, .cores = mame_cs},
    {.devid = DEVID_X1_010, .label = "X1-010", .channels = s16ch_cg, .cores = mame_cs},
    {.devid = DEVID_C352,
     .label = "C352",
     .channels = s32ch_cg,
     .cores = mame_cs,
     .options = c352_opts},
    {.devid = DEVID_GA20,
     .label = "Irem GA20",
     .channels = s4ch_cg,
     .cores = mame_cs,
     .options = ga20_opts},
    {.devid = DEVID_MIKEY, .label = "MIKEY", .channels = s4ch_cg, .cores = mikey_cs}};

static_assert(lib_devices_p[index_YMF262].devid == DEVID_YMF262, "index_YMF262 is incorrect.");
static_assert(lib_devices_p[index_AY8910].devid == DEVID_AY8910, "index_AY8910 is incorrect.");
}  // namespace

const std::size_t vgmtk::num_devices = std::size(lib_devices_p);
const DeviceConfigList vgmtk::lib_devices(lib_devices_p, num_devices);

const char* vgmtk::core_name(uint32_t core_id) {
  switch (core_id) {
    case FCC_MAME:
      return "MAME";
    case FCC_MAXM:
      return "Maxim";
    case FCC_EMU_:
      return "EMU2413/EMU2149";
    case FCC_GENS:
      return "Gens";
    case FCC_GPGX:
      return "GPGX";
    case FCC_ADLE:
      return "AdLibEmu";
    case FCC_OMSX:
      return "openMSX";
    case FCC_NSFP:
      return "NSFPlay";
    case FCC_OOTK:
      return "Ootake";
    case FCC_MEDN:
      return "Mednafen";
    case FCC_NUKE:
      return "Nuked";
    // currently unused - case FCC_NRS_: return "NewRisingSun";
    case FCC_VBEL:
      return "Valley Bell";
    case FCC_CTR_:
      return "ctr";
    case FCC_LAOO:
      return "laoo";
    default:
      return "Invalid";
  }
}

// FIXME: This does not handle YM2612 correctly at the least.
const char* vgmtk::accurate_chip_name(const PLR_DEV_INFO& info) {
  const uint32_t flags = info.devCfg->flags;
  switch (info.type) {
    case DEVID_SN76496: {
      if (info.instance == 1 && flags & (1 << 31)) {
        return "T6W28";
      }
      uint8_t shift_reg_width = (flags >> 16) & 0xFF;
      uint8_t psg_flags = (flags >> 24) & 0x7F;
      uint16_t psg_feedback = (flags & 0xFFFF);
      switch (shift_reg_width) {
        case 0x0F: {
          if (psg_flags & 0x08) return "SN94624";
          return "SN76489";
        }
        case 0x10: {
          if (psg_feedback == 0x0009) return "SEGA PSG";
          if (psg_feedback == 0x0022) return "NCR7496";
        } break;
        case 0x11: {
          if (psg_flags & 0x08) return "SN76494";
          return "SN76489A";
        }
          // SN76489A/SN76496
        default:
          break;
      }
    } break;
    case DEVID_YM2413:
      if (flags & 0x01) return "VRC7";
      break;
    case DEVID_YM2610:
      if (flags & 0x01) return "YM2610B";
      break;
    case DEVID_AY8910: {
      switch (flags & 0xFF) {
        case 0x00:
          return "AY-3-8910";
        case 0x01:
          return "AY-3-8912";
        case 0x02:
          return "AY-3-8913";
        case 0x03:
          return "AY8930";
        case 0x04:
          return "AY-3-8914";
        case 0x10:
          return "YM2149";
        case 0x11:
          return "YM3439";
        case 0x12:
          return "YMZ284";
        case 0x13:
          return "YMZ294";
        default:
          break;
      }
    } break;
    case DEVID_NES_APU:
      if (flags & 0x01) return "NES APU + FDS";
      break;
    case DEVID_K051649:
      if (flags & 0x01) return "K052539";
      break;
    case DEVID_C219:
      return "C140 (219)";
    case DEVID_ES5506:
      if (!(flags & (1 << 31))) return "ES5505";
      break;
    default:
      break;
  }
  for (const auto& di : lib_devices) {
    if (di.devid == info.type) {
      return di.label;
    }
  }
  return "";
}

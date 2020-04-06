
#include "libvgm-dev-meta.h"

#include <emu/EmuCores.h>
#include <emu/SoundDevs.h>
#include <emu/cores/2612intf.h>
#include <emu/cores/c352.h>
#include <emu/cores/gb.h>
#include <emu/cores/iremga20.h>
#include <emu/cores/nesintf.h>
#include <emu/cores/okim6258.h>
#include <emu/cores/scsp.h>

namespace {

const char* sn76489_cname[] = {"Noise"};
const char* ym2413_cname[] = {"Bass Drum", "Snare Drum", "Tom Tom", "Cymbal", "Hi-Hat"};
const char* ym2612_cname[] = {"DAC"};
const char* delta_t_cname[] = {"Delta-T"};
const char* gb_cname[] = {"Square 1", "Square 2", "Prog. Wave", "Noise"};
const char* nes_cname[] = {"Square 1", "Square 2", "Triangle", "Noise", "DPCM", "FDS"};

using ChG = vgm::ChannelGroup;
using Cr = vgm::CoreSpec;

const ChG sn76489_cg[] = {{3, sn76489_cname}};
const ChG ym2413_cg[] = {{9, ym2413_cname}};
const ChG ym2612_cg[] = {{6, ym2612_cname}};
const ChG s1ch_cg[] = {{1}}, s3ch_cg[]{{3}}, s4ch_cg[]{{4}}, s5ch_cg[]{{5}},
          s6ch_cg[]{{5}}, s8ch_cg[]{{8}}, s12ch_cg[]{{12}}, s16ch_cg[]{{16}},
          s24ch_cg[]{{24}}, s28ch_cg[]{{28}}, s32ch_cg[]{{32}};
const ChG ym22_cg[] = {{3, nullptr, "FM"}};
const ChG ym26_cg[] = {{6, nullptr, "FM"}, {6, nullptr, "PCM"}, {0, delta_t_cname}};
const ChG y8950_cg[] = {{9, ym2413_cname}, {0, delta_t_cname}};
const ChG ymf262_cg[] = {{18, ym2413_cname}};
const ChG gb_cg[] = {{0, gb_cname}};
const ChG nes_cg[] = {{0, nes_cname}};

const Cr mame_cs[] = {{FCC_MAME}};
const Cr sn76489_cs[] = {{FCC_MAME}, {FCC_MAXM, "Maxim", true}};
const Cr ym2413_cs[] = {
    {FCC_EMU_, "EMU2413", true}, {FCC_MAME}, {FCC_NUKE, "Nuked OPLL"}};
const Cr ym2612_cs[] = {{FCC_GPGX, "GPGX", false},
                        {FCC_NUKE, "Nuked OPN2", false},
                        {FCC_GENS, "GENS", false}};
const Cr rf5c68_cs[] = {{FCC_MAME}, {FCC_GENS, "GENS"}};
const Cr ym3812_cs[] = {{FCC_MAME}, {FCC_ADLE, "AdLibEmu"}};
const Cr ymf278b_cs[] = {{FCC_OMSX, "openMSX"}};
const Cr gens_cs[] = {{FCC_GENS, "Gens/GS"}};
const Cr ay8910_cs[] = {{FCC_EMU_, "EMU2149", true}};
const Cr nes_cs[] = {{FCC_NSFP, "NSFPlay"}, {FCC_MAME}};
const Cr c6280_cs[] = {{FCC_OOTK, "Ootake"}, {FCC_MAME}};
const Cr qsound_cs[] = {{FCC_CTR_, "ctr"}, {FCC_MAME}};
const Cr wswan_cs[] = {{0, "in_wsr"}};
const Cr vboy_cs[] = {{FCC_MEDN, "Mednafen"}};
const Cr saa_cs[] = {{FCC_MAME}, {FCC_VBEL, "Valley Bell"}};

constexpr vgm::SwitchOption ym2612_chip_type_opts[]{
    {"YM2612", "Emulate original YM2612", OPT_YM2612_TYPE_OPN2},
    {"YM3438 ASIC", "Emulate ASIC YM3438.", OPT_YM2612_TYPE_OPN2C_ASIC},
    {"YM3438 Discrete", "Emulate Discrete YM3438.", OPT_YM2612_TYPE_OPN2C_DISC}};

constexpr vgm::OptionSpec ym2612_opts[]{
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

constexpr vgm::OptionSpec gb_opts[]{
    {"Boost Wave Channel", "Boost volume of wave channel by 6dB.",
     OPT_GB_DMG_BOOST_WAVECH},
    {"Disable WaveRAM corruption",
     "Non-GBC models overwrite parts of the WaveRAM when triggered while "
     "reading a sample.",
     OPT_GB_DMG_NO_WAVE_CORRUPT},
    {"Legacy Mode",
     "Simulate behavior of old MAME core. Required for playing older VGM "
     "files optimized with vgm_cmp.",
     OPT_GB_DMG_LEGACY_MODE}};

constexpr vgm::OptionSpec nes_opts[]{
    {"Unmute on reset", "Enable all channels by default after reset.",
     OPT_NES_UNMUTE_ON_RESET, FCC_NSFP},
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
    {"Randomize Noise", "Randomize noise at device reset", OPT_NES_RANDOMIZE_NOISE,
     FCC_NSFP},
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

constexpr vgm::OptionSpec m6258_opts[]{
    {"Force 12-bit", "Enforce 12-bit output precision", OPT_OKIM6258_FORCE_12BIT}};

constexpr vgm::OptionSpec scsp_opts[]{
    {"Bypass DSP", "skip all DSP calculations (huge speedup, DSP is broken anyway)",
     OPT_SCSP_BYPASS_DSP}};
constexpr vgm::OptionSpec c352_opts[]{{"Disable Rear",
                                       "Mute rear speakers regardless of configuration",
                                       OPT_C352_MUTE_REAR}};
constexpr vgm::OptionSpec ga20_opts[]{{"Linear interpolation",
                                       "Enable linear interpolation for samples.",
                                       OPT_GA20_INTERPOLATE}};

}  // namespace

namespace vgm {

const DeviceConfig lib_devices[] = {
    {DEVID_SN76496, "SN76489", sn76489_cg, sn76489_cs},
    {DEVID_YM2413, "YM2413", ym2413_cg, ym2413_cs},
    {DEVID_YM2612, "YM2612", ym2612_cg, ym2612_cs, ym2612_opts},
    {DEVID_YM2151, "YM2151", s8ch_cg, mame_cs},
    {DEVID_SEGAPCM, "Sega PCM", s16ch_cg, mame_cs},
    {DEVID_RF5C68, "RF5C68", s8ch_cg, rf5c68_cs},
    {DEVID_YM2203, "YM2203", ym22_cg, mame_cs, .linked = LinkSpec{DEVID_AY8910, "PSG"}},
    {DEVID_YM2608, "YM2608", ym26_cg, mame_cs, .linked = LinkSpec{DEVID_AY8910, "PSG"}},
    {DEVID_YM2610, "YM2610", ym26_cg, mame_cs, .linked = LinkSpec{DEVID_AY8910, "PSG"}},
    {DEVID_YM3812, "YM3812", ym2413_cg, ym3812_cs},
    {DEVID_YM3526, "YM3526", ym2413_cg, mame_cs},
    {DEVID_Y8950, "Y8950", y8950_cg, mame_cs},
    {DEVID_YMF262, "YMF262", ymf262_cg, ym3812_cs},
    {DEVID_YMF278B, "YMF278B", s24ch_cg, ymf278b_cs,
     .linked = LinkSpec{DEVID_YMF262, "FM"}},
    {DEVID_YMF271, "YMF271", s12ch_cg, mame_cs},
    {DEVID_YMZ280B, "YMZ280B", s8ch_cg, mame_cs},
    {DEVID_32X_PWM, "PWM", s1ch_cg, gens_cs},
    {DEVID_AY8910, "AY8910", s3ch_cg, ay8910_cs},
    {DEVID_GB_DMG, "GameBoy DMG", gb_cg, mame_cs, gb_opts},
    {DEVID_NES_APU, "NES APU", nes_cg, nes_cs, nes_opts},
    {DEVID_YMW258, "YMW258", s28ch_cg, mame_cs},
    {DEVID_uPD7759, "uPD7759", s1ch_cg, mame_cs},
    {DEVID_OKIM6258, "MSM6258", s1ch_cg, mame_cs},
    {DEVID_OKIM6295, "MSM6295", s4ch_cg, mame_cs},
    {DEVID_K051649, "K051649", s5ch_cg, mame_cs},
    {DEVID_K054539, "K054539", s8ch_cg, mame_cs},
    {DEVID_C6280, "HuC6280", s6ch_cg, c6280_cs},
    {DEVID_C140, "C140", s24ch_cg, mame_cs},
    {DEVID_C219, "C219", s16ch_cg, mame_cs},
    {DEVID_K053260, "K053260", s4ch_cg, mame_cs},
    {DEVID_POKEY, "POKEY", s4ch_cg, mame_cs},
    {DEVID_QSOUND, "QSound", s16ch_cg, qsound_cs},
    {DEVID_SCSP, "SCSP", s32ch_cg, mame_cs, scsp_opts},
    {DEVID_WSWAN, "WonderSwan", s4ch_cg, wswan_cs},
    {DEVID_VBOY_VSU, "VSU-VUE", s6ch_cg, vboy_cs},
    {DEVID_SAA1099, "SAA1099", s6ch_cg, saa_cs},
    {DEVID_ES5503, "ES5503", s32ch_cg, mame_cs},
    {DEVID_ES5506, "ES5506", s32ch_cg, mame_cs},
    {DEVID_X1_010, "X1-010", s16ch_cg, mame_cs},
    {DEVID_C352, "C352", s32ch_cg, mame_cs, c352_opts},
    {DEVID_GA20, "Irem GA20", s4ch_cg, mame_cs, ga20_opts}};

const size_t num_devices = sizeof(lib_devices) / sizeof(lib_devices[0]);

}  // namespace vgm
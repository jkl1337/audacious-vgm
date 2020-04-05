
#include "libvgm-dev-meta.h"

#include <emu/SoundDevs.h>
#include <emu/EmuCores.h>

#include <emu/cores/nesintf.h>
#include <emu/cores/2612intf.h>
#include <emu/cores/gb.h>
#include <emu/cores/okim6258.h>
#include <emu/cores/scsp.h>
#include <emu/cores/c352.h>
#include <emu/cores/iremga20.h>

#include <array>

using namespace vgm;

static const vgm::CoreSpec core_mame_only[] = { {FCC_MAME} };
static const vgm::LinkSpec core_linked_ay8910 = {DEVID_AY8910, "PSG"};
static const vgm::LinkSpec core_linked_ymf262 = {DEVID_YMF262, "FM"};


std::array<DeviceConfig, 1> lib_devices[] = {
        {
            DEVID_SN76496, "SN76489", { {3, {"Noise"}} },
                {{ {FCC_MAME}, {FCC_MAXM, "Maxim", true} }},
        },

};

#if 0
__attribute__((visibility("default"))) DeviceConfig lib_devices[] = {
        vgm::DeviceConfig(
                DEVID_SN76496, "SN76489",
                {{3, {{"Noise"}}}},
                {{FCC_MAME},
                 {FCC_MAXM, "Maxim", true}}),
        vgm::DeviceConfig(
                DEVID_YM2413, "YM2413",
                {{9, {{"Bass Drum", "Snare Drum", "Tom Tom", "Cymbal", "Hi-Hat"}}}},
                {{FCC_EMU_, "EMU2413", true},
                 {FCC_MAME},
                 {FCC_NUKE, "Nuked OPLL"}}),
        vgm::DeviceConfig(
                DEVID_YM2612, "YM2612",
                {{6, {{"DAC"}}}},
                {
                        {FCC_GPGX, "GPGX",       false,
                                {
                                        {"Pseudo Stereo", "Pseudo Stereo mode. Alternate between updating left and right speaker, adds some stereo effect to sharp and noisy sounds.", OPT_YM2612_PSEUDO_STEREO},
                                        {"Legacy Mode", "Simulate behavior of older emulation core. Not recommended, but required for playing GYM files.", OPT_YM2612_LEGACY_MODE}
                                }
                        },
                        {
                         FCC_NUKE, "Nuked OPN2", false,
                                {
                                        {"Chip Type",     nullptr, {
                                                                                                                                                                                               {"YM2612", "Emulate original YM2612", OPT_YM2612_TYPE_OPN2},
                                                                                                                                                                                               {"YM3438 ASIC", "Emulate ASIC YM3438.", OPT_YM2612_TYPE_OPN2C_ASIC},
                                                                                                                                                                                               {"YM3438 Discrete", "Emulate Discrete YM3438.", OPT_YM2612_TYPE_OPN2C_DISC}
                                                                                                                                                                                       }}
                                }
                        },
                        {
                         FCC_GENS, "GENS",       false,
                                {
                                        {"DAC highpass",  "Enable DAC highpass filter.",                                                                                               OPT_YM2612_DAC_HIGHPASS},
                                        {"SSG-EG",      "Enable (broken) SSG-EG emulation.",                                                               OPT_YM2612_SSGEG}
                                }
                        }}
        ),
        vgm::DeviceConfig(DEVID_YM2151, "YM2151", {{8}}, core_mame_only),
        vgm::DeviceConfig(DEVID_SEGAPCM, "Sega PCM", {{16}}, core_mame_only),
        vgm::DeviceConfig(DEVID_RF5C68, "RF5C68", {{8}},
                          {{FCC_MAME},
                           {FCC_GENS, "GENS"}}),
        vgm::DeviceConfig(DEVID_YM2203, "YM2203", {{3, nullptr, "FM"}}, core_mame_only, &core_linked_ay8910),
        vgm::DeviceConfig(DEVID_YM2608, "YM2608", {{6, nullptr, "FM"},
                                                   {6, nullptr, "PCM"},
                                                   {0, nullptr, "Delta-T"}}, core_mame_only, &core_linked_ay8910),
        vgm::DeviceConfig(DEVID_YM2610, "YM2610", {{6, nullptr, "FM"},
                                                   {6, nullptr, "PCM"},
                                                   {0, nullptr, "Delta-T"}}, core_mame_only, &core_linked_ay8910),

        vgm::DeviceConfig(DEVID_YM3812, "YM3812", {{9, {{"Bass Drum", "Snare Drum", "Tom Tom", "Cymbal", "Hi-Hat"}}}},
                          core_mame_only),
        vgm::DeviceConfig(DEVID_Y8950, "Y8950",
                          {{9, {{"Bass Drum", "Snare Drum", "Tom Tom", "Cymbal", "Hi-Hat", "Delta-T"}}}},
                          core_mame_only),
        vgm::DeviceConfig(DEVID_YMF262, "YMF262", {{18, {{"Bass Drum", "Snare Drum", "Tom Tom", "Cymbal", "Hi-Hat"}}}},
                          {{FCC_MAME},
                           {FCC_ADLE, "AdLibEmu"}}),

        vgm::DeviceConfig(DEVID_YMF278B, "YMF262", {{18, {{"Bass Drum", "Snare Drum", "Tom Tom", "Cymbal", "Hi-Hat"}}}},
                          {{FCC_OMSX, "openMSX"}}, &core_linked_ymf262),


        vgm::DeviceConfig(DEVID_YMF271, "YMF271", {{12}}, core_mame_only),
        vgm::DeviceConfig(DEVID_YMZ280B, "YMZ280B", {{8}}, core_mame_only),
        vgm::DeviceConfig(DEVID_32X_PWM, "PWM", {{1}}, {{FCC_GENS, "Gens/GS"}}),

        vgm::DeviceConfig(DEVID_AY8910, "AY8910", {{3}}, {{FCC_EMU_, "EMU2149", true},
                                                          {FCC_MAME}}),

        vgm::DeviceConfig(
                DEVID_GB_DMG, "GameBoy DMG",
                {{0, {{"Square 1", "Square 2", "Triangle", "Noise"}}}},
                core_mame_only,
                {
                        {"Boost Wave Channel",         "Boost volume of wave channel by 6dB.",                                                             OPT_GB_DMG_BOOST_WAVECH},
                        {"Disable WaveRAM corruption", "Non-GBC models overwrite parts of the WaveRAM when triggered while reading a sample.",             OPT_GB_DMG_NO_WAVE_CORRUPT},
                        {"Legacy Mode",                "Simulate behavior of old MAME core. Required for playing older VGM files optimized with vgm_cmp.", OPT_GB_DMG_LEGACY_MODE}
                }),

        vgm::DeviceConfig(
                DEVID_NES_APU, "NES APU",
                {{0, {{"Square 1", "Square 2", "Triangle", "Noise", "DPCM", "FDS"}}}},
                {{FCC_NSFP, "NSFPlay", false,
                         {
                                 {"Unmute on reset", "Enable all channels by default after reset.", OPT_NES_UNMUTE_ON_RESET},
                                 {"Non-linear Mixer", nullptr, OPT_NES_NONLINEAR_MIXER},
                                 {"Phrase Refresh", nullptr, OPT_NES_PHASE_REFRESH},
                                 {"Duty Swap", "Swap 25:75 and 50:50 duty cycles (behavior of some Famiclones)", OPT_NES_DUTY_SWAP},
                                 {"Enable $4011", "Enable register 4011 writes", OPT_NES_ENABLE_4011},
                                 {"Enable PNoise", "Enable \"loop noise\" (disable for early Famicon console + Arcade boards)", OPT_NES_ENABLE_PNOISE},
                                 {"DPCM Anti Click", "Nullify register 4011 writes, but preserve nonlinearity", OPT_NES_DPCM_ANTI_CLICK},
                                 {"Randomize Noise", "Randomize noise at device reset", OPT_NES_RANDOMIZE_NOISE},
                                 {"Tri Mute", "Stops Triangle wave if set to freq = 0, processes it a very high rate otherwise.", OPT_NES_TRI_MUTE},
                                 {"Tri Null", "Always make triangle return to null-level when stopping, prevents clicking (Valley Bell mod).", OPT_NES_TRI_NULL}
                         }
                 },
                 {FCC_MAME}
                },
                {
                        {"4085 Reset",  "Reset modulation phase on 4085 writes.", OPT_NES_4085_RESET},
                        {"FDS Disable", "Disable FDS",                            OPT_NES_FDS_DISABLE}
                }),

        vgm::DeviceConfig(DEVID_YMW258, "YMW258", {{28}}, core_mame_only),
        vgm::DeviceConfig(DEVID_YMF271, "YMF271", {{12}}, core_mame_only),
        vgm::DeviceConfig(DEVID_uPD7759, "uPD7759", {{1}}, core_mame_only),
        vgm::DeviceConfig(DEVID_OKIM6258, "MSM6258", {{1}}, core_mame_only,
                          {
                                  {"Force 12-bit", "Enforce 12-bit output precision", OPT_OKIM6258_FORCE_12BIT}
                          }),
        vgm::DeviceConfig(DEVID_OKIM6295, "OKIM6295", {{4}}, core_mame_only),
        vgm::DeviceConfig(DEVID_K051649, "K051649", {{5}}, core_mame_only),
        vgm::DeviceConfig(DEVID_K054539, "K054539", {{8}}, core_mame_only),
        vgm::DeviceConfig(DEVID_C6280, "HuC6280", {{6}}, {{FCC_OOTK, "Ootake"},
                                                          {FCC_MAME}}),
        vgm::DeviceConfig(DEVID_C140, "C140", {{24}}, core_mame_only),
        vgm::DeviceConfig(DEVID_C219, "C219", {{16}}, core_mame_only),
        vgm::DeviceConfig(DEVID_K053260, "K053260", {{4}}, core_mame_only),
        vgm::DeviceConfig(DEVID_POKEY, "POKEY", {{4}}, core_mame_only),
        vgm::DeviceConfig(DEVID_QSOUND, "QSound", {{16}}, {{FCC_CTR_, "ctr"},
                                                           {FCC_MAME}}),
        vgm::DeviceConfig(DEVID_SCSP, "SCSP", {{32}}, core_mame_only,
                          {
                                  {"Bypass DSP", "skip all DSP calculations (huge speedup, DSP is broken anyway)", OPT_SCSP_BYPASS_DSP}
                          }),
        vgm::DeviceConfig(DEVID_WSWAN, "WonderSwan", {{4}}, {{0, "in_wsr"}}),
        vgm::DeviceConfig(DEVID_VBOY_VSU, "VSU-VUE", {{6}}, {{FCC_MEDN, "Mednafen"}}),
        vgm::DeviceConfig(DEVID_SAA1099, "SAA1099", {{6}}, {{FCC_MAME},
                                                            {FCC_VBEL, "Valley Bell"}}),
        vgm::DeviceConfig(DEVID_ES5503, "ES5503", {{32}}, core_mame_only),
        vgm::DeviceConfig(DEVID_ES5506, "ES5506", {{32}}, core_mame_only),
        vgm::DeviceConfig(DEVID_X1_010, "X1-010", {{16}}, core_mame_only),
        vgm::DeviceConfig(DEVID_C352, "C352", {{32}}, core_mame_only,
                          {
                                  {"Disable Rear", "Mute rear speakers regardless of configuration", OPT_C352_MUTE_REAR}
                          }),
        vgm::DeviceConfig(DEVID_GA20, "Irem GA20", {{4}}, core_mame_only,
                          {
                                  {"Linear interpolation", "Enable linear interpolation for samples.", OPT_GA20_INTERPOLATE}
                          }),


};
#endif
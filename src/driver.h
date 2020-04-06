#ifndef AUDVGM__DRIVER_H_
#define AUDVGM__DRIVER_H_

#include <atomic>
#include <array>

typedef struct {
    std::array<std::atomic_uint_least32_t, 2> mute_masks;
} AudaciousVgmPlaybackSettings;

extern AudaciousVgmPlaybackSettings audvgm_playback_settings;

#endif // AUDVGM__DRIVER_H_

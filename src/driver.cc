
#include "driver.h"

#include <libaudcore/audstrings.h>
#include <libaudcore/runtime.h>
#include <utils/DataLoader.h>
#include <utils/FileLoader.h>

#include <cstring>
#include <player/playerbase.hpp>
#include <player/vgmplayer.hpp>
#include <vector>

#include "configure.h"
#include "log.h"
#include "plugin.h"
#include "vfsloader.h"

AudaciousVgmPlaybackSettings audvgm_playback_settings;

namespace {

class VgmFileHandler {
 public:
  String path_;

  int load(int sample_rate = -1);

  explicit VgmFileHandler(VFSFile &fd);
  ~VgmFileHandler();

  PlayerBase *player() { return player_; }

 private:
  DATA_LOADER *loader_ = nullptr;

  PlayerBase *player_ = nullptr;
};

VgmFileHandler::VgmFileHandler(VFSFile &fd) {
  loader_ = audvgm::VFSLoader_Init(fd);

  if (!loader_) return;
  // TODO: multiple track support
  // uri_parse(path, nullptr, nullptr, &sub, &m_track);

  DataLoader_SetPreloadBytes(loader_, 0x100);
  if (DataLoader_Load(loader_)) {
    DataLoader_CancelLoading(loader_);
    loader_ = nullptr;
    DataLoader_Deinit(loader_);
    return;
  }

  if (VGMPlayer::IsMyFile(loader_) == 0) {
    player_ = new VGMPlayer();
  } else {
    audvgm::log_err("Unsupported file type\n");
  }
}

VgmFileHandler::~VgmFileHandler() {
  delete player_;
  DataLoader_Deinit(loader_);
}

int VgmFileHandler::load(int sample_rate) {
  if (!player_) {
    return 1;
  }

  if (player_->LoadFile(loader_)) {
    audvgm::log_err("Failed to load file");
    return 1;
  }

  if (!sample_rate) sample_rate = 44100;

  player_->SetSampleRate(sample_rate);
  player_->Start();

  if (sample_rate < 0) return 0;

  return 0;
}

}  // namespace

static unsigned long get_total_frames(const PlayerBase &player, unsigned sample_rate) {
  UINT32 ticks = player.GetTotalPlayTicks(audcfg.num_loops);
  UINT32 total_samples = player.Tick2Sample(ticks);

  if (player.GetLoopTicks()) {
    total_samples += audcfg.fade_length * sample_rate;
  }

  return total_samples;
}

static int get_track_length(const PlayerBase &player) {
  UINT32 ticks = player.GetTotalPlayTicks(audcfg.num_loops);
  double total_time = player.Tick2Second(ticks);

  if (player.GetLoopTicks()) {
    total_time += audcfg.fade_length;
  }

  return static_cast<int>(total_time * 1000);
}

static void get_file_info(PlayerBase &player, Tuple &tuple) {
  const char *const *tags = player.GetTags();

  auto set_str = [&tuple](Tuple::Field f, const char *s) {
    if (s[0]) tuple.set_str(f, s);
  };

  while (*tags) {
    Tuple::Field field = Tuple::Invalid;
    if (!strcmp(tags[0], "TITLE"))
      field = Tuple::Title;
    else if (!strcmp(tags[0], "ARTIST"))
      field = Tuple::Artist;
    else if (!strcmp(tags[0], "GAME"))
      field = Tuple::Album;
    else if (!strcmp(tags[0], "DATE"))
      field = Tuple::Date;
    else if (!strcmp(tags[0], "COMMENT"))
      field = Tuple::Comment;
    else if (!strcmp(tags[0], "SYSTEM"))
      field = Tuple::Codec;

    if (field != Tuple::Invalid) {
      set_str(field, tags[1]);
    }
    tags += 2;
  }

  tuple.set_int(Tuple::Length, get_track_length(player));
}

bool VgmPlugin::read_tag(const char *, VFSFile &file, Tuple &tuple,
                         Index<char> *) {
  VgmFileHandler fh(file);

  if (fh.load(0)) return false;

  get_file_info(*fh.player(), tuple);
  return true;
}

constexpr long kSampMin = -(1 << 24) / 2;
constexpr long kSampMax = (1 << 24) / 2 - 1;

static void pack_frames(int16_t *buf, unsigned int frame_count, const WAVE_32BS *data) {
  while (frame_count--) {
    WAVE_32BS s = *data++;

    if (s.L < kSampMin)
      s.L = kSampMin;
    else if (s.L > kSampMax)
      s.L = kSampMax;

    if (s.R < kSampMin)
      s.R = kSampMin;
    else if (s.R > kSampMax)
      s.R = kSampMax;

    *buf++ = static_cast<unsigned>(s.L) >> 8;
    *buf++ = static_cast<unsigned>(s.R) >> 8;
  }
}

/* apply a fade to a buffer of frames */
/* frames_rem - remaining total frames, includes fade frames */
/* frames_fade - total number of fade samples */
/* frame_count - number of frames being rendered right now */
static void fade_frames(unsigned int frames_rem, unsigned int frames_fade,
                        unsigned int frame_count, WAVE_32BS *data) {
  unsigned int i = 0;
  unsigned int f = frames_fade;
  UINT64 fade_vol;

  if (frames_rem - frame_count > frames_fade) return;
  if (frames_rem > frames_fade) {
    i = frames_rem - frames_fade;
    f += i;
  } else {
    f = frames_rem;
  }
  while (i < frame_count) {
    fade_vol = (UINT64)((f - i)) * (1 << 16);
    fade_vol /= frames_fade;
    fade_vol *= fade_vol;
    fade_vol >>= 16;

    // FIXME: This conversion is suspect and also need to take into account minimum length
    data[i].L = (INT32)(((UINT64)data[i].L * fade_vol) >> 16);
    data[i].R = (INT32)(((UINT64)data[i].R * fade_vol) >> 16);
    i++;
  }
}

static void dump_info(PlayerBase &player) {
  auto &vgm_player = reinterpret_cast<VGMPlayer &>(player);

  PLR_SONG_INFO song_info = {};
  player.GetSongInfo(song_info);

  std::vector<PLR_DEV_INFO> dev_info_list;
  player.GetSongDeviceInfo(dev_info_list);

  for (auto dev : dev_info_list) {
    int chip_id = dev.instance < 2 ? dev.instance : 0;

    auto chip_device = vgm_player.GetDevicePtr(dev.type, chip_id);
    auto base_dev = &chip_device->base;

    auto dev_info = &base_dev->defInf;
    auto dev_def = dev_info->devDef;

    printf(R"EOT(
name: %s
author: %s
)EOT",
           dev_def->name, dev_def->author);

    printf(R"EOT(srcSR: %u dstSR: %u linkDevCount: %u
)EOT",
           base_dev->resmpl.smpRateSrc, base_dev->resmpl.smpRateDst,
           dev_info->linkDevCount);
    if (dev_info->linkDevCount) {
      printf("link ID: %d\n", dev_info->linkDevs[0].devID);
    }
    printf(R"(ID: %u
Type: 0x%x
Instance: 0x%x
Volume: %u
Core: 0x%x
Clock: %u
Params: 0x%x
SR: %u


)",
           dev.id, dev.type, dev.instance, dev.volume, dev.core, dev.clock, dev.cParams,
           dev.smplRate);
    fflush(stdout);
  }
}

bool VgmPlugin::play(const char *filename, VFSFile &file) {
  VgmFileHandler fh(file);

  int sample_rate = 44100;

  if (fh.load(sample_rate)) return false;

  PlayerBase *player = fh.player();

  dump_info(*player);

  Tuple file_tuple;
  get_file_info(*player, file_tuple);
  set_playback_tuple(std::move(file_tuple));

  open_audio(FMT_S16_LE, sample_rate, 2);

  unsigned long total_frames = get_total_frames(*player, sample_rate);
  unsigned long frame_pos = 0;

  auto frames_fade = static_cast<unsigned>(audcfg.fade_length * sample_rate);

  const int buf_size = 4096;

  std::vector<WAVE_32BS> buf(buf_size);
  std::vector<int16_t> buf_packed(buf_size * 2);

  std::array<uint_least32_t, 2> last_mute_mask = {audvgm_playback_settings.mute_masks[0],
                                                  audvgm_playback_settings.mute_masks[1]};
  for (int i = 0; i < 2; ++i) {
    PLR_MUTE_OPTS mute_opts = {.chnMute = {last_mute_mask[i]}};
    player->SetDeviceMuting(i, mute_opts);
  }

  while (!check_stop()) {
    int seek_value = check_seek();
    if (seek_value >= 0) {
      unsigned long new_pos = int64_t(seek_value) * sample_rate / 1000;

      player->Seek(PLAYPOS_SAMPLE, new_pos);
      frame_pos = new_pos;
    }

    for (int i = 0; i < 2; ++i) {
      unsigned current_mute_mask = audvgm_playback_settings.mute_masks[i];
      if (current_mute_mask != last_mute_mask[i]) {
        PLR_MUTE_OPTS mute_opts = {.chnMute = {current_mute_mask}};
        player->SetDeviceMuting(i, mute_opts);
        last_mute_mask[i] = current_mute_mask;
      }
    }

    memset(buf.data(), 0, buf.size() * sizeof buf[0]);

    unsigned long frames_remaining = total_frames - frame_pos;

    int n_frames =
        buf.size() > frames_remaining ? static_cast<int>(frames_remaining) :
        static_cast<int>(buf.size());
    n_frames = player->Render(n_frames, buf.data());

    fade_frames(total_frames - frame_pos, frames_fade, n_frames, buf.data());
    pack_frames(buf_packed.data(), n_frames, buf.data());
    write_audio(buf_packed.data(), n_frames * 4);

    frame_pos += n_frames;

    if (frame_pos >= total_frames) break;
  }
  return true;
}

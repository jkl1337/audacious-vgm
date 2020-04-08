
#include "tags.h"

#include <libaudcore/runtime.h>
#include <libaudcore/tuple.h>

#include <climits>
#include <cstring>
#include <player/playerbase.hpp>

#include "config.h"
#include "internal.h"
#include "vgm.h"

namespace audvgm {

namespace {

int get_track_length(const PlayerBase &player, int loop_count,
                     int_least32_t fade_length) {
  double total_time_ms =
      player.Tick2Second(player.GetTotalPlayTicks(loop_count)) * 1000.0;

  if (player.GetLoopTicks()) {
    total_time_ms += fade_length;
  }
  return std::clamp<double>(total_time_ms, 0, INT_MAX);
}

}  // namespace

void TagHandler::get_tags(PlayerBase &player, Tuple &tuple) {
  struct MultiTag {
    const char *en;
    const char *jp;
    Tuple::Field field;
    const char *val;
  };
  std::array<MultiTag, 4> final_tags = {{{"TITLE", "TITLE-JPN", Tuple::Title},
                                         {"ARTIST", "ARTIST-JPN", Tuple::Artist},
                                         {"GAME", "GAME-JPN", Tuple::Album},
                                         {"SYSTEM", "SYSTEM-JPN", Tuple::Codec}}};

  int loop_count = aud_get_int(VGM_CFGID, cfg::kLoopCount);
  int fade_length = aud_get_int(VGM_CFGID, cfg::kFadeLength);
  bool prefer_jpn_tag = aud_get_bool(VGM_CFGID, "prefer_jpn_tag");
  const char *const *tags = player.GetTags();

  auto set_str = [&tuple](Tuple::Field f, const char *s) {
    if (s[0]) tuple.set_str(f, s);
  };

  for (; *tags; tags += 2) {
    Tuple::Field field = Tuple::Invalid;

    for (auto &f : final_tags) {
      if (!std::strcmp(tags[0], f.en)) {
        if (!prefer_jpn_tag || !f.val) f.val = tags[1];
        goto next_tag;
      }
      if (!std::strcmp(tags[0], f.jp)) {
        if (prefer_jpn_tag || !f.val) f.val = tags[1];
        goto next_tag;
      }
    }

    if (!std::strcmp(tags[0], "DATE")) {
      field = Tuple::Date;
    } else if (!std::strcmp(tags[0], "COMMENT")) {
      field = Tuple::Comment;
    }

    if (field != Tuple::Invalid) {
      set_str(field, tags[1]);
    }
  next_tag:;
  }

  for (const auto &f : final_tags) {
    if (f.val) set_str(f.field, f.val);
  }

  tuple.set_int(Tuple::Length, get_track_length(player, loop_count, fade_length));
}

bool Plugin::read_tag(const char *, VFSFile &file, Tuple &tuple, Index<char> *) {
  FileHandler fh(file);
  if (!fh.load(0)) return false;

  TagHandler::get_tags(*fh.player(), tuple);
  return true;
}

}  // namespace audvgm

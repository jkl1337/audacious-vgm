
#include "tags.h"

#include <libaudcore/runtime.h>
#include <libaudcore/tuple.h>

#include <cstring>
#include <player/playerbase.hpp>

#include "config.h"
#include "internal.h"
#include "vgm.h"

namespace audvgm {

namespace {

int get_track_length(const PlayerBase &player) {
  UINT32 ticks = player.GetTotalPlayTicks(aud_get_int(VGM_CFGID, cfg::LOOP_COUNT));
  double total_time = player.Tick2Second(ticks);

  if (player.GetLoopTicks()) {
    total_time += aud_get_int(VGM_CFGID, cfg::FADE_LENGTH) / 1000.0;
  }
  return static_cast<int>(total_time * 1000);
}

}  // namespace

void TagHandler::get_tags(PlayerBase &player, Tuple &tuple) {
  struct FinalTag {
    const char *en;
    const char *jp;
    Tuple::Field field;
    const char *val;
  };
  std::array<FinalTag, 4> final_tags = {{{"TITLE", "TITLE-JPN", Tuple::Title},
                                         {"ARTIST", "ARTIST-JPN", Tuple::Artist},
                                         {"GAME", "GAME-JPN", Tuple::Album},
                                         {"SYSTEM", "SYSTEM-JPN", Tuple::Codec}}};

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

  tuple.set_int(Tuple::Length, get_track_length(player));
}

bool Plugin::read_tag(const char *, VFSFile &file, Tuple &tuple, Index<char> *) {
  FileHandler fh(file);
  if (!fh.load(0)) return false;

  TagHandler::get_tags(*fh.player(), tuple);
  return true;
}

}  // namespace audvgm

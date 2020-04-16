#ifndef AUDACIOUS_VGM_TAGS_H
#define AUDACIOUS_VGM_TAGS_H

class PlayerBase;
class Tuple;

namespace vgm {

class TagHandler {

 public:
  static void get_tags(PlayerBase &player, Tuple &tuple);
};

}
#endif  // AUDACIOUS_VGM_TAGS_H

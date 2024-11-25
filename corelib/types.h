#ifndef VGMTK_TYPES_H_
#define VGMTK_TYPES_H_

namespace vgmtk {

struct InstanceId {
  int dev_idx;
  int instance;
  int vgm_type;

  bool operator==(const InstanceId &rhs) const {
    return dev_idx == rhs.dev_idx && instance == rhs.instance;
  }
};

}  // namespace vgmtk

#endif  // VGMTK_TYPES_H_

//
// Created by luebsj on 4/16/20.
//

#ifndef VGMTK_TAGGING_H
#define VGMTK_TAGGING_H

#include <string>

namespace vgmtk {

template <typename I>
std::string n2hexstr(I w) {
  static const char* digits = "0123456789abcdef";
  size_t len = 2;
  for (I k{w >> 8}; k; k >>= 8) {
    len += 2;
  }
  std::string rc(len + 2, '0');
  rc[1] = 'x';
  for (size_t i = 2, j = (len - 1) * 4; i < len + 2; ++i, j -= 4) {
    rc[i] = digits[(w >> j) & 0x0f];
  }
  return rc;
}

}  // namespace vgmtk

#endif  // VGMTK_TAGGING_H

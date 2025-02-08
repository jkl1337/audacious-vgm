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

template <typename I>
I hexstr2n(const char* s) {
  while (s && *s == '0') ++s;
  if (!s) {
    return 0;
  }
  if (*s == 'x' || *s == 'X') {
    ++s;
  }
  I rc = 0;
  for (char c = *s; c; c = *++s) {
    rc <<= 4;
    if (c >= '0' && c <= '9') {
      rc |= c - '0';
    } else if (c >= 'a' && c <= 'f') {
      rc |= c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
      rc |= c - 'A' + 10;
    } else {
      break;
    }
  }
  return rc;
}

}  // namespace vgmtk

#endif  // VGMTK_TAGGING_H

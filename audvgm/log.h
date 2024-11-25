#ifndef AUDVGM__LOG_H_
#define AUDVGM__LOG_H_

#include <libaudcore/runtime.h>

namespace vgmtk {
static void log_err(const char *err) {
  if (err) AUDERR("%s\n", err);
}
}  // namespace vgmtk

#endif  // AUDVGM__LOG_H_

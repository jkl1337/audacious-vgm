#ifndef AUDVGM__LOG_H_
#define AUDVGM__LOG_H_

#include <libaudcore/runtime.h>

namespace vgm {
    static void log_err(const char *err)
    {
        if (err)
            AUDERR("%s\n", err);
    }

    static void log_warning(const char *err)
    {
        if (err)
            AUDWARN("%s\n", err);
    }
}

#endif // AUDVGM__LOG_H_

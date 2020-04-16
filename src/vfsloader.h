#ifndef AUDVGM__VFS_LOADER_H_
#define AUDVGM__VFS_LOADER_H_

#define WANT_VFS_STDIO_COMPAT
#include <libaudcore/vfs.h>

typedef struct _data_loader DATA_LOADER;

namespace vgm {
DATA_LOADER *VFSLoader_Init(VFSFile &file);
}

#endif // AUDVGM__VFS_LOADER_H_

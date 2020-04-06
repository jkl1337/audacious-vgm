#include "libvgm-dev-meta.h"

using namespace std;
using namespace vgm;
#include <cstdio>

int main() {

    for (int i=0; i<num_devices; ++i) {
        const DeviceConfig &d = lib_devices[i];
        std::printf("name: %s, num_cg: %zu num_core: %zu ", d.label, d.channels.len, d.cores.len);
        printf("num-opts: %zu\n", d.options.len);
    }
    std::printf("num-devs: %zu\n", num_devices);
}
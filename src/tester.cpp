#include "libvgm-dev-meta.h"

using namespace vgm;
#include <algorithm>
#include <cstdio>
#include <iostream>

int main() {

  std::string s = "FUCKYOU", out;
  out.reserve(10);
  out.resize(5);

  std::transform(s.begin(), s.end(), out.begin(),
      [](char c) { return std::tolower(c); });

  for (int i = 0; i < num_devices; ++i) {
    const DeviceConfig &d = lib_devices[i];
    std::printf("name: %s, num_cg: %zu num_core: %zu ", d.label, d.channels.len,
                d.cores.len);
    std::printf("num-opts: %zu\n", d.options.len);
  }
  std::printf("num-devs: %zu\n", num_devices);

  std::cout << out << std::endl;
}
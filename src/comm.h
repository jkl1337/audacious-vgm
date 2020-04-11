//
// Created by luebsj on 4/9/20.
//

#ifndef AUDACIOUS_VGM_COMM_H
#define AUDACIOUS_VGM_COMM_H

#include <functional>
#include <mutex>
#include <player/playerbase.hpp>
#include <vector>

namespace audvgm {

class PlaybackStatus {
 public:
  typedef void (*NotifyFunc)(const PlaybackStatus &, void *);

  ~PlaybackStatus();

  void set_device_info(std::vector<PLR_DEV_INFO> &&dev_info_list);
  std::vector<PLR_DEV_INFO> device_info() const;

  void hook(NotifyFunc func, void *ctx = nullptr);
  void unhook(NotifyFunc func, void *ctx = nullptr);

 private:
  mutable std::mutex dev_info_lock_;
  std::vector<PLR_DEV_INFO> dev_info_list_;

  static void hook_cb(void *, void*);
  void call_hooks();
  struct HookInfo {
    PlaybackStatus::NotifyFunc func;
    void *ctx;
  };
  std::mutex hook_lock_;
  std::vector<HookInfo> hooks_;
};

}

#endif  // AUDACIOUS_VGM_COMM_H

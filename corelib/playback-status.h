#ifndef AUDACIOUS_VGM_PLAYBACK_STATUS_H
#define AUDACIOUS_VGM_PLAYBACK_STATUS_H

#include <mutex>
#include <vector>

struct PLR_DEV_INFO;
namespace vgmtk {

class PlaybackStatus {
 public:
  typedef void (*NotifyFunc)(const PlaybackStatus &, void *);

  ~PlaybackStatus();

  void set_device_info(std::vector<PLR_DEV_INFO> dev_info_list);
  std::vector<PLR_DEV_INFO> get_device_info() const;

  void subscribe(NotifyFunc func, void *ctx = nullptr);
  void unsubscribe(NotifyFunc func, void *ctx = nullptr);

 private:
  mutable std::mutex dev_info_lock_;
  std::vector<PLR_DEV_INFO> dev_info_list_;

  static void hook_cb(void *, void *);
  void call_hooks();
  struct HookInfo {
    NotifyFunc func;
    void *ctx;
  };
  std::mutex hook_lock_;
  std::vector<HookInfo> hooks_;
};
}  // namespace vgmtk

#endif

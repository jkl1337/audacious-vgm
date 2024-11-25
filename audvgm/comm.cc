#include <corelib/libvgm-rt.h>
#include <libaudcore/hook.h>

using namespace vgmtk;

constexpr static const char *kUpdateEvent = "audvgm player update";

PlaybackStatus::~PlaybackStatus() {
  std::lock_guard lock(hook_lock_);
  if (!hooks_.empty()) hook_dissociate(kUpdateEvent, hook_cb, this);
}

void PlaybackStatus::set_device_info(std::vector<PLR_DEV_INFO> dev_info_list) {
  std::unique_lock lock(dev_info_lock_);
  std::swap(dev_info_list_, dev_info_list);
  lock.unlock();
  event_queue(kUpdateEvent, this);
}

std::vector<PLR_DEV_INFO> PlaybackStatus::get_device_info() const {
  std::lock_guard lock(dev_info_lock_);
  return dev_info_list_;
}

void PlaybackStatus::subscribe(PlaybackStatus::NotifyFunc func, void *ctx) {
  std::lock_guard lock(hook_lock_);
  if (hooks_.empty()) hook_associate(kUpdateEvent, hook_cb, this);
  hooks_.push_back(HookInfo{func, ctx});
}

void PlaybackStatus::unsubscribe(PlaybackStatus::NotifyFunc func, void *ctx) {
  std::lock_guard lock(hook_lock_);
  hooks_.erase(std::remove_if(hooks_.begin(), hooks_.end(),
                              [=](const HookInfo &info) {
                                return info.func == func && (ctx == info.ctx || !ctx);
                              }),
               hooks_.end());
  if (hooks_.empty()) hook_dissociate(kUpdateEvent, hook_cb, this);
}

void PlaybackStatus::hook_cb(void *data, void *ctx) {
  static_cast<PlaybackStatus *>(data)->call_hooks();
}

void PlaybackStatus::call_hooks() {
  std::unique_lock lock(hook_lock_);
  auto hooks = hooks_;
  lock.unlock();
  for (const auto &h : hooks) {
    h.func(*this, h.ctx);
  }
}

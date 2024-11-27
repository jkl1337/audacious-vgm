#ifndef VGMTK_QTUI_DEVICEICE_FLAGS_WIDGET_H
#define VGMTK_QTUI_DEVICEICE_FLAGS_WIDGET_H
#include <QtWidgets/QWidget>

#include "corelib/device-config.h"

namespace vgmqt {
using namespace vgmtk;

class DeviceFlagsWidgetPrivate;

class DeviceFlagsWidget : public QWidget {
  Q_OBJECT

  Q_PROPERTY(uint32_t selected_core READ selected_core WRITE set_selected_core NOTIFY core_changed)
  Q_PROPERTY(uint32_t flags MEMBER flags_ WRITE set_flags)

 Q_SIGNALS:
  void core_changed(uint32_t core);
  void core_edited(uint32_t core);
  void flags_changed(uint32_t flags);
  void flags_edited(uint32_t flags);

 public:
  explicit DeviceFlagsWidget(int dev_idx, uint32_t flags = 0, uint32_t core = 0,
                             QWidget *parent = nullptr,
                             const DeviceConfigList &info_list = lib_devices);
  ~DeviceFlagsWidget() override;

  uint32_t selected_core() const;
  Q_SLOT void set_selected_core(uint32_t core);
  uint32_t flags() const { return flags_; }
  Q_SLOT void set_flags(uint32_t flags);

 private:
  uint32_t flags_;
  std::unique_ptr<DeviceFlagsWidgetPrivate> const d_ptr;
  Q_DECLARE_PRIVATE(DeviceFlagsWidget)
};

}  // namespace vgmqt
#endif  // VGMTK_QTUI_DEVICEICE_FLAGS_WIDGET_H
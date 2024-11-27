#ifndef VGMTK_QTUI_CORE_SELECTOR_H
#define VGMTK_QTUI_CORE_SELECTOR_H

#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QRadioButton>
#include <array>

#include "corelib/device-config.h"

namespace vgmqt {

using namespace vgmtk;

class CoreSelectorPrivate;

class CoreSelector : public QWidget {
  Q_OBJECT

  Q_PROPERTY(
      uint32_t selected_core MEMBER core_ WRITE set_selected_core NOTIFY core_changed USER true)

 Q_SIGNALS:
  void core_changed(uint32_t core);
  void core_edited(uint32_t core);

 public:
  explicit CoreSelector(int dev_idx, uint32_t core = 0, QWidget *parent = nullptr,
                        const DeviceConfigList &info_list = lib_devices);
  ~CoreSelector() override;

  constexpr uint32_t selected_core() const { return core_; }
  Q_SLOT void set_selected_core(uint32_t core);

 private:
  uint32_t core_;
  std::unique_ptr<CoreSelectorPrivate> const d_ptr;
  Q_DECLARE_PRIVATE(CoreSelector)
};

}  // namespace vgmqt

#endif  // VGMTK_QTUI_CORE_SELECTOR_H

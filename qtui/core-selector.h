//
// Created by luebsj on 4/16/20.
//

#ifndef VGMTK_CORE_SELECTOR_H
#define VGMTK_CORE_SELECTOR_H

#include <QtWidgets/QRadioButton>
#include <QtWidgets/QWidget>
#include <array>

namespace vgmtk {
class DeviceConfig;
}

namespace vgmqt {

class CoresWidget : public QWidget {
  Q_OBJECT

 public:
  explicit CoresWidget(QWidget *parent = nullptr);

  void setDevice(const vgmtk::DeviceConfig &config, size_t select);

  Q_PROPERTY(unsigned long coreSelected MEMBER core_ NOTIFY coreChanged)

 signals:
  void coreChanged(unsigned long);

 private:
  std::array<QRadioButton *, 3> buttons_{};
  std::array<uint32_t, 3> cores_{};

  unsigned long core_;
};

}  // namespace vgmqt
#endif  // VGMTK_CORE_SELECTOR_H

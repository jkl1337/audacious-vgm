#include "core-selector.h"

#include <corelib/libvgm-dev-meta.h>

#include <QtWidgets/QVBoxLayout>

using namespace vgmqt;
using namespace vgmtk;

CoresWidget::CoresWidget(QWidget *parent) : QWidget(parent) {
  auto *vbox = new QVBoxLayout(this);

  int idx = 0;
  for (auto &button : buttons_) {
    button = new QRadioButton();
    vbox->addWidget(button);
    button->setVisible(false);

    auto toggle_handler = [idx, this](bool checked) {
      if (!checked) return;
      core_ = cores_[idx];
      coreChanged(core_);
    };
    connect(button, &QAbstractButton::toggled, this, toggle_handler);
    ++idx;
  }

  auto fm = buttons_[0]->fontMetrics();
  int max_label_width = 0;

  for (const auto &d : vgmtk::lib_devices) {
    for (const auto &c : d.cores) {
      if (!c.label[0]) continue;
      int width = fm.horizontalAdvance(c.label);
      if (width > max_label_width) max_label_width = width;
    }
  }
  setMinimumWidth(buttons_[0]->minimumSizeHint().width() + max_label_width);
  vbox->addStretch(1);
}

void CoresWidget::setDevice(const vgmtk::DeviceConfig &config, size_t select) {
  size_t i{};
  for (const auto &c : config.cores) {
    auto *const button = buttons_[i];
    button->setText(c.label[0] ? c.label : core_name(c.core));
    button->setVisible(true);
    cores_[i] = c.core;
    if (i == select) {
      button->setChecked(true);
    }
    ++i;
  }
  for (; i < buttons_.size(); ++i) {
    buttons_[i]->setVisible(false);
    cores_[i] = 0;
  }
}

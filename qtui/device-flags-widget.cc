
#include "device-flags-widget.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFormLayout>

#include "core-selector.h"

namespace {
struct FlagDescription {
  uint32_t group_mask;
  uint32_t select_mask;
  QAbstractButton *button;
};

}  // namespace

namespace vgmqt {

class DeviceFlagsWidgetPrivate {
 public:
  DeviceFlagsWidgetPrivate(DeviceFlagsWidget *q, int dev_idx, uint32_t core,
                           const DeviceConfigList &info_list);

 private:
  DeviceFlagsWidget *const q_ptr;
  QFormLayout layout;
  CoreSelector core_selector;
  uint32_t mask{};
  QVarLengthArray<FlagDescription, 8> flag_descriptors;
  Q_DISABLE_COPY(DeviceFlagsWidgetPrivate)
  Q_DECLARE_PUBLIC(DeviceFlagsWidget)

  void handle_click(const FlagDescription &fd, bool checked);
};

DeviceFlagsWidget::DeviceFlagsWidget(int dev_idx, uint32_t flags, uint32_t core, QWidget *parent,
                                     const DeviceConfigList &info_list)
    : QWidget(parent),
      flags_(flags),
      d_ptr(std::make_unique<DeviceFlagsWidgetPrivate>(this, dev_idx, core, info_list)) {}

DeviceFlagsWidget::~DeviceFlagsWidget() = default;

uint32_t DeviceFlagsWidget::selected_core() const {
  return d_func()->core_selector.selected_core();
}

void DeviceFlagsWidget::set_selected_core(uint32_t core) {
  d_func()->core_selector.set_selected_core(core);
}

void DeviceFlagsWidget::set_flags(uint32_t flags) {
  Q_D(DeviceFlagsWidget);
  flags &= d->mask;
  if (flags_ == flags) return;
  const auto old_flags = flags_;
  flags_ = flags;
  for (const auto &fd : d->flag_descriptors) {
    if (fd.select_mask & (old_flags ^ flags)) fd.button->setChecked(fd.select_mask & flags);
  }
  emit flags_changed(flags);
}

void DeviceFlagsWidgetPrivate::handle_click(const FlagDescription &fd, bool checked) {
  Q_Q(DeviceFlagsWidget);
  uint32_t bit = checked ? fd.select_mask : 0;
  uint32_t new_flags = (q->flags_ & ~fd.group_mask) | bit;
  if (new_flags == q->flags_) return;
  q->flags_ = new_flags;
  emit q->flags_edited(new_flags);
  emit q->flags_changed(new_flags);
}

DeviceFlagsWidgetPrivate::DeviceFlagsWidgetPrivate(DeviceFlagsWidget *q, int dev_idx, uint32_t core,
                                                   const DeviceConfigList &info_list)
    : q_ptr(q), layout(q), core_selector(dev_idx, core, q, info_list) {
  layout.addRow(DeviceFlagsWidget::tr("Core"), &core_selector);
  QObject::connect(&core_selector, &CoreSelector::core_changed, q,
                   &DeviceFlagsWidget::core_changed);
  QObject::connect(&core_selector, &CoreSelector::core_edited, q, &DeviceFlagsWidget::core_edited);

  const auto &device = info_list[dev_idx];

  auto init_flags = q->flags_;

  for (int i = 0; const auto &opt : device.options) {
    if (opt.type == OptionSpec::Type::onoff) {
      const auto &switch_info = opt.sw;

      auto *button = new QCheckBox(switch_info.label, q);
      button->setChecked(switch_info.value & init_flags);

      flag_descriptors.emplace_back(switch_info.value, switch_info.value, button);
      QObject::connect(
          button, &QCheckBox::clicked, q,
          [this, &fd = flag_descriptors[i]](bool checked) { handle_click(fd, checked); });

      button->setToolTip(switch_info.doc);
      layout.addRow(nullptr, button);
      mask |= switch_info.value;

      ++i;
    } else if (opt.type == OptionSpec::Type::choice) {
      const auto &choice_info = opt.choice;
      auto *hbox = new QHBoxLayout();
      auto *group = new QButtonGroup(q);

      uint32_t group_mask = 0;
      for (const auto &c : choice_info.choices) {
        group_mask |= c.value;
      }
      mask |= group_mask;

      for (const auto &c : choice_info.choices) {
        auto *button = new QRadioButton(c.label, q);
        button->setChecked((init_flags & group_mask) == c.value);

        flag_descriptors.emplace_back(group_mask, c.value, button);
        QObject::connect(
            button, &QRadioButton::clicked, q,
            [this, &fd = flag_descriptors[i]](bool checked) { handle_click(fd, checked); });

        button->setToolTip(c.doc);
        group->addButton(button, c.value);
        hbox->addWidget(button);
        ++i;
      }
      layout.addRow(choice_info.label, hbox);
    }
  }
  q->flags_ &= mask;
}

}  // namespace vgmqt

#include "moc_device-flags-widget.cpp"
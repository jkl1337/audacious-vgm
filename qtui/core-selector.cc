#include "core-selector.h"

#include <QtWidgets/QLayout>

#include "corelib/settings-manager.h"

namespace vgmqt {
using namespace vgmtk;

class CoreSelectorPrivate {
 public:
  CoreSelectorPrivate(CoreSelector *q, int dev_idx, const DeviceConfigList &info_list);

  Q_DISABLE_COPY(CoreSelectorPrivate)
  Q_DECLARE_PUBLIC(CoreSelector)

 private:
  CoreSelector *const q_ptr;
  QHBoxLayout layout;
  QButtonGroup group;
  QRadioButton default_button;
  QVarLengthArray<uint32_t, 8> id_to_core;
};

CoreSelector::CoreSelector(int dev_idx, uint32_t core, QWidget *parent,
                           const DeviceConfigList &info_list)
    : QWidget(parent),
      core_(core),
      d_ptr(std::make_unique<CoreSelectorPrivate>(this, dev_idx, info_list)) {}

CoreSelector::~CoreSelector() = default;

void CoreSelector::set_selected_core(uint32_t core) {
  Q_D(CoreSelector);
  if (core_ == core) return;
  auto i = d->id_to_core.indexOf(core);
  if (i == -1) return;
  core_ = core;
  d->group.button(i)->setChecked(true);
  emit core_changed(core);
}

CoreSelectorPrivate::CoreSelectorPrivate(CoreSelector *q, int dev_idx,
                                         const DeviceConfigList &info_list)
    : q_ptr(q), layout(q), group(q), default_button(CoreSelector::tr("Default")), id_to_core(1, 0) {
  const auto &device = info_list.get(dev_idx);
  const auto &cores = device.cores;

  group.addButton(&default_button, 0);
  layout.addWidget(&default_button);
  layout.setContentsMargins(0, 0, 0, 0);

  for (size_t i = 0; i < cores.size(); ++i) {
    const auto &core = cores[i];
    id_to_core.push_back(core.core);
    auto name = core.name();
    auto *button = new QRadioButton(QString::fromUtf8(name.data(), name.size()));
    group.addButton(button, i + 1);
    layout.addWidget(button);
  }
  {
    auto *b = group.button(id_to_core.indexOf(q->core_));
    if (b) b->setChecked(true);
  }

  group.connect(&group, &QButtonGroup::idClicked, q, [this](int id) {
    Q_Q(CoreSelector);
    const auto new_core = id_to_core[id];
    if (q->core_ == new_core) return;
    q->core_ = new_core;
    emit q->core_edited(new_core);
    emit q->core_changed(new_core);
  });
}

}  // namespace vgmqt

#include "moc_core-selector.cpp"
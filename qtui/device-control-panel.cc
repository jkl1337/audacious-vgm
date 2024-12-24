#include "device-control-panel.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QMouseEvent>
#include <QShortcut>
#include <QStackedLayout>
#include <QStyledItemDelegate>
#include <QTreeView>

#include "channel-select.h"
#include "core-selector.h"
#include "corelib/settings-manager.h"
#include "device-flags-widget.h"
#include "device-select.h"
#include "qvgm-factory.h"

using namespace vgmqt;
using namespace vgmtk;

namespace {

class DeviceListViewItemDelegate : public QStyledItemDelegate {
 public:
  explicit DeviceListViewItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    QVariant data = index.data(DeviceTreeModel::PlayingDeviceRole);
    opt.font.setBold(data.toBool());
    QStyledItemDelegate::paint(painter, opt, index);
  }
};

class SliderDelegate : public QStyledItemDelegate {
  QSize sizeHint_;

 public:
  explicit SliderDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {
    QSlider slider(Qt::Horizontal);
    slider.setRange(-256, 256);
    slider.setTickInterval(64);
    slider.setTickPosition(QSlider::TicksBelow);
    sizeHint_ = slider.sizeHint();
  }

  bool eventFilter(QObject *object, QEvent *event) override {
    if (event->type() != QEvent::MouseButtonDblClick) return false;
    auto *mouse_event = static_cast<QMouseEvent *>(event);
    if (mouse_event->button() != Qt::LeftButton) return false;
    auto *slider = qobject_cast<QSlider *>(object);
    if (!slider) return false;
    slider->setSliderPosition(0);
    mouse_event->accept();
    return true;
  }

  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                        const QModelIndex &) const override {
    auto *slider = new QSlider(Qt::Horizontal, parent);
    slider->setRange(-256, 256);
    slider->setTickInterval(64);
    slider->setTickPosition(QSlider::TicksBelow);

    connect(slider, &QSlider::valueChanged, this,
            [this, slider](int) { const_cast<SliderDelegate *>(this)->commitData(slider); });

    return slider;
  }

  void paint(QPainter *, const QStyleOptionViewItem &, const QModelIndex &) const override {}

  QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override {
    return sizeHint_;
  }
};

}  // namespace

namespace vgmqt {

class DeviceControlPanelPrivate {
  Q_DISABLE_COPY(DeviceControlPanelPrivate)
  Q_DECLARE_PUBLIC(DeviceControlPanel)
  DeviceControlPanel *const q_ptr;

  QVBoxLayout top;
  QHBoxLayout hbox;
  QTreeView device_tree;
  DeviceTreeModel device_tree_model;
  QTreeView channel_tree;
  ChannelTreeModel channel_tree_model;

 public:
  DeviceControlPanelPrivate(DeviceControlPanel *q, PlaybackStatus &playback_status,
                            SettingsManager &settings_manager);
};

DeviceControlPanelPrivate::DeviceControlPanelPrivate(DeviceControlPanel *q,
                                                     PlaybackStatus &playback_status,
                                                     SettingsManager &settings_manager)
    : q_ptr(q),
      top(q),
      device_tree_model(settings_manager, playback_status, &device_tree),
      channel_tree_model(settings_manager, &playback_status, &channel_tree) {
  top.addLayout(&hbox), hbox.addWidget(&device_tree, 0);
  device_tree.setItemDelegate(new DeviceListViewItemDelegate(&device_tree));

  // auto *filter_model = new vgmqt::DeviceTreeFiltered(model);
  // filter_model->setSourceModel(model);

  device_tree.setSortingEnabled(false);
  device_tree.setMinimumHeight(400);
  device_tree.setModel(&device_tree_model);
  // device_tree.expandRecursively(QModelIndex(), 1);
  device_tree.setUniformRowHeights(true);
  device_tree.setTextElideMode(Qt::ElideNone);
  device_tree.header()->setStretchLastSection(false);
  device_tree.header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  device_tree.resizeColumnToContents(0);

  channel_tree.setItemDelegateForColumn(3, new SliderDelegate(&channel_tree));

  hbox.addWidget(&channel_tree, 1);
  channel_tree.setMinimumWidth(400);
  channel_tree.setModel(&channel_tree_model);

  // NOLINTNEXTLINE(*-unused-return-value)
  QObject::connect(&channel_tree_model, &QAbstractItemModel::modelReset, &channel_tree, [this]() {
    int rc = channel_tree_model.rowCount(QModelIndex());

    for (int r = 0; r < rc; ++r) {
      auto p = channel_tree_model.index(r, 0, QModelIndex());
      auto c = channel_tree_model.index(0, 3, p);

      if (!(channel_tree_model.flags(c) & Qt::ItemIsEditable)) continue;

      for (int i = 0; c.isValid(); c = channel_tree_model.index(++i, 3, p)) {
        channel_tree.openPersistentEditor(c);
      }
    }
  });
  channel_tree.header()->setStretchLastSection(false);
  channel_tree.header()->setSectionResizeMode(0, QHeaderView::Stretch);
  channel_tree.header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  channel_tree.header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

  QStackedLayout *stack = new QStackedLayout(&top);
  for (int i = 0; auto [dev, meta] : settings_manager.current().devices_with_meta()) {
    auto *df = new DeviceFlagsWidget(i, dev.core_opts, dev.core, q);
    stack->addWidget(df);
    df->connect(df, &DeviceFlagsWidget::core_edited, q, [&settings_manager, i](uint32_t core) {
      settings_manager.writer().set_core(i, core);
    });
    df->connect(df, &DeviceFlagsWidget::flags_edited, q, [&settings_manager, i](uint32_t flags) {
      settings_manager.writer().set_core_opts(i, UINT32_MAX, flags);
    });
    ++i;
  }

  QObject::connect(device_tree.selectionModel(), &QItemSelectionModel::currentChanged, q,
                   [this, stack](const QModelIndex &current, const QModelIndex &) {
                     auto dev_idx =
                         static_cast<DeviceTreeModel *>(device_tree.model())->dev_idx(current);
                     stack->setCurrentIndex(dev_idx);
                   });
  // auto *shortcut = new QShortcut(QKeySequence("Alt+1"), this);
}

DeviceControlPanel::DeviceControlPanel(PlaybackStatus &playback_status,
                                       SettingsManager &settings_manager, QWidget *parent)
    : QWidget(parent),
      d_ptr(std::make_unique<DeviceControlPanelPrivate>(this, playback_status, settings_manager)) {}

DeviceControlPanel::~DeviceControlPanel() = default;

QWidget *create_qvgm_control_panel(PlaybackStatus &ps, SettingsManager &sm, QWidget *parent) {
  return new DeviceControlPanel(ps, sm, parent);
}

}  // namespace vgmqt

#include "moc_device-control-panel.cpp"

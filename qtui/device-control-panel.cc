#include "device-control-panel.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QMouseEvent>
#include <QShortcut>
#include <QStyledItemDelegate>
#include <QTreeView>

#include "channel-select.h"
#include "core-selector.h"
#include "corelib/settings-manager.h"
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
    std::unique_ptr<QSlider> slider(new QSlider(Qt::Horizontal));
    slider->setRange(-256, 256);
    slider->setTickInterval(64);
    slider->setTickPosition(QSlider::TicksBelow);
    sizeHint_ = slider->sizeHint();
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
  Q_DISABLE_COPY(DeviceControlPanelPrivate);
  Q_DECLARE_PUBLIC(DeviceControlPanel);
  DeviceControlPanel *const q_ptr;

  QHBoxLayout layout;
  QTreeView device_tree;

 public:
  DeviceControlPanelPrivate(DeviceControlPanel *q, vgmtk::PlaybackStatus &playback_status,
                            vgmtk::SettingsManager &settings_manager);
};

DeviceControlPanelPrivate::DeviceControlPanelPrivate(DeviceControlPanel *q,
                                                     vgmtk::PlaybackStatus &playback_status,
                                                     vgmtk::SettingsManager &settings_manager)
    : q_ptr(q), layout(q) {

  auto *tree_view = new QTreeView();
  layout.addWidget(tree_view, 0);

  tree_view->setItemDelegate(new DeviceListViewItemDelegate(tree_view));
  auto *model = new DeviceTreeModel(settings_manager, playback_status, tree_view);

  // auto *filter_model = new vgmqt::DeviceTreeFiltered(model);
  // filter_model->setSourceModel(model);

  tree_view->setSortingEnabled(false);
  tree_view->setMinimumHeight(600);
  tree_view->setModel(model);
  // tree_view->expandRecursively(QModelIndex(), 1);
  tree_view->setUniformRowHeights(true);
  tree_view->setTextElideMode(Qt::ElideNone);
  tree_view->header()->setStretchLastSection(false);
  tree_view->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  tree_view->resizeColumnToContents(0);

  auto *chan_view = new QTreeView();
  chan_view->setItemDelegateForColumn(3, new SliderDelegate(chan_view));

  layout.addWidget(chan_view, 1);
  chan_view->setMinimumWidth(300);

  auto *chan_model = new ChannelTreeModel(settings_manager, &playback_status, chan_view);
  chan_view->setModel(chan_model);

  // NOLINTNEXTLINE(*-unused-return-value)
  QObject::connect(chan_model, &QAbstractItemModel::modelReset, chan_view,
                   [chan_model, chan_view]() {
                     int rc = chan_model->rowCount(QModelIndex());

                     for (int r = 0; r < rc; ++r) {
                       auto p = chan_model->index(r, 0, QModelIndex());
                       auto c = chan_model->index(0, 3, p);

                       if (!(chan_model->flags(c) & Qt::ItemIsEditable)) continue;

                       for (int i = 0; c.isValid(); c = chan_model->index(++i, 3, p)) {
                         chan_view->openPersistentEditor(c);
                       }
                     }
                   });
  chan_view->header()->setStretchLastSection(false);
  chan_view->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  chan_view->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  chan_view->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

  auto *cores = new CoresWidget();
  cores->setDevice(lib_devices[35], 1);
  layout.addWidget(cores);

  // auto *shortcut = new QShortcut(QKeySequence("Alt+1"), this);
}

DeviceControlPanel::DeviceControlPanel(PlaybackStatus &playback_status,
                                       SettingsManager &settings_manager, QWidget *parent)
    : QWidget(parent),
      d_ptr(std::make_unique<DeviceControlPanelPrivate>(this, playback_status, settings_manager)) {}

DeviceControlPanel::~DeviceControlPanel() = default;

QWidget *create_qvgm_control_panel(PlaybackStatus &ps, SettingsManager &sm,
                                   QWidget *parent) {
  return new DeviceControlPanel(ps, sm, parent);
}

}  // namespace vgmqt

#include "moc_device-control-panel.cpp"
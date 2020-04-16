#include "prefs.h"

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudqt/libaudqt.h>

#include <QtCore/QAbstractListModel>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListView>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QShortcut>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QTreeView>
#include <bitset>
#include <memory>

#include "channel-select.h"
#include "comm.h"
#include "device-select.h"
#include "libvgm-dev-meta.h"

namespace vgm {

constexpr int kNumChips = 2;
constexpr int kMaxNumDevices = 41;

class DeviceListModel;

class DeviceListModel : public QAbstractListModel {
 public:
  using DisableMask = PlayerSettings::DisableMask;
  enum ItemDataRole { RunningDeviceRole = Qt::UserRole };

 private:
  class ChipListModel : public QAbstractListModel {
   public:
    explicit ChipListModel(DeviceListModel &device_list) : device_list_(device_list) {}

    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

   private:
    friend class DeviceListModel;
    DeviceListModel &device_list_;
  };

 public:
  explicit DeviceListModel(PlaybackStatus &playback_status, SettingsManager &settings_manager,
                           QObject *parent = nullptr);

  QVariant data(const QModelIndex &index, int role) const override;
  bool setData(const QModelIndex &index, const QVariant &value, int role) override;
  int rowCount(const QModelIndex &parent) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;

  ChipListModel *chip_list_model() { return &chip_list_model_; }
  int num_devices() const { return settings_manager_.current().devices.size(); }

  bool modify_chip_disable(int dev_idx, int chip, bool enable, DisableMask mask);
  bool modify_device_disable(int dev_idx, bool enable, DisableMask mask);

  int current_device() const { return current_device_; }
  void set_current_device(int i);

 private:
  bool modify_helper(int dev_idx);
  ChipListModel chip_list_model_;
  SettingsManager &settings_manager_;

  std::bitset<kMaxNumDevices> running_devices_;
  int current_device_ = 0;
};

DeviceListModel::DeviceListModel(PlaybackStatus &playback_status, SettingsManager &settings_manager,
                                 QObject *parent)
    : QAbstractListModel(parent), chip_list_model_(*this), settings_manager_(settings_manager) {
  auto playback_status_hook = [](const PlaybackStatus &ps, void *ctx) {
    auto *self = static_cast<DeviceListModel *>(ctx);
    self->running_devices_.reset();

    for (const auto &a : ps.get_device_info()) {
      for (int i = 0; i < vgm::lib_devices.len; ++i) {
        if (vgm::lib_devices[i].devid == a.type) {
          self->running_devices_[i] = true;
          auto mi = self->createIndex(i, 0);
          self->dataChanged(mi, mi, QVector<int>({RunningDeviceRole}));
        }
      }
    }
  };
  playback_status.subscribe(playback_status_hook, this);
  connect(this, &QObject::destroyed, this,
          [&]() { playback_status.unsubscribe(playback_status_hook, this); });
}

bool DeviceListModel::modify_chip_disable(int dev_idx, int chip, bool enable,
                                          DeviceListModel::DisableMask mask) {
  if (dev_idx < 0 || dev_idx >= num_devices() || chip < 0 || chip > kNumChips) return false;

  {
    auto m = settings_manager_.writer();
    if (!m.set_disable(dev_idx, chip, !enable, mask)) return true;
  }
  return modify_helper(dev_idx);
}

bool DeviceListModel::modify_device_disable(int dev_idx, bool enable,
                                            DeviceListModel::DisableMask mask) {
  if (dev_idx < 0 || dev_idx >= num_devices()) return false;

  {
    auto m = settings_manager_.writer();
    if (!m.set_disable(dev_idx, !enable, mask)) return true;
  }
  return modify_helper(dev_idx);
}

bool DeviceListModel::modify_helper(int dev_idx) {
  QVector<int> r({Qt::CheckStateRole});
  auto mi = createIndex(dev_idx, 0);
  dataChanged(mi, mi, r);

  if (current_device_ == dev_idx) {
    auto &clm = chip_list_model_;
    clm.dataChanged(clm.createIndex(0, 0), clm.createIndex(kNumChips - 1, 0), r);
  }
  return true;
}

bool DeviceListModel::setData(const QModelIndex &index, const QVariant &value, int role) {
  int row = index.row();
  if (row < 0 || row >= num_devices() || role != Qt::CheckStateRole) return false;

  modify_device_disable(row, value.value<Qt::CheckState>() == Qt::Checked, DisableMask().set());

  return true;
}

QVariant DeviceListModel::data(const QModelIndex &index, int role) const {
  int row = index.row();
  if (row < 0 || row >= num_devices()) return QVariant();

  const auto &dev = vgm::lib_devices[row];

  switch (role) {
    case Qt::DisplayRole:
      return QVariant(dev.label);

    case Qt::CheckStateRole: {
      Qt::CheckState c = Qt::Unchecked;
      const auto &store = settings_manager_.current().devices[row];
      // FIXME: abstract
      if (store.chips[0].disable.none() && store.chips[1].disable.none())
        c = Qt::Checked;
      else if (store.chips[0].disable.to_ulong() < 3 || store.chips[1].disable.to_ulong() < 3)
        c = Qt::PartiallyChecked;
      return QVariant(c);
    }
    case DeviceListModel::RunningDeviceRole: {
      return QVariant(running_devices_.test(row));
    }
    default:
      break;
  }
  return QVariant();
}

int DeviceListModel::rowCount(const QModelIndex &parent) const { return num_devices(); }

Qt::ItemFlags DeviceListModel::flags(const QModelIndex &index) const {
  return Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate |
         Qt::ItemIsEnabled;
}

void DeviceListModel::set_current_device(int i) {
  chip_list_model_.beginResetModel();
  current_device_ = std::clamp(i, -1, num_devices());
  chip_list_model_.endResetModel();
}

QVariant DeviceListModel::ChipListModel::data(const QModelIndex &index, int role) const {
  unsigned row = index.row();
  if (row < 0 || row >= kNumChips) {
    return QVariant();
  }
  int current_device = device_list_.current_device_;
  if ((current_device < 0 || current_device >= device_list_.num_devices()) &&
      role == Qt::DisplayRole) {
    return QVariant(QString(N_("No Device")));
  }

  switch (role) {
    case Qt::DisplayRole:
      return QVariant(QString(N_("Chip %1")).arg(row + 1));
    case Qt::CheckStateRole: {
      const auto &store = device_list_.settings_manager_.current().devices[current_device];
      Qt::CheckState cs = Qt::PartiallyChecked;

      if (store.chips[row].disable.none()) {
        cs = Qt::Checked;
      } else if (store.chips[row].disable.all()) {
        cs = Qt::Unchecked;
      }
      return QVariant(cs);
    }
    default:
      break;
  }
  return QVariant();
}

bool DeviceListModel::ChipListModel::setData(const QModelIndex &index, const QVariant &value,
                                             int role) {
  int row = index.row();

  if (row < 0 || row >= kNumChips || role != Qt::CheckStateRole) return false;

  int current_device = device_list_.current_device_;
  if (current_device < 0 || current_device > device_list_.num_devices()) return false;

  device_list_.modify_chip_disable(
      current_device, row, value.value<Qt::CheckState>() == Qt::Checked, DisableMask().set());

  return true;
}

int DeviceListModel::ChipListModel::rowCount(const QModelIndex &parent) const {
  int current_device = device_list_.current_device_;
  if (current_device >= 0 && current_device < device_list_.num_devices()) {
    return kNumChips;
  } else {
    return 1;
  }
}

Qt::ItemFlags DeviceListModel::ChipListModel::flags(const QModelIndex &index) const {
  int current_device = device_list_.current_device_;
  if (current_device >= 0 && current_device < device_list_.num_devices()) {
    return Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;
  } else {
    return Qt::NoItemFlags;
  }
}

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

  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                        const QModelIndex &index) const override {
    auto *slider = new QSlider(Qt::Horizontal, parent);
    slider->setRange(-256, 256);
    slider->setTickInterval(64);
    slider->setTickPosition(QSlider::TicksBelow);

    connect(slider, &QSlider::valueChanged, this,
            [=](int) { const_cast<SliderDelegate *>(this)->commitData(slider); });

    return slider;
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {}

  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
    return sizeHint_;
  }
};

class DeviceListViewItemDelegate : public QStyledItemDelegate {
 public:
  explicit DeviceListViewItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    QVariant data = index.data(DeviceListModel::RunningDeviceRole);
    opt.font.setBold(data.toBool());
    QStyledItemDelegate::paint(painter, opt, index);
  }
};

class CoresWidget : public QWidget {
  Q_OBJECT

 public:
  explicit CoresWidget(QWidget *parent = nullptr) : QWidget(parent) {
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

    for (const auto &d : vgm::lib_devices) {
      for (const auto &c : d.cores) {
        if (!c.label[0]) continue;
        int width = fm.horizontalAdvance(c.label);
        if (width > max_label_width) max_label_width = width;
      }
    }
    setMinimumWidth(buttons_[0]->minimumSizeHint().width() + max_label_width);
    vbox->addStretch(1);
  }

  void set_device(const vgm::DeviceConfig &config, int select) {
    int i = 0;
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

  Q_PROPERTY(unsigned long coreSelected MEMBER core_ NOTIFY coreChanged)

 signals:
  void coreChanged(unsigned long);

 private:
  std::array<QRadioButton *, 3> buttons_;
  std::array<uint32_t, 3> cores_;

  unsigned long core_;
};

class ChipListView : public QListView {
 public:
  explicit ChipListView(QWidget *parent = nullptr) : QListView(parent) {}

  void setModel(QAbstractItemModel *model) override {
    QAbstractItemView::setModel(model);

    if (model) {
      // Assume that the initial setting of invalid device is the widest
    }
  }
};

class DeviceListView : public QListView {
 public:
  explicit DeviceListView(QWidget *parent = nullptr) : QListView(parent) {
    setItemDelegate(&item_delegate_);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
  }

 protected:
  void currentChanged(const QModelIndex &current, const QModelIndex &previous) override {
    QListView::currentChanged(current, previous);
    if (device_list_model_) device_list_model_->set_current_device(current.row());
  }

  void setModel(QAbstractItemModel *model) override { QAbstractItemView::setModel(model); }

 public:
  void set_device_list_model(DeviceListModel *model) {
    setModel(model);
    device_list_model_ = model;
  }

  void disable_selected_devices() {
    if (!device_list_model_) return;
    for (const auto &a : selectionModel()->selectedRows()) {
      device_list_model_->modify_device_disable(a.row(), false,
                                                DeviceListModel::DisableMask().set());
    }
  }

  void enable_selected_devices() {
    if (!device_list_model_) return;
    for (const auto &a : selectionModel()->selectedRows()) {
      device_list_model_->modify_device_disable(a.row(), true,
                                                DeviceListModel::DisableMask().set());
    }
  }

 private:
  DeviceListModel *device_list_model_ = nullptr;
  DeviceListViewItemDelegate item_delegate_;
};

class ChipControlWidget : public QWidget {
 public:
  explicit ChipControlWidget(PlaybackStatus &playback_status, SettingsManager &settings_manager,
                             QWidget *parent = nullptr);

 private:
  DeviceListModel device_list_;
};

ChipControlWidget::ChipControlWidget(PlaybackStatus &playback_status,
                                     SettingsManager &settings_manager, QWidget *parent)
    : QWidget(parent), device_list_(playback_status, settings_manager, nullptr) {
  auto *layout = new QHBoxLayout(this);
  auto *vbox = new QVBoxLayout;
  layout->addLayout(vbox);

  auto *device_list_view = new DeviceListView();
  vbox->addWidget(device_list_view);

  auto *hbox = new QHBoxLayout;
  vbox->addLayout(hbox, 1);

  auto *button_dev_ena = new QPushButton(N_("Enable selected"));
  hbox->addWidget(button_dev_ena, 1);
  auto *button_dev_dis = new QPushButton(N_("Disable selected"));
  hbox->addWidget(button_dev_dis, 1);

  auto *chip_list_view = new ChipListView();
  layout->addWidget(chip_list_view);

  auto *info_label = new QLabel("Potter");
  layout->addWidget(info_label, 1);

  device_list_view->set_device_list_model(&device_list_);

  auto *clv = chip_list_view;
  clv->setModel(device_list_.chip_list_model());
  clv->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  clv->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  clv->setFixedSize(
      clv->sizeHintForColumn(0) + 2 * (clv->frameWidth() + clv->spacing()) +
          clv->style()->pixelMetric(QStyle::PM_ScrollBarExtent),
      clv->sizeHintForRow(0) * (kNumChips * 256) / 192 + 2 * (clv->frameWidth() + clv->spacing()));

  QObject::connect(button_dev_ena, &QPushButton::clicked, device_list_view,
                   [device_list_view]() { device_list_view->enable_selected_devices(); });

  QObject::connect(button_dev_dis, &QPushButton::clicked, device_list_view,
                   [device_list_view]() { device_list_view->disable_selected_devices(); });
}

void *construct_chip_control_widget(PlaybackStatus &playback_status,
                                    SettingsManager &settings_manager) {
  auto *widget = new QWidget();
  auto *layout = audqt::make_hbox(widget);

  auto *tree_view = new QTreeView();
  layout->addWidget(tree_view, 0);

  tree_view->setItemDelegate(new DeviceListViewItemDelegate(tree_view));

  auto *model = new vgmqt::DeviceTreeModel(settings_manager, playback_status, tree_view);
  auto *filter_model = new vgmqt::DeviceTreeFiltered(model);
  filter_model->setSourceModel(model);

  tree_view->setSortingEnabled(false);
  tree_view->setMinimumHeight(800);
  tree_view->setModel(model);
  // tree_view->expandRecursively(QModelIndex(), 1);
  tree_view->setUniformRowHeights(true);
  tree_view->header()->setStretchLastSection(false);
  tree_view->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  // tree_view->resizeColumnToContents(0);

  auto *chan_view = new QTreeView();
  chan_view->setItemDelegateForColumn(3, new SliderDelegate(chan_view));

  layout->addWidget(chan_view, 1);

  auto *chan_model = new vgmqt::ChannelTreeModel(settings_manager, &playback_status, chan_view);
  chan_view->setModel(chan_model);

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
  cores->set_device(lib_devices[2], 1);
  layout->addWidget(cores);

  auto *shortcut = new QShortcut(QKeySequence("Alt+1"), widget);
  QObject::connect(shortcut, &QShortcut::activated, widget, [](){
    printf("MOTHER\n");
  });
  return widget;
  // return ChipControlWidget::construct(playback_status, settings_manager, nullptr);
}

}  // namespace vgm

#include "prefs.moc"

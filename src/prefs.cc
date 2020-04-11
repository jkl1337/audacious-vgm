#include "prefs.h"

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudqt/libaudqt.h>

#include <QtCore/QAbstractListModel>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListView>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyledItemDelegate>
#include <bitset>

#include "comm.h"
#include "libvgm-dev-meta.h"

namespace audvgm {

constexpr int kNumChips = 2;
constexpr int kMaxNumDevices = 41;

class DeviceListModel;

class DeviceListModel : public QAbstractListModel {
 public:
  typedef std::bitset<kNumChips> ChipSwitch;
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
    DeviceListModel &device_list_;
  };

 public:
  explicit DeviceListModel(PlaybackStatus &playback_status, QObject *parent = nullptr);

  QVariant data(const QModelIndex &index, int role) const override;
  bool setData(const QModelIndex &index, const QVariant &value, int role) override;
  int rowCount(const QModelIndex &parent) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;

  ChipListModel *chip_list_model() { return &chip_list_model_; }
  int num_devices() const { return state_.size(); }

  bool set_chip_state(int i, ChipSwitch enable, ChipSwitch disable);
  bool set_chip_state(int i, int chip, bool enable) {
    auto c = ChipSwitch().set(chip, true);
    if (enable) {
      return set_chip_state(i, c, 0);
    } else {
      return set_chip_state(i, 0, c);
    }
  }
  bool set_device_state(int i, bool enable) {
    auto c = ChipSwitch().set();
    if (enable) {
      return set_chip_state(i, c, 0);
    } else {
      return set_chip_state(i, 0, c);
    }
  }

  int current_device() const { return current_device_; }
  void set_current_device(int i);

 private:
  ChipListModel chip_list_model_;
  std::bitset<kMaxNumDevices> running_devices_;
  std::vector<ChipSwitch> state_;
  int current_device_ = 0;
};

DeviceListModel::DeviceListModel(PlaybackStatus &playback_status, QObject *parent)
    : QAbstractListModel(parent), chip_list_model_(*this), state_(vgm::lib_devices.len) {
  auto playback_status_hook = [](const PlaybackStatus &ps, void *ctx) {
    auto *self = static_cast<DeviceListModel *>(ctx);
    self->running_devices_.reset();

    for (const auto &a : ps.device_info()) {
      for (int i = 0; i < vgm::lib_devices.len; ++i) {
        if (vgm::lib_devices[i].id == a.type) {
          self->running_devices_[i] = true;
          auto mi = self->createIndex(i, 0);
          self->dataChanged(mi, mi, QVector<int>({RunningDeviceRole}));
        }
      }
    }
  };
  playback_status.hook(playback_status_hook, this);
  connect(this, &QObject::destroyed, this,
          [&]() { playback_status.unhook(playback_status_hook, this); });
}

bool DeviceListModel::set_chip_state(int i, DeviceListModel::ChipSwitch enable,
                                     DeviceListModel::ChipSwitch disable) {
  if (i < 0 || i >= num_devices()) return false;

  auto &s = state_[i];
  const ChipSwitch prev = s;

  s |= enable;
  s &= ~disable;

  if (prev == s) return false;

  QVector<int> r({Qt::CheckStateRole});
  auto idx = createIndex(i, 0);
  dataChanged(idx, idx, r);

  if (current_device_ == i) {
    auto &clm = chip_list_model_;
    clm.dataChanged(clm.createIndex(0, 0), clm.createIndex(kNumChips - 1, 0), r);
  }
  return true;
}

bool DeviceListModel::setData(const QModelIndex &index, const QVariant &value, int role) {
  int row = index.row();
  if (row < 0 || row >= num_devices() || role != Qt::CheckStateRole) return false;

  switch (value.value<Qt::CheckState>()) {
    case Qt::Checked:
      set_chip_state(row, ChipSwitch().set(), ChipSwitch());
      break;
    case Qt::Unchecked:
      set_chip_state(row, ChipSwitch(), ChipSwitch().set());
      break;
    default:
      break;
  }
  return true;
}

QVariant DeviceListModel::data(const QModelIndex &index, int role) const {
  int row = index.row();
  if (row < 0 || row >= num_devices()) return QVariant();

  auto &dev = vgm::lib_devices[row];

  switch (role) {
    case Qt::DisplayRole:
      return QVariant(dev.label);

    case Qt::CheckStateRole: {
      Qt::CheckState c;
      auto &store = state_[row];
      if (store.all())
        c = Qt::Checked;
      else if (store.any())
        c = Qt::PartiallyChecked;
      else
        c = Qt::Unchecked;
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
      auto store = device_list_.state_[current_device];
      return QVariant(store.test(row) ? Qt::Checked : Qt::Unchecked);
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

  device_list_.set_chip_state(current_device, row, value.value<Qt::CheckState>() == Qt::Checked);

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
    return Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;
  } else {
    return Qt::NoItemFlags;
  }
}

class DeviceListViewItemDelegate : public QStyledItemDelegate {
 public:
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    QVariant data = index.data(DeviceListModel::RunningDeviceRole);
    opt.font.setBold(data.toBool());
    QStyledItemDelegate::paint(painter, opt, index);
  }
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

 protected:
  void setModel(QAbstractItemModel *model) override { QAbstractItemView::setModel(model); }

 public:
  void set_device_list_model(DeviceListModel *model) {
    setModel(model);
    device_list_model_ = model;
  }

  void disable_selected_devices() {
    if (!device_list_model_) return;
    for (const auto &a : selectionModel()->selectedRows()) {
      device_list_model_->set_device_state(a.row(), false);
    }
  }

  void enable_selected_devices() {
    if (!device_list_model_) return;
    for (const auto &a : selectionModel()->selectedRows()) {
      device_list_model_->set_device_state(a.row(), true);
    }
  }

 private:
  DeviceListModel *device_list_model_ = nullptr;
  DeviceListViewItemDelegate item_delegate_;
};

class ChipControlWidget : public QWidget {
 public:
  static ChipControlWidget *construct(PlaybackStatus &playback_status, QWidget *parent = nullptr);

 private:
  explicit ChipControlWidget(PlaybackStatus &playback_status) : device_list_(playback_status) {}

  DeviceListModel device_list_;
};

ChipControlWidget *ChipControlWidget::construct(PlaybackStatus &playback_status, QWidget *parent) {
  auto ptr = std::unique_ptr<ChipControlWidget>(new ChipControlWidget(playback_status));
  auto self = ptr.get();
  auto layout = audqt::make_hbox(self);

  auto vbox = new QVBoxLayout;
  layout->addLayout(vbox);

  auto device_list_view = new DeviceListView();
  vbox->addWidget(device_list_view);

  auto hbox = new QHBoxLayout;
  vbox->addLayout(hbox, 1);

  auto button_dev_ena = new QPushButton(N_("Enable selected"));
  hbox->addWidget(button_dev_ena, 1);
  auto button_dev_dis = new QPushButton(N_("Disable selected"));
  hbox->addWidget(button_dev_dis, 1);

  auto chip_list_view = new ChipListView();
  layout->addWidget(chip_list_view);

  auto info_label = new QLabel("Potter");
  layout->addWidget(info_label, 1);

  device_list_view->set_device_list_model(&self->device_list_);

  auto clv = chip_list_view;
  clv->setModel(self->device_list_.chip_list_model());
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

  auto ret = ptr.release();
  if (parent) ret->setParent(parent);
  return ret;
}

void *construct_chip_control_widget(PlaybackStatus &playback_status) {
  return ChipControlWidget::construct(playback_status);
}

}  // namespace audvgm
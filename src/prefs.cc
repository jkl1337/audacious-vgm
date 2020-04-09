#include <QtCore/QAbstractListModel>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QListView>
#include <QtWidgets/QStyledItemDelegate>
#include <bitset>

#include "libvgm-dev-meta.h"

namespace audvgm {

constexpr int kNumChips = 2;

class DeviceMasterModel;

class DeviceListModel : public QAbstractListModel {
 public:
  explicit DeviceListModel(DeviceMasterModel &master) : master_(master) {}

  QVariant data(const QModelIndex &index, int role) const override;
  bool setData(const QModelIndex &index, const QVariant &value, int role) override;
  int rowCount(const QModelIndex &parent) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;

 private:
  friend class DeviceMasterModel;
  DeviceMasterModel &master_;
  void notify_enable_state_changed(int device);
};

class ChipListModel : public QAbstractListModel {
 public:
  explicit ChipListModel(DeviceMasterModel &master) : master_(master) {}

  QVariant data(const QModelIndex &index, int role) const override;
  int rowCount(const QModelIndex &parent) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  bool setData(const QModelIndex &index, const QVariant &value, int role) override;
  void set_current_device(int idx);

  int current_device() const { return current_device_; }


 private:
  friend class DeviceMasterModel;
  DeviceMasterModel &master_;
  int current_device_ = -1;

  void notify_enable_state_changed();
};

class DeviceMasterModel : public QObject {
 public:
  typedef std::bitset<kNumChips> ChipSwitch;


 public:

  enum ItemDataRole { RunningDeviceRole = Qt::UserRole };

 private:

  int num_devices_;
  std::vector<ChipSwitch> state_;
  DeviceListModel device_list_model_;
  ChipListModel chip_list_model_;

 public:
  explicit DeviceMasterModel(QObject *parent = nullptr)
      : QObject(parent),
        chip_list_model_(*this),
        device_list_model_(*this),
        num_devices_(vgm::lib_devices_a.len),
        state_(num_devices_, 3) {}

  DeviceListModel *device_list_model() { return &device_list_model_; }
  ChipListModel *chip_list_model() { return &chip_list_model_; }

  void set_device_enable(int device, ChipSwitch chips);
  void set_chip_enable(int device, int chip, bool enable);
  ChipSwitch get_device_enable(int device) {
    if (device < 0 || device > state_.size()) return ChipSwitch();
    return state_[device];
  };

  int num_devices() { return num_devices_; }
};

void DeviceMasterModel::set_device_enable(int device, ChipSwitch chips) {
  if (device < 0 || device > state_.size()) return;

  std::swap(state_[device], chips);
  if (state_[device] != chips) {
    if (chip_list_model_.current_device() == device) {
      chip_list_model_.notify_enable_state_changed();
    }
    device_list_model_.notify_enable_state_changed(device);
  }
}

void DeviceMasterModel::set_chip_enable(int device, int chip, bool enable) {
  if (device < 0 || device > state_.size()) return;
  ChipSwitch cs = state_[device];
  set_device_enable(device, cs.set(chip, enable));
}

bool DeviceListModel::setData(const QModelIndex &index, const QVariant &value, int role) {
  int row = index.row();
  if (row >= master_.num_devices() || role != Qt::CheckStateRole) return false;
  auto state = value.value<Qt::CheckState>();
  master_.set_device_enable(row, state == Qt::Checked ? DeviceMasterModel::ChipSwitch().set()
                                                      : DeviceMasterModel::ChipSwitch());
  return true;
}

QVariant DeviceListModel::data(const QModelIndex &index, int role) const {
  int row = index.row();
  if (row >= master_.num_devices()) return QVariant();

  auto &dev = vgm::lib_devices_a[row];

  if (role == Qt::DisplayRole) {
    return QVariant(dev.label);
  } else if (role == Qt::CheckStateRole) {
    Qt::CheckState c = Qt::Unchecked;
    DeviceMasterModel::ChipSwitch state = master_.get_device_enable(row);
    if (state.all()) {
      c = Qt::Checked;
    } else if (state.any()) {
      c = Qt::PartiallyChecked;
    }
    return QVariant(c);
  }
  return QVariant();
}

int DeviceListModel::rowCount(const QModelIndex &parent) const { return vgm::lib_devices_a.len; }

Qt::ItemFlags DeviceListModel::flags(const QModelIndex &index) const {
  return Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate |
         Qt::ItemIsEnabled;
}

void DeviceListModel::notify_enable_state_changed(int device) {
  dataChanged(createIndex(device, 0), createIndex(device, 0), QVector<int>({Qt::CheckStateRole}));
}


QVariant ChipListModel::data(const QModelIndex &index, int role) const {
  unsigned row = index.row();
  if (row >= kNumChips) return QVariant();

  if (current_device_ >= 0 && current_device_ < master_.num_devices()) {
    if (role == Qt::DisplayRole) {
      return QVariant(QString(tr("Chip %1")).arg(row + 1));
    } else if (role == Qt::CheckStateRole) {
      return QVariant(master_.get_device_enable(current_device_).test(row) ? Qt::Checked
                                                                           : Qt::Unchecked);
    }
  } else {
    if (role == Qt::DisplayRole) {
      return QVariant(QString(tr("No Device")));
    }
  }
  return QVariant();
}

bool ChipListModel::setData(const QModelIndex &index, const QVariant &value, int role) {
  int row = index.row();
  if (row >= kNumChips || role != Qt::CheckStateRole) return false;

  master_.set_chip_enable(current_device_, row, value.value<Qt::CheckState>() == Qt::Checked);

  return true;
}

void ChipListModel::set_current_device(int idx) {
  beginResetModel();
  current_device_ = idx;
  endResetModel();
}

int ChipListModel::rowCount(const QModelIndex &parent) const {
  if (current_device_ >= 0 && current_device_ < master_.num_devices()) {
    return kNumChips;
  } else {
    return 1;
  }
}

Qt::ItemFlags ChipListModel::flags(const QModelIndex &index) const {
  if (current_device_ >= 0 && current_device_ < master_.num_devices()) {
    return Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;
  } else {
    return Qt::NoItemFlags;
  }
}

void ChipListModel::notify_enable_state_changed() {
  dataChanged(createIndex(0, 0), createIndex(kNumChips - 1, 0), QVector<int>({Qt::CheckStateRole}));
}

class DeviceListViewItemDelegate : public QStyledItemDelegate {
 public:
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    QVariant data = index.data(DeviceMasterModel::RunningDeviceRole);
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
      setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
      setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
      setFixedSize(sizeHintForColumn(0) + 2 * (frameWidth() + spacing()) +
                       style()->pixelMetric(QStyle::PM_ScrollBarExtent),
                   sizeHintForRow(0) * (kNumChips * 256) / 192 + 2 * (frameWidth() + spacing()));
    }
  }
};

class DeviceListView : public QListView {
 public:
  explicit DeviceListView(ChipListModel *chip_list_model = nullptr, QWidget *parent = nullptr)
      : QListView(parent), chip_list_model_(chip_list_model) {}

 protected:
  void currentChanged(const QModelIndex &current, const QModelIndex &previous) override {
    QListView::currentChanged(current, previous);
    if (chip_list_model_) chip_list_model_->set_current_device(current.row());
  }

 private:
  ChipListModel *chip_list_model_;
};

void *populate_chip_prefs() {
  auto widget = std::make_unique<QWidget>();
  auto layout = new QHBoxLayout(widget.get());
  widget->setLayout(layout);

  auto model = new DeviceMasterModel(widget.get());
  ChipListModel *chip_list_model = model->chip_list_model();

  auto device_list_view = new DeviceListView(chip_list_model, widget.get());
  layout->addWidget(device_list_view, 1);
  auto chip_list_view = new ChipListView(widget.get());
  layout->addWidget(chip_list_view);
  device_list_view->setModel(model->device_list_model());
  chip_list_view->setModel(chip_list_model);

  return widget.release();
}

}  // namespace audvgm
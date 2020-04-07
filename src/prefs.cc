
#include "prefs.h"

#include <QtCore/QAbstractListModel>

namespace audvgm {

namespace {

class DeviceListModel : public QAbstractListModel {
 public:
  DeviceListModel : QAbstractListModel()
  QVariant data(const QModelIndex& index, int role) const override { return QVariant(); }
  int rowCount(const QModelIndex& parent) const override { return 0; }

  Qt::ItemFlags flags(const QModelIndex& index) const override {
    return QAbstractListModel::flags(index);
  }
};

class ChipListModel : public QAbstractListModel {

};

}
}
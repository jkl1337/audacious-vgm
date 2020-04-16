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

#include "../qtui/channel-select.h"
#include "../qtui/device-select.h"
#include "libvgm-dev-meta.h"
#include "libvgm-rt.h"

namespace vgmtk {


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
  return widget;
}

}  // namespace vgm

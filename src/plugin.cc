#include "configure.h"
#include "plugin.h"
#include "driver.h"

LIBAUDCORE_PUBLIC VgmPlugin aud_plugin_instance;

const char VgmPlugin::about[] =
        N_("VGM music decoder based on libvgm 1.0\n"
           "Supported formats: vgm\n\n"
           "Audacious plugin by:\n"
           "John Luebs <jkl@johnluebs.com>\n");

const char *const VgmPlugin::exts[] = {
        "vgm", "vgz", nullptr
};

static ComboItem chip_select_combo[] = {
        ComboItem(N_("Chip #1"), 1),
        ComboItem(N_("Chip #2"), 2),
};


static void mute_cb(unsigned ch, unsigned i) {
    audvgm_playback_settings.mute_masks[i].fetch_xor(1 << ch);
}

constexpr WidgetConfig WidgetMask(void (*callback)(),
                                  const char *hook = nullptr) {
    return WidgetConfig(WidgetConfig::Bool, nullptr, 0, 0, callback, hook);
}

static constexpr PreferencesWidget vgm_widgets_dev_chip_sel[] = {
        WidgetLabel(N_("Select Chip")),
        WidgetCombo(nullptr, WidgetInt(audcfg.chip_sel),
                    {{chip_select_combo}}),
        WidgetCheck("Channel 1", WidgetMask([] { mute_cb(0, 0); })),
        WidgetCheck("Channel 2", WidgetMask([] { mute_cb(1, 0); })),
        WidgetCheck("Channel 3", WidgetMask([] { mute_cb(2, 0); })),
        WidgetCheck("Channel 4", WidgetMask([] { mute_cb(3, 0); })),
        WidgetCheck("Channel 5", WidgetMask([] { mute_cb(4, 0); })),
        WidgetCheck("Channel 6", WidgetMask([] { mute_cb(5, 0); })),
        WidgetCheck("Channel 7", WidgetMask([] { mute_cb(6, 0); })),
        WidgetCheck("Channel 8", WidgetMask([] { mute_cb(7, 0); })),
        WidgetCheck("Channel 9", WidgetMask([] { mute_cb(8, 0); })),
        WidgetCheck("Channel 10", WidgetMask([] { mute_cb(9, 0); })),
        WidgetCheck("Channel 11", WidgetMask([] { mute_cb(10, 0); })),
        WidgetCheck("Channel 12", WidgetMask([] { mute_cb(11, 0); })),
        WidgetCheck("Channel 13", WidgetMask([] { mute_cb(12, 0); })),
        WidgetCheck("Channel 14", WidgetMask([] { mute_cb(13, 0); })),
        WidgetCheck("Channel 15", WidgetMask([] { mute_cb(14, 0); })),
        WidgetCheck("Channel 16", WidgetMask([] { mute_cb(15, 0); })),
        WidgetCheck("Channel 1", WidgetMask([] { mute_cb(0, 1); })),
        WidgetCheck("Channel 2", WidgetMask([] { mute_cb(1, 1); })),
        WidgetCheck("Channel 3", WidgetMask([] { mute_cb(2, 1); })),
        WidgetCheck("Channel 4", WidgetMask([] { mute_cb(3, 1); })),
        WidgetCheck("Channel 5", WidgetMask([] { mute_cb(4, 1); })),
        WidgetCheck("Channel 6", WidgetMask([] { mute_cb(5, 1); })),
        WidgetCheck("Channel 7", WidgetMask([] { mute_cb(6, 1); })),
        WidgetCheck("Channel 8", WidgetMask([] { mute_cb(7, 1); })),
        WidgetCheck("Channel 9", WidgetMask([] { mute_cb(8, 1); })),
        WidgetCheck("Channel 10", WidgetMask([] { mute_cb(9, 1); })),
        WidgetCheck("Channel 11", WidgetMask([] { mute_cb(10, 1); })),
        WidgetCheck("Channel 12", WidgetMask([] { mute_cb(11, 1); })),
        WidgetCheck("Channel 13", WidgetMask([] { mute_cb(12, 1); })),
        WidgetCheck("Channel 14", WidgetMask([] { mute_cb(13, 1); })),
        WidgetCheck("Channel 15", WidgetMask([] { mute_cb(14, 1); })),
        WidgetCheck("Channel 16", WidgetMask([] { mute_cb(15, 1); })),

};

static const PreferencesWidget vgm_widgets_chip_opts[] = {
        WidgetBox({{vgm_widgets_dev_chip_sel}}),
};


#include <QList>
#include <QString>
#include <QTreeWidget>
#include <QTreeView>
#include <QHeaderView>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QTableWidget>
#include <QLayout>

#include <libaudqt/libaudqt.h>

class NoEditDelegate : public QStyledItemDelegate {
public:
    explicit NoEditDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    QWidget *
    createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        return nullptr;
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

    QWidget *
    createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        auto *slider = new QSlider(Qt::Horizontal, parent);
        slider->setRange(-256, 256);
        slider->setTickInterval(64);
        slider->setTickPosition(QSlider::TicksBelow);

        connect(slider, &QSlider::valueChanged, [=](int) {
            const_cast<SliderDelegate *>(this)->commitData(slider);
        });
        return slider;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        return sizeHint_;
    }
};

class ConfigurationWidget : public QWidget {
public:
    explicit ConfigurationWidget(QWidget *parent = nullptr);

};

#include <QPushButton>

ConfigurationWidget::ConfigurationWidget(QWidget *parent)
        : QWidget(parent) {
    auto widget = new QTreeWidget(this);
    auto layout = new QVBoxLayout;
    this->setLayout(layout);
    layout->addWidget(widget, 1);

    //auto widget = new QTreeWidget(this);
    widget->setColumnCount(5);

    auto no_edit_delegate = new NoEditDelegate();
    widget->setItemDelegateForColumn(0, no_edit_delegate);
    widget->setItemDelegateForColumn(1, no_edit_delegate);
    widget->setItemDelegateForColumn(2, no_edit_delegate);
    //widget->setItemDelegateForColumn(3, no_edit_delegate);
    widget->setItemDelegateForColumn(3, new SliderDelegate());

    auto t1 = new QTreeWidgetItem(widget, {"YM2612"});
    t1->setExpanded(true);
    t1->setFlags(Qt::ItemIsEnabled);

    for (int i = 0; i < 6; ++i) {
        auto item = new QTreeWidgetItem(t1);
        item->setText(0, QString("Channel %1").arg(i + 1));
        item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
        item->setCheckState(1, Qt::Unchecked);
        item->setCheckState(2, Qt::Unchecked);
        widget->openPersistentEditor(item, 3);
//        auto slider = new QSlider(Qt::Horizontal, widget);
//        slider->setRange(-256, 256);
//        slider->setTickInterval(64);
//        slider->setTickPosition(QSlider::TicksBelow);
        //widget->setItemWidget(item, 3, slider);
    }
    new QTreeWidgetItem(t1, {"DAC"});


    auto hi = widget->headerItem();
    hi->setFlags(hi->flags() | Qt::ItemIsUserCheckable);
    hi->setCheckState(1, Qt::Unchecked);
    hi->setCheckState(2, Qt::Unchecked);

    widget->setHeaderLabels({"Channel", "M", "S", "Pan", "Label"});
    widget->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    widget->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    widget->resizeColumnToContents(0);
    widget->resizeColumnToContents(1);
    widget->resizeColumnToContents(2);
    widget->resizeColumnToContents(3);

    widget->setSelectionMode(QAbstractItemView::ExtendedSelection);


//    QObject::connect(m, &QTableWidget::itemChanged, [this] (){
//
//    });

    QObject::connect(widget, &QTreeWidget::itemChanged, [=](QTreeWidgetItem *item, int column) {
        printf("FUCK %u\n", column);
        fflush(stdout);
    });

}

static void *populate() {
    return new ConfigurationWidget();
}

static const PreferencesWidget vgm_widgets_general[] = {
        WidgetLabel(N_("<b>Playback</b>")),
        WidgetSpin(N_("Default number of loops:"),
                   WidgetInt(audcfg.num_loops),
                   {1, 7200, 1, N_("loops")}),
        WidgetSpin(N_("Fade time:"),
                   WidgetFloat(audcfg.fade_length),
                   {0, 7200, 1, N_("seconds")}),
        WidgetCustomQt(populate, WIDGET_CHILD),
};

static const NotebookTab vgm_notebook_tabs[] = {
        {N_("General"),       {vgm_widgets_general}},
        {N_("Chip Options"),  {vgm_widgets_chip_opts}},
        {N_("Other Options"), {}}
};

static const PreferencesWidget widgets[] = {
        WidgetNotebook({{vgm_notebook_tabs}})
};

const PluginPreferences VgmPlugin::prefs = {widgets};


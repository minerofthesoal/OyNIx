/*
 * OyNIx Browser v3.1 — ExtensionPanel implementation
 */

#include "ExtensionPanel.h"
#include "interop/CoreBridge.h"
#include "theme/ThemeEngine.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonObject>

ExtensionPanel::ExtensionPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    setupStyles();
    refresh();
}

ExtensionPanel::~ExtensionPanel() = default;

void ExtensionPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header
    auto *header = new QWidget(this);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 8, 12, 8);

    auto *title = new QLabel(QStringLiteral("Extensions"), this);
    title->setObjectName(QStringLiteral("panelTitle"));
    headerLayout->addWidget(title);

    m_countLabel = new QLabel(QStringLiteral("0"), this);
    m_countLabel->setObjectName(QStringLiteral("badge"));
    headerLayout->addWidget(m_countLabel);

    headerLayout->addStretch();

    m_installBtn = new QPushButton(QStringLiteral("Install"), this);
    m_installBtn->setObjectName(QStringLiteral("smallBtn"));
    connect(m_installBtn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Install Extension"), QString(),
            tr("Extensions (*.npi *.xpi *.zip)"));
        if (!path.isEmpty()) {
            if (CoreBridge::instance().extInstall(path)) {
                refresh();
            }
        }
    });
    headerLayout->addWidget(m_installBtn);

    layout->addWidget(header);

    // Extension list
    m_listWidget = new QListWidget(this);
    m_listWidget->setObjectName(QStringLiteral("extList"));
    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_listWidget->setSpacing(2);

    connect(m_listWidget, &QListWidget::itemClicked,
            this, &ExtensionPanel::onItemClicked);
    connect(m_listWidget, &QListWidget::customContextMenuRequested,
            this, &ExtensionPanel::onContextMenu);

    layout->addWidget(m_listWidget, 1);

    setFixedWidth(240);
}

void ExtensionPanel::setupStyles()
{
    const auto &c = ThemeEngine::instance().colors();

    QString ss;
    ss += QStringLiteral("ExtensionPanel { background: ") + c["bg-darkest"] + QStringLiteral("; }\n");
    ss += QStringLiteral("#panelTitle { color: ") + c["purple-light"]
       + QStringLiteral("; font-size: 12px; font-weight: 700;"
                        " letter-spacing: 0.08em; text-transform: uppercase; }\n");
    ss += QStringLiteral("#badge { color: ") + c["bg-darkest"]
       + QStringLiteral("; background: ") + c["purple-mid"]
       + QStringLiteral("; font-size: 10px; font-weight: 700;"
                        " padding: 1px 7px; border-radius: 8px; }\n");
    ss += QStringLiteral("#smallBtn { background: rgba(110,106,179,0.15); color: ")
       + c["purple-light"] + QStringLiteral("; border: 1px solid rgba(110,106,179,0.3);"
                                             " border-radius: 6px; padding: 3px 10px;"
                                             " font-size: 11px; }\n");
    ss += QStringLiteral("#smallBtn:hover { background: rgba(110,106,179,0.3); }\n");
    ss += QStringLiteral("#extList { background: ") + c["bg-dark"]
       + QStringLiteral("; color: ") + c["text-primary"]
       + QStringLiteral("; border: none; outline: none; font-size: 12px; }\n");
    ss += QStringLiteral("#extList::item { padding: 6px 10px; margin: 1px 4px;"
                         " border-radius: 6px; }\n");
    ss += QStringLiteral("#extList::item:selected { background: ") + c["selection"]
       + QStringLiteral("; color: ") + c["purple-pale"] + QStringLiteral("; }\n");
    ss += QStringLiteral("#extList::item:hover:!selected { background: rgba(110,106,179,0.12); }\n");
    ss += QStringLiteral("QScrollBar:vertical { background: transparent; width: 5px; }\n");
    ss += QStringLiteral("QScrollBar::handle:vertical { background: ") + c["scrollbar"]
       + QStringLiteral("; border-radius: 2px; }\n");
    ss += QStringLiteral("QScrollBar::handle:vertical:hover { background: ") + c["scrollbar-hover"]
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }\n");
    setStyleSheet(ss);
}

void ExtensionPanel::refresh()
{
    m_listWidget->clear();

    auto &bridge = CoreBridge::instance();
    if (!bridge.isLoaded()) {
        m_countLabel->setText(QStringLiteral("0"));
        return;
    }

    const QJsonArray extensions = bridge.extList();
    m_countLabel->setText(QString::number(extensions.size()));

    for (const auto &val : extensions) {
        auto ext = val.toObject();
        const QString name = ext[QStringLiteral("Name")].toString();
        const QString version = ext[QStringLiteral("Version")].toString();
        const bool enabled = ext[QStringLiteral("Enabled")].toBool();

        auto *item = new QListWidgetItem(m_listWidget);
        item->setText(QStringLiteral("%1 v%2").arg(name, version));
        item->setData(Qt::UserRole, name);
        item->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);

        const QString desc = ext[QStringLiteral("Description")].toString();
        if (!desc.isEmpty())
            item->setToolTip(desc);
    }
}

void ExtensionPanel::onItemClicked(QListWidgetItem *item)
{
    if (!item) return;
    const QString name = item->data(Qt::UserRole).toString();
    const bool enabled = (item->checkState() == Qt::Checked);

    CoreBridge::instance().extSetEnabled(name, enabled);
    emit extensionToggled(name, enabled);
}

void ExtensionPanel::onContextMenu(const QPoint &pos)
{
    auto *item = m_listWidget->itemAt(pos);
    if (!item) return;

    const QString name = item->data(Qt::UserRole).toString();

    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(tr("Open Popup"), this, [this, name]() {
        emit extensionPopupRequested(name);
    });

    menu->addSeparator();

    menu->addAction(tr("Remove"), this, [this, name]() {
        if (CoreBridge::instance().extRemove(name)) {
            refresh();
            emit extensionRemoved(name);
        }
    });

    menu->popup(m_listWidget->viewport()->mapToGlobal(pos));
}

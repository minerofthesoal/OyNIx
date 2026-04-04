/*
 * OyNIx Browser v3.1 — ExtensionPanel
 * Inline sidebar panel for managing extensions.
 * Replaces the old popup-window approach.
 */

#pragma once

#include <QWidget>

class QVBoxLayout;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QLabel;

class ExtensionPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ExtensionPanel(QWidget *parent = nullptr);
    ~ExtensionPanel() override;

    void refresh();

signals:
    void extensionToggled(const QString &name, bool enabled);
    void extensionInstallRequested();
    void extensionRemoved(const QString &name);
    void extensionPopupRequested(const QString &name);

private:
    void setupUi();
    void onItemClicked(QListWidgetItem *item);
    void onContextMenu(const QPoint &pos);

    QListWidget *m_listWidget = nullptr;
    QPushButton *m_installBtn = nullptr;
    QLabel      *m_countLabel = nullptr;
};

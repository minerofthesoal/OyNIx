#pragma once

#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>

class QLineEdit;
class QComboBox;
class QTabWidget;
class QTextEdit;
class QLabel;

/**
 * SearchEngineBuilder — Dialog for creating and managing custom search engines.
 * Supports visual builder, OpenSearch XML import, and code-based definitions.
 */
class SearchEngineBuilder : public QDialog
{
    Q_OBJECT

public:
    explicit SearchEngineBuilder(QWidget *parent = nullptr);
    ~SearchEngineBuilder() override;

    [[nodiscard]] QJsonArray customEngines() const;
    void loadEngines(const QJsonArray &engines);

signals:
    void enginesChanged(const QJsonArray &engines);

private:
    void setupUi();
    void applyStyles();

    QWidget *createVisualBuilder();
    QWidget *createImportTab();
    QWidget *createCodeTab();

    void addEngine();
    void testEngine();
    void removeSelectedEngine();
    void loadFromDisk();
    void saveToDisk();

    QTabWidget *m_tabs = nullptr;

    // Visual builder
    QLineEdit *m_nameEdit     = nullptr;
    QLineEdit *m_urlEdit      = nullptr;
    QLineEdit *m_iconEdit     = nullptr;
    QLineEdit *m_suggestEdit  = nullptr;

    // Import
    QTextEdit *m_importEdit   = nullptr;

    // Code
    QTextEdit *m_codeEdit     = nullptr;

    // Engine list
    QComboBox *m_engineList   = nullptr;
    QLabel    *m_testResult   = nullptr;

    QJsonArray m_engines;
};

#pragma once

#include <QWidget>
#include <QJsonArray>

class QVBoxLayout;
class QHBoxLayout;
class QTextEdit;
class QPushButton;
class QLabel;
class QScrollArea;
class QPropertyAnimation;

/**
 * AiChatPanel — Slide-out chat panel for the Nyx AI assistant.
 * Features message bubbles, typing indicator, quick action pills,
 * and animated transitions.
 */
class AiChatPanel : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int panelWidth READ panelWidth WRITE setPanelWidth)

public:
    explicit AiChatPanel(QWidget *parent = nullptr);
    ~AiChatPanel() override;

    void togglePanel();
    void showPanel();
    void hidePanel();
    [[nodiscard]] bool isPanelVisible() const { return m_visible; }

    int panelWidth() const { return m_panelWidth; }
    void setPanelWidth(int w);

signals:
    void summarizePageRequested();
    void explainCodeRequested();
    void panelVisibilityChanged(bool visible);

private:
    void setupUi();
    void setupStyles();

    void addUserMessage(const QString &text);
    void addAiMessage(const QString &text);
    void addSystemMessage(const QString &text);

    QWidget *createMessageBubble(const QString &text, bool isUser);
    QWidget *createTypingIndicator();

    void sendMessage();
    void onAiResponse(const QString &response);
    void onStatusChanged(int status, const QString &message);

    void scrollToBottom();
    void updateStatusDot();

    // UI elements
    QScrollArea   *m_scrollArea    = nullptr;
    QWidget       *m_messagesContainer = nullptr;
    QVBoxLayout   *m_messagesLayout = nullptr;
    QTextEdit     *m_inputField    = nullptr;
    QPushButton   *m_sendButton    = nullptr;
    QLabel        *m_statusLabel   = nullptr;
    QLabel        *m_statusDot     = nullptr;
    QWidget       *m_typingIndicator = nullptr;

    // Quick actions
    QPushButton   *m_summarizeBtn  = nullptr;
    QPushButton   *m_explainBtn    = nullptr;
    QPushButton   *m_clearBtn      = nullptr;

    // Animation
    QPropertyAnimation *m_slideAnim = nullptr;
    bool m_visible = false;
    int  m_panelWidth = 0;
    static constexpr int PanelMaxWidth = 380;
};

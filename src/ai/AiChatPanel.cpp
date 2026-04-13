#include "AiChatPanel.h"
#include "AiManager.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextEdit>
#include <QPropertyAnimation>
#include <QTimer>
#include <QDateTime>
#include <QKeyEvent>
#include <QGraphicsOpacityEffect>
#include <QRegularExpression>

// ── Constructor / Destructor ─────────────────────────────────────────
AiChatPanel::AiChatPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    setupStyles();

    // Connect to AI manager
    connect(&AiManager::instance(), &AiManager::responseReceived,
            this, &AiChatPanel::onAiResponse);
    connect(&AiManager::instance(), &AiManager::statusChanged,
            this, [this](AiManager::Status status, const QString &msg) {
                onStatusChanged(static_cast<int>(status), msg);
            });

    // Start hidden
    setFixedWidth(0);
    m_panelWidth = 0;
    m_visible = false;
}

AiChatPanel::~AiChatPanel() = default;

// ── UI setup ─────────────────────────────────────────────────────────
void AiChatPanel::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header
    auto *header = new QWidget(this);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 8, 12, 8);

    m_statusDot = new QLabel(this);
    m_statusDot->setFixedSize(10, 10);
    headerLayout->addWidget(m_statusDot);

    auto *titleLabel = new QLabel(QStringLiteral("Nyx AI"), this);
    titleLabel->setObjectName(QStringLiteral("aiTitle"));
    headerLayout->addWidget(titleLabel);

    headerLayout->addStretch();

    m_statusLabel = new QLabel(QStringLiteral("Fallback"), this);
    m_statusLabel->setObjectName(QStringLiteral("aiStatus"));
    headerLayout->addWidget(m_statusLabel);

    mainLayout->addWidget(header);

    // Quick action pills
    auto *pillsWidget = new QWidget(this);
    auto *pillsLayout = new QHBoxLayout(pillsWidget);
    pillsLayout->setContentsMargins(8, 4, 8, 4);
    pillsLayout->setSpacing(6);

    m_summarizeBtn = new QPushButton(QStringLiteral("Summarize Page"), this);
    m_summarizeBtn->setObjectName(QStringLiteral("aiPill"));
    connect(m_summarizeBtn, &QPushButton::clicked, this, &AiChatPanel::summarizePageRequested);

    m_explainBtn = new QPushButton(QStringLiteral("Explain Code"), this);
    m_explainBtn->setObjectName(QStringLiteral("aiPill"));
    connect(m_explainBtn, &QPushButton::clicked, this, &AiChatPanel::explainCodeRequested);

    m_clearBtn = new QPushButton(QStringLiteral("Clear"), this);
    m_clearBtn->setObjectName(QStringLiteral("aiPill"));
    connect(m_clearBtn, &QPushButton::clicked, this, [this]() {
        AiManager::instance().clearHistory();
        // Clear messages
        while (m_messagesLayout->count() > 0) {
            auto *item = m_messagesLayout->takeAt(0);
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        addSystemMessage(QStringLiteral("Chat history cleared."));
    });

    pillsLayout->addWidget(m_summarizeBtn);
    pillsLayout->addWidget(m_explainBtn);
    pillsLayout->addWidget(m_clearBtn);
    pillsLayout->addStretch();
    mainLayout->addWidget(pillsWidget);

    // Messages scroll area
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setObjectName(QStringLiteral("aiScrollArea"));

    m_messagesContainer = new QWidget(this);
    m_messagesLayout = new QVBoxLayout(m_messagesContainer);
    m_messagesLayout->setContentsMargins(8, 8, 8, 8);
    m_messagesLayout->setSpacing(8);
    m_messagesLayout->addStretch();

    m_scrollArea->setWidget(m_messagesContainer);
    mainLayout->addWidget(m_scrollArea, 1);

    // Input area
    auto *inputWidget = new QWidget(this);
    auto *inputLayout = new QHBoxLayout(inputWidget);
    inputLayout->setContentsMargins(8, 6, 8, 8);
    inputLayout->setSpacing(6);

    m_inputField = new QTextEdit(this);
    m_inputField->setPlaceholderText(QStringLiteral("Ask Nyx anything..."));
    m_inputField->setMaximumHeight(80);
    m_inputField->setObjectName(QStringLiteral("aiInput"));
    m_inputField->installEventFilter(this);
    inputLayout->addWidget(m_inputField, 1);

    m_sendButton = new QPushButton(QStringLiteral(">"), this);
    m_sendButton->setObjectName(QStringLiteral("aiSendBtn"));
    m_sendButton->setFixedSize(36, 36);
    connect(m_sendButton, &QPushButton::clicked, this, &AiChatPanel::sendMessage);
    inputLayout->addWidget(m_sendButton);

    mainLayout->addWidget(inputWidget);

    // Typing indicator (initially hidden)
    m_typingIndicator = createTypingIndicator();
    m_typingIndicator->hide();

    // Welcome message
    addSystemMessage(QStringLiteral("Welcome! I'm Nyx, your AI assistant. Ask me anything!"));

    // Slide animation
    m_slideAnim = new QPropertyAnimation(this, "panelWidth", this);
    m_slideAnim->setDuration(250);
    m_slideAnim->setEasingCurve(QEasingCurve::OutCubic);

    updateStatusDot();
}

void AiChatPanel::setupStyles()
{
    // Minimal overrides — global theme handles base styling
    setStyleSheet(QStringLiteral(
        "AiChatPanel { border-left: 1px solid palette(mid); }"
        "#aiTitle { font-size: 13px; font-weight: 600; }"
        "#aiStatus { font-size: 11px; opacity: 0.6; }"
        "#aiScrollArea { background: transparent; border: none; }"
        "#aiInput { border-radius: 8px; padding: 6px; }"
        "#aiSendBtn { border-radius: 16px; font-size: 14px; font-weight: bold;"
        "  min-width: 32px; min-height: 32px; padding: 0; }"
        "#aiPill { border-radius: 10px; padding: 3px 10px; font-size: 11px; }"
    ));
}

// ── Message bubbles ──────────────────────────────────────────────────
QWidget *AiChatPanel::createMessageBubble(const QString &text, bool isUser)
{
    auto *bubble = new QWidget(this);
    auto *layout = new QHBoxLayout(bubble);
    layout->setContentsMargins(4, 2, 4, 2);

    auto *avatar = new QLabel(isUser ? QStringLiteral("U") : QStringLiteral("N"), bubble);
    avatar->setFixedSize(26, 26);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setStyleSheet(isUser
        ? QStringLiteral("background: #3d3a6b; color: #d4d2ee; border-radius: 13px;"
                         " font-size: 11px; font-weight: 600;")
        : QStringLiteral("background: #6e6ab3; color: #1a1b26; border-radius: 13px;"
                         " font-size: 11px; font-weight: 600;"));

    auto *contentWidget = new QWidget(bubble);
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(2);

    // Basic markdown rendering for AI responses
    QString displayText = text;
    if (!isUser) {
        // Convert markdown to rich text
        displayText.replace(QLatin1String("&"), QLatin1String("&amp;"));
        displayText.replace(QLatin1String("<"), QLatin1String("&lt;"));
        displayText.replace(QLatin1String(">"), QLatin1String("&gt;"));
        // Code blocks: ```...```
        static QRegularExpression codeBlockRe(QStringLiteral("```[\\w]*\\n?([\\s\\S]*?)```"));
        displayText.replace(codeBlockRe, QStringLiteral(
            "<pre style='background:#1a1b26;padding:8px;border-radius:6px;"
            "font-family:monospace;font-size:12px;color:#a09cd8;'>"
            "\\1</pre>"));
        // Inline code: `...`
        static QRegularExpression inlineCodeRe(QStringLiteral("`([^`]+)`"));
        displayText.replace(inlineCodeRe, QStringLiteral(
            "<code style='background:#1a1b26;padding:1px 4px;border-radius:3px;"
            "font-family:monospace;font-size:12px;color:#a09cd8;'>\\1</code>"));
        // Bold: **...**
        static QRegularExpression boldRe(QStringLiteral("\\*\\*([^*]+)\\*\\*"));
        displayText.replace(boldRe, QStringLiteral("<b>\\1</b>"));
        // Italic: *...*
        static QRegularExpression italicRe(QStringLiteral("(?<!\\*)\\*([^*]+)\\*(?!\\*)"));
        displayText.replace(italicRe, QStringLiteral("<i>\\1</i>"));
        // Bullet lists: - item
        displayText.replace(QRegularExpression(QStringLiteral("^- (.+)$"),
            QRegularExpression::MultilineOption),
            QStringLiteral("&bull; \\1<br>"));
        // Numbered lists: 1. item
        displayText.replace(QRegularExpression(QStringLiteral("^(\\d+)\\. (.+)$"),
            QRegularExpression::MultilineOption),
            QStringLiteral("\\1. \\2<br>"));
        // Line breaks
        displayText.replace(QLatin1String("\n\n"), QLatin1String("<br><br>"));
        displayText.replace(QLatin1String("\n"), QLatin1String("<br>"));
    }

    auto *msgLabel = new QLabel(isUser ? text : displayText, contentWidget);
    msgLabel->setWordWrap(true);
    msgLabel->setTextFormat(isUser ? Qt::PlainText : Qt::RichText);
    msgLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    msgLabel->setOpenExternalLinks(false);
    msgLabel->setStyleSheet(isUser
        ? QStringLiteral("background: #2a2d42; color: #c8cad8; border-radius: 8px;"
                         " padding: 8px 12px; font-size: 13px;")
        : QStringLiteral("background: #24263a; color: #b4b1e0; border-radius: 8px;"
                         " padding: 8px 12px; font-size: 13px;"));

    auto *timeLabel = new QLabel(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm")),
                                 contentWidget);
    timeLabel->setStyleSheet(QStringLiteral("color: #565b7e; font-size: 10px; padding-left: 4px;"));

    contentLayout->addWidget(msgLabel);
    contentLayout->addWidget(timeLabel);

    if (isUser) {
        layout->addStretch();
        layout->addWidget(contentWidget, 0, Qt::AlignRight);
        layout->addWidget(avatar);
    } else {
        layout->addWidget(avatar);
        layout->addWidget(contentWidget, 0, Qt::AlignLeft);
        layout->addStretch();
    }

    // Fade-in animation
    auto *opacity = new QGraphicsOpacityEffect(bubble);
    opacity->setOpacity(0.0);
    bubble->setGraphicsEffect(opacity);

    auto *fadeIn = new QPropertyAnimation(opacity, "opacity", bubble);
    fadeIn->setDuration(200);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);

    return bubble;
}

QWidget *AiChatPanel::createTypingIndicator()
{
    auto *widget = new QWidget(this);
    auto *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(40, 4, 4, 4);

    for (int i = 0; i < 3; ++i) {
        auto *dot = new QLabel(QStringLiteral("."), widget);
        dot->setStyleSheet(QStringLiteral("color: #6e6ab3; font-size: 20px; font-weight: bold;"));
        layout->addWidget(dot);
    }
    layout->addStretch();
    return widget;
}

// ── Message actions ──────────────────────────────────────────────────
void AiChatPanel::addUserMessage(const QString &text)
{
    // Insert before the stretch at the end
    int insertPos = m_messagesLayout->count() - 1;
    if (insertPos < 0) insertPos = 0;
    m_messagesLayout->insertWidget(insertPos, createMessageBubble(text, true));
    scrollToBottom();
}

void AiChatPanel::addAiMessage(const QString &text)
{
    int insertPos = m_messagesLayout->count() - 1;
    if (insertPos < 0) insertPos = 0;
    m_messagesLayout->insertWidget(insertPos, createMessageBubble(text, false));
    scrollToBottom();
}

void AiChatPanel::addSystemMessage(const QString &text)
{
    auto *label = new QLabel(text, this);
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(QStringLiteral(
        "color: #605878; font-size: 11px; font-style: italic; padding: 8px;"));
    int insertPos = m_messagesLayout->count() - 1;
    if (insertPos < 0) insertPos = 0;
    m_messagesLayout->insertWidget(insertPos, label);
    scrollToBottom();
}

void AiChatPanel::sendMessage()
{
    const QString text = m_inputField->toPlainText().trimmed();
    if (text.isEmpty()) return;

    m_inputField->clear();
    addUserMessage(text);

    // Show typing indicator
    m_typingIndicator->show();
    int insertPos = m_messagesLayout->count() - 1;
    if (insertPos < 0) insertPos = 0;
    m_messagesLayout->insertWidget(insertPos, m_typingIndicator);

    AiManager::instance().chat(text);
}

void AiChatPanel::onAiResponse(const QString &response)
{
    // Remove typing indicator
    m_messagesLayout->removeWidget(m_typingIndicator);
    m_typingIndicator->hide();

    addAiMessage(response);
}

void AiChatPanel::onStatusChanged(int status, const QString &message)
{
    Q_UNUSED(status)
    if (m_statusLabel)
        m_statusLabel->setText(message);
    updateStatusDot();
}

void AiChatPanel::updateStatusDot()
{
    if (!m_statusDot) return;
    const auto status = AiManager::instance().status();
    QString color;
    switch (status) {
    case AiManager::Status::Ready:       color = QStringLiteral("#73c991"); break;
    case AiManager::Status::Loading:     color = QStringLiteral("#e5a84b"); break;
    case AiManager::Status::Downloading: color = QStringLiteral("#6e9dd4"); break;
    case AiManager::Status::Error:       color = QStringLiteral("#d4565e"); break;
    case AiManager::Status::Fallback:    color = QStringLiteral("#565b7e"); break;
    }
    m_statusDot->setStyleSheet(QStringLiteral(
        "background: %1; border-radius: 5px;").arg(color));
}

void AiChatPanel::scrollToBottom()
{
    QTimer::singleShot(50, this, [this]() {
        if (m_scrollArea)
            m_scrollArea->verticalScrollBar()->setValue(
                m_scrollArea->verticalScrollBar()->maximum());
    });
}

// ── Panel visibility ─────────────────────────────────────────────────
void AiChatPanel::togglePanel()
{
    if (m_visible) hidePanel();
    else           showPanel();
}

void AiChatPanel::showPanel()
{
    m_visible = true;
    m_slideAnim->stop();
    m_slideAnim->setStartValue(m_panelWidth);
    m_slideAnim->setEndValue(PanelMaxWidth);
    m_slideAnim->start();
    emit panelVisibilityChanged(true);
}

void AiChatPanel::hidePanel()
{
    m_visible = false;
    m_slideAnim->stop();
    m_slideAnim->setStartValue(m_panelWidth);
    m_slideAnim->setEndValue(0);
    m_slideAnim->start();
    emit panelVisibilityChanged(false);
}

void AiChatPanel::setPanelWidth(int w)
{
    m_panelWidth = w;
    setFixedWidth(w);
}

bool AiChatPanel::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_inputField && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        // Enter sends message; Shift+Enter inserts newline
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
            && !(keyEvent->modifiers() & Qt::ShiftModifier))
        {
            sendMessage();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

#include "AudioPlayer.h"
#include "theme/ThemeEngine.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QListWidget>
#include <QTimer>
#include <QFileDialog>
#include <QInputDialog>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QFileInfo>

#ifdef HAS_MULTIMEDIA
#include <QMediaPlayer>
#include <QAudioOutput>
#endif

// ── Constructor / Destructor ─────────────────────────────────────────
AudioPlayer::AudioPlayer(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    setupStyles();

#ifdef HAS_MULTIMEDIA
    m_audioOutput = new QAudioOutput(this);
    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(m_audioOutput);

    connect(m_player, &QMediaPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState state) {
                m_isPlaying = (state == QMediaPlayer::PlayingState);
                m_playBtn->setText(m_isPlaying ? QStringLiteral("||") : QStringLiteral(">"));
                emit playbackStateChanged(m_isPlaying);
            });

    connect(m_player, &QMediaPlayer::durationChanged, this,
            [this](qint64 duration) {
                m_progressSlider->setMaximum(static_cast<int>(duration / 1000));
            });

    connect(m_player, &QMediaPlayer::positionChanged, this,
            [this](qint64 position) {
                if (!m_progressSlider->isSliderDown())
                    m_progressSlider->setValue(static_cast<int>(position / 1000));
                updateProgress();
            });

    connect(m_progressSlider, &QSlider::sliderReleased, this, [this]() {
        m_player->setPosition(static_cast<qint64>(m_progressSlider->value()) * 1000);
    });

    connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        m_audioOutput->setVolume(value / 100.0f);
    });
#endif

    loadPlaylist();
}

AudioPlayer::~AudioPlayer()
{
    savePlaylist();
}

// ── UI Setup ─────────────────────────────────────────────────────────
void AudioPlayer::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(6);

    const auto &c = ThemeEngine::instance().colors();

    // Title
    auto *header = new QLabel(QStringLiteral("Audio Player"), this);
    header->setStyleSheet(QStringLiteral("color: ") + c["purple-light"]
        + QStringLiteral("; font-size: 13px; font-weight: 700;"
                         " letter-spacing: 0.05em; text-transform: uppercase;"));
    layout->addWidget(header);

    // Now playing info
    m_titleLabel = new QLabel(QStringLiteral("No track selected"), this);
    m_titleLabel->setStyleSheet(QStringLiteral("color: ") + c["text-primary"]
        + QStringLiteral("; font-size: 13px;"));
    m_titleLabel->setMaximumWidth(300);
    layout->addWidget(m_titleLabel);

    m_artistLabel = new QLabel(QString(), this);
    m_artistLabel->setStyleSheet(QStringLiteral("color: ") + c["text-secondary"]
        + QStringLiteral("; font-size: 11px;"));
    layout->addWidget(m_artistLabel);

    // Progress
    auto *progressRow = new QHBoxLayout;
    m_progressSlider = new QSlider(Qt::Horizontal, this);
    m_progressSlider->setRange(0, 100);
    progressRow->addWidget(m_progressSlider, 1);

    m_timeLabel = new QLabel(QStringLiteral("0:00 / 0:00"), this);
    m_timeLabel->setStyleSheet(QStringLiteral("color: ") + c["text-secondary"]
        + QStringLiteral("; font-size: 10px;"));
    m_timeLabel->setFixedWidth(80);
    progressRow->addWidget(m_timeLabel);
    layout->addLayout(progressRow);

    // Controls
    auto *controlsRow = new QHBoxLayout;
    m_prevBtn = new QPushButton(QStringLiteral("<<"), this);
    m_prevBtn->setFixedSize(32, 32);
    connect(m_prevBtn, &QPushButton::clicked, this, &AudioPlayer::previous);

    m_playBtn = new QPushButton(QStringLiteral(">"), this);
    m_playBtn->setFixedSize(40, 40);
    connect(m_playBtn, &QPushButton::clicked, this, &AudioPlayer::togglePlayPause);

    m_nextBtn = new QPushButton(QStringLiteral(">>"), this);
    m_nextBtn->setFixedSize(32, 32);
    connect(m_nextBtn, &QPushButton::clicked, this, &AudioPlayer::next);

    controlsRow->addStretch();
    controlsRow->addWidget(m_prevBtn);
    controlsRow->addWidget(m_playBtn);
    controlsRow->addWidget(m_nextBtn);
    controlsRow->addStretch();

    // Volume
    auto *volIcon = new QLabel(QStringLiteral("Vol"), this);
    volIcon->setStyleSheet(QStringLiteral("color: ") + c["text-secondary"]
        + QStringLiteral("; font-size: 10px;"));
    controlsRow->addWidget(volIcon);
    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(70);
    m_volumeSlider->setFixedWidth(80);
    controlsRow->addWidget(m_volumeSlider);

    layout->addLayout(controlsRow);

    // Add buttons
    auto *addRow = new QHBoxLayout;
    m_addFileBtn = new QPushButton(QStringLiteral("+ File"), this);
    connect(m_addFileBtn, &QPushButton::clicked, this, [this]() {
        const QStringList files = QFileDialog::getOpenFileNames(this,
            QStringLiteral("Add Audio Files"),
            QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
            QStringLiteral("Audio (*.mp3 *.wav *.ogg *.flac *.m4a *.aac *.wma)"));
        for (const auto &f : files)
            addFile(f);
    });

    m_addUrlBtn = new QPushButton(QStringLiteral("+ URL"), this);
    connect(m_addUrlBtn, &QPushButton::clicked, this, [this]() {
        bool ok;
        const QString url = QInputDialog::getText(this,
            QStringLiteral("Add URL"), QStringLiteral("Audio URL:"),
            QLineEdit::Normal, QString(), &ok);
        if (ok && !url.isEmpty())
            addUrl(QUrl(url));
    });

    addRow->addWidget(m_addFileBtn);
    addRow->addWidget(m_addUrlBtn);
    addRow->addStretch();
    layout->addLayout(addRow);

    // Playlist
    m_playlistWidget = new QListWidget(this);
    m_playlistWidget->setMaximumHeight(200);
    connect(m_playlistWidget, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *item) {
                Q_UNUSED(item)
                m_currentIndex = m_playlistWidget->currentRow();
                play();
            });
    layout->addWidget(m_playlistWidget, 1);
}

void AudioPlayer::setupStyles()
{
    const auto &c = ThemeEngine::instance().colors();

    QString ss;
    ss += QStringLiteral("AudioPlayer { background: ") + c["bg-darkest"] + QStringLiteral("; }\n");
    ss += QStringLiteral("QPushButton { background: ") + c["bg-mid"]
       + QStringLiteral("; color: ") + c["text-primary"]
       + QStringLiteral("; border: 1px solid ") + c["border"]
       + QStringLiteral("; border-radius: 6px; font-size: 12px; font-weight: bold; }\n");
    ss += QStringLiteral("QPushButton:hover { background: ") + c["purple-mid"]
       + QStringLiteral("; border-color: ") + c["purple-mid"] + QStringLiteral("; }\n");
    ss += QStringLiteral("QSlider::groove:horizontal { background: ") + c["bg-mid"]
       + QStringLiteral("; height: 4px; border-radius: 2px; }\n");
    ss += QStringLiteral("QSlider::handle:horizontal { background: ") + c["purple-mid"]
       + QStringLiteral("; width: 12px; height: 12px; margin: -4px 0; border-radius: 6px; }\n");
    ss += QStringLiteral("QSlider::sub-page:horizontal { background: ") + c["purple-mid"]
       + QStringLiteral("; border-radius: 2px; }\n");
    ss += QStringLiteral("QListWidget { background: ") + c["bg-dark"]
       + QStringLiteral("; color: ") + c["text-primary"]
       + QStringLiteral("; border: 1px solid ") + c["border"]
       + QStringLiteral("; border-radius: 8px; font-size: 12px; }\n");
    ss += QStringLiteral("QListWidget::item { padding: 4px 8px; border-radius: 4px; }\n");
    ss += QStringLiteral("QListWidget::item:selected { background: ") + c["selection"]
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QListWidget::item:hover:!selected { background: rgba(110,106,179,0.12); }\n");
    setStyleSheet(ss);
}

// ── Playlist management ──────────────────────────────────────────────
void AudioPlayer::addFile(const QString &filePath)
{
    PlaylistItem item;
    item.title = QFileInfo(filePath).fileName();
    item.url   = QUrl::fromLocalFile(filePath);
    m_playlist.append(item);

    m_playlistWidget->addItem(item.title);
    savePlaylist();
}

void AudioPlayer::addUrl(const QUrl &url)
{
    PlaylistItem item;
    item.title = url.fileName().isEmpty() ? url.toString() : url.fileName();
    item.url   = url;
    item.isWeb = true;
    m_playlist.append(item);

    m_playlistWidget->addItem(QStringLiteral("[Web] ") + item.title);
    savePlaylist();
}

void AudioPlayer::setWebMediaPlaying(const QString &title, const QUrl &url)
{
    // Add or update web media entry
    for (int i = 0; i < m_playlist.size(); ++i) {
        if (m_playlist[i].url == url) {
            m_currentIndex = i;
            updateDisplay();
            return;
        }
    }
    addUrl(url);
    m_currentIndex = m_playlist.size() - 1;
    m_titleLabel->setText(title);
}

// ── Playback controls ────────────────────────────────────────────────
void AudioPlayer::play()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_playlist.size()) return;

#ifdef HAS_MULTIMEDIA
    m_player->setSource(m_playlist[m_currentIndex].url);
    m_player->play();
#endif
    updateDisplay();
}

void AudioPlayer::pause()
{
#ifdef HAS_MULTIMEDIA
    m_player->pause();
#endif
    m_isPlaying = false;
    m_playBtn->setText(QStringLiteral(">"));
}

void AudioPlayer::stop()
{
#ifdef HAS_MULTIMEDIA
    m_player->stop();
#endif
    m_isPlaying = false;
    m_playBtn->setText(QStringLiteral(">"));
}

void AudioPlayer::next()
{
    if (m_playlist.isEmpty()) return;
    m_currentIndex = (m_currentIndex + 1) % m_playlist.size();
    play();
}

void AudioPlayer::previous()
{
    if (m_playlist.isEmpty()) return;
    m_currentIndex = (m_currentIndex - 1 + m_playlist.size()) % m_playlist.size();
    play();
}

void AudioPlayer::togglePlayPause()
{
    if (m_isPlaying) pause();
    else {
        if (m_currentIndex < 0 && !m_playlist.isEmpty())
            m_currentIndex = 0;
        play();
    }
}

// ── Display update ───────────────────────────────────────────────────
void AudioPlayer::updateDisplay()
{
    if (m_currentIndex >= 0 && m_currentIndex < m_playlist.size()) {
        m_titleLabel->setText(m_playlist[m_currentIndex].title);
        m_playlistWidget->setCurrentRow(m_currentIndex);
    }
}

void AudioPlayer::updateProgress()
{
#ifdef HAS_MULTIMEDIA
    const qint64 pos = m_player->position() / 1000;
    const qint64 dur = m_player->duration() / 1000;
    m_timeLabel->setText(QStringLiteral("%1:%2 / %3:%4")
        .arg(pos / 60).arg(pos % 60, 2, 10, QLatin1Char('0'))
        .arg(dur / 60).arg(dur % 60, 2, 10, QLatin1Char('0')));
#endif
}

// ── Playlist persistence ─────────────────────────────────────────────
void AudioPlayer::loadPlaylist()
{
    const QString path = QDir::homePath() + QStringLiteral("/.config/oynix/audio_playlist.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;

    auto doc = QJsonDocument::fromJson(f.readAll());
    for (const auto &val : doc.array()) {
        auto obj = val.toObject();
        PlaylistItem item;
        item.title = obj[QStringLiteral("title")].toString();
        item.url   = QUrl(obj[QStringLiteral("url")].toString());
        item.isWeb = obj[QStringLiteral("is_web")].toBool();
        m_playlist.append(item);

        const QString display = item.isWeb
            ? QStringLiteral("[Web] ") + item.title : item.title;
        m_playlistWidget->addItem(display);
    }
}

void AudioPlayer::savePlaylist()
{
    const QString dirPath = QDir::homePath() + QStringLiteral("/.config/oynix");
    QDir().mkpath(dirPath);

    QJsonArray arr;
    for (const auto &item : m_playlist) {
        QJsonObject obj;
        obj[QStringLiteral("title")]  = item.title;
        obj[QStringLiteral("url")]    = item.url.toString();
        obj[QStringLiteral("is_web")] = item.isWeb;
        arr.append(obj);
    }

    QFile f(dirPath + QStringLiteral("/audio_playlist.json"));
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(arr).toJson());
}

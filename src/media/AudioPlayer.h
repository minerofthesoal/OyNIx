#pragma once

#include <QWidget>
#include <QStringList>
#include <QUrl>

class QLabel;
class QPushButton;
class QSlider;
class QListWidget;
class QTimer;

#ifdef HAS_MULTIMEDIA
class QMediaPlayer;
class QAudioOutput;
#endif

/**
 * AudioPlayer — Built-in audio player with playlist management.
 * Plays local files and detected web media.
 * Uses Qt Multimedia when available, otherwise displays placeholder.
 */
class AudioPlayer : public QWidget
{
    Q_OBJECT

public:
    explicit AudioPlayer(QWidget *parent = nullptr);
    ~AudioPlayer() override;

    void addFile(const QString &filePath);
    void addUrl(const QUrl &url);
    void setWebMediaPlaying(const QString &title, const QUrl &url);

signals:
    void playbackStateChanged(bool playing);

public slots:
    void play();
    void pause();
    void stop();
    void next();
    void previous();
    void togglePlayPause();

private:
    void setupUi();
    void setupStyles();
    void loadPlaylist();
    void savePlaylist();
    void updateDisplay();
    void updateProgress();

    struct PlaylistItem {
        QString title;
        QUrl    url;
        bool    isWeb = false;
    };

    // UI
    QLabel      *m_titleLabel    = nullptr;
    QLabel      *m_artistLabel   = nullptr;
    QLabel      *m_timeLabel     = nullptr;
    QSlider     *m_progressSlider = nullptr;
    QSlider     *m_volumeSlider  = nullptr;
    QPushButton *m_prevBtn       = nullptr;
    QPushButton *m_playBtn       = nullptr;
    QPushButton *m_nextBtn       = nullptr;
    QPushButton *m_addFileBtn    = nullptr;
    QPushButton *m_addUrlBtn     = nullptr;
    QListWidget *m_playlistWidget = nullptr;

    // State
    QList<PlaylistItem> m_playlist;
    int  m_currentIndex = -1;
    bool m_isPlaying    = false;

#ifdef HAS_MULTIMEDIA
    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audioOutput = nullptr;
#endif
    QTimer *m_progressTimer = nullptr;
};

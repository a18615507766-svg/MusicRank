#pragma once

#include "domain/Song.h"

#include <QList>
#include <QMainWindow>

class MusicDatabase;
class QAudioOutput;
class QComboBox;
class QLabel;
class QLineEdit;
class QMediaPlayer;
class QPushButton;
class QSlider;
class QTableWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(
        MusicDatabase &database,
        const QString &appDir,
        QWidget *parent = nullptr);

    static QString formatTime(qint64 milliseconds);
    static QString resolveAudioPath(const QString &audioPath, const QString &appDir);

private:
    void buildUi();
    void connectUi();
    void loadPlaylists(qint64 selectedId = 0);
    void refresh(qint64 selectedSongId = 0);
    void fillTable();
    qint64 selectedSongId() const;
    int rowForSong(qint64 songId) const;
    SongSort selectedSort() const;
    SortDirection selectedDirection() const;
    void addSong();
    void editSong();
    void deleteSong();
    void createPlaylist();
    void addToCurrentPlaylist();
    void reactLike();
    void reactDislike();
    void addCoin();
    void playSelected();
    void playRow(int row);
    void playRelative(int offset);
    QString audioFilePath(const Song &song) const;
    void updatePlayerUi();
    void showDatabaseError(const QString &context);

    MusicDatabase &database_;
    QString appDir_;
    QList<Song> songs_;
    qint64 currentSongId_ = 0;
    int currentQueueRow_ = -1;
    bool seeking_ = false;
    bool pendingAutoplay_ = false;

    QLineEdit *searchEdit_ = nullptr;
    QComboBox *sortCombo_ = nullptr;
    QComboBox *directionCombo_ = nullptr;
    QComboBox *playlistCombo_ = nullptr;
    QTableWidget *table_ = nullptr;
    QPushButton *likeButton_ = nullptr;
    QPushButton *dislikeButton_ = nullptr;
    QPushButton *coinButton_ = nullptr;
    QLabel *currentSongLabel_ = nullptr;
    QPushButton *playPauseButton_ = nullptr;
    QSlider *positionSlider_ = nullptr;
    QLabel *timeLabel_ = nullptr;
    QSlider *volumeSlider_ = nullptr;
    QMediaPlayer *player_ = nullptr;
    QAudioOutput *audioOutput_ = nullptr;
};

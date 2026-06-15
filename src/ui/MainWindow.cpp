#include "ui/MainWindow.h"

#include "database/MusicDatabase.h"
#include "domain/Playlist.h"
#include "ui/SongDialog.h"

#include <QAbstractItemView>
#include <QAudioOutput>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <climits>
#include <optional>

namespace
{
QString text(const char *value)
{
    return QString::fromUtf8(value);
}
}

MainWindow::MainWindow(
    MusicDatabase &database,
    const QString &appDir,
    QWidget *parent)
    : QMainWindow(parent),
      database_(database),
      appDir_(QDir(appDir).absolutePath())
{
    setWindowTitle(text(u8"歌曲排行榜"));
    setMinimumSize(1100, 700);
    buildUi();
    connectUi();
    loadPlaylists();
    refresh();
}

QString MainWindow::formatTime(qint64 milliseconds)
{
    const qint64 totalSeconds = std::max<qint64>(0, milliseconds / 1000);
    const qint64 minutes = totalSeconds / 60;
    const qint64 seconds = totalSeconds % 60;
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(16, 16, 16, 12);
    mainLayout->setSpacing(10);

    auto *toolbar = new QHBoxLayout;
    searchEdit_ = new QLineEdit(central);
    searchEdit_->setObjectName(QStringLiteral("globalSearch"));
    searchEdit_->setPlaceholderText(text(u8"搜索歌名、演唱者、作词、作曲或专辑"));
    searchEdit_->setClearButtonEnabled(true);

    sortCombo_ = new QComboBox(central);
    const QList<QPair<QString, int>> sorts = {
        {text(u8"综合热度"), static_cast<int>(SongSort::Heat)},
        {text(u8"播放"), static_cast<int>(SongSort::PlayCount)},
        {text(u8"点赞"), static_cast<int>(SongSort::LikeCount)},
        {text(u8"投币"), static_cast<int>(SongSort::CoinCount)},
        {text(u8"发行日期"), static_cast<int>(SongSort::ReleaseDate)},
        {text(u8"歌名"), static_cast<int>(SongSort::Title)},
        {text(u8"演唱者"), static_cast<int>(SongSort::Performer)}
    };
    for (const auto &entry : sorts) {
        sortCombo_->addItem(entry.first, entry.second);
    }

    directionCombo_ = new QComboBox(central);
    directionCombo_->addItem(text(u8"降序"), static_cast<int>(SortDirection::Descending));
    directionCombo_->addItem(text(u8"升序"), static_cast<int>(SortDirection::Ascending));

    playlistCombo_ = new QComboBox(central);
    playlistCombo_->setMinimumWidth(140);
    auto *newPlaylistButton = new QPushButton(text(u8"新建歌单"), central);
    auto *refreshButton = new QPushButton(text(u8"刷新"), central);
    auto *addButton = new QPushButton(text(u8"新增"), central);
    auto *editButton = new QPushButton(text(u8"编辑"), central);
    auto *deleteButton = new QPushButton(text(u8"删除"), central);

    toolbar->addWidget(searchEdit_, 1);
    toolbar->addWidget(sortCombo_);
    toolbar->addWidget(directionCombo_);
    toolbar->addWidget(playlistCombo_);
    toolbar->addWidget(newPlaylistButton);
    toolbar->addWidget(refreshButton);
    toolbar->addWidget(addButton);
    toolbar->addWidget(editButton);
    toolbar->addWidget(deleteButton);
    mainLayout->addLayout(toolbar);

    table_ = new QTableWidget(central);
    table_->setObjectName(QStringLiteral("rankingTable"));
    table_->setColumnCount(9);
    table_->setHorizontalHeaderLabels({
        text(u8"排名"), text(u8"歌名"), text(u8"演唱者"), text(u8"风格"),
        text(u8"发行日期"), text(u8"播放"), text(u8"点赞"), text(u8"踩"),
        text(u8"投币")
    });
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    mainLayout->addWidget(table_, 1);

    auto *actions = new QHBoxLayout;
    auto *playButton = new QPushButton(text(u8"播放"), central);
    likeButton_ = new QPushButton(text(u8"点赞"), central);
    likeButton_->setObjectName(QStringLiteral("likeButton"));
    dislikeButton_ = new QPushButton(text(u8"踩"), central);
    dislikeButton_->setObjectName(QStringLiteral("dislikeButton"));
    coinButton_ = new QPushButton(text(u8"投币"), central);
    coinButton_->setObjectName(QStringLiteral("coinButton"));
    auto *addToPlaylistButton = new QPushButton(text(u8"加入当前歌单"), central);
    actions->addWidget(playButton);
    actions->addWidget(likeButton_);
    actions->addWidget(dislikeButton_);
    actions->addWidget(coinButton_);
    actions->addWidget(addToPlaylistButton);
    actions->addStretch();
    mainLayout->addLayout(actions);

    auto *playerBar = new QWidget(central);
    playerBar->setObjectName(QStringLiteral("playerBar"));
    auto *playerLayout = new QHBoxLayout(playerBar);
    playerLayout->setContentsMargins(10, 8, 10, 8);
    currentSongLabel_ = new QLabel(text(u8"当前未播放"), playerBar);
    currentSongLabel_->setMinimumWidth(180);
    auto *previousButton = new QPushButton(text(u8"上一首"), playerBar);
    playPauseButton_ = new QPushButton(text(u8"播放"), playerBar);
    auto *nextButton = new QPushButton(text(u8"下一首"), playerBar);
    positionSlider_ = new QSlider(Qt::Horizontal, playerBar);
    positionSlider_->setRange(0, 0);
    timeLabel_ = new QLabel(QStringLiteral("00:00 / 00:00"), playerBar);
    volumeSlider_ = new QSlider(Qt::Horizontal, playerBar);
    volumeSlider_->setRange(0, 100);
    volumeSlider_->setValue(70);
    volumeSlider_->setMaximumWidth(110);
    playerLayout->addWidget(currentSongLabel_);
    playerLayout->addWidget(previousButton);
    playerLayout->addWidget(playPauseButton_);
    playerLayout->addWidget(nextButton);
    playerLayout->addWidget(positionSlider_, 1);
    playerLayout->addWidget(timeLabel_);
    playerLayout->addWidget(new QLabel(text(u8"音量"), playerBar));
    playerLayout->addWidget(volumeSlider_);
    mainLayout->addWidget(playerBar);

    player_ = new QMediaPlayer(this);
    audioOutput_ = new QAudioOutput(this);
    audioOutput_->setVolume(0.7f);
    player_->setAudioOutput(audioOutput_);

    setCentralWidget(central);
    setStyleSheet(QStringLiteral(
        "QMainWindow, QWidget { background: #f6f7fb; color: #202124; }"
        "QLineEdit, QComboBox, QTableWidget { background: white; border: 1px solid #d9dce5;"
        " border-radius: 5px; padding: 5px; }"
        "QPushButton { background: #6558d3; color: white; border: none;"
        " border-radius: 5px; padding: 7px 12px; }"
        "QPushButton:hover { background: #5548c5; }"
        "QHeaderView::section { background: #eceefa; padding: 7px; border: none; }"
        "#playerBar { background: white; border: 1px solid #d9dce5; border-radius: 7px; }"));

    connect(newPlaylistButton, &QPushButton::clicked, this, &MainWindow::createPlaylist);
    connect(refreshButton, &QPushButton::clicked, this, [this] { refresh(selectedSongId()); });
    connect(addButton, &QPushButton::clicked, this, &MainWindow::addSong);
    connect(editButton, &QPushButton::clicked, this, &MainWindow::editSong);
    connect(deleteButton, &QPushButton::clicked, this, &MainWindow::deleteSong);
    connect(playButton, &QPushButton::clicked, this, &MainWindow::playSelected);
    connect(addToPlaylistButton, &QPushButton::clicked, this, &MainWindow::addToCurrentPlaylist);
    connect(previousButton, &QPushButton::clicked, this, [this] { playRelative(-1); });
    connect(nextButton, &QPushButton::clicked, this, [this] { playRelative(1); });
}

void MainWindow::connectUi()
{
    connect(searchEdit_, &QLineEdit::textChanged, this, [this] { refresh(); });
    connect(sortCombo_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(directionCombo_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(playlistCombo_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        playRow(row);
    });
    connect(likeButton_, &QPushButton::clicked, this, &MainWindow::reactLike);
    connect(dislikeButton_, &QPushButton::clicked, this, &MainWindow::reactDislike);
    connect(coinButton_, &QPushButton::clicked, this, &MainWindow::addCoin);
    connect(playPauseButton_, &QPushButton::clicked, this, [this] {
        if (player_->playbackState() == QMediaPlayer::PlayingState) {
            player_->pause();
        } else if (currentSongId_ != 0) {
            player_->play();
        } else {
            playSelected();
        }
    });
    connect(player_, &QMediaPlayer::playbackStateChanged, this, [this] {
        updatePlayerUi();
    });
    connect(player_, &QMediaPlayer::durationChanged, this, [this](qint64 duration) {
        positionSlider_->setRange(0, static_cast<int>(std::min<qint64>(duration, INT_MAX)));
        updatePlayerUi();
    });
    connect(player_, &QMediaPlayer::positionChanged, this, [this](qint64 position) {
        if (!seeking_) {
            positionSlider_->setValue(static_cast<int>(std::min<qint64>(position, INT_MAX)));
        }
        updatePlayerUi();
    });
    connect(positionSlider_, &QSlider::sliderPressed, this, [this] { seeking_ = true; });
    connect(positionSlider_, &QSlider::sliderReleased, this, [this] {
        seeking_ = false;
        player_->setPosition(positionSlider_->value());
    });
    connect(volumeSlider_, &QSlider::valueChanged, this, [this](int value) {
        audioOutput_->setVolume(value / 100.0f);
    });
    connect(player_, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia) {
            playRelative(1);
        }
    });
    connect(player_, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &message) {
                statusBar()->showMessage(text(u8"播放错误：") + message, 8000);
            });
}

void MainWindow::loadPlaylists(qint64 selectedId)
{
    playlistCombo_->blockSignals(true);
    playlistCombo_->clear();
    const QList<Playlist> playlists = database_.playlists();
    for (const Playlist &playlist : playlists) {
        QString displayName = playlist.name;
        if (playlist.isSystem) {
            displayName = text(u8"全部歌曲");
        }
        playlistCombo_->addItem(displayName, playlist.id);
        playlistCombo_->setItemData(
            playlistCombo_->count() - 1, playlist.isSystem, Qt::UserRole + 1);
    }
    const int selectedIndex = playlistCombo_->findData(selectedId);
    playlistCombo_->setCurrentIndex(selectedIndex >= 0 ? selectedIndex : 0);
    playlistCombo_->blockSignals(false);
}

void MainWindow::refresh(qint64 selectedId)
{
    if (selectedId == 0) {
        selectedId = selectedSongId();
    }
    std::optional<qint64> playlistId;
    if (playlistCombo_->currentIndex() >= 0
        && !playlistCombo_->currentData(Qt::UserRole + 1).toBool()) {
        playlistId = playlistCombo_->currentData().toLongLong();
    }
    songs_ = database_.listSongs(
        searchEdit_->text().trimmed(), selectedSort(), selectedDirection(), playlistId);
    fillTable();
    const int selectedRow = rowForSong(selectedId);
    if (selectedRow >= 0) {
        table_->selectRow(selectedRow);
    } else if (!songs_.isEmpty()) {
        table_->selectRow(0);
    }
    if (!database_.lastError().isEmpty()) {
        showDatabaseError(text(u8"刷新失败"));
    }
}

void MainWindow::fillTable()
{
    table_->setRowCount(songs_.size());
    for (int row = 0; row < songs_.size(); ++row) {
        const Song &song = songs_.at(row);
        const QStringList values = {
            QString::number(row + 1),
            song.title,
            song.performerText(),
            song.genre,
            song.releaseDate.isValid() ? song.releaseDate.toString(Qt::ISODate) : QString(),
            QString::number(song.playCount),
            QString::number(song.likeCount),
            QString::number(song.dislikeCount),
            QString::number(song.coinCount)
        };
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values.at(column));
            item->setData(Qt::UserRole, song.id);
            if (column == 0 || column >= 5) {
                item->setTextAlignment(Qt::AlignCenter);
            }
            table_->setItem(row, column, item);
        }
    }
}

qint64 MainWindow::selectedSongId() const
{
    const int row = table_->currentRow();
    return row >= 0 && row < songs_.size() ? songs_.at(row).id : 0;
}

int MainWindow::rowForSong(qint64 songId) const
{
    for (int row = 0; row < songs_.size(); ++row) {
        if (songs_.at(row).id == songId) {
            return row;
        }
    }
    return -1;
}

SongSort MainWindow::selectedSort() const
{
    return static_cast<SongSort>(sortCombo_->currentData().toInt());
}

SortDirection MainWindow::selectedDirection() const
{
    return static_cast<SortDirection>(directionCombo_->currentData().toInt());
}

void MainWindow::addSong()
{
    SongDialog dialog(appDir_, this);
    dialog.setWindowTitle(text(u8"新增歌曲"));
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const qint64 id = database_.addSong(dialog.song());
    if (id == 0) {
        showDatabaseError(text(u8"新增失败"));
        return;
    }
    refresh(id);
}

void MainWindow::editSong()
{
    const qint64 id = selectedSongId();
    const std::optional<Song> existing = database_.song(id);
    if (!existing) {
        QMessageBox::information(this, text(u8"编辑歌曲"), text(u8"请先选择一首歌曲。"));
        return;
    }
    SongDialog dialog(appDir_, this);
    dialog.setWindowTitle(text(u8"编辑歌曲"));
    dialog.setSong(*existing);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (!database_.updateSong(dialog.song())) {
        showDatabaseError(text(u8"编辑失败"));
        return;
    }
    refresh(id);
}

void MainWindow::deleteSong()
{
    const qint64 id = selectedSongId();
    if (id == 0) {
        QMessageBox::information(this, text(u8"删除歌曲"), text(u8"请先选择一首歌曲。"));
        return;
    }
    const auto answer = QMessageBox::question(
        this,
        text(u8"删除歌曲"),
        text(u8"确定删除所选歌曲的数据库记录吗？\n不会删除 MP3 文件。"));
    if (answer != QMessageBox::Yes) {
        return;
    }
    if (!database_.removeSong(id)) {
        showDatabaseError(text(u8"删除失败"));
        return;
    }
    if (currentSongId_ == id) {
        player_->stop();
        currentSongId_ = 0;
        currentQueueRow_ = -1;
        currentSongLabel_->setText(text(u8"当前未播放"));
    }
    refresh();
}

void MainWindow::createPlaylist()
{
    bool accepted = false;
    const QString name = QInputDialog::getText(
        this, text(u8"新建歌单"), text(u8"歌单名称"), QLineEdit::Normal, {}, &accepted);
    if (!accepted || name.trimmed().isEmpty()) {
        return;
    }
    const qint64 id = database_.createPlaylist(name);
    if (id == 0) {
        showDatabaseError(text(u8"新建歌单失败"));
        return;
    }
    loadPlaylists(id);
    refresh();
}

void MainWindow::addToCurrentPlaylist()
{
    const qint64 songId = selectedSongId();
    if (songId == 0) {
        QMessageBox::information(this, text(u8"加入歌单"), text(u8"请先选择一首歌曲。"));
        return;
    }
    if (playlistCombo_->currentData(Qt::UserRole + 1).toBool()) {
        QMessageBox::information(
            this, text(u8"加入歌单"), text(u8"“全部歌曲”不能手动添加歌曲。"));
        return;
    }
    if (!database_.addSongToPlaylist(
            playlistCombo_->currentData().toLongLong(), songId)) {
        showDatabaseError(text(u8"加入歌单失败"));
        return;
    }
    statusBar()->showMessage(text(u8"已加入当前歌单"), 3000);
    refresh(songId);
}

void MainWindow::reactLike()
{
    const qint64 id = selectedSongId();
    if (id != 0 && database_.toggleLike(id)) {
        refresh(id);
    } else if (id != 0) {
        showDatabaseError(text(u8"点赞失败"));
    }
}

void MainWindow::reactDislike()
{
    const qint64 id = selectedSongId();
    if (id != 0 && database_.toggleDislike(id)) {
        refresh(id);
    } else if (id != 0) {
        showDatabaseError(text(u8"操作失败"));
    }
}

void MainWindow::addCoin()
{
    const qint64 id = selectedSongId();
    if (id != 0 && database_.addCoin(id)) {
        refresh(id);
    } else if (id != 0) {
        showDatabaseError(text(u8"投币失败"));
    }
}

void MainWindow::playSelected()
{
    playRow(table_->currentRow());
}

void MainWindow::playRow(int row)
{
    if (row < 0 || row >= songs_.size()) {
        QMessageBox::information(this, text(u8"播放"), text(u8"请先选择一首歌曲。"));
        return;
    }
    const Song song = songs_.at(row);
    if (song.id == currentSongId_) {
        player_->play();
        return;
    }

    const QString path = audioFilePath(song);
    if (!QFileInfo::exists(path)) {
        statusBar()->showMessage(text(u8"MP3 文件不存在：") + path, 8000);
        return;
    }
    currentSongId_ = song.id;
    currentQueueRow_ = row;
    currentSongLabel_->setText(song.title + QStringLiteral(" - ") + song.performerText());
    player_->setSource(QUrl::fromLocalFile(path));
    if (!database_.incrementPlay(song.id)) {
        showDatabaseError(text(u8"播放计数更新失败"));
    }
    player_->play();
    refresh(song.id);
}

void MainWindow::playRelative(int offset)
{
    if (songs_.isEmpty()) {
        return;
    }
    int row = rowForSong(currentSongId_);
    if (row < 0) {
        row = currentQueueRow_ >= 0 && currentQueueRow_ < songs_.size()
            ? currentQueueRow_
            : 0;
    }
    row = (row + offset + songs_.size()) % songs_.size();
    table_->selectRow(row);
    if (songs_.at(row).id == currentSongId_ && songs_.size() == 1) {
        player_->setPosition(0);
        player_->play();
        return;
    }
    playRow(row);
}

QString MainWindow::audioFilePath(const Song &song) const
{
    return QDir::isAbsolutePath(song.audioPath)
        ? QDir::cleanPath(song.audioPath)
        : QDir(appDir_).absoluteFilePath(song.audioPath);
}

void MainWindow::updatePlayerUi()
{
    playPauseButton_->setText(
        player_->playbackState() == QMediaPlayer::PlayingState
            ? text(u8"暂停")
            : text(u8"播放"));
    timeLabel_->setText(
        formatTime(player_->position()) + QStringLiteral(" / ") + formatTime(player_->duration()));
}

void MainWindow::showDatabaseError(const QString &context)
{
    QMessageBox::warning(
        this, context, context + QStringLiteral("：") + database_.lastError());
}

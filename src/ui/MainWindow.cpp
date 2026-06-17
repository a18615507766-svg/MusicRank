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
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <climits>
#include <optional>

namespace
{
constexpr int kMaxAutoplayRetries = 8;
constexpr int kAutoplayRetryIntervalMs = 450;

QString text(const char *value)
{
    return QString::fromUtf8(value);
}

QString styleUrlPath(QString path)
{
    return QDir::cleanPath(path).replace(QLatin1Char('\\'), QLatin1Char('/'));
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
    loadGenres();
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

QString MainWindow::resolveAudioPath(const QString &audioPath, const QString &appDir)
{
    const QString trimmed = audioPath.trimmed();
    if (QDir::isAbsolutePath(trimmed)) {
        return QDir::cleanPath(trimmed);
    }

    QDir searchDir(appDir);
    QString firstCandidate;
    for (int depth = 0; depth <= 6; ++depth) {
        const QString candidate = QDir::cleanPath(searchDir.absoluteFilePath(trimmed));
        if (firstCandidate.isEmpty()) {
            firstCandidate = candidate;
        }
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
        if (!searchDir.cdUp()) {
            break;
        }
    }
    return firstCandidate;
}

QString MainWindow::resolveBackgroundImagePath(const QString &appDir)
{
    const QStringList names = {
        QStringLiteral("background.png"),
        QStringLiteral("background.jpg"),
        QStringLiteral("background.jpeg"),
        QStringLiteral("background.bmp")
    };
    const QStringList folders = {
        QString(),
        QStringLiteral("assets"),
        QStringLiteral("data")
    };

    const QDir directory(appDir);
    for (const QString &folder : folders) {
        const QDir candidateDir(folder.isEmpty()
            ? directory.absolutePath()
            : directory.absoluteFilePath(folder));
        for (const QString &name : names) {
            const QString candidate = QDir::cleanPath(candidateDir.absoluteFilePath(name));
            if (QFileInfo::exists(candidate)) {
                return candidate;
            }
        }
    }
    return {};
}

bool MainWindow::shouldRetryAutoplay(
    QMediaPlayer::PlaybackState,
    qint64 position,
    int attempts,
    int maxAttempts)
{
    return position <= 0 && attempts < maxAttempts;
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("mainSurface"));
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

    genreCombo_ = new QComboBox(central);
    genreCombo_->setObjectName(QStringLiteral("genreFilter"));
    genreCombo_->setMinimumWidth(120);

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
    toolbar->addWidget(genreCombo_);
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
    autoplayRetryTimer_ = new QTimer(this);
    autoplayRetryTimer_->setInterval(kAutoplayRetryIntervalMs);

    setCentralWidget(central);
    const QString backgroundImage = resolveBackgroundImagePath(appDir_);
    const QString mainBackground = backgroundImage.isEmpty()
        ? QStringLiteral(
            "#mainSurface {"
            " background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
            " stop:0 #dfe9f3, stop:0.45 #ffffff, stop:1 #d7e1ec);"
            "}")
        : QStringLiteral(
            "#mainSurface {"
            " border-image: url(\"%1\") 0 0 0 0 stretch stretch;"
            "}").arg(styleUrlPath(backgroundImage));
    setStyleSheet(mainBackground + QStringLiteral(
        "QWidget { color: #1f2433; font-family: \"Microsoft YaHei UI\", \"Segoe UI\"; font-size: 13px; }"
        "QLineEdit, QComboBox { background: rgba(255, 255, 255, 155);"
        " border: 1px solid rgba(255, 255, 255, 170); border-radius: 12px;"
        " padding: 8px 10px; selection-background-color: #6b7cff; }"
        "QLineEdit:focus, QComboBox:focus { border: 1px solid rgba(77, 103, 255, 210); }"
        "QTableWidget#rankingTable { background: rgba(255, 255, 255, 118);"
        " alternate-background-color: rgba(244, 247, 255, 72);"
        " border: 1px solid rgba(255, 255, 255, 145); border-radius: 16px;"
        " gridline-color: rgba(110, 125, 160, 50); padding: 8px; }"
        "QTableWidget#rankingTable::item { background: rgba(255, 255, 255, 48); padding: 7px; border-radius: 7px; }"
        "QTableWidget#rankingTable::item:selected { background: rgba(76, 100, 255, 170); color: white; }"
        "QHeaderView::section { background: rgba(244, 247, 255, 145); color: #26324d;"
        " padding: 9px; border: none; border-bottom: 1px solid rgba(150, 160, 190, 75); font-weight: 600; }"
        "QPushButton { background: rgba(58, 82, 210, 218); color: white; border: none;"
        " border-radius: 12px; padding: 8px 14px; font-weight: 600; }"
        "QPushButton:hover { background: rgba(40, 62, 185, 235); }"
        "QPushButton:pressed { background: rgba(30, 46, 150, 245); }"
        "#playerBar { background: rgba(255, 255, 255, 125);"
        " border: 1px solid rgba(255, 255, 255, 165); border-radius: 18px; }"
        "QSlider::groove:horizontal { height: 8px; background: rgba(88, 99, 130, 55); border-radius: 4px; }"
        "QSlider::sub-page:horizontal { background: rgba(58, 82, 210, 210); border-radius: 4px; }"
        "QSlider::handle:horizontal { background: white; border: 2px solid rgba(58, 82, 210, 220);"
        " width: 16px; margin: -5px 0; border-radius: 8px; }"
        "QStatusBar { background: rgba(255, 255, 255, 130); }"));

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
    connect(genreCombo_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
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
        if (pendingAutoplay_ && position > 0) {
            stopAutoplayRetry();
        }
        updatePlayerUi();
    });
    connect(positionSlider_, &QSlider::sliderPressed, this, [this] { seeking_ = true; });
    connect(positionSlider_, &QSlider::sliderReleased, this, [this] {
        seeking_ = false;
        player_->setPosition(positionSlider_->value());
        if (player_->playbackState() != QMediaPlayer::PlayingState && currentSongId_ != 0) {
            player_->play();
        }
    });
    connect(volumeSlider_, &QSlider::valueChanged, this, [this](int value) {
        audioOutput_->setVolume(value / 100.0f);
    });
    connect(player_, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia) {
            stopAutoplayRetry();
            playRelative(1);
        } else if (pendingAutoplay_
                   && (status == QMediaPlayer::LoadedMedia
                       || status == QMediaPlayer::BufferedMedia)) {
            retryAutoplay();
        } else if (status == QMediaPlayer::InvalidMedia) {
            stopAutoplayRetry();
        }
    });
    connect(player_, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &message) {
                stopAutoplayRetry();
                statusBar()->showMessage(text(u8"播放错误：") + message, 8000);
            });
    connect(autoplayRetryTimer_, &QTimer::timeout, this, &MainWindow::retryAutoplay);
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

void MainWindow::loadGenres(const QString &selectedGenre)
{
    const QString keepGenre = selectedGenre.isEmpty() ? this->selectedGenre() : selectedGenre;
    genreCombo_->blockSignals(true);
    genreCombo_->clear();
    genreCombo_->addItem(text(u8"全部分类"), QString());
    for (const QString &genre : database_.genres()) {
        genreCombo_->addItem(genre, genre);
    }
    const int selectedIndex = genreCombo_->findData(keepGenre);
    genreCombo_->setCurrentIndex(selectedIndex >= 0 ? selectedIndex : 0);
    genreCombo_->blockSignals(false);
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
        searchEdit_->text().trimmed(), selectedSort(), selectedDirection(), playlistId, selectedGenre());
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

QString MainWindow::selectedGenre() const
{
    return genreCombo_->currentData().toString();
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
    loadGenres(dialog.song().genre);
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
    loadGenres(dialog.song().genre);
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
        stopAutoplayRetry();
        currentSongId_ = 0;
        currentQueueRow_ = -1;
        currentSongLabel_->setText(text(u8"当前未播放"));
    }
    loadGenres();
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
        startAutoplayRetry();
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
    player_->stop();
    player_->setSource(QUrl::fromLocalFile(path));
    player_->setPosition(0);
    startAutoplayRetry();
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
    return resolveAudioPath(song.audioPath, appDir_);
}

void MainWindow::startAutoplayRetry()
{
    pendingAutoplay_ = true;
    autoplayRetryCount_ = 0;
    if (autoplayRetryTimer_ != nullptr) {
        autoplayRetryTimer_->start();
    }
}

void MainWindow::stopAutoplayRetry()
{
    pendingAutoplay_ = false;
    autoplayRetryCount_ = 0;
    if (autoplayRetryTimer_ != nullptr) {
        autoplayRetryTimer_->stop();
    }
}

void MainWindow::retryAutoplay()
{
    if (!pendingAutoplay_) {
        return;
    }
    if (!shouldRetryAutoplay(
            player_->playbackState(),
            player_->position(),
            autoplayRetryCount_,
            kMaxAutoplayRetries)) {
        stopAutoplayRetry();
        return;
    }

    ++autoplayRetryCount_;
    const qint64 duration = player_->duration();
    const qint64 nudgePosition = duration > 1000 ? 100 : 0;
    player_->setPosition(nudgePosition);
    player_->play();
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

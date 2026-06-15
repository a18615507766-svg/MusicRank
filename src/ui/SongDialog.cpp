#include "ui/SongDialog.h"

#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

SongDialog::SongDialog(const QString &appDir, QWidget *parent)
    : QDialog(parent), appDir_(QDir(appDir).absolutePath())
{
    setWindowTitle(QString::fromUtf8(u8"歌曲信息"));
    setMinimumWidth(560);

    titleEdit_ = new QLineEdit(this);
    performersEdit_ = new QLineEdit(this);
    performersEdit_->setPlaceholderText(QString::fromUtf8(u8"多位演唱者用“、”分隔"));
    lyricistEdit_ = new QLineEdit(this);
    composerEdit_ = new QLineEdit(this);
    albumEdit_ = new QLineEdit(this);
    releaseDateEdit_ = new QDateEdit(QDate::currentDate(), this);
    releaseDateEdit_->setCalendarPopup(true);
    releaseDateEdit_->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    genreEdit_ = new QLineEdit(this);
    languageEdit_ = new QLineEdit(this);
    audioPathEdit_ = new QLineEdit(this);

    auto *browseButton = new QPushButton(QString::fromUtf8(u8"选择 MP3"), this);
    auto *audioLayout = new QHBoxLayout;
    audioLayout->addWidget(audioPathEdit_, 1);
    audioLayout->addWidget(browseButton);

    auto *form = new QFormLayout;
    form->addRow(QString::fromUtf8(u8"歌名 *"), titleEdit_);
    form->addRow(QString::fromUtf8(u8"演唱者 *"), performersEdit_);
    form->addRow(QString::fromUtf8(u8"作词"), lyricistEdit_);
    form->addRow(QString::fromUtf8(u8"作曲"), composerEdit_);
    form->addRow(QString::fromUtf8(u8"专辑"), albumEdit_);
    form->addRow(QString::fromUtf8(u8"发行日期"), releaseDateEdit_);
    form->addRow(QString::fromUtf8(u8"风格"), genreEdit_);
    form->addRow(QString::fromUtf8(u8"语言"), languageEdit_);
    form->addRow(QString::fromUtf8(u8"MP3 路径 *"), audioLayout);

    buttons_ = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons_->button(QDialogButtonBox::Ok)->setText(QString::fromUtf8(u8"保存"));
    buttons_->button(QDialogButtonBox::Cancel)->setText(QString::fromUtf8(u8"取消"));

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons_);

    connect(browseButton, &QPushButton::clicked, this, &SongDialog::chooseAudioFile);
    connect(buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(titleEdit_, &QLineEdit::textChanged, this, &SongDialog::validate);
    connect(performersEdit_, &QLineEdit::textChanged, this, &SongDialog::validate);
    connect(audioPathEdit_, &QLineEdit::textChanged, this, &SongDialog::validate);
    validate();
}

void SongDialog::setSong(const Song &song)
{
    songId_ = song.id;
    durationMs_ = song.durationMs;
    playCount_ = song.playCount;
    likeCount_ = song.likeCount;
    dislikeCount_ = song.dislikeCount;
    coinCount_ = song.coinCount;
    titleEdit_->setText(song.title);
    performersEdit_->setText(song.performerText());
    lyricistEdit_->setText(song.lyricist);
    composerEdit_->setText(song.composer);
    albumEdit_->setText(song.album);
    releaseDateEdit_->setDate(song.releaseDate.isValid()
                                  ? song.releaseDate
                                  : QDate::currentDate());
    genreEdit_->setText(song.genre);
    languageEdit_->setText(song.language);
    audioPathEdit_->setText(song.audioPath);
}

Song SongDialog::song() const
{
    Song result;
    result.id = songId_;
    result.title = titleEdit_->text().trimmed();
    result.performers = performersEdit_->text().split(
        QString::fromUtf8(u8"、"), Qt::SkipEmptyParts);
    for (QString &performer : result.performers) {
        performer = performer.trimmed();
    }
    result.lyricist = lyricistEdit_->text().trimmed();
    result.composer = composerEdit_->text().trimmed();
    result.album = albumEdit_->text().trimmed();
    result.releaseDate = releaseDateEdit_->date();
    result.genre = genreEdit_->text().trimmed();
    result.language = languageEdit_->text().trimmed();
    result.durationMs = durationMs_;
    result.audioPath = storedAudioPath();
    result.playCount = playCount_;
    result.likeCount = likeCount_;
    result.dislikeCount = dislikeCount_;
    result.coinCount = coinCount_;
    return result;
}

void SongDialog::chooseAudioFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QString::fromUtf8(u8"选择 MP3 文件"),
        appDir_,
        QString::fromUtf8(u8"MP3 音频 (*.mp3);;所有文件 (*)"));
    if (!path.isEmpty()) {
        audioPathEdit_->setText(QDir::toNativeSeparators(path));
    }
}

void SongDialog::validate()
{
    const bool valid = !titleEdit_->text().trimmed().isEmpty()
        && !performersEdit_->text().trimmed().isEmpty()
        && !audioPathEdit_->text().trimmed().isEmpty();
    buttons_->button(QDialogButtonBox::Ok)->setEnabled(valid);
}

QString SongDialog::storedAudioPath() const
{
    const QString entered = QDir::fromNativeSeparators(audioPathEdit_->text().trimmed());
    if (entered.isEmpty() || QDir::isRelativePath(entered)) {
        return entered;
    }

    const QString absolute = QFileInfo(entered).absoluteFilePath();
    const QString relative = QDir(appDir_).relativeFilePath(absolute);
    if (relative != QStringLiteral("..")
        && !relative.startsWith(QStringLiteral("../"))) {
        return QDir::fromNativeSeparators(relative);
    }
    return absolute;
}

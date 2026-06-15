#pragma once

#include "domain/Song.h"

#include <QDialog>

class QDateEdit;
class QDialogButtonBox;
class QLineEdit;

class SongDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SongDialog(const QString &appDir, QWidget *parent = nullptr);

    void setSong(const Song &song);
    Song song() const;

private:
    void chooseAudioFile();
    void validate();
    QString storedAudioPath() const;

    QString appDir_;
    qint64 songId_ = 0;
    qint64 durationMs_ = 0;
    qint64 playCount_ = 0;
    qint64 likeCount_ = 0;
    qint64 dislikeCount_ = 0;
    qint64 coinCount_ = 0;
    QLineEdit *titleEdit_ = nullptr;
    QLineEdit *performersEdit_ = nullptr;
    QLineEdit *lyricistEdit_ = nullptr;
    QLineEdit *composerEdit_ = nullptr;
    QLineEdit *albumEdit_ = nullptr;
    QDateEdit *releaseDateEdit_ = nullptr;
    QLineEdit *genreEdit_ = nullptr;
    QLineEdit *languageEdit_ = nullptr;
    QLineEdit *audioPathEdit_ = nullptr;
    QDialogButtonBox *buttons_ = nullptr;
};

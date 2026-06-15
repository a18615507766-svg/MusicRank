#pragma once

#include <QDate>
#include <QString>
#include <QStringList>
#include <QtTypes>

struct Song
{
    qint64 id = 0;
    QString title;
    QStringList performers;
    QString lyricist;
    QString composer;
    QString album;
    QDate releaseDate;
    QString genre;
    QString language;
    qint64 durationMs = 0;
    QString audioPath;
    qint64 playCount = 0;
    qint64 likeCount = 0;
    qint64 dislikeCount = 0;
    qint64 coinCount = 0;

    QString performerText() const
    {
        return performers.join(QStringLiteral("、"));
    }

    bool isValid() const
    {
        return !title.trimmed().isEmpty()
            && !performers.isEmpty()
            && !audioPath.trimmed().isEmpty()
            && durationMs >= 0
            && playCount >= 0
            && likeCount >= 0
            && dislikeCount >= 0
            && coinCount >= 0;
    }
};

enum class SongSort
{
    Heat,
    PlayCount,
    LikeCount,
    DislikeCount,
    CoinCount,
    ReleaseDate,
    Title,
    Performer
};

enum class SortDirection
{
    Ascending,
    Descending
};

#pragma once

#include <QDateTime>
#include <QString>
#include <QtTypes>

struct Playlist
{
    qint64 id = 0;
    QString name;
    QString description;
    bool isSystem = false;
    QDateTime createdAt;
};

#pragma once

#include "domain/Playlist.h"
#include "domain/Song.h"

#include <QList>
#include <QString>

#include <optional>

class MusicDatabase
{
public:
    explicit MusicDatabase(QString databasePath);
    ~MusicDatabase();

    MusicDatabase(const MusicDatabase &) = delete;
    MusicDatabase &operator=(const MusicDatabase &) = delete;

    bool open();
    bool initialize();
    QString lastError() const;

    qint64 addSong(const Song &song);
    bool importCsv(const QString &csvPath);
    bool updateSong(const Song &song);
    bool removeSong(qint64 songId);
    std::optional<Song> song(qint64 songId);
    qint64 count();
    QList<Song> listSongs(
        const QString &search = {},
        SongSort sort = SongSort::Heat,
        SortDirection direction = SortDirection::Descending,
        std::optional<qint64> playlistId = std::nullopt);

    bool toggleLike(qint64 songId);
    bool toggleDislike(qint64 songId);
    bool addCoin(qint64 songId);
    bool incrementPlay(qint64 songId);
    int reaction(qint64 songId);

    qint64 createPlaylist(const QString &name);
    bool renamePlaylist(qint64 playlistId, const QString &name);
    bool removePlaylist(qint64 playlistId);
    bool addSongToPlaylist(qint64 playlistId, qint64 songId);
    bool removeSongFromPlaylist(qint64 playlistId, qint64 songId);
    QList<Playlist> playlists();

private:
    bool toggleReaction(qint64 songId, int requestedReaction);
    bool playlistIsAllSongs(qint64 playlistId);
    void setLastError(const QString &error);

    QString m_databasePath;
    QString m_connectionName;
    QString m_lastError;
};

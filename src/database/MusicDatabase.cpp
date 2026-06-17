#include "database/MusicDatabase.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>
#include <QTextStream>

#include <array>
#include <utility>

namespace
{
const QString songColumns = QStringLiteral(
    "s.id, s.title, s.performers, s.lyricist, s.composer, s.album, "
    "s.release_date, s.genre, s.language, s.duration_ms, s.audio_path, "
    "s.play_count, s.like_count, s.dislike_count, s.coin_count");

QString encodePerformers(const QStringList &performers)
{
    QJsonArray values;
    for (const QString &performer : performers) {
        values.append(performer);
    }
    return QString::fromUtf8(QJsonDocument(values).toJson(QJsonDocument::Compact));
}

QStringList decodePerformers(const QString &value)
{
    const QJsonDocument document = QJsonDocument::fromJson(value.toUtf8());
    QStringList performers;
    for (const QJsonValue &performer : document.array()) {
        performers.append(performer.toString());
    }
    return performers;
}

QString textOrEmpty(const QString &value)
{
    return value.isNull() ? QStringLiteral("") : value;
}

QStringList parseCsvLine(const QString &line)
{
    QStringList fields;
    QString field;
    bool quoted = false;
    for (qsizetype index = 0; index < line.size(); ++index) {
        const QChar character = line.at(index);
        if (character == QLatin1Char('"')) {
            if (quoted && index + 1 < line.size() && line.at(index + 1) == QLatin1Char('"')) {
                field.append(QLatin1Char('"'));
                ++index;
            } else {
                quoted = !quoted;
            }
        } else if (character == QLatin1Char(',') && !quoted) {
            fields.append(field);
            field.clear();
        } else {
            field.append(character);
        }
    }
    fields.append(field);
    return fields;
}

Song readSong(const QSqlQuery &query)
{
    Song song;
    song.id = query.value(0).toLongLong();
    song.title = query.value(1).toString();
    song.performers = decodePerformers(query.value(2).toString());
    song.lyricist = query.value(3).toString();
    song.composer = query.value(4).toString();
    song.album = query.value(5).toString();
    song.releaseDate = QDate::fromString(query.value(6).toString(), Qt::ISODate);
    song.genre = query.value(7).toString();
    song.language = query.value(8).toString();
    song.durationMs = query.value(9).toLongLong();
    song.audioPath = query.value(10).toString();
    song.playCount = query.value(11).toLongLong();
    song.likeCount = query.value(12).toLongLong();
    song.dislikeCount = query.value(13).toLongLong();
    song.coinCount = query.value(14).toLongLong();
    return song;
}

QString sortExpression(SongSort sort)
{
    switch (sort) {
    case SongSort::Heat:
        return QStringLiteral(
            "(s.play_count + s.like_count * 20 + s.coin_count * 30 "
            "- s.dislike_count * 10)");
    case SongSort::PlayCount:
        return QStringLiteral("s.play_count");
    case SongSort::LikeCount:
        return QStringLiteral("s.like_count");
    case SongSort::DislikeCount:
        return QStringLiteral("s.dislike_count");
    case SongSort::CoinCount:
        return QStringLiteral("s.coin_count");
    case SongSort::ReleaseDate:
        return QStringLiteral("s.release_date");
    case SongSort::Title:
        return QStringLiteral("s.title");
    case SongSort::Performer:
        return QStringLiteral("s.performers");
    }
    return QStringLiteral("s.id");
}

void bindSong(QSqlQuery &query, const Song &song)
{
    query.bindValue(QStringLiteral(":title"), song.title.trimmed());
    query.bindValue(QStringLiteral(":performers"), encodePerformers(song.performers));
    query.bindValue(QStringLiteral(":lyricist"), textOrEmpty(song.lyricist));
    query.bindValue(QStringLiteral(":composer"), textOrEmpty(song.composer));
    query.bindValue(QStringLiteral(":album"), textOrEmpty(song.album));
    query.bindValue(QStringLiteral(":release_date"),
                    song.releaseDate.isValid()
                        ? song.releaseDate.toString(Qt::ISODate)
                        : QStringLiteral(""));
    query.bindValue(QStringLiteral(":genre"), textOrEmpty(song.genre));
    query.bindValue(QStringLiteral(":language"), textOrEmpty(song.language));
    query.bindValue(QStringLiteral(":duration_ms"), song.durationMs);
    query.bindValue(QStringLiteral(":audio_path"), song.audioPath.trimmed());
    query.bindValue(QStringLiteral(":play_count"), song.playCount);
    query.bindValue(QStringLiteral(":like_count"), song.likeCount);
    query.bindValue(QStringLiteral(":dislike_count"), song.dislikeCount);
    query.bindValue(QStringLiteral(":coin_count"), song.coinCount);
}
}

MusicDatabase::MusicDatabase(QString databasePath)
    : m_databasePath(std::move(databasePath)),
      m_connectionName(QStringLiteral("musicrank-%1").arg(
          QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

MusicDatabase::~MusicDatabase()
{
    if (!QSqlDatabase::contains(m_connectionName)) {
        return;
    }

    {
        QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
        database.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool MusicDatabase::open()
{
    m_lastError.clear();
    if (m_databasePath.trimmed().isEmpty()) {
        setLastError(QStringLiteral("Database path is empty."));
        return false;
    }

    QSqlDatabase database = QSqlDatabase::contains(m_connectionName)
        ? QSqlDatabase::database(m_connectionName)
        : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    database.setDatabaseName(m_databasePath);
    if (!database.open()) {
        setLastError(database.lastError().text());
        return false;
    }

    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        setLastError(query.lastError().text());
        database.close();
        return false;
    }
    return true;
}

bool MusicDatabase::initialize()
{
    m_lastError.clear();
    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    if (!database.isOpen()) {
        setLastError(QStringLiteral("Database is not open."));
        return false;
    }
    if (!database.transaction()) {
        setLastError(database.lastError().text());
        return false;
    }

    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS songs ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "title TEXT NOT NULL,"
            "performers TEXT NOT NULL,"
            "lyricist TEXT NOT NULL DEFAULT '',"
            "composer TEXT NOT NULL DEFAULT '',"
            "album TEXT NOT NULL DEFAULT '',"
            "release_date TEXT NOT NULL DEFAULT '',"
            "genre TEXT NOT NULL DEFAULT '',"
            "language TEXT NOT NULL DEFAULT '',"
            "duration_ms INTEGER NOT NULL DEFAULT 0 CHECK(duration_ms >= 0),"
            "audio_path TEXT NOT NULL,"
            "play_count INTEGER NOT NULL DEFAULT 0 CHECK(play_count >= 0),"
            "like_count INTEGER NOT NULL DEFAULT 0 CHECK(like_count >= 0),"
            "dislike_count INTEGER NOT NULL DEFAULT 0 CHECK(dislike_count >= 0),"
            "coin_count INTEGER NOT NULL DEFAULT 0 CHECK(coin_count >= 0)"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS playlists ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT NOT NULL UNIQUE,"
            "description TEXT NOT NULL DEFAULT '',"
            "is_system INTEGER NOT NULL DEFAULT 0 CHECK(is_system IN (0, 1)),"
            "created_at TEXT NOT NULL"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS playlist_songs ("
            "playlist_id INTEGER NOT NULL,"
            "song_id INTEGER NOT NULL,"
            "PRIMARY KEY (playlist_id, song_id),"
            "FOREIGN KEY (playlist_id) REFERENCES playlists(id) ON DELETE CASCADE,"
            "FOREIGN KEY (song_id) REFERENCES songs(id) ON DELETE CASCADE"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS reactions ("
            "song_id INTEGER PRIMARY KEY,"
            "reaction INTEGER NOT NULL DEFAULT 0 CHECK(reaction IN (-1, 0, 1)),"
            "FOREIGN KEY (song_id) REFERENCES songs(id) ON DELETE CASCADE"
            ")"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_songs_title ON songs(title)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_songs_performers ON songs(performers)")
    };

    QSqlQuery query(database);
    for (const QString &statement : statements) {
        if (!query.exec(statement)) {
            setLastError(query.lastError().text());
            database.rollback();
            return false;
        }
    }

    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO playlists(name, description, is_system, created_at) "
        "VALUES(:name, '', :is_system, :created_at)"));
    const QString createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    const std::array<std::pair<QString, int>, 2> defaultPlaylists = {
        std::pair<QString, int>{QStringLiteral("全部歌曲"), 1},
        std::pair<QString, int>{QStringLiteral("我的收藏"), 0}
    };
    for (const auto &[name, isSystem] : defaultPlaylists) {
        query.bindValue(QStringLiteral(":name"), name);
        query.bindValue(QStringLiteral(":is_system"), isSystem);
        query.bindValue(QStringLiteral(":created_at"), createdAt);
        if (!query.exec()) {
            setLastError(query.lastError().text());
            database.rollback();
            return false;
        }
    }

    if (!database.commit()) {
        setLastError(database.lastError().text());
        database.rollback();
        return false;
    }
    return true;
}

QString MusicDatabase::lastError() const
{
    return m_lastError;
}

qint64 MusicDatabase::addSong(const Song &song)
{
    m_lastError.clear();
    if (!song.isValid()) {
        setLastError(QStringLiteral("Song is invalid."));
        return 0;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "INSERT INTO songs("
        "title, performers, lyricist, composer, album, release_date, genre, language, "
        "duration_ms, audio_path, play_count, like_count, dislike_count, coin_count"
        ") VALUES("
        ":title, :performers, :lyricist, :composer, :album, :release_date, :genre, :language, "
        ":duration_ms, :audio_path, :play_count, :like_count, :dislike_count, :coin_count"
        ")"));
    bindSong(query, song);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return 0;
    }
    return query.lastInsertId().toLongLong();
}

bool MusicDatabase::importCsv(const QString &csvPath)
{
    m_lastError.clear();
    QFile file(csvPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setLastError(file.errorString());
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    if (stream.atEnd()) {
        setLastError(QStringLiteral("CSV is empty."));
        return false;
    }
    stream.readLine();

    QSqlDatabase database = QSqlDatabase::database(m_connectionName);
    if (!database.transaction()) {
        setLastError(database.lastError().text());
        return false;
    }

    int imported = 0;
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        if (line.trimmed().isEmpty()) {
            continue;
        }
        const QStringList values = parseCsvLine(line);
        if (values.size() != 10) {
            database.rollback();
            setLastError(QStringLiteral("Invalid CSV row %1.").arg(imported + 2));
            return false;
        }

        Song songValue;
        songValue.title = values.at(0).trimmed();
        songValue.performers = values.at(1).split(QChar(0x3001), Qt::SkipEmptyParts);
        songValue.lyricist = values.at(2);
        songValue.composer = values.at(3);
        songValue.album = values.at(4);
        songValue.releaseDate = QDate::fromString(values.at(5), Qt::ISODate);
        songValue.genre = values.at(6);
        songValue.language = values.at(7);
        songValue.durationMs = values.at(8).toLongLong();
        songValue.audioPath = values.at(9);
        if (addSong(songValue) <= 0) {
            const QString error = m_lastError;
            database.rollback();
            setLastError(error);
            return false;
        }
        ++imported;
    }

    if (!database.commit()) {
        setLastError(database.lastError().text());
        return false;
    }
    return true;
}

bool MusicDatabase::updateSong(const Song &song)
{
    m_lastError.clear();
    if (song.id <= 0 || !song.isValid()) {
        setLastError(QStringLiteral("Song is invalid."));
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "UPDATE songs SET "
        "title=:title, performers=:performers, lyricist=:lyricist, composer=:composer, "
        "album=:album, release_date=:release_date, genre=:genre, language=:language, "
        "duration_ms=:duration_ms, audio_path=:audio_path, play_count=:play_count, "
        "like_count=:like_count, dislike_count=:dislike_count, coin_count=:coin_count "
        "WHERE id=:id"));
    bindSong(query, song);
    query.bindValue(QStringLiteral(":id"), song.id);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() != 1) {
        setLastError(QStringLiteral("Song was not found."));
        return false;
    }
    return true;
}

bool MusicDatabase::removeSong(qint64 songId)
{
    m_lastError.clear();
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("DELETE FROM songs WHERE id=:id"));
    query.bindValue(QStringLiteral(":id"), songId);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() != 1) {
        setLastError(QStringLiteral("Song was not found."));
        return false;
    }
    return true;
}

std::optional<Song> MusicDatabase::song(qint64 songId)
{
    m_lastError.clear();
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("SELECT %1 FROM songs s WHERE s.id=:id").arg(songColumns));
    query.bindValue(QStringLiteral(":id"), songId);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return std::nullopt;
    }
    if (!query.next()) {
        return std::nullopt;
    }
    return readSong(query);
}

qint64 MusicDatabase::count()
{
    m_lastError.clear();
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM songs")) || !query.next()) {
        setLastError(query.lastError().text());
        return -1;
    }
    return query.value(0).toLongLong();
}

QList<Song> MusicDatabase::listSongs(
    const QString &search,
    SongSort sort,
    SortDirection direction,
    std::optional<qint64> playlistId,
    const QString &genre)
{
    m_lastError.clear();
    QList<Song> songs;
    QString sql = QStringLiteral("SELECT %1 FROM songs s ").arg(songColumns);
    const bool filterPlaylist = playlistId.has_value() && !playlistIsAllSongs(*playlistId);
    if (filterPlaylist) {
        sql += QStringLiteral("JOIN playlist_songs ps ON ps.song_id=s.id ");
    }
    sql += QStringLiteral(
        "WHERE (:search = '' OR s.title LIKE :term OR s.performers LIKE :term "
        "OR s.lyricist LIKE :term OR s.composer LIKE :term OR s.album LIKE :term) ");
    sql += QStringLiteral("AND (:genre = '' OR s.genre=:genre) ");
    if (filterPlaylist) {
        sql += QStringLiteral("AND ps.playlist_id=:playlist_id ");
    }
    sql += QStringLiteral("ORDER BY %1 %2, s.id ASC")
               .arg(sortExpression(sort),
                    direction == SortDirection::Ascending
                        ? QStringLiteral("ASC")
                        : QStringLiteral("DESC"));

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(sql);
    const QString genreFilter = genre.trimmed();
    query.bindValue(QStringLiteral(":search"), search);
    query.bindValue(QStringLiteral(":term"), QStringLiteral("%") + search + QStringLiteral("%"));
    query.bindValue(
        QStringLiteral(":genre"),
        genreFilter.isNull() ? QStringLiteral("") : genreFilter);
    if (filterPlaylist) {
        query.bindValue(QStringLiteral(":playlist_id"), *playlistId);
    }
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return songs;
    }
    while (query.next()) {
        songs.append(readSong(query));
    }
    return songs;
}

QStringList MusicDatabase::genres()
{
    m_lastError.clear();
    QStringList result;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.exec(QStringLiteral(
            "SELECT DISTINCT genre FROM songs "
            "WHERE TRIM(genre) <> '' "
            "ORDER BY genre COLLATE NOCASE ASC"))) {
        setLastError(query.lastError().text());
        return result;
    }
    while (query.next()) {
        result.append(query.value(0).toString());
    }
    return result;
}

bool MusicDatabase::toggleLike(qint64 songId)
{
    return toggleReaction(songId, 1);
}

bool MusicDatabase::toggleDislike(qint64 songId)
{
    return toggleReaction(songId, -1);
}

bool MusicDatabase::addCoin(qint64 songId)
{
    m_lastError.clear();
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("UPDATE songs SET coin_count=coin_count+1 WHERE id=:id"));
    query.bindValue(QStringLiteral(":id"), songId);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() != 1) {
        setLastError(QStringLiteral("Song was not found."));
        return false;
    }
    return true;
}

bool MusicDatabase::incrementPlay(qint64 songId)
{
    m_lastError.clear();
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("UPDATE songs SET play_count=play_count+1 WHERE id=:id"));
    query.bindValue(QStringLiteral(":id"), songId);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() != 1) {
        setLastError(QStringLiteral("Song was not found."));
        return false;
    }
    return true;
}

int MusicDatabase::reaction(qint64 songId)
{
    m_lastError.clear();
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("SELECT reaction FROM reactions WHERE song_id=:song_id"));
    query.bindValue(QStringLiteral(":song_id"), songId);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return 0;
    }
    return query.next() ? query.value(0).toInt() : 0;
}

qint64 MusicDatabase::createPlaylist(const QString &name)
{
    m_lastError.clear();
    const QString normalizedName = name.trimmed();
    if (normalizedName.isEmpty()) {
        setLastError(QStringLiteral("Playlist name is empty."));
        return 0;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "INSERT INTO playlists(name, description, is_system, created_at) "
        "VALUES(:name, '', 0, :created_at)"));
    query.bindValue(QStringLiteral(":name"), normalizedName);
    query.bindValue(QStringLiteral(":created_at"),
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return 0;
    }
    return query.lastInsertId().toLongLong();
}

bool MusicDatabase::renamePlaylist(qint64 playlistId, const QString &name)
{
    m_lastError.clear();
    const QString normalizedName = name.trimmed();
    if (normalizedName.isEmpty()) {
        setLastError(QStringLiteral("Playlist name is empty."));
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "UPDATE playlists SET name=:name "
        "WHERE id=:id AND NOT (is_system=1 AND name='全部歌曲')"));
    query.bindValue(QStringLiteral(":name"), normalizedName);
    query.bindValue(QStringLiteral(":id"), playlistId);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() != 1) {
        setLastError(QStringLiteral("Playlist was not found or is protected."));
        return false;
    }
    return true;
}

bool MusicDatabase::removePlaylist(qint64 playlistId)
{
    m_lastError.clear();
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "DELETE FROM playlists "
        "WHERE id=:id AND NOT (is_system=1 AND name='全部歌曲')"));
    query.bindValue(QStringLiteral(":id"), playlistId);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() != 1) {
        setLastError(QStringLiteral("Playlist was not found or is protected."));
        return false;
    }
    return true;
}

bool MusicDatabase::addSongToPlaylist(qint64 playlistId, qint64 songId)
{
    m_lastError.clear();
    if (playlistIsAllSongs(playlistId)) {
        setLastError(QStringLiteral("All Songs does not store explicit membership."));
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO playlist_songs(playlist_id, song_id) "
        "VALUES(:playlist_id, :song_id)"));
    query.bindValue(QStringLiteral(":playlist_id"), playlistId);
    query.bindValue(QStringLiteral(":song_id"), songId);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return query.numRowsAffected() == 1;
}

bool MusicDatabase::removeSongFromPlaylist(qint64 playlistId, qint64 songId)
{
    m_lastError.clear();
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "DELETE FROM playlist_songs WHERE playlist_id=:playlist_id AND song_id=:song_id"));
    query.bindValue(QStringLiteral(":playlist_id"), playlistId);
    query.bindValue(QStringLiteral(":song_id"), songId);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return query.numRowsAffected() == 1;
}

QList<Playlist> MusicDatabase::playlists()
{
    m_lastError.clear();
    QList<Playlist> result;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.exec(QStringLiteral(
            "SELECT id, name, description, is_system, created_at "
            "FROM playlists ORDER BY id ASC"))) {
        setLastError(query.lastError().text());
        return result;
    }
    while (query.next()) {
        Playlist playlist;
        playlist.id = query.value(0).toLongLong();
        playlist.name = query.value(1).toString();
        playlist.description = query.value(2).toString();
        playlist.isSystem = query.value(3).toBool();
        playlist.createdAt = QDateTime::fromString(query.value(4).toString(), Qt::ISODateWithMs);
        result.append(playlist);
    }
    return result;
}

bool MusicDatabase::toggleReaction(qint64 songId, int requestedReaction)
{
    m_lastError.clear();
    QSqlDatabase database = QSqlDatabase::database(m_connectionName);
    if (!database.transaction()) {
        setLastError(database.lastError().text());
        return false;
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT id FROM songs WHERE id=:id"));
    query.bindValue(QStringLiteral(":id"), songId);
    if (!query.exec() || !query.next()) {
        setLastError(query.lastError().isValid()
                         ? query.lastError().text()
                         : QStringLiteral("Song was not found."));
        database.rollback();
        return false;
    }

    int previousReaction = 0;
    query.prepare(QStringLiteral("SELECT reaction FROM reactions WHERE song_id=:song_id"));
    query.bindValue(QStringLiteral(":song_id"), songId);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        database.rollback();
        return false;
    }
    if (query.next()) {
        previousReaction = query.value(0).toInt();
    }
    const int nextReaction =
        previousReaction == requestedReaction ? 0 : requestedReaction;
    const int likeDelta =
        (nextReaction == 1 ? 1 : 0) - (previousReaction == 1 ? 1 : 0);
    const int dislikeDelta =
        (nextReaction == -1 ? 1 : 0) - (previousReaction == -1 ? 1 : 0);

    query.prepare(QStringLiteral(
        "INSERT INTO reactions(song_id, reaction) VALUES(:song_id, :reaction) "
        "ON CONFLICT(song_id) DO UPDATE SET reaction=excluded.reaction"));
    query.bindValue(QStringLiteral(":song_id"), songId);
    query.bindValue(QStringLiteral(":reaction"), nextReaction);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        database.rollback();
        return false;
    }

    query.prepare(QStringLiteral(
        "UPDATE songs SET "
        "like_count=MAX(like_count+:like_delta, 0), "
        "dislike_count=MAX(dislike_count+:dislike_delta, 0) "
        "WHERE id=:song_id"));
    query.bindValue(QStringLiteral(":like_delta"), likeDelta);
    query.bindValue(QStringLiteral(":dislike_delta"), dislikeDelta);
    query.bindValue(QStringLiteral(":song_id"), songId);
    if (!query.exec() || query.numRowsAffected() != 1) {
        setLastError(query.lastError().isValid()
                         ? query.lastError().text()
                         : QStringLiteral("Song was not found."));
        database.rollback();
        return false;
    }

    if (!database.commit()) {
        setLastError(database.lastError().text());
        database.rollback();
        return false;
    }
    return true;
}

bool MusicDatabase::playlistIsAllSongs(qint64 playlistId)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "SELECT 1 FROM playlists "
        "WHERE id=:id AND is_system=1 AND name='全部歌曲'"));
    query.bindValue(QStringLiteral(":id"), playlistId);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return query.next();
}

void MusicDatabase::setLastError(const QString &error)
{
    m_lastError = error;
}

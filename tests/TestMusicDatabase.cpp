#include "database/MusicDatabase.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <array>
#include <optional>

namespace
{
Song makeSong(const QString &title, const QString &performer, const QString &audioPath)
{
    Song song;
    song.title = title;
    song.performers = {performer};
    song.audioPath = audioPath;
    song.durationMs = 180000;
    return song;
}
}

class TestMusicDatabase : public QObject
{
    Q_OBJECT

private slots:
    void initializationIsIdempotent();
    void supportsSongCrudWithoutDeletingAudio();
    void searchesFiveMetadataFields();
    void filtersSongsByGenre();
    void supportsEverySongSort();
    void persistsInteractionsAndSortsByHeat();
    void likeAndDislikeAreExclusiveAndCancelable();
    void managesPlaylistsAndProtectsAllSongs();
    void persistsAfterReopen();
    void importsCsvTransactionally();
};

void TestMusicDatabase::initializationIsIdempotent()
{
    MusicDatabase database(QStringLiteral(":memory:"));

    QVERIFY2(database.open(), qPrintable(database.lastError()));
    QVERIFY2(database.initialize(), qPrintable(database.lastError()));
    QVERIFY2(database.initialize(), qPrintable(database.lastError()));
    QCOMPARE(database.count(), 0);

    const QList<Playlist> playlists = database.playlists();
    QCOMPARE(playlists.size(), 2);
    QCOMPARE(playlists.at(0).name, QStringLiteral("全部歌曲"));
    QVERIFY(playlists.at(0).isSystem);
    QCOMPARE(playlists.at(1).name, QStringLiteral("我的收藏"));
}

void TestMusicDatabase::supportsSongCrudWithoutDeletingAudio()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString audioPath = directory.filePath(QStringLiteral("track.mp3"));
    QFile audioFile(audioPath);
    QVERIFY(audioFile.open(QIODevice::WriteOnly));
    audioFile.write("audio");
    audioFile.close();

    MusicDatabase database(QStringLiteral(":memory:"));
    QVERIFY2(database.open(), qPrintable(database.lastError()));
    QVERIFY2(database.initialize(), qPrintable(database.lastError()));

    Song song = makeSong(QStringLiteral("First"), QStringLiteral("Artist"), audioPath);
    const qint64 id = database.addSong(song);
    QVERIFY2(id > 0, qPrintable(database.lastError()));
    QCOMPARE(database.count(), 1);

    std::optional<Song> stored = database.song(id);
    QVERIFY(stored.has_value());
    QCOMPARE(stored->title, QStringLiteral("First"));
    QCOMPARE(stored->performers, QStringList{QStringLiteral("Artist")});

    stored->title = QStringLiteral("Updated");
    stored->album = QStringLiteral("Album");
    stored->releaseDate = QDate(2025, 6, 1);
    QVERIFY2(database.updateSong(*stored), qPrintable(database.lastError()));
    QCOMPARE(database.song(id)->title, QStringLiteral("Updated"));
    QCOMPARE(database.song(id)->album, QStringLiteral("Album"));

    QVERIFY2(database.removeSong(id), qPrintable(database.lastError()));
    QCOMPARE(database.count(), 0);
    QVERIFY(!database.song(id).has_value());
    QVERIFY(QFile::exists(audioPath));
}

void TestMusicDatabase::searchesFiveMetadataFields()
{
    MusicDatabase database(QStringLiteral(":memory:"));
    QVERIFY(database.open());
    QVERIFY(database.initialize());

    Song song = makeSong(QStringLiteral("Blue Sky"), QStringLiteral("Mira"), QStringLiteral("blue.mp3"));
    song.lyricist = QStringLiteral("Lin Words");
    song.composer = QStringLiteral("Chen Notes");
    song.album = QStringLiteral("Morning Album");
    const qint64 id = database.addSong(song);
    QVERIFY(id > 0);

    const std::array<QString, 5> searches = {
        QStringLiteral("blue"),
        QStringLiteral("mira"),
        QStringLiteral("words"),
        QStringLiteral("notes"),
        QStringLiteral("morning")
    };
    for (const QString &search : searches) {
        const QList<Song> results = database.listSongs(
            search, SongSort::Title, SortDirection::Ascending);
        QCOMPARE(results.size(), 1);
        QCOMPARE(results.first().id, id);
    }
    QVERIFY(database.listSongs(QStringLiteral("missing")).isEmpty());
}

void TestMusicDatabase::filtersSongsByGenre()
{
    MusicDatabase database(QStringLiteral(":memory:"));
    QVERIFY(database.open());
    QVERIFY(database.initialize());

    Song pop = makeSong(QStringLiteral("Pop Song"), QStringLiteral("Mira"), QStringLiteral("pop.mp3"));
    pop.genre = QStringLiteral("Pop");
    QVERIFY(database.addSong(pop) > 0);

    Song rock = makeSong(QStringLiteral("Rock Song"), QStringLiteral("Mira"), QStringLiteral("rock.mp3"));
    rock.genre = QStringLiteral("Rock");
    QVERIFY(database.addSong(rock) > 0);

    QCOMPARE(database.genres(), QStringList({QStringLiteral("Pop"), QStringLiteral("Rock")}));

    const QList<Song> popSongs = database.listSongs(
        {}, SongSort::Title, SortDirection::Ascending, std::nullopt, QStringLiteral("Pop"));
    QCOMPARE(popSongs.size(), 1);
    QCOMPARE(popSongs.first().title, QStringLiteral("Pop Song"));

    const QList<Song> searchedRock = database.listSongs(
        QStringLiteral("Song"),
        SongSort::Title,
        SortDirection::Ascending,
        std::nullopt,
        QStringLiteral("Rock"));
    QCOMPARE(searchedRock.size(), 1);
    QCOMPARE(searchedRock.first().title, QStringLiteral("Rock Song"));
}

void TestMusicDatabase::supportsEverySongSort()
{
    MusicDatabase database(QStringLiteral(":memory:"));
    QVERIFY(database.open());
    QVERIFY(database.initialize());

    Song alpha = makeSong(QStringLiteral("Alpha"), QStringLiteral("Zulu"), QStringLiteral("a.mp3"));
    alpha.releaseDate = QDate(2020, 1, 1);
    alpha.playCount = 1;
    alpha.likeCount = 2;
    alpha.dislikeCount = 3;
    alpha.coinCount = 4;
    QVERIFY(database.addSong(alpha) > 0);

    Song beta = makeSong(QStringLiteral("Beta"), QStringLiteral("Able"), QStringLiteral("b.mp3"));
    beta.releaseDate = QDate(2024, 1, 1);
    beta.playCount = 10;
    beta.likeCount = 20;
    beta.dislikeCount = 30;
    beta.coinCount = 40;
    QVERIFY(database.addSong(beta) > 0);

    const std::array<SongSort, 8> sorts = {
        SongSort::Heat,
        SongSort::PlayCount,
        SongSort::LikeCount,
        SongSort::DislikeCount,
        SongSort::CoinCount,
        SongSort::ReleaseDate,
        SongSort::Title,
        SongSort::Performer
    };
    for (SongSort sort : sorts) {
        QCOMPARE(database.listSongs({}, sort, SortDirection::Ascending).size(), 2);
        QCOMPARE(database.listSongs({}, sort, SortDirection::Descending).size(), 2);
    }

    QCOMPARE(database.listSongs({}, SongSort::Title, SortDirection::Ascending).first().title,
             QStringLiteral("Alpha"));
    QCOMPARE(database.listSongs({}, SongSort::Performer, SortDirection::Ascending).first().title,
             QStringLiteral("Beta"));
}

void TestMusicDatabase::persistsInteractionsAndSortsByHeat()
{
    MusicDatabase database(QStringLiteral(":memory:"));
    QVERIFY(database.open());
    QVERIFY(database.initialize());

    const qint64 coldId = database.addSong(
        makeSong(QStringLiteral("Cold"), QStringLiteral("A"), QStringLiteral("cold.mp3")));
    const qint64 hotId = database.addSong(
        makeSong(QStringLiteral("Hot"), QStringLiteral("B"), QStringLiteral("hot.mp3")));
    QVERIFY(coldId > 0);
    QVERIFY(hotId > 0);

    QVERIFY(database.incrementPlay(coldId));
    QVERIFY(database.toggleDislike(coldId));
    QVERIFY(database.incrementPlay(hotId));
    QVERIFY(database.toggleLike(hotId));
    QVERIFY(database.addCoin(hotId));
    QVERIFY(database.addCoin(hotId));

    const Song hot = *database.song(hotId);
    QCOMPARE(hot.playCount, 1);
    QCOMPARE(hot.likeCount, 1);
    QCOMPARE(hot.coinCount, 2);

    const QList<Song> ranked =
        database.listSongs({}, SongSort::Heat, SortDirection::Descending);
    QCOMPARE(ranked.first().id, hotId);
}

void TestMusicDatabase::likeAndDislikeAreExclusiveAndCancelable()
{
    MusicDatabase database(QStringLiteral(":memory:"));
    QVERIFY(database.open());
    QVERIFY(database.initialize());
    const qint64 id = database.addSong(
        makeSong(QStringLiteral("State"), QStringLiteral("Artist"), QStringLiteral("state.mp3")));
    QVERIFY(id > 0);

    QVERIFY(database.toggleLike(id));
    QCOMPARE(database.reaction(id), 1);
    QCOMPARE(database.song(id)->likeCount, 1);

    QVERIFY(database.toggleDislike(id));
    QCOMPARE(database.reaction(id), -1);
    QCOMPARE(database.song(id)->likeCount, 0);
    QCOMPARE(database.song(id)->dislikeCount, 1);

    QVERIFY(database.toggleDislike(id));
    QCOMPARE(database.reaction(id), 0);
    QCOMPARE(database.song(id)->dislikeCount, 0);

    QVERIFY(database.toggleLike(id));
    QVERIFY(database.toggleLike(id));
    QCOMPARE(database.reaction(id), 0);
    QCOMPARE(database.song(id)->likeCount, 0);
}

void TestMusicDatabase::managesPlaylistsAndProtectsAllSongs()
{
    MusicDatabase database(QStringLiteral(":memory:"));
    QVERIFY(database.open());
    QVERIFY(database.initialize());
    const qint64 songId = database.addSong(
        makeSong(QStringLiteral("Track"), QStringLiteral("Artist"), QStringLiteral("track.mp3")));
    QVERIFY(songId > 0);

    const QList<Playlist> initial = database.playlists();
    const qint64 allSongsId = initial.at(0).id;
    const qint64 playlistId = database.createPlaylist(QStringLiteral("Commute"));
    QVERIFY2(playlistId > 0, qPrintable(database.lastError()));
    QVERIFY(database.renamePlaylist(playlistId, QStringLiteral("Road")));

    QVERIFY(database.addSongToPlaylist(playlistId, songId));
    QVERIFY(!database.addSongToPlaylist(playlistId, songId));
    QCOMPARE(database.listSongs({}, SongSort::Title, SortDirection::Ascending, playlistId).size(), 1);
    QCOMPARE(database.listSongs({}, SongSort::Title, SortDirection::Ascending, allSongsId).size(), 1);

    QVERIFY(database.removeSongFromPlaylist(playlistId, songId));
    QVERIFY(database.listSongs({}, SongSort::Title, SortDirection::Ascending, playlistId).isEmpty());
    QVERIFY(!database.removePlaylist(allSongsId));
    QVERIFY(database.removePlaylist(playlistId));
}

void TestMusicDatabase::persistsAfterReopen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("catalog.sqlite"));
    qint64 songId = 0;
    qint64 playlistId = 0;

    {
        MusicDatabase database(databasePath);
        QVERIFY2(database.open(), qPrintable(database.lastError()));
        QVERIFY2(database.initialize(), qPrintable(database.lastError()));
        songId = database.addSong(
            makeSong(QStringLiteral("Persistent"), QStringLiteral("Artist"),
                     QStringLiteral("persistent.mp3")));
        playlistId = database.createPlaylist(QStringLiteral("Saved"));
        QVERIFY(songId > 0);
        QVERIFY(playlistId > 0);
        QVERIFY(database.toggleLike(songId));
        QVERIFY(database.addCoin(songId));
        QVERIFY(database.incrementPlay(songId));
        QVERIFY(database.addSongToPlaylist(playlistId, songId));
    }

    {
        MusicDatabase database(databasePath);
        QVERIFY2(database.open(), qPrintable(database.lastError()));
        QVERIFY2(database.initialize(), qPrintable(database.lastError()));
        QCOMPARE(database.count(), 1);
        const Song song = *database.song(songId);
        QCOMPARE(song.playCount, 1);
        QCOMPARE(song.likeCount, 1);
        QCOMPARE(song.coinCount, 1);
        QCOMPARE(database.reaction(songId), 1);
        QCOMPARE(database.listSongs({}, SongSort::Title, SortDirection::Ascending, playlistId).size(),
                 1);
    }
}

void TestMusicDatabase::importsCsvTransactionally()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString csvPath = directory.filePath(QStringLiteral("songs.csv"));
    QFile csv(csvPath);
    QVERIFY(csv.open(QIODevice::WriteOnly | QIODevice::Text));
    csv.write("title,performers,lyricist,composer,album,release_date,genre,language,duration_ms,audio_path\n");
    csv.write("\"Song, One\",Artist,,,,2024-01-02,Pop,English,123000,media/one.mp3\n");
    csv.write("Song Two,\"Artist A\\xE3\\x80\\x81Artist B\",,,,2023-02-03,Rock,Chinese,0,media/two.mp3\n");
    csv.close();

    MusicDatabase database(QStringLiteral(":memory:"));
    QVERIFY(database.open());
    QVERIFY(database.initialize());
    QVERIFY2(database.importCsv(csvPath), qPrintable(database.lastError()));
    QCOMPARE(database.count(), 2);
    QCOMPARE(database.listSongs(QStringLiteral("Song, One")).size(), 1);
}

QTEST_GUILESS_MAIN(TestMusicDatabase)

#include "TestMusicDatabase.moc"

#include "domain/Song.h"

#include <QTest>

class TestSongDomain : public QObject
{
    Q_OBJECT

private slots:
    void joinsPerformersForDisplay();
    void rejectsNegativePlayCount();
    void rejectsMissingRequiredFields();
    void rejectsNegativeDurationAndCounters();
    void acceptsValidSong();
};

void TestSongDomain::joinsPerformersForDisplay()
{
    Song song;
    song.performers = {QStringLiteral("周杰伦"), QStringLiteral("费玉清")};

    QCOMPARE(song.performerText(), QStringLiteral("周杰伦、费玉清"));
}

void TestSongDomain::rejectsNegativePlayCount()
{
    Song song;
    song.title = QStringLiteral("千里之外");
    song.performers = {QStringLiteral("周杰伦"), QStringLiteral("费玉清")};
    song.audioPath = QStringLiteral("media/qian-li-zhi-wai.mp3");
    song.playCount = -1;

    QVERIFY(!song.isValid());
}

void TestSongDomain::rejectsMissingRequiredFields()
{
    Song song;
    QVERIFY(!song.isValid());

    song.title = QStringLiteral("千里之外");
    QVERIFY(!song.isValid());

    song.performers = {QStringLiteral("周杰伦"), QStringLiteral("费玉清")};
    QVERIFY(!song.isValid());

    song.audioPath = QStringLiteral("media/qian-li-zhi-wai.mp3");
    QVERIFY(song.isValid());

    song.title = QStringLiteral("   ");
    QVERIFY(!song.isValid());

    song.title = QStringLiteral("千里之外");
    song.performers.clear();
    QVERIFY(!song.isValid());

    song.performers = {QStringLiteral("周杰伦")};
    song.audioPath = QStringLiteral("   ");
    QVERIFY(!song.isValid());
}

void TestSongDomain::rejectsNegativeDurationAndCounters()
{
    Song song;
    song.title = QStringLiteral("千里之外");
    song.performers = {QStringLiteral("周杰伦"), QStringLiteral("费玉清")};
    song.audioPath = QStringLiteral("media/qian-li-zhi-wai.mp3");

    song.durationMs = -1;
    QVERIFY(!song.isValid());
    song.durationMs = 0;

    song.likeCount = -1;
    QVERIFY(!song.isValid());
    song.likeCount = 0;

    song.dislikeCount = -1;
    QVERIFY(!song.isValid());
    song.dislikeCount = 0;

    song.coinCount = -1;
    QVERIFY(!song.isValid());
}

void TestSongDomain::acceptsValidSong()
{
    Song song;
    song.title = QStringLiteral("千里之外");
    song.performers = {QStringLiteral("周杰伦"), QStringLiteral("费玉清")};
    song.durationMs = 257000;
    song.audioPath = QStringLiteral("media/qian-li-zhi-wai.mp3");
    song.playCount = 10;
    song.likeCount = 8;
    song.dislikeCount = 1;
    song.coinCount = 3;

    QVERIFY(song.isValid());
}

QTEST_APPLESS_MAIN(TestSongDomain)

#include "TestSongDomain.moc"

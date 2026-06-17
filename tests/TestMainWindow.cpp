#include "database/MusicDatabase.h"
#include "ui/MainWindow.h"

#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QWidget>

class TestMainWindow : public QObject
{
    Q_OBJECT

private slots:
    void createsRequiredControls();
    void formatsDuration();
    void resolvesAudioPathFromAncestorMediaDirectory();
    void retriesAutoplayWhilePlaybackHasNotAdvanced();
    void resolvesBackgroundImageFromAppDirectory();
};

void TestMainWindow::createsRequiredControls()
{
    MusicDatabase database(QStringLiteral(":memory:"));
    QVERIFY(database.open());
    QVERIFY(database.initialize());

    MainWindow window(database, QCoreApplication::applicationDirPath());

    QVERIFY(window.findChild<QLineEdit *>(QStringLiteral("globalSearch")));
    QVERIFY(window.findChild<QTableWidget *>(QStringLiteral("rankingTable")));
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("likeButton")));
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("dislikeButton")));
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("coinButton")));
    QVERIFY(window.findChild<QWidget *>(QStringLiteral("playerBar")));
}

void TestMainWindow::formatsDuration()
{
    QCOMPARE(MainWindow::formatTime(134000), QStringLiteral("02:14"));
}

void TestMainWindow::resolvesAudioPathFromAncestorMediaDirectory()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());

    QDir rootDir(root.path());
    QVERIFY(rootDir.mkpath(QStringLiteral("media")));
    QFile mediaFile(rootDir.filePath(QStringLiteral("media/song.mp3")));
    QVERIFY(mediaFile.open(QIODevice::WriteOnly));
    mediaFile.write("fake");
    mediaFile.close();

    QVERIFY(rootDir.mkpath(QStringLiteral("build/windows-debug/src")));
    const QString appDir = rootDir.filePath(QStringLiteral("build/windows-debug/src"));

    QCOMPARE(
        MainWindow::resolveAudioPath(QStringLiteral("media/song.mp3"), appDir),
        QDir::cleanPath(rootDir.filePath(QStringLiteral("media/song.mp3"))));
}

void TestMainWindow::retriesAutoplayWhilePlaybackHasNotAdvanced()
{
    QVERIFY(MainWindow::shouldRetryAutoplay(
        QMediaPlayer::PlayingState, 0, 0, 6));
    QVERIFY(MainWindow::shouldRetryAutoplay(
        QMediaPlayer::StoppedState, 0, 5, 6));
    QVERIFY(!MainWindow::shouldRetryAutoplay(
        QMediaPlayer::PlayingState, 1500, 0, 6));
    QVERIFY(!MainWindow::shouldRetryAutoplay(
        QMediaPlayer::StoppedState, 0, 6, 6));
}

void TestMainWindow::resolvesBackgroundImageFromAppDirectory()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());

    QDir rootDir(root.path());
    QFile imageFile(rootDir.filePath(QStringLiteral("background.png")));
    QVERIFY(imageFile.open(QIODevice::WriteOnly));
    imageFile.write("fake");
    imageFile.close();

    QCOMPARE(
        MainWindow::resolveBackgroundImagePath(root.path()),
        QDir::cleanPath(rootDir.filePath(QStringLiteral("background.png"))));
}

QTEST_MAIN(TestMainWindow)

#include "TestMainWindow.moc"

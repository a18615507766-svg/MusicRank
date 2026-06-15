#include "database/MusicDatabase.h"
#include "ui/MainWindow.h"

#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTest>
#include <QWidget>

class TestMainWindow : public QObject
{
    Q_OBJECT

private slots:
    void createsRequiredControls();
    void formatsDuration();
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

QTEST_MAIN(TestMainWindow)

#include "TestMainWindow.moc"

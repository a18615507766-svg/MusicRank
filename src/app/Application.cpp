#include "app/Application.h"

#include "database/MusicDatabase.h"
#include "ui/MainWindow.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>

Application::Application() = default;

Application::~Application() = default;

int Application::run()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    QDir directory(appDir);
    if (!directory.mkpath(QStringLiteral("data"))) {
        QMessageBox::critical(
            nullptr,
            QString::fromUtf8(u8"启动失败"),
            QString::fromUtf8(u8"无法创建数据目录：") + directory.filePath(QStringLiteral("data")));
        return 1;
    }

    database_ = std::make_unique<MusicDatabase>(
        directory.filePath(QStringLiteral("data/musicrank.db")));
    if (!database_->open() || !database_->initialize()) {
        QMessageBox::critical(
            nullptr,
            QString::fromUtf8(u8"启动失败"),
            QString::fromUtf8(u8"无法打开或初始化数据库：\n") + database_->lastError());
        database_.reset();
        return 1;
    }

    if (database_->count() == 0) {
        const QString csvPath = directory.filePath(QStringLiteral("data/songs.csv"));
        if (QFileInfo::exists(csvPath) && !database_->importCsv(csvPath)) {
            QMessageBox::warning(
                nullptr,
                QString::fromUtf8(u8"曲库导入失败"),
                database_->lastError());
        }
    }

    mainWindow_ = std::make_unique<MainWindow>(*database_, appDir);
    mainWindow_->show();

    return QCoreApplication::exec();
}

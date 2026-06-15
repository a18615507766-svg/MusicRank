#include "app/Application.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QApplication qtApplication(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("MusicRank"));
    QCoreApplication::setOrganizationName(QStringLiteral("MusicRank"));

    Application application;
    return application.run();
}

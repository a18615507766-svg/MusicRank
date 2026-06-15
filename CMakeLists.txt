cmake_minimum_required(VERSION 3.25)

project(MusicRank VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

enable_testing()

find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS
    Core
    Widgets
    Sql
    Multimedia
    Test
)

function(musicrank_configure_qt_test_runtime test_name)
    set_tests_properties(${test_name} PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_PREFIX_PATH}/bin"
    )
endfunction()

set(QT_SMOKE_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/qt_smoke.cpp")
file(WRITE "${QT_SMOKE_SOURCE}" [=[
#include <QCoreApplication>
#include <QMediaPlayer>
#include <QSqlDatabase>
#include <QTest>
#include <QWidget>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    (void)application;
    (void)sizeof(QWidget);

    QMediaPlayer mediaPlayer;
    (void)mediaPlayer;

    const QString connectionName = QStringLiteral("musicrank-smoke-sqlite");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        if (!database.open()) {
            return 1;
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    return 0;
}
]=])

add_executable(qt_smoke "${QT_SMOKE_SOURCE}")
target_link_libraries(qt_smoke PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Sql
    Qt6::Multimedia
    Qt6::Test
)

add_test(NAME qt_smoke COMMAND qt_smoke)
musicrank_configure_qt_test_runtime(qt_smoke)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/CMakeLists.txt")
    add_subdirectory(src)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()

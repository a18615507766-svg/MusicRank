add_executable(TestSongDomain TestSongDomain.cpp)
target_include_directories(TestSongDomain PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)
target_link_libraries(TestSongDomain PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME TestSongDomain COMMAND TestSongDomain)
musicrank_configure_qt_test_runtime(TestSongDomain)

add_executable(TestMusicDatabase
    TestMusicDatabase.cpp
    "${CMAKE_SOURCE_DIR}/src/database/MusicDatabase.cpp"
    "${CMAKE_SOURCE_DIR}/src/database/MusicDatabase.h"
)
target_include_directories(TestMusicDatabase PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)
target_link_libraries(TestMusicDatabase PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Test
)

add_test(NAME TestMusicDatabase COMMAND TestMusicDatabase)
musicrank_configure_qt_test_runtime(TestMusicDatabase)

add_executable(TestMainWindow
    TestMainWindow.cpp
    "${CMAKE_SOURCE_DIR}/src/database/MusicDatabase.cpp"
    "${CMAKE_SOURCE_DIR}/src/database/MusicDatabase.h"
    "${CMAKE_SOURCE_DIR}/src/ui/MainWindow.cpp"
    "${CMAKE_SOURCE_DIR}/src/ui/MainWindow.h"
    "${CMAKE_SOURCE_DIR}/src/ui/SongDialog.cpp"
    "${CMAKE_SOURCE_DIR}/src/ui/SongDialog.h"
)
target_include_directories(TestMainWindow PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)
target_link_libraries(TestMainWindow PRIVATE
    Qt6::Core
    Qt6::Multimedia
    Qt6::Sql
    Qt6::Test
    Qt6::Widgets
)

add_test(NAME TestMainWindow COMMAND TestMainWindow)
set_tests_properties(TestMainWindow PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
musicrank_configure_qt_test_runtime(TestMainWindow)

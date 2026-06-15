add_executable(MusicRank WIN32
    main.cpp
    app/Application.cpp
    app/Application.h
    database/MusicDatabase.cpp
    database/MusicDatabase.h
    domain/Playlist.h
    domain/Song.h
    ui/MainWindow.cpp
    ui/MainWindow.h
    ui/SongDialog.cpp
    ui/SongDialog.h
)

target_include_directories(MusicRank PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(MusicRank PRIVATE
    Qt6::Core
    Qt6::Multimedia
    Qt6::Sql
    Qt6::Widgets
)

add_custom_command(TARGET MusicRank POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:MusicRank>/data"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_SOURCE_DIR}/data/songs.csv"
        "$<TARGET_FILE_DIR:MusicRank>/data/songs.csv"
)

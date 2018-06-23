cmake_minimum_required(VERSION 3.1.0)

project(skautan)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)


set(SOURCES
    AudioEffects.cpp
    AVPP.cpp
    BackgroundTasks.cpp
    Database.cpp
    DatabaseUpgrade.cpp
    DlgBackgroundTaskList.cpp
    DlgEditTemplate.cpp
    DlgEditTemplateItem.cpp
    DlgHistory.cpp
    DlgChooseImportTemplates.cpp
    DlgPickTemplate.cpp
    DlgPickTemplateFavoriteItem.cpp
    DlgRemovedSongs.cpp
    DlgSongProperties.cpp
    DlgSongs.cpp
    DlgTapTempo.cpp
    DlgTemplatesList.cpp
    DlgTempoDetect.cpp
    LengthHashCalculator.cpp
    MetadataScanner.cpp
    PlaybackBuffer.cpp
    Player.cpp
    PlayerWindow.cpp
    Playlist.cpp
    PlaylistItemModel.cpp
    PlaylistItemSong.cpp
    RingBuffer.cpp
    Settings.cpp
    Song.cpp
    SongDecoder.cpp
    SongModel.cpp
    Stopwatch.cpp
    Template.cpp
    TempoDetector.cpp
    Utils.cpp
    WaveformDisplay.cpp
    main.cpp
)

set(RESOURCES
    res/SkauTan.qrc
)

set(QT_LIBRARIES Qt5::Core Qt5::Widgets Qt5::Multimedia Qt5::Sql Qt5::Xml)
find_package(Qt5Core CONFIG REQUIRED)
find_package(Qt5Widgets CONFIG REQUIRED)
find_package(Qt5Multimedia CONFIG REQUIRED)
find_package(Qt5Sql CONFIG REQUIRED)
find_package(Qt5Xml CONFIG REQUIRED)
find_package(Taglib REQUIRED)
find_package(FFmpeg REQUIRED)

add_executable(${PROJECT_NAME} ${SOURCES} ${RESOURCES})

target_link_libraries(${PROJECT_NAME} ${QT_LIBRARIES} ${FFMPEG_LIBRARIES} ${TAGLIB_LIBRARIES})
include_directories(${FFMPEG_INCLUDE_DIR})

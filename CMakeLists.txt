cmake_minimum_required(VERSION 3.0)

project (SkauTan LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 14)

find_package (Qt5Widgets REQUIRED)
find_package (Qt5Network REQUIRED)
find_package (Qt5Sql REQUIRED)
find_package (Qt5Xml REQUIRED)
find_package (Qt5Multimedia REQUIRED)


add_subdirectory (lib/LibCppHttpParser EXCLUDE_FROM_ALL)
add_subdirectory (lib/QrCodeGen        EXCLUDE_FROM_ALL)
add_subdirectory (lib/RtMidi           EXCLUDE_FROM_ALL)

include_directories(lib)





# Qt5-specific setup (adapted from https://www.kdab.com/using-cmake-with-qt-5/ ):

# Tell CMake to run moc / uic / rcc when necessary:
set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTOUIC ON)
set (CMAKE_AUTORCC ON)

# As moc files are generated in the binary dir, tell CMake
# to always look for includes there:
set (CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories (
	${Qt5Widgets_INCLUDES}
	${Qt5Network_INCLUDES}
	${Qt5Sql_INCLUDES}
	${Qt5Xml_INCLUDES}
	${Qt5Multimedia_INCLUDES}
)
add_definitions (
	${Qt5Widgets_DEFINITIONS}
	${Qt5Network_DEFINITIONS}
	${Qt5Sql_DEFINITIONS}
	${Qt5Xml_DEFINITIONS}
	${Qt5Multimedia_DEFINITIONS}
)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")





# Add the preprocessor macros used for distinguishing between debug and release builds (CMake does this automatically for MSVC):
if (NOT MSVC)
	set (CMAKE_CXX_FLAGS_DEBUG    "${CMAKE_CXX_FLAGS_DEBUG}    -D_DEBUG")
	set (CMAKE_C_FLAGS_DEBUG      "${CMAKE_C_FLAGS_DEBUG}      -D_DEBUG")
	set (CMAKE_CXX_FLAGS_RELEASE  "${CMAKE_CXX_FLAGS_RELEASE}  -DNDEBUG")
	set (CMAKE_C_FLAGS_RELEASE    "${CMAKE_C_FLAGS_RELEASE}    -DNDEBUG")
endif ()

if (MSVC)
	# Make build use multiple threads under MSVC:
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

	# Make release builds use link-time code generation:
	set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL")
	set (CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE}   /GL")
	set (CMAKE_EXE_LINKER_FLAGS_RELEASE    "${CMAKE_EXE_LINKER_FLAGS_RELEASE}    /LTCG")
	set (CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG")
	set (CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} /LTCG")
endif()




# Set up extra compiler warnings:
if (MSVC)
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")
else ()
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunknown-pragmas")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wundef")
	# TODO: set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wold-style-cast")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wdisabled-optimization")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Winit-self")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wpointer-arith")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wswitch")
endif ()





add_definitions(-DQT_DEPRECATED_WARNINGS)
add_definitions(-DTAGLIB_STATIC)


set (SOURCES
	# Audio-related sources:
	src/Audio/AudioEffects.cpp
	src/Audio/AVPP.cpp
	src/Audio/PlaybackBuffer.cpp
	src/Audio/Player.cpp
	src/Audio/RingBuffer.cpp
	src/Audio/SongDecoder.cpp

	# Controller-related sources:
	src/Controller/MidiController.cpp
	src/Controller/MidiEnumerator.cpp
	src/Controller/MidiPort.cpp

	# DB-related sources:
	src/DB/Database.cpp
	src/DB/DatabaseBackup.cpp
	src/DB/DatabaseImport.cpp
	src/DB/DatabaseUpgrade.cpp

	# UI-related sources:
	src/UI/ColorDelegate.cpp
	src/UI/PlayerWindow.cpp
	src/UI/PlaylistItemModel.cpp
	src/UI/SongModel.cpp
	src/UI/WaveformDisplay.cpp

	# UI Dialog-related sources:
	src/UI/Dlg/DlgBackgroundTaskList.cpp
	src/UI/Dlg/DlgChooseImportTemplates.cpp
	src/UI/Dlg/DlgEditFilter.cpp
	src/UI/Dlg/DlgHistory.cpp
	src/UI/Dlg/DlgImportDB.cpp
	src/UI/Dlg/DlgLvsStatus.cpp
	src/UI/Dlg/DlgManageFilters.cpp
	src/UI/Dlg/DlgPickTemplate.cpp
	src/UI/Dlg/DlgRemovedSongs.cpp
	src/UI/Dlg/DlgSongProperties.cpp
	src/UI/Dlg/DlgSongs.cpp
	src/UI/Dlg/DlgTapTempo.cpp
	src/UI/Dlg/DlgTemplatesList.cpp
	src/UI/Dlg/DlgTempoDetect.cpp

	# Other sources:
	src/BackgroundTasks.cpp
	src/ComponentCollection.cpp
	src/DJControllers.cpp
	src/Filter.cpp
	src/InstallConfiguration.cpp
	src/LengthHashCalculator.cpp
	src/LocalVoteServer.cpp
	src/MetadataScanner.cpp
	src/Playlist.cpp
	src/PlaylistItemSong.cpp
	src/Settings.cpp
	src/Song.cpp
	src/Stopwatch.cpp
	src/Template.cpp
	src/TempoDetector.cpp
	src/Utils.cpp
	src/main.cpp
)





set (HEADERS
	# Audio-related headers:
	src/Audio/AudioDataSource.hpp
	src/Audio/AudioEffects.hpp
	src/Audio/AVPP.hpp
	src/Audio/PlaybackBuffer.hpp
	src/Audio/Player.hpp
	src/Audio/RingBuffer.hpp
	src/Audio/SongDecoder.hpp

	# Controller-related headers:
	src/Controller/AbstractController.hpp
	src/Controller/AbstractEnumerator.hpp
	src/Controller/MidiController.hpp
	src/Controller/MidiEnumerator.hpp
	src/Controller/MidiPort.hpp

	#DB-related headers:
	src/DB/Database.hpp
	src/DB/DatabaseBackup.hpp
	src/DB/DatabaseImport.hpp
	src/DB/DatabaseUpgrade.hpp

	# UI-related headers:
	src/UI/ColorDelegate.hpp
	src/UI/PlayerWindow.hpp
	src/UI/PlaylistItemModel.hpp
	src/UI/SongModel.hpp
	src/UI/WaveformDisplay.hpp

	# UI Dialog-related headers:
	src/UI/Dlg/DlgBackgroundTaskList.hpp
	src/UI/Dlg/DlgChooseImportTemplates.hpp
	src/UI/Dlg/DlgEditFilter.hpp
	src/UI/Dlg/DlgHistory.hpp
	src/UI/Dlg/DlgImportDB.hpp
	src/UI/Dlg/DlgLvsStatus.hpp
	src/UI/Dlg/DlgManageFilters.hpp
	src/UI/Dlg/DlgPickTemplate.hpp
	src/UI/Dlg/DlgRemovedSongs.hpp
	src/UI/Dlg/DlgSongProperties.hpp
	src/UI/Dlg/DlgSongs.hpp
	src/UI/Dlg/DlgTapTempo.hpp
	src/UI/Dlg/DlgTemplatesList.hpp
	src/UI/Dlg/DlgTempoDetect.hpp

	# Other headers:
	src/BackgroundTasks.hpp
	src/ComponentCollection.hpp
	src/DJControllers.hpp
	src/DatedOptional.hpp
	src/Exception.hpp
	src/Filter.hpp
	src/IPlaylistItem.hpp
	src/InstallConfiguration.hpp
	src/LengthHashCalculator.hpp
	src/LocalVoteServer.hpp
	src/MetadataScanner.hpp
	src/Playlist.hpp
	src/PlaylistItemSong.hpp
	src/Settings.hpp
	src/Song.hpp
	src/Stopwatch.hpp
	src/Template.hpp
	src/TempoDetector.hpp
	src/Utils.hpp
)

set (UI
	src/UI/PlayerWindow.ui
	src/UI/Dlg/DlgBackgroundTaskList.ui
	src/UI/Dlg/DlgChooseImportTemplates.ui
	src/UI/Dlg/DlgEditFilter.ui
	src/UI/Dlg/DlgHistory.ui
	src/UI/Dlg/DlgImportDB.ui
	src/UI/Dlg/DlgLvsStatus.ui
	src/UI/Dlg/DlgManageFilters.ui
	src/UI/Dlg/DlgPickTemplate.ui
	src/UI/Dlg/DlgRemovedSongs.ui
	src/UI/Dlg/DlgSongProperties.ui
	src/UI/Dlg/DlgSongs.ui
	src/UI/Dlg/DlgTapTempo.ui
	src/UI/Dlg/DlgTemplatesList.ui
	src/UI/Dlg/DlgTempoDetect.ui
)

set (RESOURCES
	res/SkauTan.qrc
)




# TODO: BeatDetectTest
set (BEATDETECTTEST_SOURCES
	BeatDetectTest/Stubs.cpp
	BeatDetectTest/BeatDetectTest.cpp
)



# Special handling: The WaveformDisplay.hpp file needs to be accessible from moc_PlayerWindow.h compiled from PlayerWindow.cpp:
set_source_files_properties (
	src/UI/PlayerWindow.cpp
	PROPERTIES COMPILE_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/src/UI"
)

add_executable (${PROJECT_NAME} ${SOURCES} ${HEADERS} ${UI} ${RESOURCES})

target_link_libraries (${PROJECT_NAME}
	${Qt5Widgets_LIBRARIES}
	${Qt5Network_LIBRARIES}
	${Qt5Sql_LIBRARIES}
	${Qt5Xml_LIBRARIES}
	${Qt5Multimedia_LIBRARIES}
	LibCppHttpParser-static
	QrCodeGen-static
	rtmidi_static

	# Ffmpeg libraries (assumed installed on the build system):
	avformat
	avutil
	avcodec
	swresample

	# TagLib libraries (assumed installed on the build system):
	tag$<$<CONFIG:Debug>:d>
)

if (WIN32)
	target_link_libraries(${PROJECT_NAME}
		zlib$<$<CONFIG:Debug>:d>
	)
else ()
	target_link_libraries(${PROJECT_NAME}
		z
	)
endif ()
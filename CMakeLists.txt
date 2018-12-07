cmake_minimum_required(VERSION 3.1)

project (SkauTan LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 14)

find_package (Qt5 COMPONENTS Widgets Network Sql Xml Multimedia LinguistTools REQUIRED)





option (FORCE_STATIC_RUNTIME "Force MSVC to use static runtime (use with static builds)" FALSE)




if (MSVC)
	# Make build use multiple threads under MSVC:
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

	# Make release builds use link-time code generation:
	set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL")
	set (CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE}   /GL")
	set (CMAKE_EXE_LINKER_FLAGS_RELEASE    "${CMAKE_EXE_LINKER_FLAGS_RELEASE}    /LTCG")
	set (CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG")
	set (CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} /LTCG")
	set (CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} /LTCG")

	if (FORCE_STATIC_RUNTIME)
		message(STATUS "Forcing a static runtime")
		string(REGEX REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
		string(REGEX REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

		# Precompiled Ffmpeg libs seem to trigger a SAFESEH failure, fix it:
		set (CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /SAFESEH:NO")

		# For some reason the Qt modules don't have their dependencies on other libraries set right, we need to explicitly add them:
		set (QT_LIB_PATH ${CMAKE_PREFIX_PATH}/lib)
		set (ADDITIONAL_LIBRARIES
			ws2_32.lib
			opengl32.lib
			imm32.lib
			winmm.lib
			iphlpapi.lib
			crypt32.lib
			netapi32.lib
			userenv.lib
			version.lib
			dwmapi.lib
			strmiids.lib
			${QT_LIB_PATH}/qtfreetype.lib
			${QT_LIB_PATH}/qtharfbuzz.lib
			${QT_LIB_PATH}/qtlibpng.lib
			${QT_LIB_PATH}/qtpcre2.lib
			${QT_LIB_PATH}/Qt5FontDatabaseSupport.lib
			${QT_LIB_PATH}/Qt5EventDispatcherSupport.lib
			${QT_LIB_PATH}/Qt5ThemeSupport.lib
			${QT_LIB_PATH}/Qt5AccessibilitySupport.lib
			${CMAKE_PREFIX_PATH}/plugins/audio/qtaudio_windows.lib
			${CMAKE_PREFIX_PATH}/plugins/platforms/qwindows.lib
			${CMAKE_PREFIX_PATH}/plugins/sqldrivers/qsqlite.lib
		)
		add_definitions(-DFORCE_STATIC_RUNTIME)
	endif ()
endif ()





add_subdirectory (lib/LibCppHttpParser EXCLUDE_FROM_ALL)
add_subdirectory (lib/QrCodeGen        EXCLUDE_FROM_ALL)
add_subdirectory (lib/RtMidi           EXCLUDE_FROM_ALL)

include_directories (lib)





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




# Add the precompiled libraries: Ffmpeg and TagLib
include_directories($ENV{INCLUDE})
find_library(AVCODEC NAMES avcodec PATHS ENV LIB)
if ("${AVCODEC}" STREQUAL "AVCODEC-NOTFOUND")
	message (FATAL_ERROR "Cannot find the Ffmpeg library avcodec")
endif ()
find_library(AVFORMAT NAMES avformat PATHS ENV LIB)
if ("${AVFORMAT}" STREQUAL "AVFORMAT-NOTFOUND")
	message (FATAL_ERROR "Cannot find the Ffmpeg library avformat")
endif ()
find_library(AVUTIL NAMES avutil PATHS ENV LIB)
if ("${AVUTIL}" STREQUAL "AVUTIL-NOTFOUND")
	message (FATAL_ERROR "Cannot find the Ffmpeg library avutil")
endif ()
find_library(SWRESAMPLE NAMES swresample PATHS ENV LIB)
if ("${SWRESAMPLE}" STREQUAL "SWRESAMPLE-NOTFOUND")
	message (FATAL_ERROR "Cannot find the Ffmpeg library swresample")
endif ()

find_library(TAGLIB_RELEASE NAMES tag PATHS ENV LIB)

if (MSVC)
	find_library(TAGLIB_DEBUG NAMES tagd PATHS ENV LIB)
	find_library(ZLIB_RELEASE NAMES zlib z PATHS ENV LIB)
	find_library(ZLIB_DEBUG NAMES zlibd PATHS ENV LIB)
else ()
	set (TAGLIB_DEBUG ${TAGLIB_RELEASE})
endif()


find_path(AVFORMAT_HDR libavformat/avformat.h NAME avformat PATHS ENV INCLUDE)
if (AVFORMAT_HDR-NOTFOUND)
	message(FATAL Cannot find the Ffmpeg library include folder base)
endif ()

include_directories (${AVFORMAT_HDR})






# Add the preprocessor macros used for distinguishing between debug and release builds (CMake does this automatically for MSVC):
if (NOT MSVC)
	set (CMAKE_CXX_FLAGS_DEBUG    "${CMAKE_CXX_FLAGS_DEBUG}    -D_DEBUG")
	set (CMAKE_C_FLAGS_DEBUG      "${CMAKE_C_FLAGS_DEBUG}      -D_DEBUG")
	set (CMAKE_CXX_FLAGS_RELEASE  "${CMAKE_CXX_FLAGS_RELEASE}  -DNDEBUG")
	set (CMAKE_C_FLAGS_RELEASE    "${CMAKE_C_FLAGS_RELEASE}    -DNDEBUG")
endif ()





# Set up extra compiler warnings:
if (MSVC)
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")
else ()
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunknown-pragmas")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wundef")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wold-style-cast")
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
	src/DB/TagImportExport.cpp

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
	src/UI/Dlg/DlgLibraryMaintenance.cpp
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
	src/main.cpp
	src/MetadataScanner.cpp
	src/Playlist.cpp
	src/PlaylistImportExport.cpp
	src/PlaylistItemSong.cpp
	src/Settings.cpp
	src/Song.cpp
	src/Stopwatch.cpp
	src/Template.cpp
	src/TempoDetector.cpp
	src/Utils.cpp
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
	src/DB/TagImportExport.hpp

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
	src/UI/Dlg/DlgLibraryMaintenance.hpp
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
	src/PlaylistImportExport.hpp
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
	src/UI/Dlg/DlgLibraryMaintenance.ui
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

if (WIN32)
	list (APPEND RESOURCES res/win.rc)
endif ()

set (TSFILES
	translations/SkauTan_cs.ts
)

# TODO: BeatDetectTest
set (BEATDETECTTEST_SOURCES
	BeatDetectTest/Stubs.cpp
	BeatDetectTest/BeatDetectTest.cpp
)





# Process translations (update, compile):
set(ts_lst_file "${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/ts_lst_file")
set(lst_file_srcs)
foreach(lst_file_src ${SOURCES} ${HEADERS} ${UI})
	set(lst_file_srcs "${lst_file_src}\n${lst_file_srcs}")
endforeach()
file (WRITE ${ts_lst_file} "${lst_file_srcs}")

add_custom_target(tsfiles
	COMMENT "Updating TS files"
	COMMAND "${Qt5_LUPDATE_EXECUTABLE}" ${_lupdate_options} @${ts_lst_file} -ts ${TSFILES}
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/ts_byproduct_file"
	DEPENDS ${SOURCES} ${HEADERS} ${TSFILES} VERBATIM
	SOURCES ${TSFILES}
)
set (QMFILES "")
make_directory("${CMAKE_CURRENT_BINARY_DIR}/translations")
foreach(_current_FILE ${TSFILES})
	get_filename_component(_abs_FILE ${_current_FILE} ABSOLUTE)
	get_filename_component(qm ${_abs_FILE} NAME)
	# everything before the last dot has to be considered the file name (including other dots)
	string(REGEX REPLACE "\\.[^.]*$" "" FILE_NAME ${qm})
	set(qm "${CMAKE_CURRENT_BINARY_DIR}/translations/${FILE_NAME}.qm")

	add_custom_command(OUTPUT ${qm}
		COMMENT "Compiling QM files"
		COMMAND ${Qt5_LRELEASE_EXECUTABLE}
		ARGS ${_abs_FILE} -qm ${qm}
		DEPENDS ${_abs_FILE} VERBATIM
	)
	list(APPEND QMFILES ${qm})
endforeach()




# Special handling: The WaveformDisplay.hpp file needs to be accessible from moc_PlayerWindow.h compiled from PlayerWindow.cpp:
set_source_files_properties (
	src/UI/PlayerWindow.cpp
	PROPERTIES COMPILE_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/src/UI"
)

add_executable (${PROJECT_NAME} WIN32 MACOSX_BUNDLE ${SOURCES} ${HEADERS} ${UI} ${RESOURCES} ${QMFILES})

target_link_libraries (${PROJECT_NAME}
	Qt5::Widgets
	Qt5::Network
	Qt5::Sql
	Qt5::Xml
	Qt5::Multimedia
	${ADDITIONAL_LIBRARIES}
	LibCppHttpParser-static
	QrCodeGen-static
	rtmidi_static

	# Ffmpeg libraries (assumed installed on the build system):
	${AVFORMAT}
	${AVUTIL}
	${AVCODEC}
	${SWRESAMPLE}

	# TagLib libraries (assumed installed on the build system):
	$<$<CONFIG:Debug>:${TAGLIB_DEBUG}>
	$<$<CONFIG:Release>:${TAGLIB_RELEASE}>
)

if (MSVC)
	target_link_libraries(${PROJECT_NAME}
		$<$<CONFIG:Debug>:${ZLIB_DEBUG}>
		$<$<CONFIG:Release>:${ZLIB_RELEASE}>
	)
else ()
	target_link_libraries(${PROJECT_NAME}
		z
	)
endif ()

add_dependencies(${PROJECT_NAME} tsfiles)

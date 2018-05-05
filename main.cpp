#include "PlayerWindow.h"
#include <QApplication>
#include "BackgroundTasks.h"
#include "Database.h"
#include "MetadataScanner.h"
#include "HashCalculator.h"
#include "Player.h"
#include "PlaylistItemSong.h"





int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	// Initialize singletons / subsystems:
	BackgroundTasks::get();

	// Create the main app objects:
	Database mainDB;
	MetadataScanner scanner;
	HashCalculator hashCalc;
	Player player;

	// Connect the main objects together:
	app.connect(&mainDB,   &Database::needSongHash,             &hashCalc, &HashCalculator::queueHashSong);
	app.connect(&hashCalc, &HashCalculator::songHashCalculated, &mainDB,  &Database::songHashCalculated);
	app.connect(&mainDB,   &Database::needSongMetadata,         &scanner, &MetadataScanner::queueScanSong);
	app.connect(&scanner,  &MetadataScanner::songScanned,       &mainDB,  &Database::songScanned);
	app.connect(&player,   &Player::startedPlayback, [&](IPlaylistItem * a_Item)
		{
			// Update the "last played" value in the DB:
			auto spi = dynamic_cast<PlaylistItemSong *>(a_Item);
			if (spi != nullptr)
			{
				mainDB.songPlaybackStarted(spi->song());
			}
		}
	);

	// Load the DB:
	mainDB.open("SkauTan.sqlite");

	// Show the UI:
	PlayerWindow w(mainDB, scanner, player);
	w.showMaximized();

	return app.exec();
}

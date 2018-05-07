#include "PlayerWindow.h"
#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QDebug>
#include "BackgroundTasks.h"
#include "Database.h"
#include "MetadataScanner.h"
#include "HashCalculator.h"
#include "Player.h"
#include "PlaylistItemSong.h"
#include "PlaybackBuffer.h"





void initTranslations(QApplication & a_App)
{
	auto translator = std::make_unique<QTranslator>();
	auto locale = QLocale::system();
	if (!translator->load("SkauTan_" + locale.name(), "translations"))
	{
		qWarning() << "Could not load translations for locale " << locale.name() << ", trying all UI languages " << locale.uiLanguages();
		if (!translator->load(locale, "SkauTan", "_", "translations"))
		{
			qWarning() << "Could not load translations for " << locale;
			return;
		}
	}
	a_App.installTranslator(translator.release());
}





int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	// Initialize translations:
	initTranslations(app);

	// Initialize singletons / subsystems:
	BackgroundTasks::get();
	qRegisterMetaType<SongPtr>();

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
	app.connect(&player,   &Player::startedPlayback, [&](IPlaylistItemPtr a_Item)
		{
			// Update the "last played" value in the DB:
			auto spi = std::dynamic_pointer_cast<PlaylistItemSong>(a_Item);
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

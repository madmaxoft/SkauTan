#include "PlayerWindow.h"
#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QDebug>
#include <QMessageBox>
#include <QFile>
#include "BackgroundTasks.h"
#include "Database.h"
#include "MetadataScanner.h"
#include "LengthHashCalculator.h"
#include "Player.h"
#include "PlaylistItemSong.h"
#include "PlaybackBuffer.h"
#include "Template.h"
#include "Settings.h"
#include "TempoDetector.h"





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





void importDefaultTemplates(Database & a_DB)
{
	qDebug() << "No templates in the DB, inserting defaults...";
	QFile f(":/other/STT-LAT.SkauTanTemplates");
	if (!f.open(QFile::ReadOnly))
	{
		QMessageBox::warning(
			nullptr,
			QApplication::tr("SkauTan: Error"),
			QApplication::tr("Cannot import default templates, the definition file is inaccessible.\n%1").arg(f.errorString())
		);
		return;
	}
	auto templates = TemplateXmlImport::run(f.readAll());
	f.close();
	for (const auto & tmpl: templates)
	{
		a_DB.addTemplate(tmpl);
		a_DB.saveTemplate(*tmpl);
	}
}





int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	try
	{
		// Initialize translations:
		initTranslations(app);

		// Initialize singletons / subsystems:
		BackgroundTasks::get();
		qRegisterMetaType<SongPtr>();
		qRegisterMetaType<Song::SharedDataPtr>();
		qRegisterMetaType<TempoDetector::ResultPtr>();
		Settings::init("SkauTan.ini");

		// Create the main app objects:
		Database mainDB;
		MetadataScanner scanner;
		LengthHashCalculator lhCalc;
		Player player;

		// Connect the main objects together:
		app.connect(&mainDB,  &Database::needFileHash,                     &lhCalc,            &LengthHashCalculator::queueHashFile);
		app.connect(&mainDB,  &Database::needSongLength,                   &lhCalc,            &LengthHashCalculator::queueLengthSong);
		app.connect(&lhCalc,  &LengthHashCalculator::fileHashCalculated,   &mainDB,            &Database::songHashCalculated);
		app.connect(&lhCalc,  &LengthHashCalculator::fileHashFailed,       &mainDB,            &Database::songHashFailed);
		app.connect(&lhCalc,  &LengthHashCalculator::songLengthCalculated, &mainDB,            &Database::songLengthCalculated);
		app.connect(&mainDB,  &Database::needSongTagRescan,                &scanner,           &MetadataScanner::queueScanSong);
		app.connect(&mainDB,  &Database::songRemoved,                      &player.playlist(), &Playlist::removeSong);
		app.connect(&scanner, &MetadataScanner::songScanned,               &mainDB,            &Database::songScanned);
		app.connect(&player,  &Player::startedPlayback, [&](IPlaylistItemPtr a_Item)
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

		// Add default templates, if none in the DB:
		if (mainDB.templates().empty())
		{
			importDefaultTemplates(mainDB);
		}

		// Show the UI:
		PlayerWindow w(mainDB, scanner, lhCalc, player);
		w.showMaximized();

		return app.exec();
	}
	catch (const std::runtime_error & exc)
	{
		QMessageBox::warning(
			nullptr,
			QApplication::tr("SkauTan: Fatal error"),
			QApplication::tr("SkauTan has detected a fatal error:\n\n%1").arg(exc.what())
		);
		return -1;
	}
}

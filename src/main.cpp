#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QDebug>
#include <QMessageBox>
#include <QFile>
#include "Audio/Player.hpp"
#include "Audio/PlaybackBuffer.hpp"
#include "DB/Database.hpp"
#include "DB/DatabaseBackup.hpp"
#include "UI/PlayerWindow.hpp"
#include "BackgroundTasks.hpp"
#include "MetadataScanner.hpp"
#include "LengthHashCalculator.hpp"
#include "PlaylistItemSong.hpp"
#include "Template.hpp"
#include "Settings.hpp"
#include "TempoDetector.hpp"
#include "Utils.hpp"
#include "DJControllers.hpp"
#include "LocalVoteServer.hpp"
#include "InstallConfiguration.hpp"





// From http://stackoverflow.com/a/27807379
// When linking statically with CMake + MSVC, we need to add the platform plugin
#if defined(_MSC_VER) && defined(FORCE_STATIC_RUNTIME)
	#include <QtPlugin>
	Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#endif





/** Loads translation for the specified locale from all reasonable locations.
Returns true if successful, false on failure.
Tries: resources, <curdir>/translations, <exepath>/translations. */
static bool tryLoadTranslation(QTranslator & a_Translator, const QLocale & a_Locale)
{
	static const QString exePath = QCoreApplication::applicationDirPath();

	if (a_Translator.load("SkauTan_" + a_Locale.name(), ":/translations"))
	{
		qDebug() << "Loaded translation " << a_Locale.name() << " from resources";
		return true;
	}
	if (a_Translator.load("SkauTan_" + a_Locale.name(), "translations"))
	{
		qDebug() << "Loaded translation " << a_Locale.name() << " from current folder";
		return true;
	}
	if (a_Translator.load("SkauTan_" + a_Locale.name(), exePath + "/translations"))
	{
		qDebug() << "Loaded translation " << a_Locale.name() << " from exe folder";
		return true;
	}
	return false;
}





void initTranslations(QApplication & a_App)
{
	auto translator = std::make_unique<QTranslator>();
	auto locale = QLocale::system();
	if (!tryLoadTranslation(*translator, locale))
	{
		qWarning() << "Could not load translations for locale " << locale.name() << ", trying all UI languages " << locale.uiLanguages();
		if (!translator->load(locale, "SkauTan", "_", "translations"))
		{
			qWarning() << "Could not load translations for " << locale;
			return;
		}
	}
	qDebug() << "Translator empty: " << translator->isEmpty();
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
		auto instConf = std::make_shared<InstallConfiguration>();
		Settings::init(instConf->dataLocation("SkauTan.ini"));

		// Create the main app objects:
		ComponentCollection cc;
		cc.addComponent(instConf);
		auto mainDB = cc.addNew<Database>(cc);
		auto scanner = cc.addNew<MetadataScanner>();
		auto lhCalc = cc.addNew<LengthHashCalculator>();
		auto player = cc.addNew<Player>();
		auto midiControllers = cc.addNew<DJControllers>();
		auto voteServer      = cc.addNew<LocalVoteServer>(cc);

		// Connect the main objects together:
		app.connect(mainDB.get(),     &Database::needFileHash,                     lhCalc.get(),        &LengthHashCalculator::queueHashFile);
		app.connect(mainDB.get(),     &Database::needSongLength,                   lhCalc.get(),        &LengthHashCalculator::queueLengthSong);
		app.connect(lhCalc.get(),     &LengthHashCalculator::fileHashCalculated,   mainDB.get(),        &Database::songHashCalculated);
		app.connect(lhCalc.get(),     &LengthHashCalculator::fileHashFailed,       mainDB.get(),        &Database::songHashFailed);
		app.connect(lhCalc.get(),     &LengthHashCalculator::songLengthCalculated, mainDB.get(),        &Database::songLengthCalculated);
		app.connect(mainDB.get(),     &Database::needSongTagRescan,                scanner.get(),       &MetadataScanner::queueScanSong);
		app.connect(mainDB.get(),     &Database::songRemoved,                      &player->playlist(), &Playlist::removeSong);
		app.connect(scanner.get(),    &MetadataScanner::songScanned,               mainDB.get(),        &Database::songScanned);
		app.connect(player.get(),     &Player::startedPlayback,                    voteServer.get(),    &LocalVoteServer::startedPlayback);
		app.connect(voteServer.get(), &LocalVoteServer::addVoteRhythmClarity,      mainDB.get(),        &Database::addVoteRhythmClarity);
		app.connect(voteServer.get(), &LocalVoteServer::addVoteGenreTypicality,    mainDB.get(),        &Database::addVoteGenreTypicality);
		app.connect(voteServer.get(), &LocalVoteServer::addVotePopularity,         mainDB.get(),        &Database::addVotePopularity);
		app.connect(player.get(),  &Player::startedPlayback, [&](IPlaylistItemPtr a_Item)
			{
				// Update the "last played" value in the DB:
				auto spi = std::dynamic_pointer_cast<PlaylistItemSong>(a_Item);
				if (spi != nullptr)
				{
					mainDB->songPlaybackStarted(spi->song());
				}
			}
		);

		// Load the DB:
		auto dbFile = instConf->dbFileName();
		DatabaseBackup::dailyBackupOnStartup(dbFile, instConf->dbBackupsFolder());
		mainDB->open(dbFile);

		// Add default templates, if none in the DB:
		if (mainDB->templates().empty())
		{
			importDefaultTemplates(*mainDB);
		}

		// If the server was started the last time, start it again:
		if (Settings::loadValue("LocalVoteServer", "isStarted").toBool())
		{
			voteServer->startServer();
		}

		// Show the UI:
		PlayerWindow w(cc);
		w.showMaximized();

		// Run the app:
		auto res = app.exec();

		// Save the server state:
		Settings::saveValue("LocalVoteServer", "isStarted", voteServer->isStarted());

		// Stop all background tasks:
		BackgroundTasks::get().stopAll();

		return res;
	}
	catch (const std::exception & exc)
	{
		QMessageBox::warning(
			nullptr,
			QApplication::tr("SkauTan: Fatal error"),
			QApplication::tr("SkauTan has detected a fatal error:\n\n%1").arg(exc.what())
		);
		return -1;
	}
	catch (...)
	{
		QMessageBox::warning(
			nullptr,
			QApplication::tr("SkauTan: Fatal error"),
			QApplication::tr("SkauTan has detected an unknown fatal error. Use a debugger to view detailed runtime log.")
		);
		return -1;
	}
}

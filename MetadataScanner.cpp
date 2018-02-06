#include "MetadataScanner.h"
#include <assert.h>
#include <QDebug>
#include <taglib/fileref.h>
#include "Song.h"
#include "HashCalculator.h"





/** Processes a single song.
Scans the metadata and updates them directly in the Song object. */
class SongProcessor
{
public:
	SongProcessor(SongPtr a_Song):
		m_Song(a_Song)
	{
	}


	void process()
	{
		HashCalculator::calc(*m_Song);
		parseTagLibMetadata(m_Song->fileName());
		parseFileNameIntoMetadata(m_Song->fileName());
	}


protected:

	/** The song being processed. */
	SongPtr m_Song;


	/** Parses any tags within the song using TagLib. */
	void parseTagLibMetadata(const QString & a_FileName)
	{
		TagLib::FileRef fr(a_FileName.toUtf8().constData());
		if (fr.isNull())
		{
			// File format not recognized
			qDebug() << __FUNCTION__ << ": Unable to parse file.";
			return;
		}

		// Set the song length:
		auto ap = fr.audioProperties();
		if (ap != nullptr)
		{
			m_Song->setLength(static_cast<double>(ap->lengthInMilliseconds()) / 1000);
		}
		else
		{
			qDebug() << __FUNCTION__ << ": AudioProperties not found.";
		}

		// Set the author / title:
		auto tag = fr.tag();
		if (tag == nullptr)
		{
			qDebug() << __FUNCTION__ << ": No TagLib-extractable information found";
			return;
		}
		if (!m_Song->author().isValid())
		{
			m_Song->setAuthor(QString::fromStdString(tag->artist().to8Bit(true)));
		}
		if (!m_Song->title().isValid())
		{
			m_Song->setTitle(QString::fromStdString(tag->title().to8Bit(true)));
		}
	}


	/** Attempts to parse the filename into metadata.
	Assumes that most of our songs are named "[30 BPM] Author - Title.mp3" and are in genre-based folders. */
	void parseFileNameIntoMetadata(const QString & a_FileName)
	{
		auto idxLastSlash = a_FileName.lastIndexOf('/');
		if (idxLastSlash < 0)
		{
			return;
		}

		// Auto-detect genre from the folder name:
		if (!m_Song->genre().isValid())
		{
			auto idxPrevSlash = a_FileName.lastIndexOf('/', idxLastSlash - 1);
			if (idxPrevSlash >= 0)
			{
				auto lastFolder = a_FileName.mid(idxPrevSlash + 1, idxLastSlash - idxPrevSlash - 1);
				auto genre = folderNameToGenre(lastFolder);
				if (!genre.isEmpty())
				{
					m_Song->setGenre(genre);
				}
			}
		}

		// Detect MPM from the "[30 BPM]" string:
		auto fileBareName = a_FileName.mid(idxLastSlash + 1);
		auto idxExt = fileBareName.lastIndexOf('.');
		if (idxExt >= 0)
		{
			fileBareName.remove(idxExt, fileBareName.length());
		}
		int start = 0;
		if (
			(fileBareName.length() > 8) &&
			(fileBareName[0] == '[') &&
			(fileBareName.mid(1, 2).toInt() > 0) &&
			(fileBareName.mid(3, 5) == " BPM]")
		)
		{
			if (!m_Song->measuresPerMinute().isValid())
			{
				m_Song->setMeasuresPerMinute(fileBareName.mid(1, 2).toInt());
			}
			start = 8;
		}
		auto idxSeparator = fileBareName.indexOf(" - ", start);
		if (idxSeparator < 0)
		{
			// No separator, consider the entire filename the title:
			if (!m_Song->title().isValid())
			{
				m_Song->setTitle(fileBareName.mid(start));
			}
		}
		else
		{
			if (!m_Song->title().isValid() && !m_Song->author().isValid())
			{
				m_Song->setAuthor(fileBareName.mid(start, idxSeparator - start).trimmed());
				m_Song->setTitle(fileBareName.mid(idxSeparator + 3).trimmed());
			}
		}
	}


	/** Returns the song genre that is usually contained in a folder of the specified name.
	Returns empty string if no such song genre is known. */
	static QString folderNameToGenre(const QString & a_FolderName)
	{
		static const std::map<QString, QString> genreMap =
		{
			{ "waltz",     "SW" },
			{ "tango",     "TG" },
			{ "valčík",    "VW" },
			{ "slowfox",   "SF" },
			{ "quickstep", "QS" },
			{ "samba",     "SB" },
			{ "chacha",    "CH" },
			{ "rumba",     "RB" },
			{ "paso",      "PD" },
			{ "pasodoble", "PD" },
			{ "jive",      "JI" },
		};
		auto itr = genreMap.find(a_FolderName.toLower());
		if (itr == genreMap.end())
		{
			return QString();
		}
		return itr->second;
	}
};





////////////////////////////////////////////////////////////////////////////////
// MetadataScanner:

MetadataScanner::MetadataScanner():
	m_ShouldTerminate(false)
{

}





MetadataScanner::~MetadataScanner()
{
	// Notify the worker thread to terminate:
	m_ShouldTerminate = true;
	m_SongAvailable.wakeAll();

	// Wait for the thread to finish:
	wait();
}





void MetadataScanner::queueScan(SongPtr a_Song)
{
	qDebug() << __FUNCTION__ << ": Adding song " << a_Song->fileName() << " to metadata scan queue.";
	{
		QMutexLocker lock(&m_MtxQueue);
		m_Queue.push_back(a_Song);
	}
	m_SongAvailable.wakeAll();
}





void MetadataScanner::run()
{
	while (true)
	{
		auto song = getSongToProcess();
		if (song == nullptr)
		{
			return;
		}
		qDebug() << __FUNCTION__ << ": Scanning metadata in " << song->fileName();
		processSong(song);
		qDebug() << __FUNCTION__ << ": Metadata scanned in " << song->fileName();
	}
}





SongPtr MetadataScanner::getSongToProcess()
{
	if (m_ShouldTerminate.load())
	{
		return nullptr;
	}
	QMutexLocker lock(&m_MtxQueue);
	while (m_Queue.empty())
	{
		m_SongAvailable.wait(&m_MtxQueue);
		if (m_ShouldTerminate.load())
		{
			return nullptr;
		}
	}
	assert(!m_Queue.empty());
	auto res = m_Queue.back();
	m_Queue.pop_back();
	return res;
}





void MetadataScanner::processSong(SongPtr a_Song)
{
	SongProcessor proc(a_Song);
	proc.process();
	emit songScanned(a_Song.get());
}

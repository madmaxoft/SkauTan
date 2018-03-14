#include "MetadataScanner.h"
#include <assert.h>
#include <QDebug>
#include <QRegularExpression>
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
		auto comment = QString::fromStdString(tag->comment().to8Bit(true));
		comment = tryMatchBPM(comment);
		comment = tryMatchGenreMPM(comment);
		Q_UNUSED(comment);
		auto genre = QString::fromStdString(tag->genre().to8Bit(true));
		genre = tryMatchGenreMPM(genre);
		Q_UNUSED(genre);
	}


	/** Attempts to parse the filename into metadata.
	Assumes that most of our songs have some info in their filename or the containing folders. */
	void parseFileNameIntoMetadata(const QString & a_FileName)
	{
		auto idxLastSlash = a_FileName.lastIndexOf('/');
		if (idxLastSlash < 0)
		{
			return;
		}

		// Auto-detect genre from the parent folders names:
		if (!m_Song->genre().isValid())
		{
			auto parents = a_FileName.split('/', QString::SkipEmptyParts);
			for (const auto & p: parents)
			{
				tryMatchGenreMPM(p);
				if (m_Song->genre().isValid())  // Did we assign a genre?
				{
					break;
				}
			}
		}

		// Detect and remove genre + MPM from filename substring:
		auto fileBareName = a_FileName.mid(idxLastSlash + 1);
		auto idxExt = fileBareName.lastIndexOf('.');
		if (idxExt >= 0)
		{
			fileBareName.remove(idxExt, fileBareName.length());
		}
		fileBareName = tryMatchGenreMPM(fileBareName);
		fileBareName = tryMatchBPM(fileBareName);

		// Try split into author - title:
		auto idxSeparator = fileBareName.indexOf(" - ");
		if (idxSeparator < 0)
		{
			// No separator, consider the entire filename the title:
			if (!m_Song->title().isValid())
			{
				m_Song->setTitle(fileBareName);
			}
		}
		else
		{
			if (!m_Song->title().isValid() && !m_Song->author().isValid())
			{
				m_Song->setAuthor(fileBareName.mid(0, idxSeparator).trimmed());
				m_Song->setTitle(fileBareName.mid(idxSeparator + 3).trimmed());
			}
		}
	}


	/** Attempts to find the genre in the specified string.
	Detects strings such as "SW" and "SW30".
	If found, and the song doesn't have its genre / MPM set, it is set into the song.
	Returns the string excluding the genre / MPM substring. */
	QString tryMatchGenreMPM(const QString & a_Input)
	{
		auto res = a_Input;

		// Pairs of regexp -> genre
		static const std::vector<std::pair<QRegularExpression, QString>> genreMap =
		{
			// Shortcut-based genre + optional MPM:
			{ QRegularExpression("(^|[\\W])SW(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "SW" },
			{ QRegularExpression("(^|[\\W])TG(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "TG" },
			{ QRegularExpression("(^|[\\W])TA(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "TG" },
			{ QRegularExpression("(^|[\\W])VW(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "VW" },
			{ QRegularExpression("(^|[\\W])SF(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "SF" },
			{ QRegularExpression("(^|[\\W])QS(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "QS" },
			{ QRegularExpression("(^|[\\W])SB(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "SB" },
			{ QRegularExpression("(^|[\\W])SA(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "SB" },
			{ QRegularExpression("(^|[\\W])CH(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "CH" },
			{ QRegularExpression("(^|[\\W])CC(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "CH" },
			{ QRegularExpression("(^|[\\W])RU(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "RU" },
			{ QRegularExpression("(^|[\\W])RB(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "RU" },
			{ QRegularExpression("(^|[\\W])PD(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "PD" },
			{ QRegularExpression("(^|[\\W])JI(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "JI" },
			{ QRegularExpression("(^|[\\W])BL(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "BL" },
			{ QRegularExpression("(^|[\\W])PO(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "PO" },

			// Full genre name + optional MPM:
			{ QRegularExpression("(^|[\\W])slow[-\\s]?waltz\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",    QRegularExpression::CaseInsensitiveOption), "SW" },
			{ QRegularExpression("(^|[\\W])tango\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",               QRegularExpression::CaseInsensitiveOption), "TG" },
			{ QRegularExpression("(^|[\\W])valčík\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",              QRegularExpression::CaseInsensitiveOption), "VW" },
			{ QRegularExpression("(^|[\\W])viennese[-\\s]waltz\\s?(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "VW" },
			{ QRegularExpression("(^|[\\W])slow\\-?fox(trot)?\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  QRegularExpression::CaseInsensitiveOption), "SF" },
			{ QRegularExpression("(^|[\\W])quick\\-?step\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",           QRegularExpression::CaseInsensitiveOption), "QS" },
			{ QRegularExpression("(^|[\\W])samba\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",               QRegularExpression::CaseInsensitiveOption), "SB" },
			{ QRegularExpression("(^|[\\W])cha\\-?cha(\\-?cha)\\s?(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "CH" },
			{ QRegularExpression("(^|[\\W])rh?umba\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",             QRegularExpression::CaseInsensitiveOption), "RU" },
			{ QRegularExpression("(^|[\\W])paso[-\\s]?doble\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",    QRegularExpression::CaseInsensitiveOption), "PD" },
			{ QRegularExpression("(^|[\\W])jive\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                QRegularExpression::CaseInsensitiveOption), "JI" },
			{ QRegularExpression("(^|[\\W])blues\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",               QRegularExpression::CaseInsensitiveOption), "BL" },
			{ QRegularExpression("(^|[\\W])polka\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",               QRegularExpression::CaseInsensitiveOption), "PO" },
		};

		for (const auto & p: genreMap)
		{
			auto match = p.first.match(a_Input);
			if (!match.hasMatch())
			{
				continue;
			}
			if (!m_Song->measuresPerMinute().isValid())
			{
				m_Song->setMeasuresPerMinute(match.captured("mpm").toInt());
			}
			if (!m_Song->genre().isValid())
			{
				m_Song->setGenre(p.second);
			}
			auto prefix = (match.capturedStart(1) > 0) ? a_Input.left(match.capturedStart(1) - 1) : QString();
			return prefix + " " + a_Input.mid(match.capturedEnd("end"));
		}
		return res;
	}


	/** Detects and removes MPM from the "[30 BPM]" substring.
	If the substring is found and song has no MPM set, it is set into the song.
	Returns the input string after removing the potential match.
	Assumes that the match is not in the middle of "author - title", but rather at either end. */
	QString tryMatchBPM(const QString & a_Input)
	{
		static const QRegularExpression re("(.*?)(\\d+) bpm(.*)", QRegularExpression::CaseInsensitiveOption);
		auto match = re.match(a_Input);
		if (match.hasMatch())
		{
			if (!m_Song->measuresPerMinute().isValid())
			{
				m_Song->setMeasuresPerMinute(match.captured(2).toInt());
			}
			auto prefix = match.captured(1);
			if (prefix.length() < 4)
			{
				prefix.clear();
			}
			if (prefix.endsWith('['))
			{
				prefix.chop(1);
			}
			auto suffix = match.captured(3);
			if (suffix.length() < 4)
			{
				suffix.clear();
			}
			if (suffix.startsWith(']'))
			{
				suffix.remove(0, 1);
			}
			// Decide whether to further use the suffix or the prefix, regular filenames don't have BPM in the middle
			// Just use the longer one:
			if (suffix.length() > prefix.length())
			{
				return suffix;
			}
			else
			{
				return prefix;
			}
		}
		return a_Input;
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

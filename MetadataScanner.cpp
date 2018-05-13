#include "MetadataScanner.h"
#include <assert.h>
#include <QDebug>
#include <QRegularExpression>
#include <taglib/fileref.h>
#include "Song.h"
#include "BackgroundTasks.h"





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
			qDebug() << ": Unable to parse file.";
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
			qDebug() << ": AudioProperties not found.";
		}

		// Set the author / title:
		auto tag = fr.tag();
		if (tag == nullptr)
		{
			qDebug() << ": No TagLib-extractable information found";
			return;
		}
		Song::Tag songTag;
		songTag.m_Author = QString::fromStdString(tag->artist().to8Bit(true));
		songTag.m_Title = QString::fromStdString(tag->title().to8Bit(true));
		auto comment = QString::fromStdString(tag->comment().to8Bit(true));
		comment = tryMatchBPM(comment, songTag);
		comment = tryMatchGenreMPM(comment, songTag);
		Q_UNUSED(comment);
		auto genre = QString::fromStdString(tag->genre().to8Bit(true));
		genre = tryMatchGenreMPM(genre, songTag);
		m_Song->setId3Tag(songTag);
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
		Song::Tag songTag;
		auto parents = a_FileName.split('/', QString::SkipEmptyParts);
		for (const auto & p: parents)
		{
			tryMatchGenreMPM(p, songTag);
			if (songTag.m_Genre.isValid())  // Did we assign a genre?
			{
				break;
			}
		}

		// Detect and remove genre + MPM from filename substring:
		auto fileBareName = a_FileName.mid(idxLastSlash + 1);
		auto idxExt = fileBareName.lastIndexOf('.');
		if (idxExt >= 0)
		{
			fileBareName.remove(idxExt, fileBareName.length());
		}
		fileBareName = tryMatchGenreMPM(fileBareName, songTag);
		fileBareName = tryMatchBPM(fileBareName, songTag);

		// Try split into author - title:
		auto idxSeparator = fileBareName.indexOf(" - ");
		if (idxSeparator < 0)
		{
			// No separator, consider the entire filename the title:
			songTag.m_Title = fileBareName;
		}
		else
		{
			songTag.m_Author = fileBareName.mid(0, idxSeparator).trimmed();
			songTag.m_Title = fileBareName.mid(idxSeparator + 3).trimmed();
		}
		m_Song->setFileNameTag(songTag);
	}


	/** Attempts to find the genre and MPM in the specified string.
	Detects strings such as "SW" and "SW30". Any found matches are set into a_OutputTag.
	Returns the string excluding the genre / MPM substring. */
	QString tryMatchGenreMPM(const QString & a_Input, Song::Tag & a_OutputTag)
	{
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
			{ QRegularExpression("(^|[\\W])valčík\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                    QRegularExpression::CaseInsensitiveOption), "VW" },  // Must be earlier than SW, because many VW songs contain "waltz" that would match SW too
			{ QRegularExpression("(^|[\\W])valcik\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                    QRegularExpression::CaseInsensitiveOption), "VW" },  // Must be earlier than SW, because many VW songs contain "waltz" that would match SW too
			{ QRegularExpression("(^|[\\W])vien(nese|n?a)[-\\s]waltz\\s?(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "VW" },  // Must be earlier than SW, because "vienna waltz" matches SW too
			{ QRegularExpression("(^|[\\W])(slow[-\\s]?)?waltz\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",       QRegularExpression::CaseInsensitiveOption), "SW" },
			{ QRegularExpression("(^|[\\W])tango\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                     QRegularExpression::CaseInsensitiveOption), "TG" },
			{ QRegularExpression("(^|[\\W])slow\\-?fox(trot)?\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",        QRegularExpression::CaseInsensitiveOption), "SF" },
			{ QRegularExpression("(^|[\\W])quick\\-?step\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",             QRegularExpression::CaseInsensitiveOption), "QS" },
			{ QRegularExpression("(^|[\\W])samba\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                     QRegularExpression::CaseInsensitiveOption), "SB" },
			{ QRegularExpression("(^|[\\W])cha\\-?cha(\\-?cha)\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",       QRegularExpression::CaseInsensitiveOption), "CH" },
			{ QRegularExpression("(^|[\\W])rh?umba\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                   QRegularExpression::CaseInsensitiveOption), "RU" },
			{ QRegularExpression("(^|[\\W])paso[-\\s]?doble\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",          QRegularExpression::CaseInsensitiveOption), "PD" },
			{ QRegularExpression("(^|[\\W])jive\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                      QRegularExpression::CaseInsensitiveOption), "JI" },
			{ QRegularExpression("(^|[\\W])blues\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                     QRegularExpression::CaseInsensitiveOption), "BL" },
			{ QRegularExpression("(^|[\\W])polka\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                     QRegularExpression::CaseInsensitiveOption), "PO" },
			{ QRegularExpression("(^|[\\W])salsa\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                     QRegularExpression::CaseInsensitiveOption), "SL" },
			{ QRegularExpression("(^|[\\W])rozcvičky\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                 QRegularExpression::CaseInsensitiveOption), "RO" },
		};

		for (const auto & p: genreMap)
		{
			auto match = p.first.match(a_Input);
			if (!match.hasMatch())
			{
				continue;
			}
			if (a_OutputTag.m_Genre.isValid() && (a_OutputTag.m_Genre.toString() != p.second))
			{
				qDebug() << "String \"" << a_Input << "\" has matched genre " << p.second
					<< ", but already has genre " << a_OutputTag.m_Genre.toString() << ", ignoring the match";
				continue;
			}
			bool isOK = false;
			auto mpm = match.captured("mpm").toInt(&isOK);
			if (isOK && (mpm > 0))
			{
				a_OutputTag.m_MeasuresPerMinute = mpm;
			}
			a_OutputTag.m_Genre = p.second;
			auto prefix = (match.capturedStart(1) > 0) ? a_Input.left(match.capturedStart(1) - 1) : QString();
			return prefix + " " + a_Input.mid(match.capturedEnd("end"));
		}
		return a_Input;
	}


	/** Detects and removes MPM from the "[30 BPM]" substring.
	If the substring is found and song has no MPM set, it is set into the song.
	Returns the input string after removing the potential match.
	Assumes that the match is not in the middle of "author - title", but rather at either end. */
	QString tryMatchBPM(const QString & a_Input, Song::Tag & a_OutputTag)
	{
		static const QRegularExpression re("(.*?)(\\d+) bpm(.*)", QRegularExpression::CaseInsensitiveOption);
		auto match = re.match(a_Input);
		if (match.hasMatch())
		{
			bool isOK = false;
			auto mpm = match.captured(2).toInt(&isOK);
			if (isOK && (mpm > 0))
			{
				a_OutputTag.m_MeasuresPerMinute = mpm;
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
	m_QueueLength(0)
{
}





void MetadataScanner::queueScanSong(SongPtr a_Song)
{
	m_QueueLength += 1;
	BackgroundTasks::enqueue(tr("Scan metadata: %1").arg(a_Song->fileName()), [this, a_Song]()
		{
			SongProcessor proc(a_Song);
			proc.process();
			m_QueueLength -= 1;
			emit this->songScanned(a_Song);
		}
	);
}

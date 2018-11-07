#include "MetadataScanner.hpp"
#include <cassert>
#include <QDebug>
#include <QRegularExpression>
#include <QTextCodec>
#include <QFile>
#include <taglib/fileref.h>
#include <taglib/mp4file.h>
#include <taglib/apefile.h>
#include <taglib/mpegfile.h>
#include <taglib/tpropertymap.h>
#include "Song.hpp"
#include "BackgroundTasks.hpp"
#include "DatedOptional.hpp"





/** Sets the property in the TagLib property map to the specified value.
If the value is not present, clears the property value instead. */
static void setOrClearProp(TagLib::PropertyMap & a_Props, const char * a_PropName, const DatedOptional<QString> & a_Value)
{
	if (a_Value.isPresent())
	{
		a_Props[a_PropName] = TagLib::StringList(
			TagLib::String(a_Value.value().toStdString(), TagLib::String::Type::UTF8)
		);
	}
	else
	{
		a_Props.erase(a_PropName);
	}
}





/** Any textual invalid variant in the specified tag gets replaced with an empty string. */
static void validateSongTag(Song::Tag & a_Tag)
{
	if (a_Tag.m_Author.isEmpty())
	{
		a_Tag.m_Author = QString("");
	}
	if (a_Tag.m_Title.isEmpty())
	{
		a_Tag.m_Title = QString("");
	}
	if (a_Tag.m_Genre.isEmpty())
	{
		a_Tag.m_Genre = QString("");
	}
}




/** Attempts to find the genre and MPM in the specified string.
Detects strings such as "SW" and "SW30". Any found matches are set into a_OutputTag.
Returns the string excluding the genre / MPM substring. */
static QString tryMatchGenreMPM(const QString & a_Input, Song::Tag & a_OutputTag)
{
	// Pairs of regexp -> genre
	static const std::vector<std::pair<QRegularExpression, QString>> genreMap =
	{
		// Shortcut-based genre + optional MPM:
		{ QRegularExpression("(^|[\\W])SW(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "SW" },
		{ QRegularExpression("(^|[\\W])LW(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "SW" },
		{ QRegularExpression("(^|[\\W])EW(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "SW" },
		{ QRegularExpression("(^|[\\W])TG(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "TG" },
		{ QRegularExpression("(^|[\\W])TA(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "TG" },
		{ QRegularExpression("(^|[\\W])VW(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "VW" },
		{ QRegularExpression("(^|[\\W])VV(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "VW" },
		{ QRegularExpression("(^|[\\W])WW(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "VW" },
		{ QRegularExpression("(^|[\\W])SF(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "SF" },
		{ QRegularExpression("(^|[\\W])QS(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "QS" },
		{ QRegularExpression("(^|[\\W])SB(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "SB" },
		{ QRegularExpression("(^|[\\W])SA(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "SB" },
		{ QRegularExpression("(^|[\\W])SL(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "SL" },
		{ QRegularExpression("(^|[\\W])CH(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "CH" },
		{ QRegularExpression("(^|[\\W])CC(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "CH" },
		{ QRegularExpression("(^|[\\W])RU(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "RU" },
		{ QRegularExpression("(^|[\\W])RB(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "RU" },
		{ QRegularExpression("(^|[\\W])PD(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "PD" },
		{ QRegularExpression("(^|[\\W])JI(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "JI" },
		{ QRegularExpression("(^|[\\W])JV(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "JI" },
		{ QRegularExpression("(^|[\\W])BL(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "BL" },
		{ QRegularExpression("(^|[\\W])PO(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "PO" },

		// Full genre name + optional MPM:
		{ QRegularExpression("(^|[\\W])valčík\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                    QRegularExpression::CaseInsensitiveOption), "VW" },  // Must be earlier than SW, because many VW songs contain "waltz" that would match SW too
		{ QRegularExpression("(^|[\\W])valcik\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                    QRegularExpression::CaseInsensitiveOption), "VW" },  // Must be earlier than SW, because many VW songs contain "waltz" that would match SW too
		{ QRegularExpression("(^|[\\W])v[.-]?\\s?waltz\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",           QRegularExpression::CaseInsensitiveOption), "VW" },  // Must be earlier than SW, because "v. waltz" matches SW too
		{ QRegularExpression("(^|[\\W])vien(nese|n?a)[-\\s]waltz\\s?(?<mpm>\\d*)(?<end>[\\W]|$)", QRegularExpression::CaseInsensitiveOption), "VW" },  // Must be earlier than SW, because "vienna waltz" matches SW too
		{ QRegularExpression("(^|[\\W])(slow[-\\s]?)?waltz\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",       QRegularExpression::CaseInsensitiveOption), "SW" },
		{ QRegularExpression("(^|[\\W])tango\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                     QRegularExpression::CaseInsensitiveOption), "TG" },
		{ QRegularExpression("(^|[\\W])slow[-\\s]?fox(trot)?\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",     QRegularExpression::CaseInsensitiveOption), "SF" },
		{ QRegularExpression("(^|[\\W])quick[-\\s]?step\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",          QRegularExpression::CaseInsensitiveOption), "QS" },
		{ QRegularExpression("(^|[\\W])samba\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                     QRegularExpression::CaseInsensitiveOption), "SB" },
		{ QRegularExpression("(^|[\\W])cha[-\\s]?cha(\\-?cha)?\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",   QRegularExpression::CaseInsensitiveOption), "CH" },
		{ QRegularExpression("(^|[\\W])rh?umba\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                   QRegularExpression::CaseInsensitiveOption), "RU" },
		{ QRegularExpression("(^|[\\W])paso[-\\s]?(doble)?\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",       QRegularExpression::CaseInsensitiveOption), "PD" },
		{ QRegularExpression("(^|[\\W])jive\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                      QRegularExpression::CaseInsensitiveOption), "JI" },
		{ QRegularExpression("(^|[\\W])blues\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                     QRegularExpression::CaseInsensitiveOption), "BL" },
		{ QRegularExpression("(^|[\\W])polk[ay]\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                  QRegularExpression::CaseInsensitiveOption), "PO" },
		{ QRegularExpression("(^|[\\W])mazurk[ay]\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                QRegularExpression::CaseInsensitiveOption), "MA" },
		{ QRegularExpression("(^|[\\W])salsa\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                     QRegularExpression::CaseInsensitiveOption), "SL" },
		{ QRegularExpression("(^|[\\W])rozcvi[cč]k[ay]\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",           QRegularExpression::CaseInsensitiveOption), "RO" },
	};

	for (const auto & p: genreMap)
	{
		auto match = p.first.match(a_Input);
		if (!match.hasMatch())
		{
			continue;
		}
		if (!a_OutputTag.m_Genre.isEmpty() && (a_OutputTag.m_Genre.value() != p.second))
		{
			qDebug() << "String \"" << a_Input << "\" has matched genre " << p.second
				<< ", but already has genre " << a_OutputTag.m_Genre.value() << ", ignoring the match";
			continue;
		}
		bool isOK = false;
		auto mpm = match.captured("mpm").toInt(&isOK);
		if (isOK && (mpm > 0))
		{
			a_OutputTag.m_MeasuresPerMinute = mpm;
		}
		a_OutputTag.m_Genre = p.second;
		auto prefix = (match.capturedStart(1) > 0) ? a_Input.left(match.capturedStart(1)) : QString();
		return prefix + " " + a_Input.mid(match.capturedEnd("end"));
	}
	return a_Input;
}





/** Detects and removes MPM from the "[30 BPM]" substring.
If the substring is found and song has no MPM set, it is set into the song.
Returns the input string after removing the potential match.
Assumes that the match is not in the middle of "author - title", but rather at either end. */
static QString tryMatchBPM(const QString & a_Input, Song::Tag & a_OutputTag)
{
	static const QRegularExpression re("(.*?)(\\d+)\\s?[bmt]pm(.*)", QRegularExpression::CaseInsensitiveOption);
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
			prefix = prefix.trimmed();
		}
		auto suffix = match.captured(3);
		if (suffix.length() < 4)
		{
			suffix.clear();
		}
		if (suffix.startsWith(']'))
		{
			suffix = suffix.remove(0, 1).trimmed();
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





/** Extracts as much metadata form the input string as possible.
The extracted data is saved into a_OutputTag and removed from the string. */
static QString extract(const QString & a_Input, Song::Tag & a_OutputTag)
{
	// First process all parentheses, brackets and braces:
	QString intermediate;
	auto len = a_Input.length();
	int last = 0;  // last boundary of the outer-most parenthesis / bracket / brace
	int numNestingLevels = 0;
	bool hasExtracted = false;
	for (int i = 0; i < len; ++i)
	{
		switch (a_Input[i].unicode())
		{
			case '(':
			case '{':
			case '[':
			{
				if (numNestingLevels == 0)
				{
					if (i > last)
					{
						intermediate.append(a_Input.mid(last, i - last));
					}
					last = i + 1;
				}
				numNestingLevels += 1;
				break;
			}
			case ')':
			case '}':
			case ']':
			{
				numNestingLevels -= 1;
				if (numNestingLevels == 0)
				{
					auto subBlock = a_Input.mid(last, i - last);
					auto outBlock = tryMatchGenreMPM(tryMatchBPM(subBlock, a_OutputTag), a_OutputTag);
					if (subBlock == outBlock)
					{
						// No extractable information within this parenthesis, just append all of it:
						intermediate.append(subBlock);
					}
					else
					{
						// This block had metadata, do NOT append any of it
						hasExtracted = true;
					}
					last = i + 1;
				}
				break;
			}
		}
	}  // for i - a_Input[]
	if (last < len)
	{
		intermediate.append(a_Input.mid(last, len - last));
	}

	// If anything was extracted from the parentheses, don't extract anything from the leftover text:
	if (hasExtracted)
	{
		return intermediate;
	}

	// Nothing useful in parentheses, run extraction on the bare text:
	return tryMatchGenreMPM(tryMatchBPM(intermediate, a_OutputTag), a_OutputTag);
}





////////////////////////////////////////////////////////////////////////////////
// MetadataScanner:

MetadataScanner::MetadataScanner():
	m_QueueLength(0)
{
}





void MetadataScanner::writeTagToSong(SongPtr a_Song, const Tag & a_Tag)
{
	auto fr = openTagFile(a_Song->fileName());
	if (fr.isNull())
	{
		// File format not recognized
		qDebug() << "Unable to parse file " << a_Song->fileName();
		return;
	}
	auto props = fr.file()->properties();
	setOrClearProp(props, "ARTIST",  a_Tag.m_Author);
	setOrClearProp(props, "TITLE",   a_Tag.m_Title);
	setOrClearProp(props, "GENRE",   a_Tag.m_Genre);
	setOrClearProp(props, "COMMENT", a_Tag.m_Comment);
	if (a_Tag.m_MeasuresPerMinute.isPresent())
	{
		props["BPM"] = TagLib::StringList(QString::number(a_Tag.m_MeasuresPerMinute.value()).toStdString());
	}
	else
	{
		props.erase("BPM");
	}
	auto refused = fr.file()->setProperties(props);
	if (!refused.isEmpty())
	{
		qWarning() << "Failed to set the following properties on song " << a_Song->fileName();
		for (const auto & prop: refused)
		{
			qWarning()
				<< "  " << prop.first.to8Bit(true).c_str()
				<< " (" << prop.second.toString(" | ").to8Bit(true).c_str()
			;
		}
	}
	if (!fr.save())
	{
		qWarning() << "Failed to save the new tag into song " << a_Song->fileName();
	}

	// Update the detected genre in the song:
	a_Song->setId3Tag(parseId3Tag(a_Tag));
}





std::pair<bool, MetadataScanner::Tag> MetadataScanner::readTagFromFile(const QString & a_FileName) noexcept
{
	MetadataScanner::Tag res;
	auto fr = openTagFile(a_FileName);
	if (fr.isNull())
	{
		// File format not recognized
		qDebug() << "Unable to parse file " << a_FileName;
		return std::make_pair(false, res);
	}
	auto tag = fr.tag();
	if (tag == nullptr)
	{
		qDebug() << "No TagLib-extractable information found in " << a_FileName;
		return std::make_pair(false, res);
	}

	// Extract the tag:
	res.m_Author = QString::fromStdString(tag->artist().to8Bit(true));
	res.m_Title = QString::fromStdString(tag->title().to8Bit(true));
	res.m_Comment = QString::fromStdString(tag->comment().to8Bit(true));
	res.m_Genre = QString::fromStdString(tag->genre().to8Bit(true));

	// Extract the MPM from BPM in the extended properties:
	for (const auto & prop: fr.file()->properties())
	{
		if (prop.first == "BPM")
		{
			bool isOK;
			auto bpm = QString::fromStdString(prop.second.toString().to8Bit(true)).toDouble(&isOK);
			if (isOK)
			{
				res.m_MeasuresPerMinute = bpm;
			}
		}
	}
	return std::make_pair(true, res);
}





Song::Tag MetadataScanner::parseId3Tag(const MetadataScanner::Tag & a_FileTag)
{
	Song::Tag res;
	if (a_FileTag.m_MeasuresPerMinute.isPresent())
	{
		res.m_MeasuresPerMinute = a_FileTag.m_MeasuresPerMinute;
	}
	/* genre = */   extract(a_FileTag.m_Genre.valueOrDefault(), res);
	/* comment = */ extract(a_FileTag.m_Comment.valueOrDefault(), res);
	res.m_Author =  extract(a_FileTag.m_Author.valueOrDefault(), res);
	res.m_Title =   extract(a_FileTag.m_Title.valueOrDefault(), res);
	return res;
}





MetadataScanner::Tag MetadataScanner::applyTagChanges(const MetadataScanner::Tag & a_FileTag, const MetadataScanner::Tag & a_Changes)
{
	auto res = a_FileTag;
	if (a_Changes.m_Author.isPresent())
	{
		res.m_Author = a_Changes.m_Author;
	}
	if (a_Changes.m_Title.isPresent())
	{
		res.m_Title = a_Changes.m_Title;
	}
	if (a_Changes.m_Genre.isPresent())
	{
		res.m_Genre = a_Changes.m_Genre;
	}
	if (a_Changes.m_Comment.isPresent())
	{
		res.m_Comment = a_Changes.m_Comment;
	}
	if (a_Changes.m_MeasuresPerMinute.isPresent())
	{
		res.m_MeasuresPerMinute = a_Changes.m_MeasuresPerMinute;
	}
	return res;
}





void MetadataScanner::enqueueScan(SongPtr a_Song, bool a_Prioritize)
{
	m_QueueLength += 1;
	BackgroundTasks::enqueue(tr("Scan metadata: %1").arg(a_Song->fileName()), [this, a_Song]()
		{
			scanSong(a_Song);
			m_QueueLength -= 1;
		},
		a_Prioritize
	);
}





TagLib::FileRef MetadataScanner::openTagFile(const QString & a_FileName)
{
	#ifdef _WIN32
		// TagLib on Windows needs UTF16-BE filenames (#134):
		return TagLib::FileRef(reinterpret_cast<const wchar_t *>(a_FileName.constData()), false);
	#else
		return TagLib::FileRef(a_FileName.toUtf8().constData(), false);
	#endif
}





Song::Tag MetadataScanner::parseFileNameIntoMetadata(const QString & a_FileName)
{
	Song::Tag songTag;
	auto idxLastSlash = a_FileName.lastIndexOf('/');
	if (idxLastSlash < 0)
	{
		return songTag;
	}

	// Auto-detect genre from the parent folders names:
	auto parents = a_FileName.split('/', QString::SkipEmptyParts);
	for (const auto & p: parents)
	{
		tryMatchGenreMPM(p, songTag);
		if (!songTag.m_Genre.isEmpty())  // Did we assign a genre?
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
	fileBareName = extract(fileBareName, songTag);

	// TODO: Remove a possible numerical prefix ("01 - ", "01." etc.)

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
	validateSongTag(songTag);
	return songTag;
}





void MetadataScanner::queueScanSong(SongPtr a_Song)
{
	enqueueScan(a_Song, false);
}





void MetadataScanner::queueScanSongPriority(SongPtr a_Song)
{
	enqueueScan(a_Song, true);
}





void MetadataScanner::scanSong(SongPtr a_Song)
{
	auto id3Tag = readTagFromFile(a_Song->fileName());
	if (id3Tag.first)
	{
		auto parsedId3Tag = parseId3Tag(id3Tag.second);
		validateSongTag(parsedId3Tag);
		a_Song->setId3Tag(parsedId3Tag);
	}
	auto fileTag = parseFileNameIntoMetadata(a_Song->fileName());
	a_Song->setFileNameTag(fileTag);

	emit songScanned(a_Song);
}

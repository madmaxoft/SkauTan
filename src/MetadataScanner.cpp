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





/** Any textual invalid variant in the specified tag gets replaced with an empty string.
Also adjusts MPM, based on the genre (if present). */
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
	else
	{
		// Valid genre, use it to adjust MPM, if needed:
		if (a_Tag.m_MeasuresPerMinute.isPresent())
		{
			a_Tag.m_MeasuresPerMinute = Song::adjustMpm(a_Tag.m_MeasuresPerMinute.value(), a_Tag.m_Genre.value());
		}
	}
}




/** Returns whether the value is a reasonable MPM or BPM (in the (10, 300) range. */
static bool isReasonableMpm(double a_Mpm)
{
	return (a_Mpm > 10) && (a_Mpm < 300);
}





/** Scans the string from the start, removes anything non-alpha. */
static QString stripBeginningNonAlpha(const QString & a_Input)
{
	int len = a_Input.length();
	for (int i = 0; i < len; ++i)
	{
		if (a_Input[i].isLetter())
		{
			// If there are any digits in front, include them in the result:
			while ((i > 0) && a_Input[i - 1].isLetterOrNumber())
			{
				--i;
			}
			return a_Input.mid(i, len - i);
		}
	}
	return QString();
}





/** Attempts to find the genre and MPM in the specified string.
Detects strings such as "SW" and "SW30". Any found matches are set into a_OutputTag.
Returns the string excluding the genre / MPM substring.
If a long description of the genre is found, it is only stripped if the BPM is present as well
or a_StripLongGenre is true. If the entire string specifies a genre (there are no characters outside
the regexp match), the entire string is stripped. */
static QString tryMatchGenreMPM(const QString & a_Input, Song::Tag & a_OutputTag, bool a_StripLongGenre = true)
{
	// Regexp -> Genre, LongStrip
	struct GenreRE
	{
		QRegularExpression m_RegExp;
		QString m_Genre;
		bool m_IsLongStrip;

		GenreRE(
			const char * a_RegExpStr,
			const char * a_GenreStr,
			bool a_IsLongStrip
		):
			m_RegExp(a_RegExpStr, QRegularExpression::CaseInsensitiveOption),
			m_Genre(a_GenreStr),
			m_IsLongStrip(a_IsLongStrip)
		{
		}
	};

	static const std::vector<GenreRE> genreMap =
	{
		// Shortcut-based genre + optional MPM:
		{ "(^|[\\W])SW\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  "SW", false },
		{ "(^|[\\W])LW\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  "SW", false },
		{ "(^|[\\W])EW\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  "SW", false },
		{ "(^|[\\W])TG\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  "TG", false },
		{ "(^|[\\W])TA\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  "TG", false },
		{ "(^|[\\W])VW\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  "VW", false },
		{ "(^|[\\W])VV\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  "VW", false },
		{ "(^|[\\W])WW\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  "VW", false },
		{ "(^|[\\W])SF\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  "SF", false },
		{ "(^|[\\W])QS\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  "QS", false },
		{ "(^|[\\W])SB\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  "SB", false },
		{ "(^|[\\W])SA\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  "SB", false },
		{ "(^|[\\W])SL\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  "SL", false },
		{ "(^|[\\W])CH\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  "CH", false },
		{ "(^|[\\W])CC\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  "CH", false },
		{ "(^|[\\W])CCC\\s?(?<mpm>\\d*)(?<end>[\\W]|$)", "CH", false },
		{ "(^|[\\W])RU\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  "RU", false },
		{ "(^|[\\W])RB\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  "RU", false },
		{ "(^|[\\W])PD\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  "PD", false },
		{ "(^|[\\W])JI\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  "JI", false },
		{ "(^|[\\W])JV\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  "JI", false },
		{ "(^|[\\W])BL\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  "BL", false },
		{ "(^|[\\W])PO\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",  "PO", false },

		// Single-letter genre + MPM (only some genres):
		{ "(^|[\\W])W(?<mpm>\\d+)(?<end>[\\W]|$)", "SW", false },
		{ "(^|[\\W])T(?<mpm>\\d+)(?<end>[\\W]|$)", "TG", false },
		{ "(^|[\\W])V(?<mpm>\\d+)(?<end>[\\W]|$)", "VW", false },
		{ "(^|[\\W])Q(?<mpm>\\d+)(?<end>[\\W]|$)", "QS", false },
		{ "(^|[\\W])S(?<mpm>\\d+)(?<end>[\\W]|$)", "SB", false },
		{ "(^|[\\W])C(?<mpm>\\d+)(?<end>[\\W]|$)", "CH", false },
		{ "(^|[\\W])R(?<mpm>\\d+)(?<end>[\\W]|$)", "RU", false },
		{ "(^|[\\W])J(?<mpm>\\d+)(?<end>[\\W]|$)", "JI", false },

		// Long genre name + optional MPM:
		{ "(^|[\\W])valčík\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                    "VW", true },  // Must be earlier than SW, because many VW songs contain "waltz" that would match SW too
		{ "(^|[\\W])valcik\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                    "VW", true },  // Must be earlier than SW, because many VW songs contain "waltz" that would match SW too
		{ "(^|[\\W])v[.-]?\\s?waltz\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",           "VW", true },  // Must be earlier than SW, because "v. waltz" matches SW too
		{ "(^|[\\W])vien(nese|n?a)[-\\s]waltz\\s?(?<mpm>\\d*)(?<end>[\\W]|$)", "VW", true },  // Must be earlier than SW, because "vienna waltz" matches SW too
		{ "(^|[\\W])(slow[-\\s]?)?waltz\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",       "SW", true },
		{ "(^|[\\W])tango\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                     "TG", true },
		{ "(^|[\\W])slow[-\\s]?fox(trot)?\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",     "SF", true },
		{ "(^|[\\W])quick[-\\s]?step\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",          "QS", true },
		{ "(^|[\\W])samba\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                     "SB", true },
		{ "(^|[\\W])cha[-\\s]?cha(\\-?cha)?\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",   "CH", true },
		{ "(^|[\\W])rh?umba\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                   "RU", true },
		{ "(^|[\\W])paso[-\\s]?(doble)?\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",       "PD", true },
		{ "(^|[\\W])jive\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                      "JI", true },
		{ "(^|[\\W])blues\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                     "BL", true },
		{ "(^|[\\W])polk[ay]\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                  "PO", true },
		{ "(^|[\\W])mazurk[ay]\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                "MA", true },
		{ "(^|[\\W])salsa\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",                     "SL", true },
		{ "(^|[\\W])rozcvi[cč]k[ay]\\s?(?<mpm>\\d*)(?<end>[\\W]|$)",           "RO", true },
	};

	for (const auto & p: genreMap)
	{
		auto match = p.m_RegExp.match(a_Input);
		if (!match.hasMatch())
		{
			continue;
		}
		if (!a_OutputTag.m_Genre.isEmpty() && (a_OutputTag.m_Genre.value() != p.m_Genre))
		{
			qDebug() << "String \"" << a_Input << "\" has matched genre " << p.m_Genre
				<< ", but already has genre " << a_OutputTag.m_Genre.value() << ", ignoring the match";
			continue;
		}
		bool isOK = false;
		auto mpm = match.captured("mpm").toInt(&isOK);
		auto mpmIsPresent = false;
		if (isOK && (mpm > 10) && (mpm < 300))
		{
			a_OutputTag.m_MeasuresPerMinute = mpm;
			mpmIsPresent = true;
		}
		a_OutputTag.m_Genre = p.m_Genre;
		if (p.m_IsLongStrip)
		{
			if ((match.capturedStart(1) == 0) && (match.capturedEnd("end") == a_Input.length()))
			{
				// The entire string is simply the genre, strip it always
				return QString();
			}
		}
		if (!a_StripLongGenre && !mpmIsPresent && p.m_IsLongStrip)
		{
			return a_Input;
		}
		auto prefix = (match.capturedStart(1) > 0) ? a_Input.left(match.capturedStart(1)) : QString();
		return prefix + " " + a_Input.mid(match.capturedEnd("end"));
	}
	return a_Input;
}





/** Detects and removes MPM from the "[30 BPM]" substring or a single number being the entire string.
If the substring is found and the MPM is in the (10, 300) range, it is set into the song.
Returns the input string after removing the potential match.
Assumes that the match is not in the middle of "author - title", but rather at either end. */
static QString tryMatchBPM(const QString & a_Input, Song::Tag & a_OutputTag)
{
	// Search for strings like "28mpm" inside:
	static const QRegularExpression re("(.*?)(\\d+)\\s?[bmt]pm(.*)", QRegularExpression::CaseInsensitiveOption);
	auto match = re.match(a_Input);
	if (match.hasMatch())
	{
		bool isOK = false;
		auto mpm = match.captured(2).toInt(&isOK);
		if (isOK && isReasonableMpm(mpm))
		{
			a_OutputTag.m_MeasuresPerMinute = mpm;
		}
		auto prefix = match.captured(1);
		if (prefix.endsWith('['))
		{
			prefix.chop(1);
			prefix = prefix.trimmed();
		}
		auto suffix = match.captured(3);
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

	// If the input is a number without anything else, consider it MPM as long as it's not too low / too high:
	bool isOK = true;
	auto mpm = a_Input.toInt(&isOK);
	if (isOK && isReasonableMpm(mpm))
	{
		a_OutputTag.m_MeasuresPerMinute = mpm;
		return QString();
	}

	// No match, return the input unchanged:
	return a_Input;
}





/** Extracts as much metadata form the input string as possible.
The extracted data is saved into a_OutputTag and, if appropriate, removed from the string. */
static QString extractFromPart(const QString & a_Input, Song::Tag & a_OutputTag)
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
						intermediate.append('(');
						intermediate.append(subBlock);
						intermediate.append(')');
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

	/*
	// If anything was extracted from the parentheses, don't extract anything from the leftover text:
	if (hasExtracted)
	{
		return stripBeginningNonAlpha(intermediate).simplified();
	}
	*/

	// Nothing useful in parentheses, run extraction on the bare text:
	return stripBeginningNonAlpha(
		tryMatchGenreMPM(
			tryMatchBPM(intermediate, a_OutputTag),
			a_OutputTag, false
		)
	).simplified();
}





/** Returns true if the input string is one of the "forbidden" parts, such as "unknown", "various" etc. */
static bool isForbiddenPart(const QString & a_Input)
{
	static const QRegularExpression reForbidden[] =
	{
		QRegularExpression("^various$",             QRegularExpression::CaseInsensitiveOption),
		QRegularExpression("^various\\sartists?$",  QRegularExpression::CaseInsensitiveOption),
		QRegularExpression("^VA$"),                 // case-sensitive
		QRegularExpression("^unknown$",             QRegularExpression::CaseInsensitiveOption),
		QRegularExpression("^unknown\\sartists?$",  QRegularExpression::CaseInsensitiveOption),
		QRegularExpression("^artist$",              QRegularExpression::CaseInsensitiveOption),
		QRegularExpression("^neznamy\\sinterpret$", QRegularExpression::CaseInsensitiveOption),
		QRegularExpression("^neznámý\\sinterpret$", QRegularExpression::CaseInsensitiveOption),
		QRegularExpression("^track$",               QRegularExpression::CaseInsensitiveOption),
		QRegularExpression("^track\\s?\\d+$",       QRegularExpression::CaseInsensitiveOption),
		QRegularExpression("^unknown\\s?\\d+$",     QRegularExpression::CaseInsensitiveOption),
		QRegularExpression("^stopa\\s?\\d+$",       QRegularExpression::CaseInsensitiveOption),
	};

	for (const auto & re: reForbidden)
	{
		if (re.match(a_Input).hasMatch())
		{
			return true;
		}
	}
	return false;
}





/** Breaks the string into parts delimited by dashes or slashes,
then extracts as much metadata from each part as possible.
The extracted data is saved into a_OutputTag and, if appropriate, removed from the string. */
static QString extract(const QString & a_Input, Song::Tag & a_OutputTag)
{
	// Break the input into parts by all dashes and slashes:
	QStringList parts;
	int last = 0;
	int len = a_Input.length();
	for (int i = 0; i < len; ++i)
	{
		switch (a_Input[i].unicode())
		{
			case '/':
			case '-':
			{
				if (i > last)
				{
					parts.append(a_Input.mid(last, i - last));
				}
				last = i + 1;
				break;
			}
		}
	}
	if (last < len)
	{
		parts.append(a_Input.mid(last, len - last));
	}

	// Process each part:
	QString res;
	for (const auto & part: parts)
	{
		auto simple = part.simplified();
		if (isForbiddenPart(simple))
		{
			continue;
		}
		auto parsed = extractFromPart(simple, a_OutputTag);
		if (parsed.isEmpty() || isForbiddenPart(parsed))
		{
			continue;
		}
		if (!res.isEmpty())
		{
			res.append(" - ");
		}
		res.append(parsed);
	}
	return res;
}





/** If the tag has empty author or title, attempts to split the other value into two.
Handles strings such as "author - title" in a single tag entry.
The tag is changed in-place. */
static void trySplitAuthorTitle(Song::Tag & a_Tag)
{
	if (
		(a_Tag.m_Author.isEmpty() && a_Tag.m_Title.isEmpty()) ||
		(!a_Tag.m_Author.isEmpty() && !a_Tag.m_Title.isEmpty())
	)
	{
		// Nothing to be done in this case
		return;
	}

	// Split into halves on the '-' closest to the string middle:
	auto src = a_Tag.m_Author.isEmpty() ? a_Tag.m_Title.value() : a_Tag.m_Author.value();
	auto halfLen = src.length() / 2;
	for (int i = 0; i < halfLen; ++i)
	{
		if (src[halfLen + i] == '-')
		{
			a_Tag.m_Author = src.left(halfLen + i - 1).simplified();
			a_Tag.m_Title = src.mid(halfLen + i + 1).simplified();
			return;
		}
		else if (src[halfLen - i] == '-')
		{
			a_Tag.m_Author = src.left(halfLen - i - 1).simplified();
			a_Tag.m_Title = src.mid(halfLen - i + 1).simplified();
			return;
		}
	}
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
	/* genre = */   extract(a_FileTag.m_Genre.valueOrDefault(), res);
	/* comment = */ extract(a_FileTag.m_Comment.valueOrDefault(), res);
	res.m_Author =  extract(a_FileTag.m_Author.valueOrDefault(), res);
	res.m_Title =   extract(a_FileTag.m_Title.valueOrDefault(), res);
	if (a_FileTag.m_MeasuresPerMinute.isPresent())
	{
		if (res.m_Genre.isPresent())
		{
			// Use genre to adjust from BPM to MPM:
			res.m_MeasuresPerMinute = Song::adjustMpm(a_FileTag.m_MeasuresPerMinute.value(), res.m_Genre.value());
		}
		else
		{
			// No genre, cannot adjust
			res.m_MeasuresPerMinute = a_FileTag.m_MeasuresPerMinute;
		}
	}
	trySplitAuthorTitle(res);
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





Song::Tag MetadataScanner::parseFileNameIntoMetadata(const QString & a_FileName)
{
	Song::Tag songTag;
	auto idxLastSlash = a_FileName.lastIndexOf('/');
	/*
	if (idxLastSlash < 0)
	{
		return songTag;
	}
	*/

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

	// Try split into author - title:
	songTag.m_Title = fileBareName;
	trySplitAuthorTitle(songTag);
	validateSongTag(songTag);
	return songTag;
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

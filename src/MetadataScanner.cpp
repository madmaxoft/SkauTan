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
static void setOrClearProp(TagLib::PropertyMap & aProps, const char * aPropName, const DatedOptional<QString> & aValue)
{
	if (aValue.isPresent())
	{
		aProps[aPropName] = TagLib::StringList(
			TagLib::String(aValue.value().toStdString(), TagLib::String::Type::UTF8)
		);
	}
	else
	{
		aProps.erase(aPropName);
	}
}





/** Any textual invalid variant in the specified tag gets replaced with an empty string.
Also adjusts MPM, based on the genre (if present). */
static void validateSongTag(Song::Tag & aTag)
{
	if (aTag.mAuthor.isEmpty())
	{
		aTag.mAuthor = QString("");
	}
	if (aTag.mTitle.isEmpty())
	{
		aTag.mTitle = QString("");
	}
	if (aTag.mGenre.isEmpty())
	{
		aTag.mGenre = QString("");
	}
	else
	{
		// Valid genre, use it to adjust MPM, if needed:
		if (aTag.mMeasuresPerMinute.isPresent())
		{
			aTag.mMeasuresPerMinute = Song::adjustMpm(aTag.mMeasuresPerMinute.value(), aTag.mGenre.value());
		}
	}
}




/** Returns whether the value is a reasonable MPM or BPM (in the (10, 300) range. */
static bool isReasonableMpm(double aMpm)
{
	return (aMpm > 10) && (aMpm < 300);
}





/** Scans the string from the start, removes anything non-alpha. */
static QString stripBeginningNonAlpha(const QString & aInput)
{
	int len = aInput.length();
	for (int i = 0; i < len; ++i)
	{
		if (aInput[i].isLetter())
		{
			// If there are any digits in front, include them in the result:
			while ((i > 0) && aInput[i - 1].isLetterOrNumber())
			{
				--i;
			}
			return aInput.mid(i, len - i);
		}
	}
	return QString();
}





/** Attempts to find the genre and MPM in the specified string.
Detects strings such as "SW" and "SW30". Any found matches are set into aOutputTag.
Returns the string excluding the genre / MPM substring.
If a long description of the genre is found, it is only stripped if the BPM is present as well
or aStripLongGenre is true. If the entire string specifies a genre (there are no characters outside
the regexp match), the entire string is stripped. */
static QString tryMatchGenreMPM(const QString & aInput, Song::Tag & aOutputTag, bool aStripLongGenre = true)
{
	// Regexp -> Genre, LongStrip
	struct GenreRE
	{
		QRegularExpression mRegExp;
		QString mGenre;
		bool mIsLongStrip;

		GenreRE(
			const char * aRegExpStr,
			const char * aGenreStr,
			bool aIsLongStrip
		):
			mRegExp(aRegExpStr, QRegularExpression::CaseInsensitiveOption),
			mGenre(aGenreStr),
			mIsLongStrip(aIsLongStrip)
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
		auto match = p.mRegExp.match(aInput);
		if (!match.hasMatch())
		{
			continue;
		}
		if (!aOutputTag.mGenre.isEmpty() && (aOutputTag.mGenre.value() != p.mGenre))
		{
			qDebug() << "String \"" << aInput << "\" has matched genre " << p.mGenre
				<< ", but already has genre " << aOutputTag.mGenre.value() << ", ignoring the match";
			continue;
		}
		bool isOK = false;
		auto mpm = match.captured("mpm").toInt(&isOK);
		auto mpmIsPresent = false;
		if (isOK && (mpm > 10) && (mpm < 300))
		{
			aOutputTag.mMeasuresPerMinute = mpm;
			mpmIsPresent = true;
		}
		aOutputTag.mGenre = p.mGenre;
		if (p.mIsLongStrip)
		{
			if ((match.capturedStart(1) == 0) && (match.capturedEnd("end") == aInput.length()))
			{
				// The entire string is simply the genre, strip it always
				return QString();
			}
		}
		if (!aStripLongGenre && !mpmIsPresent && p.mIsLongStrip)
		{
			return aInput;
		}
		auto prefix = (match.capturedStart(1) > 0) ? aInput.left(match.capturedStart(1)) : QString();
		return prefix + " " + aInput.mid(match.capturedEnd("end"));
	}
	return aInput;
}





/** Detects and removes MPM from the "[30 BPM]" substring or a single number being the entire string.
If the substring is found and the MPM is in the (10, 300) range, it is set into the song.
Returns the input string after removing the potential match.
Assumes that the match is not in the middle of "author - title", but rather at either end. */
static QString tryMatchBPM(const QString & aInput, Song::Tag & aOutputTag)
{
	// Search for strings like "28mpm" inside:
	static const QRegularExpression re("(.*?)(\\d+)\\s?[bmt]pm(.*)", QRegularExpression::CaseInsensitiveOption);
	auto match = re.match(aInput);
	if (match.hasMatch())
	{
		bool isOK = false;
		auto mpm = match.captured(2).toInt(&isOK);
		if (isOK && isReasonableMpm(mpm))
		{
			aOutputTag.mMeasuresPerMinute = mpm;
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
	auto mpm = aInput.toInt(&isOK);
	if (isOK && isReasonableMpm(mpm))
	{
		aOutputTag.mMeasuresPerMinute = mpm;
		return QString();
	}

	// No match, return the input unchanged:
	return aInput;
}





/** Extracts as much metadata form the input string as possible.
The extracted data is saved into aOutputTag and, if appropriate, removed from the string. */
static QString extractFromPart(const QString & aInput, Song::Tag & aOutputTag)
{
	// First process all parentheses, brackets and braces:
	QString intermediate;
	auto len = aInput.length();
	int last = 0;  // last boundary of the outer-most parenthesis / bracket / brace
	int numNestingLevels = 0;
	bool hasExtracted = false;
	for (int i = 0; i < len; ++i)
	{
		switch (aInput[i].unicode())
		{
			case '(':
			case '{':
			case '[':
			{
				if (numNestingLevels == 0)
				{
					if (i > last)
					{
						intermediate.append(aInput.mid(last, i - last));
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
					auto subBlock = aInput.mid(last, i - last);
					auto outBlock = tryMatchGenreMPM(tryMatchBPM(subBlock, aOutputTag), aOutputTag);
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
	}  // for i - aInput[]
	if (last < len)
	{
		intermediate.append(aInput.mid(last, len - last));
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
			tryMatchBPM(intermediate, aOutputTag),
			aOutputTag, false
		)
	).simplified();
}





/** Returns true if the input string is one of the "forbidden" parts, such as "unknown", "various" etc. */
static bool isForbiddenPart(const QString & aInput)
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
		if (re.match(aInput).hasMatch())
		{
			return true;
		}
	}
	return false;
}





/** Breaks the string into parts delimited by dashes or slashes,
then extracts as much metadata from each part as possible.
The extracted data is saved into aOutputTag and, if appropriate, removed from the string. */
static QString extract(const QString & aInput, Song::Tag & aOutputTag)
{
	// Break the input into parts by all dashes and slashes:
	QStringList parts;
	int last = 0;
	int len = aInput.length();
	for (int i = 0; i < len; ++i)
	{
		switch (aInput[i].unicode())
		{
			case '/':
			case '-':
			{
				if (i > last)
				{
					parts.append(aInput.mid(last, i - last));
				}
				last = i + 1;
				break;
			}
		}
	}
	if (last < len)
	{
		parts.append(aInput.mid(last, len - last));
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
		auto parsed = extractFromPart(simple, aOutputTag);
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
static void trySplitAuthorTitle(Song::Tag & aTag)
{
	if (
		(aTag.mAuthor.isEmpty() && aTag.mTitle.isEmpty()) ||
		(!aTag.mAuthor.isEmpty() && !aTag.mTitle.isEmpty())
	)
	{
		// Nothing to be done in this case
		return;
	}

	// Split into halves on the '-' closest to the string middle:
	auto src = aTag.mAuthor.isEmpty() ? aTag.mTitle.value() : aTag.mAuthor.value();
	auto halfLen = src.length() / 2;
	for (int i = 0; i < halfLen; ++i)
	{
		if (src[halfLen + i] == '-')
		{
			aTag.mAuthor = src.left(halfLen + i - 1).simplified();
			aTag.mTitle = src.mid(halfLen + i + 1).simplified();
			return;
		}
		else if (src[halfLen - i] == '-')
		{
			aTag.mAuthor = src.left(halfLen - i - 1).simplified();
			aTag.mTitle = src.mid(halfLen - i + 1).simplified();
			return;
		}
	}
}





////////////////////////////////////////////////////////////////////////////////
// MetadataScanner:

MetadataScanner::MetadataScanner():
	mQueueLength(0)
{
}





void MetadataScanner::writeTagToSong(SongPtr aSong, const Tag & aTag)
{
	auto fr = openTagFile(aSong->fileName());
	if (fr.isNull())
	{
		// File format not recognized
		qDebug() << "Unable to parse file " << aSong->fileName();
		return;
	}
	auto props = fr.file()->properties();
	setOrClearProp(props, "ARTIST",  aTag.mAuthor);
	setOrClearProp(props, "TITLE",   aTag.mTitle);
	setOrClearProp(props, "GENRE",   aTag.mGenre);
	setOrClearProp(props, "COMMENT", aTag.mComment);
	if (aTag.mMeasuresPerMinute.isPresent())
	{
		props["BPM"] = TagLib::StringList(QString::number(aTag.mMeasuresPerMinute.value()).toStdString());
	}
	else
	{
		props.erase("BPM");
	}
	auto refused = fr.file()->setProperties(props);
	if (!refused.isEmpty())
	{
		qWarning() << "Failed to set the following properties on song " << aSong->fileName();
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
		qWarning() << "Failed to save the new tag into song " << aSong->fileName();
	}

	// Update the detected genre in the song:
	aSong->setId3Tag(parseId3Tag(aTag));
}





std::pair<bool, MetadataScanner::Tag> MetadataScanner::readTagFromFile(const QString & aFileName) noexcept
{
	MetadataScanner::Tag res;
	auto fr = openTagFile(aFileName);
	if (fr.isNull())
	{
		// File format not recognized
		qDebug() << "Unable to parse file " << aFileName;
		return std::make_pair(false, res);
	}
	auto tag = fr.tag();
	if (tag == nullptr)
	{
		qDebug() << "No TagLib-extractable information found in " << aFileName;
		return std::make_pair(false, res);
	}

	// Extract the tag:
	res.mAuthor = QString::fromStdString(tag->artist().to8Bit(true));
	res.mTitle = QString::fromStdString(tag->title().to8Bit(true));
	res.mComment = QString::fromStdString(tag->comment().to8Bit(true));
	res.mGenre = QString::fromStdString(tag->genre().to8Bit(true));

	// Extract the MPM from BPM in the extended properties:
	for (const auto & prop: fr.file()->properties())
	{
		if (prop.first == "BPM")
		{
			bool isOK;
			auto bpm = QString::fromStdString(prop.second.toString().to8Bit(true)).toDouble(&isOK);
			if (isOK)
			{
				res.mMeasuresPerMinute = bpm;
			}
		}
	}
	return std::make_pair(true, res);
}





Song::Tag MetadataScanner::parseId3Tag(const MetadataScanner::Tag & aFileTag)
{
	Song::Tag res;
	/* genre = */   extract(aFileTag.mGenre.valueOrDefault(), res);
	/* comment = */ extract(aFileTag.mComment.valueOrDefault(), res);
	res.mAuthor =  extract(aFileTag.mAuthor.valueOrDefault(), res);
	res.mTitle =   extract(aFileTag.mTitle.valueOrDefault(), res);
	if (aFileTag.mMeasuresPerMinute.isPresent())
	{
		if (res.mGenre.isPresent())
		{
			// Use genre to adjust from BPM to MPM:
			res.mMeasuresPerMinute = Song::adjustMpm(aFileTag.mMeasuresPerMinute.value(), res.mGenre.value());
		}
		else
		{
			// No genre, cannot adjust
			res.mMeasuresPerMinute = aFileTag.mMeasuresPerMinute;
		}
	}
	trySplitAuthorTitle(res);
	return res;
}





MetadataScanner::Tag MetadataScanner::applyTagChanges(const MetadataScanner::Tag & aFileTag, const MetadataScanner::Tag & aChanges)
{
	auto res = aFileTag;
	if (aChanges.mAuthor.isPresent())
	{
		res.mAuthor = aChanges.mAuthor;
	}
	if (aChanges.mTitle.isPresent())
	{
		res.mTitle = aChanges.mTitle;
	}
	if (aChanges.mGenre.isPresent())
	{
		res.mGenre = aChanges.mGenre;
	}
	if (aChanges.mComment.isPresent())
	{
		res.mComment = aChanges.mComment;
	}
	if (aChanges.mMeasuresPerMinute.isPresent())
	{
		res.mMeasuresPerMinute = aChanges.mMeasuresPerMinute;
	}
	return res;
}





Song::Tag MetadataScanner::parseFileNameIntoMetadata(const QString & aFileName)
{
	Song::Tag songTag;
	auto idxLastSlash = aFileName.lastIndexOf('/');
	/*
	if (idxLastSlash < 0)
	{
		return songTag;
	}
	*/

	// Auto-detect genre from the parent folders names:
	auto parents = aFileName.split('/', Qt::SkipEmptyParts);
	for (const auto & p: parents)
	{
		tryMatchGenreMPM(p, songTag);
		if (!songTag.mGenre.isEmpty())  // Did we assign a genre?
		{
			break;
		}
	}

	// Detect and remove genre + MPM from filename substring:
	auto fileBareName = aFileName.mid(idxLastSlash + 1);
	auto idxExt = fileBareName.lastIndexOf('.');
	if (idxExt >= 0)
	{
		fileBareName.remove(idxExt, fileBareName.length());
	}
	fileBareName = extract(fileBareName, songTag);

	// Try split into author - title:
	songTag.mTitle = fileBareName;
	trySplitAuthorTitle(songTag);
	validateSongTag(songTag);
	return songTag;
}





void MetadataScanner::enqueueScan(SongPtr aSong, bool aPrioritize)
{
	mQueueLength += 1;
	BackgroundTasks::enqueue(tr("Scan metadata: %1").arg(aSong->fileName()), [this, aSong]()
		{
			scanSong(aSong);
			mQueueLength -= 1;
		},
		aPrioritize
	);
}





TagLib::FileRef MetadataScanner::openTagFile(const QString & aFileName)
{
	#ifdef _WIN32
		// TagLib on Windows needs UTF16-BE filenames (#134):
		return TagLib::FileRef(reinterpret_cast<const wchar_t *>(aFileName.constData()), false);
	#else
		return TagLib::FileRef(aFileName.toUtf8().constData(), false);
	#endif
}





void MetadataScanner::queueScanSong(SongPtr aSong)
{
	enqueueScan(aSong, false);
}





void MetadataScanner::queueScanSongPriority(SongPtr aSong)
{
	enqueueScan(aSong, true);
}





void MetadataScanner::scanSong(SongPtr aSong)
{
	auto id3Tag = readTagFromFile(aSong->fileName());
	if (id3Tag.first)
	{
		auto parsedId3Tag = parseId3Tag(id3Tag.second);
		validateSongTag(parsedId3Tag);
		aSong->setId3Tag(parsedId3Tag);
	}
	auto fileTag = parseFileNameIntoMetadata(aSong->fileName());
	aSong->setFileNameTag(fileTag);

	emit songScanned(aSong);
}

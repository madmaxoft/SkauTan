#include <iostream>
#include <QString>
#include <QFile>
#include "../SongTempoDetector.hpp"
#include "../Song.hpp"
#include "../MetadataScanner.hpp"





using namespace std;





/** Specifies whether ID3 information should be parsed from the files and included in the output.
Settable through the "-i" cmdline param. */
static bool g_ShouldIncludeID3 = false;





/** Returns the input string, escaped so that it can be embedded in Lua code in a doublequote. */
static std::string luaEscapeString(const QString & aInput)
{
	static QLatin1String singleBackslash("\\");
	static QLatin1String doubleBackslash("\\\\");
	static QLatin1String doubleQuote("\"");
	static QLatin1String escapedDoubleQuote("\\\"");
	return QString(aInput)
		.replace(singleBackslash, doubleBackslash)
		.replace(doubleQuote, escapedDoubleQuote)
		.replace("\r\n", "\n")
		.replace("\n", "\\n")
		.toStdString();
}





/** Reads the tag from the specified file and outputs it to stdout.
Outputs nothing if the tag cannot be read.
Outputs both the raw tag read from file, and the parsed tag. */
static void outputId3Tag(const QString & aFileName)
{
	const auto & tag = MetadataScanner::readTagFromFile(aFileName);
	if (!tag.first)
	{
		// No tag could be read from the file, bail out
		return;
	}
	const auto & parsedTag = MetadataScanner::parseId3Tag(tag.second);
	cout << "\trawID3Tag =" << endl;
	cout << "\t{" << endl;
	if (tag.second.mAuthor.isPresent())
	{
		cout << "\t\tauthor  = \"" << luaEscapeString(tag.second.mAuthor.value()) << "\"," << endl;
	}
	if (tag.second.mTitle.isPresent())
	{
		cout << "\t\ttitle   = \"" << luaEscapeString(tag.second.mTitle.value()) << "\"," << endl;
	}
	if (tag.second.mGenre.isPresent())
	{
		cout << "\t\tgenre   = \"" << luaEscapeString(tag.second.mGenre.value()) << "\"," << endl;
	}
	if (tag.second.mMeasuresPerMinute.isPresent())
	{
		cout << "\t\tmpm     = " << tag.second.mMeasuresPerMinute.value() << "," << endl;
	}
	if (tag.second.mComment.isPresent())
	{
		cout << "\t\tcomment = \"" << luaEscapeString(tag.second.mComment.value()) << "\"," << endl;
	}
	cout << "\t}," << endl;

	cout << "\tparsedID3Tag =" << endl;
	cout << "\t{" << endl;
	if (parsedTag.mAuthor.isPresent())
	{
		cout << "\t\tauthor = \"" << luaEscapeString(parsedTag.mAuthor.value()) << "\"," << endl;
	}
	if (parsedTag.mTitle.isPresent())
	{
		cout << "\t\ttitle = \"" << luaEscapeString(parsedTag.mTitle.value()) << "\"," << endl;
	}
	if (parsedTag.mGenre.isPresent())
	{
		cout << "\t\tgenre = \"" << luaEscapeString(parsedTag.mGenre.value()) << "\"," << endl;
	}
	if (parsedTag.mMeasuresPerMinute.isPresent())
	{
		cout << "\t\tmpm = " << parsedTag.mMeasuresPerMinute.value() << "," << endl;
	}
	cout << "\t}," << endl;
}





void processFile(const QString & aFileName, const SongTempoDetector::Options & aOptions)
{
	if (!QFile::exists(aFileName))
	{
		cerr << "File doesn't exist: " << aFileName.toStdString() << endl;
		return;
	}

	// Detect:
	cerr << "Detecting..." << endl;
	auto sd = std::make_shared<Song::SharedData>(QByteArray(), 0);  // Dummy SharedData
	SongPtr song = std::make_shared<Song>(aFileName, sd);
	SongTempoDetector td;
	auto res = td.scanSong(song, {aOptions});

	cerr << "-----------------------------------------------" << endl;;
	cerr << "Detected data for " << aFileName.toStdString() << ":" << endl;
	cerr << "Total number of beats: " << res->mBeats.size() << endl;
	cerr << "Detected tempo: " << res->mTempo << endl;
	cerr << "Confidence: " << res->mConfidence << endl;

	// Output the results to stdout as a Lua source, so that it can be consumed by a script:
	cout << "return" << endl << "{" << endl;
	cout << "\tfileName = \"" << luaEscapeString(aFileName) << "\"," << endl;
	cout << "\tbeats =" << endl;
	cout << "\t{" << endl;
	const auto & levels = res->mLevels;
	for (const auto & b: res->mBeats)
	{
		cout << "\t\t{ " << b.first << ", " << levels[b.first] << ", " << b.second << "}," << endl;
	}
	cout << "\t}," << endl;
	cout << "\tsoundLevels =" << endl;
	cout << "\t{" << endl;
	for (const auto lev: res->mLevels)
	{
		cout << "\t\t" << lev << "," << endl;
	}
	cout << "\t}," << endl;
	cout << "\ttempo = " << res->mTempo << "," << endl;
	cout << "\tconfidence = " << res->mConfidence << "," << endl;
	if (g_ShouldIncludeID3)
	{
		outputId3Tag(song->fileName());
	}
	cout << "}" << endl;
}





void printUsage()
{
	TempoDetector::Options defaultOptions;
	cerr << "BeatDetectCmd" << endl;
	cerr << "-------------" << endl;
	cerr << "Detects tempo in audio files, using various algorithms and options." << endl;
	cerr << "Part of SkauTan player, https://github.com/madmaxoft/SkauTan" << endl;
	cerr << endl;
	cerr << "Usage:" << endl;
	cerr << "BeatDetectCmd [options] [filename]" << endl;
	cerr << endl;
	cerr << "Available options:" << endl;
	cerr << "  -a <N>          ... Use sound-level algorithm N (default: " << defaultOptions.mLevelAlgorithm << ")" << endl;
	cerr << "                        0: SumDist" << endl;
	cerr << "                        1: MinMax" << endl;
	cerr << "                        2: DiscreetSineTransform" << endl;
	cerr << "  -w <N>          ... Set window size to N (default: " << defaultOptions.mWindowSize << ")" << endl;
	cerr << "  -s <N>          ... Set the stride to N (default: " << defaultOptions.mStride << ")" << endl;
	cerr << "  -p <N>          ... Set the number of levels to check for peak (default: " << defaultOptions.mLocalMaxDistance << ")" << endl;
	cerr << "  -b <filename>   ... Output debug audio with beats to file" << endl;
	cerr << "  -d <filename>   ... Output debug audio with levels to file" << endl;
	cerr << "  -h              ... Print this help" << endl;
	cerr << "  -i              ... Include the ID3 information from the file in the output" << endl;
	cerr << "  -r <samplerate> ... Convert the file to samplerate (default: " << defaultOptions.mSampleRate << ")" << endl;
	cerr << "  -n <N> <file>   ... Output debug audio with levels normalized in N samples to file" << endl;
}





#define  NEED_ARG(N) \
	if (i + N >= argc) \
	{ \
		cerr << "Bad argument " << i << " (" << aArgs[i] << "): needs " << N << " parameters" << endl; \
		return 1; \
	} \

int processArgs(const vector<string> & aArgs, SongTempoDetector::Options & aOptions)
{
	size_t argc = aArgs.size();
	for (size_t i = 0; i < argc; ++i)
	{
		if (aArgs[i][0] == '-')
		{
			switch (aArgs[i][1])
			{
				case '?':
				case 'h':
				case 'H':
				{
					printUsage();
					break;
				}
				case 'a':
				case 'A':
				{
					NEED_ARG(1);
					aOptions.mLevelAlgorithm = static_cast<TempoDetector::ELevelAlgorithm>(stoi(aArgs[i + 1]));
					i += 1;
					break;
				}
				case 'b':
				case 'B':
				{
					NEED_ARG(1);
					aOptions.mDebugAudioBeatsFileName = QString::fromStdString(aArgs[i + 1].c_str());
					i += 1;
					break;
				}
				case 'd':
				case 'D':
				{
					NEED_ARG(1);
					aOptions.mDebugAudioLevelsFileName = QString::fromStdString(aArgs[i + 1]);
					i += 1;
					break;
				}
				case 'i':
				case 'I':
				{
					g_ShouldIncludeID3 = true;
					break;
				}
				case 'n':
				case 'N':
				{
					NEED_ARG(1);
					aOptions.mNormalizeLevelsWindowSize = static_cast<size_t>(stoll(aArgs[i + 1]));
					i += 1;
					break;
				}
				case 'p':
				case 'P':
				{
					NEED_ARG(1);
					aOptions.mLocalMaxDistance = static_cast<size_t>(stoll(aArgs[i + 1]));
					i += 1;
					break;
				}
				case 'r':
				case 'R':
				{
					NEED_ARG(1);
					aOptions.mSampleRate = stoi(aArgs[i + 1]);
					i += 1;
					break;
				}
				case 's':
				case 'S':
				{
					NEED_ARG(1);
					aOptions.mStride = static_cast<size_t>(stoll(aArgs[i + 1]));
					i += 1;
					break;
				}
				case 'w':
				case 'W':
				{
					NEED_ARG(1);
					aOptions.mWindowSize = static_cast<size_t>(stoll(aArgs[i + 1]));
					i += 1;
					break;
				}
			}  // switch (arg)
			continue;
		}  // if ('-')

		processFile(QString::fromStdString(aArgs[i]), aOptions);
	}

	cerr << "Done." << std::endl;
	return 0;
}





int main(int argc, char *argv[])
{
	SongTempoDetector::Options options;
	vector<string> args;
	for (int i = 1; i < argc; ++i)
	{
		args.emplace_back(argv[i]);
	}
	return processArgs(args, options);
}

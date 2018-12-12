#include <iostream>
#include <QString>
#include <QFile>
#include "../TempoDetector.hpp"
#include "../Song.hpp"
#include "../MetadataScanner.hpp"





using namespace std;





/** Specifies whether ID3 information should be parsed from the files and included in the output.
Settable through the "-i" cmdline param. */
static bool g_ShouldIncludeID3 = false;





/** Returns the input string, escaped so that it can be embedded in Lua code in a doublequote. */
static std::string luaEscapeString(const QString & a_Input)
{
	static QLatin1String singleBackslash("\\");
	static QLatin1String doubleBackslash("\\\\");
	static QLatin1String doubleQuote("\"");
	static QLatin1String escapedDoubleQuote("\\\"");
	return QString(a_Input)
		.replace(singleBackslash, doubleBackslash)
		.replace(doubleQuote, escapedDoubleQuote)
		.replace("\r\n", "\n")
		.replace("\n", "\\n")
		.toStdString();
}





/** Reads the tag from the specified file and outputs it to stdout.
Outputs nothing if the tag cannot be read.
Outputs both the raw tag read from file, and the parsed tag. */
static void outputId3Tag(const QString & a_FileName)
{
	const auto & tag = MetadataScanner::readTagFromFile(a_FileName);
	if (!tag.first)
	{
		// No tag could be read from the file, bail out
		return;
	}
	const auto & parsedTag = MetadataScanner::parseId3Tag(tag.second);
	cout << "\trawID3Tag =" << endl;
	cout << "\t{" << endl;
	if (tag.second.m_Author.isPresent())
	{
		cout << "\t\tauthor  = \"" << luaEscapeString(tag.second.m_Author.value()) << "\"," << endl;
	}
	if (tag.second.m_Title.isPresent())
	{
		cout << "\t\ttitle   = \"" << luaEscapeString(tag.second.m_Title.value()) << "\"," << endl;
	}
	if (tag.second.m_Genre.isPresent())
	{
		cout << "\t\tgenre   = \"" << luaEscapeString(tag.second.m_Genre.value()) << "\"," << endl;
	}
	if (tag.second.m_MeasuresPerMinute.isPresent())
	{
		cout << "\t\tmpm     = " << tag.second.m_MeasuresPerMinute.value() << "," << endl;
	}
	if (tag.second.m_Comment.isPresent())
	{
		cout << "\t\tcomment = \"" << luaEscapeString(tag.second.m_Comment.value()) << "\"," << endl;
	}
	cout << "\t}," << endl;

	cout << "\tparsedID3Tag =" << endl;
	cout << "\t{" << endl;
	if (parsedTag.m_Author.isPresent())
	{
		cout << "\t\tauthor = \"" << luaEscapeString(parsedTag.m_Author.value()) << "\"," << endl;
	}
	if (parsedTag.m_Title.isPresent())
	{
		cout << "\t\ttitle = \"" << luaEscapeString(parsedTag.m_Title.value()) << "\"," << endl;
	}
	if (parsedTag.m_Genre.isPresent())
	{
		cout << "\t\tgenre = \"" << luaEscapeString(parsedTag.m_Genre.value()) << "\"," << endl;
	}
	if (parsedTag.m_MeasuresPerMinute.isPresent())
	{
		cout << "\t\tmpm = " << parsedTag.m_MeasuresPerMinute.value() << "," << endl;
	}
	cout << "\t}," << endl;
}





void processFile(const QString & a_FileName, const TempoDetector::Options & a_Options)
{
	if (!QFile::exists(a_FileName))
	{
		cerr << "File doesn't exist: " << a_FileName.toStdString() << endl;
		return;
	}

	// Detect:
	cerr << "Detecting..." << endl;
	auto sd = std::make_shared<Song::SharedData>(QByteArray(), 0);  // Dummy SharedData
	SongPtr song = std::make_shared<Song>(a_FileName, sd);
	TempoDetector td;
	auto res = td.scanSong(song, a_Options);

	cerr << "-----------------------------------------------" << endl;;
	cerr << "Detected data for " << a_FileName.toStdString() << ":" << endl;
	cerr << "Total number of beats: " << res->m_Beats.size() << endl;
	cerr << "Total number of histogram entries: " << res->m_Histogram.size() << endl;
	cerr << "Confidence for compatible tempo groups:" << endl;
	for (const auto & c: res->m_Confidences)
	{
		cerr << "  " << c.first << ": " << c.second << endl;
	}

	// Output the results to stdout as a Lua source, so that it can be consumed by a script:
	cout << "return" << endl << "{" << endl;
	cout << "\tfileName = \"" << luaEscapeString(a_FileName) << "\"," << endl;
	cout << "\thistogram =" << endl;
	cout << "\t{" << endl;
	for (const auto & h: res->m_Histogram)
	{
		cout << "\t\t{ " << h.first << ", " << h.second << "}," << endl;
	}
	cout << "\t}," << endl;
	cout << "\tconfidences =" << endl;
	cout << "\t{" << endl;
	for (const auto & c: res->m_Confidences)
	{
		cout << "\t\t{ " << c.first << ", " << c.second << "}," << endl;
	}
	cout << "\t}," << endl;
	cout << "\tbeats =" << endl;
	cout << "\t{" << endl;
	const auto & levels = res->m_Levels;
	for (const auto & b: res->m_Beats)
	{
		cout << "\t\t{ " << b << ", " << levels[b] << "}," << endl;
	}
	cout << "\t}," << endl;
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
	cerr << "  -a <N>          ... Use sound-level algorithm N (default: " << defaultOptions.m_LevelAlgorithm << ")" << endl;
	cerr << "                        0: SumDist" << endl;
	cerr << "                        1: MinMax" << endl;
	cerr << "                        2: DiscreetSineTransform" << endl;
	cerr << "  -w <N>          ... Set window size to N (default: " << defaultOptions.m_WindowSize << ")" << endl;
	cerr << "  -s <N>          ... Set the stride to N (default: " << defaultOptions.m_Stride << ")" << endl;
	cerr << "  -v <N>          ... Set the number of levels to average for beat strength (default: " << defaultOptions.m_LevelAvg << ")" << endl;
	cerr << "  -p <N>          ... Set the number of levels to check for peak (default: " << defaultOptions.m_LevelPeak << ")" << endl;
	cerr << "  -c <N>          ... Set the histogram cutoff (default: " << defaultOptions.m_HistogramCutoff << ")" << endl;
	cerr << "  -f <min> <max>  ... Fold the histogram into the specified tempo range (default: ";
	if (defaultOptions.m_ShouldFoldHistogram)
	{
		cerr << "-f " << defaultOptions.m_HistogramFoldMin << ' ' << defaultOptions.m_HistogramFoldMax << ")" << endl;
	}
	else
	{
		cerr << "do not fold)" << endl;
	}
	cerr << "  -b <filename>   ... Output debug audio with beats to file" << endl;
	cerr << "  -d <filename>   ... Output debug audio with levels to file" << endl;
	cerr << "  -h              ... Print this help" << endl;
	cerr << "  -i              ... Include the ID3 information from the file in the output" << endl;
}





#define  NEED_ARG(N) \
	if (i + N >= argc) \
	{ \
		cerr << "Bad argument " << i << " (" << a_Args[i] << "): needs " << N << " parameters" << endl; \
		return 1; \
	} \

int processArgs(const vector<string> & a_Args, TempoDetector::Options & a_Options)
{
	size_t argc = a_Args.size();
	for (size_t i = 0; i < argc; ++i)
	{
		if (a_Args[i][0] == '-')
		{
			switch (a_Args[i][1])
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
					a_Options.m_LevelAlgorithm = static_cast<TempoDetector::ELevelAlgorithm>(stoi(a_Args[i + 1]));
					i += 1;
					break;
				}
				case 'b':
				case 'B':
				{
					NEED_ARG(1);
					a_Options.m_DebugAudioBeatsFileName = QString::fromStdString(a_Args[i + 1].c_str());
					i += 1;
					break;
				}
				case 'c':
				case 'C':
				{
					NEED_ARG(1);
					a_Options.m_HistogramCutoff = static_cast<size_t>(stoll(a_Args[i + 1]));
					i += 1;
					break;
				}
				case 'd':
				case 'D':
				{
					NEED_ARG(1);
					a_Options.m_DebugAudioLevelsFileName = QString::fromStdString(a_Args[i + 1]);
					i += 1;
					break;
				}
				case 'f':
				case 'F':
				{
					NEED_ARG(2);
					a_Options.m_ShouldFoldHistogram = true;
					a_Options.m_HistogramFoldMin = stoi(a_Args[i + 1]);
					a_Options.m_HistogramFoldMax = stoi(a_Args[i + 2]);
					i += 2;
					break;
				}
				case 'i':
				case 'I':
				{
					g_ShouldIncludeID3 = true;
					break;
				}
				case 'p':
				case 'P':
				{
					NEED_ARG(1);
					a_Options.m_LevelPeak = static_cast<size_t>(stoll(a_Args[i + 1]));
					i += 1;
					break;
				}
				case 's':
				case 'S':
				{
					NEED_ARG(1);
					a_Options.m_Stride = static_cast<size_t>(stoll(a_Args[i + 1]));
					i += 1;
					break;
				}
				case 'w':
				case 'W':
				{
					NEED_ARG(1);
					a_Options.m_WindowSize = static_cast<size_t>(stoll(a_Args[i + 1]));
					i += 1;
					break;
				}
			}  // switch (arg)
			continue;
		}  // if ('-')

		processFile(QString::fromStdString(a_Args[i]), a_Options);
	}

	cerr << "Done." << std::endl;
	return 0;
}





int main(int argc, char *argv[])
{
	TempoDetector::Options options;
	vector<string> args;
	for (int i = 1; i < argc; ++i)
	{
		args.emplace_back(argv[i]);
	}
	return processArgs(args, options);
}

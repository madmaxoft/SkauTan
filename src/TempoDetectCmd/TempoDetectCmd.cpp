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





void processFile(const QString & a_FileName)
{
	if (!QFile::exists(a_FileName))
	{
		cerr << "File doesn't exist: " << a_FileName.toStdString() << endl;
		return;
	}

	// Detect:
	cerr << "Detecting..." << endl;
	auto songSD = std::make_shared<Song::SharedData>(QByteArray(), 0);  // Dummy SharedData
	SongPtr song = std::make_shared<Song>(a_FileName, songSD);
	TempoDetector td;
	if (!td.detect(songSD))
	{
		cerr << "Detection failed" << endl;
		return;
	}

	cerr << "-----------------------------------------------" << endl;;
	cerr << "Detection in " << a_FileName.toStdString() << ":" << endl;
	cerr << "Detected tempo: " << songSD->m_DetectedTempo.valueOr(-1) << endl;

	// Output the results to stdout as a Lua source, so that it can be consumed by a script:
	cout << "return" << endl << "{" << endl;
	cout << "\tfileName = \"" << luaEscapeString(a_FileName) << "\"," << endl;
	cout << "\ttempo = " << songSD->m_DetectedTempo.valueOr(-1) << "," << endl;
	if (g_ShouldIncludeID3)
	{
		outputId3Tag(song->fileName());
	}
	cout << "}" << endl;
}





void printUsage()
{
	cerr << "TempoDetectCmd" << endl;
	cerr << "-------------" << endl;
	cerr << "Detects tempo in audio files, using the algorithms and options currently used in SkauTan." << endl;
	cerr << "Part of SkauTan player, https://github.com/madmaxoft/SkauTan" << endl;
	cerr << endl;
	cerr << "Usage:" << endl;
	cerr << "TempoDetectCmd [options] [filename]" << endl;
	cerr << endl;
	cerr << "Available options:" << endl;
	cerr << "  -i ... Include the ID3 information from the file in the output" << endl;
}





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
				case 'i':
				case 'I':
				{
					g_ShouldIncludeID3 = true;
					break;
				}
			}  // switch (arg)
			continue;
		}  // if ('-')

		processFile(QString::fromStdString(a_Args[i]));
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

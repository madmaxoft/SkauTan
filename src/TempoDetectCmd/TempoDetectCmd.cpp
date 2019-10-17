#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <QString>
#include <QFile>
#include "../SongTempoDetector.hpp"
#include "../Song.hpp"
#include "../MetadataScanner.hpp"





using namespace std;





struct Results
{
	/** The detected tempo. */
	double m_Tempo;

	/** The raw ID3 tag as read from the file. */
	MetadataScanner::Tag m_RawTag;

	/** The processed ID3 tag. */
	Song::Tag m_ParsedTag;
};





/** Specifies whether ID3 information should be parsed from the files and included in the output.
Settable through the "-i" cmdline param. */
static bool g_ShouldIncludeID3 = false;

/** The global instance of the tempo detector. */
static SongTempoDetector g_TempoDetector;

/** FileNames to process.
After a filename is sent to processing, it is removed from this vector.
Protected against multithreaded access by m_Mtx. */
static vector<QString> g_FileNames;

/** Map of FileName -> Results of all successfully detected songs.
Protected against multithreaded access by m_Mtx. */
static map<QString, unique_ptr<Results>> g_Results;

/** The mutex protecting g_FileNames and g_Results against multithreaded access. */
static mutex g_Mtx;





/** Outputs the specified value to the stream, in utf-8. */
static std::ostream & operator <<(std::ostream & a_Stream, const QString & a_Value)
{
	a_Stream << a_Value.toStdString();
	return a_Stream;
}





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





static void printUsage()
{
	cerr << "TempoDetectCmd" << endl;
	cerr << "-------------" << endl;
	cerr << "Detects tempo in audio files, using the algorithms and options currently used in SkauTan." << endl;
	cerr << "Part of SkauTan player, https://github.com/madmaxoft/SkauTan" << endl;
	cerr << endl;
	cerr << "Usage:" << endl;
	cerr << "TempoDetectCmd [options] [filename] [filename] ..." << endl;
	cerr << endl;
	cerr << "Available options:" << endl;
	cerr << "  -i ... Include the ID3 information from the file in the output" << endl;
}





/** Reads the ID3 tag from the file and returns the raw data and the parsed data.
If the tag cannot be read from the file, returns empty tags. */
pair<MetadataScanner::Tag, Song::Tag> readTag(const QString & a_FileName)
{
	auto rawTag = MetadataScanner::readTagFromFile(a_FileName);
	if (!rawTag.first)
	{
		// No tag could be read from the file, bail out
		return {};
	}
	return {rawTag.second, MetadataScanner::parseId3Tag(rawTag.second)};
}





static unique_ptr<Results> runDetectionOnFile(const QString & a_FileName)
{
	if (!QFile::exists(a_FileName))
	{
		cerr << "File doesn't exist: " << a_FileName << endl;
		return nullptr;
	}

	// Extract the tags:
	auto res = make_unique<Results>();
	tie(res->m_RawTag, res->m_ParsedTag) = readTag(a_FileName);
	auto genre = res->m_ParsedTag.m_Genre.valueOrDefault();
	auto songSD = std::make_shared<Song::SharedData>(QByteArray(), 0);  // Dummy SharedData
	songSD->m_TagManual = res->m_ParsedTag;  // Use the ID3 tag for detection
	SongPtr song = std::make_shared<Song>(a_FileName, songSD);
	song->setFileNameTag(MetadataScanner::parseFileNameIntoMetadata(a_FileName));
	if (!res->m_ParsedTag.m_MeasuresPerMinute.isPresent())
	{
		// If ID3 doesn't have the MPM, copy from filename-based tag:
		res->m_ParsedTag.m_MeasuresPerMinute = song->tagFileName().m_MeasuresPerMinute;
	}

	// Run the tempo detection:
	if (!g_TempoDetector.detect(songSD))
	{
		cerr << "Detection failed: " << a_FileName << endl;
		return nullptr;
	}
	res->m_Tempo = song->detectedTempo().valueOr(-1);
	return res;
}





/** Implementation of a single worker thread.
Takes a file from g_FileNames and runs the tempo detection on it; loops until there are no more filenames.
Stores the detection results in g_Results. */
static void runDetection()
{
	unique_lock<mutex> lock(g_Mtx);
	while (!g_FileNames.empty())
	{
		auto fileName = g_FileNames.back();
		g_FileNames.pop_back();
		lock.unlock();
		auto results = runDetectionOnFile(fileName);
		lock.lock();
		if (results != nullptr)
		{
			g_Results[fileName] = std::move(results);
		}
	}
}





/** Spins up threads for the file processing, processes each file. */
static void processFiles()
{
	// Start the processing threads:
	vector<thread> threads;
	for (auto i = thread::hardware_concurrency(); i > 0; --i)
	{
		threads.push_back(thread(runDetection));
	}

	// Wait for the threads to finish:
	for (auto & thr: threads)
	{
		thr.join();
	}
}





/** Adds the contents of the list file into the file list to be processed. */
static void addListFile(const QString & a_FileName)
{
	QFile f(a_FileName);
	if (!f.open(QFile::ReadOnly | QFile::Text))
	{
		cerr << "Cannot open list file " << a_FileName << ": " << f.errorString() << endl;
		return;
	}
	auto contents = f.readAll();
	auto files = contents.split('\n');
	for (const auto & file: files)
	{
		if (!file.isEmpty())
		{
			g_FileNames.push_back(file);
		}
	}
}





#define  NEED_ARGS(N) \
	if (i + N >= argc) \
	{ \
		cerr << "Bad argument " << i << " (" << a_Args[i] << "): needs " << N << " parameters" << endl; \
		return false; \
	} \

/** Processes all command-line arguments. */
static bool processArgs(const vector<string> & a_Args)
{
	size_t argc = a_Args.size();
	vector<QString> files;
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

				case 'l':
				case 'L':
				{
					NEED_ARGS(1);
					addListFile(QString::fromStdString(a_Args[i + 1]));
					i += 1;
					break;
				}

			}  // switch (arg)
			continue;
		}  // if ('-')
		g_FileNames.push_back(QString::fromStdString(a_Args[i]));
	}
	return true;
}





/** Outputs the raw and processed ID3 tags to stdout, formatted as Lua source. */
static void outputId3Tag(const Results & a_Results)
{
	cout << "\t\trawID3Tag =" << endl;
	cout << "\t\t{" << endl;
	if (a_Results.m_RawTag.m_Author.isPresent())
	{
		cout << "\t\t\tauthor  = \"" << luaEscapeString(a_Results.m_RawTag.m_Author.value()) << "\"," << endl;
	}
	if (a_Results.m_RawTag.m_Title.isPresent())
	{
		cout << "\t\t\ttitle   = \"" << luaEscapeString(a_Results.m_RawTag.m_Title.value()) << "\"," << endl;
	}
	if (a_Results.m_RawTag.m_Genre.isPresent())
	{
		cout << "\t\t\tgenre   = \"" << luaEscapeString(a_Results.m_RawTag.m_Genre.value()) << "\"," << endl;
	}
	if (a_Results.m_RawTag.m_MeasuresPerMinute.isPresent())
	{
		cout << "\t\t\tmpm     = " << a_Results.m_RawTag.m_MeasuresPerMinute.value() << "," << endl;
	}
	if (a_Results.m_RawTag.m_Comment.isPresent())
	{
		cout << "\t\t\tcomment = \"" << luaEscapeString(a_Results.m_RawTag.m_Comment.value()) << "\"," << endl;
	}
	cout << "\t\t}," << endl;

	cout << "\t\tparsedID3Tag =" << endl;
	cout << "\t\t{" << endl;
	const auto & parsedTag = a_Results.m_ParsedTag;
	if (parsedTag.m_Author.isPresent())
	{
		cout << "\t\t\tauthor = \"" << luaEscapeString(parsedTag.m_Author.value()) << "\"," << endl;
	}
	if (parsedTag.m_Title.isPresent())
	{
		cout << "\t\t\ttitle = \"" << luaEscapeString(parsedTag.m_Title.value()) << "\"," << endl;
	}
	if (parsedTag.m_Genre.isPresent())
	{
		cout << "\t\t\tgenre = \"" << luaEscapeString(parsedTag.m_Genre.value()) << "\"," << endl;
	}
	if (parsedTag.m_MeasuresPerMinute.isPresent())
	{
		cout << "\t\t\tmpm = " << parsedTag.m_MeasuresPerMinute.value() << "," << endl;
	}
	cout << "\t\t}," << endl;
}





/** Outputs all the results to stdout, as a Lua source file. */
static void outputResults()
{
	cout << "return" << endl << "{" << endl;
	for (const auto & res: g_Results)
	{
		const auto & fileName = res.first;
		const auto & results = res.second;
		cout << "\t[\"" << luaEscapeString(fileName) << "\"] =" << endl;
		cout << "\t{" << endl;
		cout << "\t\tfileName = \"" << luaEscapeString(fileName) << "\"," << endl;
		cout << "\t\ttempo = " << results->m_Tempo << "," << endl;
		if (g_ShouldIncludeID3)
		{
			outputId3Tag(*results);
		}
		cout << "\t}," << endl;
	}
	cout << "}" << endl;
}





int main(int argc, char *argv[])
{
	vector<string> args;
	for (int i = 1; i < argc; ++i)
	{
		args.emplace_back(argv[i]);
	}
	if (!processArgs(args))
	{
		return 1;
	}
	processFiles();
	outputResults();

	return 0;
}

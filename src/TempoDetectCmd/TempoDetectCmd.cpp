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
	double mTempo;

	/** The raw ID3 tag as read from the file. */
	MetadataScanner::Tag mRawTag;

	/** The processed ID3 tag. */
	Song::Tag mParsedTag;
};





/** Specifies whether ID3 information should be parsed from the files and included in the output.
Settable through the "-i" cmdline param. */
static bool g_ShouldIncludeID3 = false;

/** The global instance of the tempo detector. */
static SongTempoDetector g_TempoDetector;

/** FileNames to process.
After a filename is sent to processing, it is removed from this vector.
Protected against multithreaded access by mMtx. */
static vector<QString> g_FileNames;

/** Map of FileName -> Results of all successfully detected songs.
Protected against multithreaded access by mMtx. */
static map<QString, unique_ptr<Results>> g_Results;

/** The mutex protecting g_FileNames and g_Results against multithreaded access. */
static mutex g_Mtx;





/** Outputs the specified value to the stream, in utf-8. */
static std::ostream & operator <<(std::ostream & aStream, const QString & aValue)
{
	aStream << aValue.toStdString();
	return aStream;
}





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
pair<MetadataScanner::Tag, Song::Tag> readTag(const QString & aFileName)
{
	auto rawTag = MetadataScanner::readTagFromFile(aFileName);
	if (!rawTag.first)
	{
		// No tag could be read from the file, bail out
		return {};
	}
	return {rawTag.second, MetadataScanner::parseId3Tag(rawTag.second)};
}





static unique_ptr<Results> runDetectionOnFile(const QString & aFileName)
{
	if (!QFile::exists(aFileName))
	{
		cerr << "File doesn't exist: " << aFileName << endl;
		return nullptr;
	}

	// Extract the tags:
	auto res = make_unique<Results>();
	tie(res->mRawTag, res->mParsedTag) = readTag(aFileName);
	auto genre = res->mParsedTag.mGenre.valueOrDefault();
	auto songSD = std::make_shared<Song::SharedData>(QByteArray(), 0);  // Dummy SharedData
	songSD->mTagManual = res->mParsedTag;  // Use the ID3 tag for detection
	SongPtr song = std::make_shared<Song>(aFileName, songSD);
	song->setFileNameTag(MetadataScanner::parseFileNameIntoMetadata(aFileName));
	if (!res->mParsedTag.mMeasuresPerMinute.isPresent())
	{
		// If ID3 doesn't have the MPM, copy from filename-based tag:
		res->mParsedTag.mMeasuresPerMinute = song->tagFileName().mMeasuresPerMinute;
	}

	// Run the tempo detection:
	if (!g_TempoDetector.detect(songSD))
	{
		cerr << "Detection failed: " << aFileName << endl;
		return nullptr;
	}
	res->mTempo = song->detectedTempo().valueOr(-1);
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
static void addListFile(const QString & aFileName)
{
	QFile f(aFileName);
	if (!f.open(QFile::ReadOnly | QFile::Text))
	{
		cerr << "Cannot open list file " << aFileName << ": " << f.errorString() << endl;
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
		cerr << "Bad argument " << i << " (" << aArgs[i] << "): needs " << N << " parameters" << endl; \
		return false; \
	} \

/** Processes all command-line arguments. */
static bool processArgs(const vector<string> & aArgs)
{
	size_t argc = aArgs.size();
	vector<QString> files;
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
					addListFile(QString::fromStdString(aArgs[i + 1]));
					i += 1;
					break;
				}

			}  // switch (arg)
			continue;
		}  // if ('-')
		g_FileNames.push_back(QString::fromStdString(aArgs[i]));
	}
	return true;
}





/** Outputs the raw and processed ID3 tags to stdout, formatted as Lua source. */
static void outputId3Tag(const Results & aResults)
{
	cout << "\t\trawID3Tag =" << endl;
	cout << "\t\t{" << endl;
	if (aResults.mRawTag.mAuthor.isPresent())
	{
		cout << "\t\t\tauthor  = \"" << luaEscapeString(aResults.mRawTag.mAuthor.value()) << "\"," << endl;
	}
	if (aResults.mRawTag.mTitle.isPresent())
	{
		cout << "\t\t\ttitle   = \"" << luaEscapeString(aResults.mRawTag.mTitle.value()) << "\"," << endl;
	}
	if (aResults.mRawTag.mGenre.isPresent())
	{
		cout << "\t\t\tgenre   = \"" << luaEscapeString(aResults.mRawTag.mGenre.value()) << "\"," << endl;
	}
	if (aResults.mRawTag.mMeasuresPerMinute.isPresent())
	{
		cout << "\t\t\tmpm     = " << aResults.mRawTag.mMeasuresPerMinute.value() << "," << endl;
	}
	if (aResults.mRawTag.mComment.isPresent())
	{
		cout << "\t\t\tcomment = \"" << luaEscapeString(aResults.mRawTag.mComment.value()) << "\"," << endl;
	}
	cout << "\t\t}," << endl;

	cout << "\t\tparsedID3Tag =" << endl;
	cout << "\t\t{" << endl;
	const auto & parsedTag = aResults.mParsedTag;
	if (parsedTag.mAuthor.isPresent())
	{
		cout << "\t\t\tauthor = \"" << luaEscapeString(parsedTag.mAuthor.value()) << "\"," << endl;
	}
	if (parsedTag.mTitle.isPresent())
	{
		cout << "\t\t\ttitle = \"" << luaEscapeString(parsedTag.mTitle.value()) << "\"," << endl;
	}
	if (parsedTag.mGenre.isPresent())
	{
		cout << "\t\t\tgenre = \"" << luaEscapeString(parsedTag.mGenre.value()) << "\"," << endl;
	}
	if (parsedTag.mMeasuresPerMinute.isPresent())
	{
		cout << "\t\t\tmpm = " << parsedTag.mMeasuresPerMinute.value() << "," << endl;
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
		cout << "\t\ttempo = " << results->mTempo << "," << endl;
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

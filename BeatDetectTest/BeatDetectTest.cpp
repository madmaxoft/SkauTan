#include <iostream>
#include <QString>
#include <QFile>
#include "../TempoDetector.h"
#include "../Song.h"





using namespace std;





void processFile(const QString & a_FileName, const TempoDetector::Options & a_Options)
{
	if (!QFile::exists(a_FileName))
	{
		cerr << "File doesn't exist: " << a_FileName.toStdString() << endl;
		return;
	}

	// Detect:
	cout << "Detecting..." << endl;
	SongPtr song = std::make_shared<Song>(a_FileName, nullptr);
	TempoDetector td;
	auto res = td.scanSong(song, a_Options);

	cout << "-----------------------------------------------" << endl;;
	cout << "Detected data for " << a_FileName.toStdString() << ":" << endl;
	cout << "Total number of beats: " << res->m_Beats.size() << endl;
	cout << "Total number of histogram entries: " << res->m_Histogram.size() << endl;
	cout << "Confidence for compatible tempo groups:" << endl;
	for (const auto & c: res->m_Confidences)
	{
		cout << "  " << c.first << ": " << c.second << endl;
	}
}





void printUsage()
{
	TempoDetector::Options options;
	cout << "BeatDetectTest" << endl;
	cout << "--------------" << endl;
	cout << "Detects tempo in audio files, using various algorithms and options." << endl;
	cout << "Part of SkauTan player, https://github.com/madmaxoft/SkauTan" << endl;
	cout << endl;
	cout << "Usage:" << endl;
	cout << "BeatDetectTest [options] [filename]" << endl;
	cout << endl;
	cout << "Available options:" << endl;
	cout << "  -a <N>          ... Use level algorithm N (default: " << options.m_LevelAlgorithm << ")" << endl;
	cout << "                        0: SumDist" << endl;
	cout << "                        1: MinMax" << endl;
	cout << "                        2: DiscreetSineTransform" << endl;
	cout << "  -w <N>          ... Set window size to N (default: " << options.m_WindowSize << ")" << endl;
	cout << "  -s <N>          ... Set the stride to N (default: " << options.m_Stride << ")" << endl;
	cout << "  -v <N>          ... Set the number of levels to average for beat strength (default: " << options.m_LevelAvg << ")" << endl;
	cout << "  -p <N>          ... Set the number of levels to check for peak (default: " << options.m_LevelPeak << ")" << endl;
	cout << "  -c <N>          ... Set the histogram cutoff (default: " << options.m_HistogramCutoff << ")" << endl;
	cout << "  -f <min> <max>  ... Fold the histogram into the specified tempo range (default: ";
	if (options.m_ShouldFoldHistogram)
	{
		cout << "-f " << options.m_HistogramFoldMin << ' ' << options.m_HistogramFoldMax << ")" << endl;
	}
	else
	{
		cout << "do not fold)" << endl;
	}
	cout << "  -b <filename>   ... Output debug audio with beats to file" << endl;
	cout << "  -d <filename>   ... Output debug audio with levels to file" << endl;
	cout << "  -h              ... Print this help" << endl;
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

	cout << "Done." << std::endl;
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

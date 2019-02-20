#include <string>
#include <vector>
#include <iostream>
#include <QDebug>
#include "../src/Audio/AVPP.hpp"
#include "../src/Audio/PlaybackBuffer.hpp"
#include "../src/Utils.hpp"





using namespace std;





struct Options
{
	QString mDebugAudioFileName;
	int     mSampleRate = 48000;
	size_t  mStride = 1024;           // Size of stride between extraction points, in samples
	size_t  mWindowSize = 1024;       // Size of the window to use for each extraction point, in samples
	size_t  mExtractFrequency = 100;  // The frequency (in Hz) to extract
};





void printUsage()
{
	cerr << "FreqExtract" << endl;
	cerr << "-----------" << endl;
	cerr << "Extracts a single frequency's strength across an audio file." << endl;
	cerr << "Part of SkauTan player, https://github.com/madmaxoft/SkauTan" << endl;
	cerr << endl;
	cerr << "Usage:" << endl;
}





void outputDebugFile(vector<float> aStrengths, const PlaybackBuffer & aAudio, const Options & aOptions)
{
	if (aOptions.mDebugAudioFileName.isEmpty())
	{
		return;
	}

	QFile f(aOptions.mDebugAudioFileName);
	if (!f.open(QFile::WriteOnly))
	{
		cerr << "Cannot open debug file " << aOptions.mDebugAudioFileName.toStdString() << " for writing" << endl;
		return;
	}

	// Find the maximum strength, for normalization:
	float maxStrength = 0;
	for (const auto s: aStrengths)
	{
		if (s > maxStrength)
		{
			maxStrength = s;
		}
		if (-s > maxStrength)
		{
			maxStrength = -s;
		}
	}
	qDebug() << "maxStrength: " << maxStrength;

	auto audio = reinterpret_cast<const qint16 *>(aAudio.audioData().data());
	auto numStrengths = aStrengths.size();
	vector<qint16> output;
	output.resize(2 * aOptions.mStride);
	for (size_t i = 0; i < numStrengths; ++i)
	{
		auto val = static_cast<qint16>(Utils::clamp(static_cast<int>(32768 * aStrengths[i] / maxStrength), -32768, 32767));
		for (size_t j = 0; j < aOptions.mStride; ++j)
		{
			output[2 * j] = audio[i * aOptions.mStride + j];
			output[2 * j + 1] = val;
		}
		f.write(
			reinterpret_cast<const char *>(output.data()),
			static_cast<int>(output.size() * sizeof(qint16))
		);
	}
}





void processFile(const QString & aFileName, const Options & aOptions)
{
	if (!QFile::exists(aFileName))
	{
		cerr << "File doesn't exist: " << aFileName.toStdString() << endl;
		return;
	}

	// Decode the audio data:
	auto context = AVPP::Format::createContext(aFileName);
	if (context == nullptr)
	{
		qWarning() << "Cannot open file " << aFileName;
		return;
	}
	QAudioFormat fmt;
	fmt.setSampleRate(aOptions.mSampleRate);
	fmt.setChannelCount(1);
	fmt.setSampleSize(16);
	fmt.setSampleType(QAudioFormat::SignedInt);
	fmt.setByteOrder(QAudioFormat::Endian(QSysInfo::ByteOrder));
	fmt.setCodec("audio/pcm");
	PlaybackBuffer buf(fmt);
	if (!context->routeAudioTo(&buf))
	{
		qWarning() << "Cannot route audio from file " << aFileName;
		return;
	}
	context->decode();

	// Extract the frequency:
	auto numSamples = buf.bufferLimit() / sizeof(qint16);
	auto audio = reinterpret_cast<const qint16 *>(buf.audioData().data());
	auto maxNumSamples = numSamples - aOptions.mWindowSize - aOptions.mStride;
	vector<float> res;
	res.reserve(maxNumSamples);
	vector<float> rateFactor;
	for (size_t k = 0; k < 10; ++k)
	{
		auto freq = aOptions.mExtractFrequency * (80 + 2 * k) / 100;
		rateFactor.push_back(static_cast<float>(2 * M_PI) / aOptions.mSampleRate * freq);
	}
	for (size_t i = 0; i < maxNumSamples; i += aOptions.mStride)
	{
		float freqStrength = 0;
		for (size_t j = 0; j < aOptions.mWindowSize; ++j)
		{
			auto sample = audio[i + j] / 32768.0f;
			for (const auto rf: rateFactor)
			{
				auto cosval = cosf(rf * (i + j));
				auto sinval = sinf(rf * (i + j));
				freqStrength += sample * cosval + sample * sinval;
			}
		}
		res.push_back(abs(freqStrength));
		// res.push_back(freqStrength);
	}

	outputDebugFile(res, buf, aOptions);
}





#define  NEED_ARG(N) \
	if (i + N >= argc) \
	{ \
		cerr << "Bad argument " << i << " (" << aArgs[i] << "): needs " << N << " parameters" << endl; \
		return 1; \
	} \

int processArgs(const vector<string> & aArgs)
{
	Options options;
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
				case 'd':
				case 'D':
				{
					NEED_ARG(1);
					options.mDebugAudioFileName = QString::fromStdString(aArgs[i + 1]);
					i += 1;
					break;
				}
				case 'f':
				case 'F':
				{
					NEED_ARG(1);
					options.mExtractFrequency = static_cast<size_t>(stoi(aArgs[i + 1]));
					i += 1;
					break;
				}
				case 'r':
				case 'R':
				{
					NEED_ARG(1);
					options.mSampleRate = stoi(aArgs[i + 1]);
					i += 1;
					break;
				}
				case 's':
				case 'S':
				{
					NEED_ARG(1);
					options.mStride = static_cast<size_t>(stoll(aArgs[i + 1]));
					i += 1;
					break;
				}
				case 'w':
				case 'W':
				{
					NEED_ARG(1);
					options.mWindowSize = static_cast<size_t>(stoll(aArgs[i + 1]));
					i += 1;
					break;
				}
			}  // switch (arg)
			continue;
		}  // if ('-')

		processFile(QString::fromStdString(aArgs[i]), options);
	}

	cerr << "Done." << std::endl;
	return 0;
}





int main(int argc, char * argv[])
{
	vector<string> args;
	for (int i = 1; i < argc; ++i)
	{
		args.emplace_back(argv[i]);
	}
	return processArgs(args);
}

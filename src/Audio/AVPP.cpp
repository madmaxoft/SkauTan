#include "AVPP.hpp"
#include <cassert>
#include "../Exception.hpp"

extern "C"
{
	#include <libavutil/opt.h>
}

#include "PlaybackBuffer.hpp"





namespace AVPP
{





////////////////////////////////////////////////////////////////////////////////
// Initializer:

/** Singleton-like class that initializes the LibAV framework.
Before using any LibAV functions, a call to Initializer::init() should be made. */
class Initializer
{
	Initializer()
	{
		av_log_set_callback(&qtLogger);
		av_register_all();
	}


	static void qtLogger(void *, int, const char * aFormat, va_list aParams)
	{
		// Ignore LibAV logging for now
		Q_UNUSED(aFormat);
		Q_UNUSED(aParams);
	}


public:

	/** Initializes the LibAV. */
	static Initializer & init()
	{
		static Initializer instance;
		return instance;
	}
};





////////////////////////////////////////////////////////////////////////////////
// FileIO:

std::shared_ptr<FileIO> FileIO::createContext(const QString & aFileName)
{
	Initializer::init();
	auto res = std::shared_ptr<FileIO>(new FileIO);
	if (res == nullptr)
	{
		return nullptr;
	}
	if (!res->Open(aFileName))
	{
		return nullptr;
	}
	static const int bufferSize = 4096;
	auto buffer = reinterpret_cast<unsigned char *>(av_malloc(bufferSize));
	res->mContext = avio_alloc_context(buffer, bufferSize, 0, res.get(), &FileIO::read, nullptr, &FileIO::seek);
	if (res->mContext == nullptr)
	{
		av_free(buffer);
		return nullptr;
	}
	return res;
}





FileIO::FileIO():
	mContext(nullptr)
{
}





FileIO::~FileIO()
{
	if (mContext != nullptr)
	{
		avio_context_free(&mContext);
	}
}





bool FileIO::Open(const QString & aFileName)
{
	std::unique_ptr<QFile> f(new QFile(aFileName));
	if (!f->open(QIODevice::ReadOnly))
	{
		return false;
	}

	std::swap(mFile, f);
	return true;
}





int FileIO::read(void * aThis, uint8_t * aDst, int aSize)
{
	auto This = reinterpret_cast<FileIO *>(aThis);
	return static_cast<int>(This->mFile->read(reinterpret_cast<char *>(aDst), aSize));
}





int64_t FileIO::seek(void * aThis, int64_t aOffset, int aWhence)
{
	// qDebug() << "Seek to offset " << aOffset << " from " << aWhence;
	auto This = reinterpret_cast<FileIO *>(aThis);
	switch (aWhence)
	{
		case SEEK_CUR:
		{
			if (!This->mFile->seek(This->mFile->pos() + aOffset))
			{
				qWarning() << "Relative-Seek failed.";
				return -1;
			}
			return This->mFile->pos();
		}
		case SEEK_SET:
		{
			if (!This->mFile->seek(aOffset))
			{
				qWarning() << "Absolute-Seek failed.";
				return -1;
			}
			return aOffset;
		}
		case SEEK_END:
		{
			if (!This->mFile->seek(This->mFile->size() - aOffset))
			{
				qWarning() << "End-Seek failed.";
				return -1;
			}
			return This->mFile->pos();
		}
	}
	// _X 2018-05-11: This seems to happen way too often upon opening a new file, disabling.
	// qWarning() << "Unexpected seek operation: " << aWhence << ", offset " << aOffset;
	return -1;
}





////////////////////////////////////////////////////////////////////////////////
// CodecContext:

std::shared_ptr<CodecContext> CodecContext::create(AVCodec * aCodec)
{
	Initializer::init();
	auto res = std::shared_ptr<CodecContext>(new CodecContext(aCodec));
	if (res->mContext == nullptr)
	{
		return nullptr;
	}
	return res;
}





CodecContext::~CodecContext()
{
	if (mContext != nullptr)
	{
		avcodec_free_context(&mContext);
	}
}





bool CodecContext::open(AVCodec * aCodec)
{
	auto ret = avcodec_open2(mContext, aCodec, nullptr);
	if (ret < 0)
	{
		qWarning() << "Cannot open codec: " << ret;
		return false;
	}
	return true;
}





CodecContext::CodecContext(AVCodec * aCodec):
	mContext(avcodec_alloc_context3(aCodec))
{
}





////////////////////////////////////////////////////////////////////////////////
// Resampler:

AVSampleFormat Resampler::sampleFormatFromSampleType(QAudioFormat::SampleType aSampleType)
{
	switch (aSampleType)
	{
		case QAudioFormat::SignedInt: return AV_SAMPLE_FMT_S16;
		case QAudioFormat::Float:     return AV_SAMPLE_FMT_FLT;
		default:
		{
			qWarning() << "Unhandled sample type: " << aSampleType;
			throw RuntimeError("Unhandled sample type: %1", aSampleType);
		}
	}
}





uint64_t Resampler::channelLayoutFromChannelCount(int aChannelCount)
{
	switch (aChannelCount)
	{
		case 1: return AV_CH_LAYOUT_MONO;
		case 2: return AV_CH_LAYOUT_STEREO;
		case 4: return AV_CH_LAYOUT_3POINT1;
		case 5: return AV_CH_LAYOUT_5POINT0;
		case 6: return AV_CH_LAYOUT_5POINT1;
		default:
		{
			qWarning() << "Unhandled ChannelCount:" << aChannelCount;
			throw RuntimeError("Unhandled ChannelCount: %1: ", aChannelCount);
		}
	}
}





Resampler * Resampler::create(
	uint64_t aSrcChannelLayout,
	int aSrcSampleRate,
	AVSampleFormat aSrcSampleFormat,
	const QAudioFormat & aOutputFormat
)
{
	Initializer::init();
	auto res = std::unique_ptr<Resampler>(new Resampler);
	if (!res->init(aSrcChannelLayout, aSrcSampleRate, aSrcSampleFormat, aOutputFormat))
	{
		return nullptr;
	}
	return res.release();
}





Resampler * Resampler::create(
	int aSrcSampleRate,
	int aDstSampleRate,
	uint64_t aChannelLayout,
	AVSampleFormat aSampleFormat
)
{
	Initializer::init();
	auto res = std::unique_ptr<Resampler>(new Resampler);
	if (!res->init(
		aChannelLayout,
		aSrcSampleRate,
		aSampleFormat,
		aChannelLayout,
		aDstSampleRate,
		aSampleFormat
	))
	{
		return nullptr;
	}
	return res.release();
}





Resampler::Resampler():
	mContext(nullptr),
	mBuffer(nullptr),
	mBufferMaxNumSamples(0),
	mBufferLineSize(0),
	mBufferSampleFormat(AV_SAMPLE_FMT_S16P)
{
}





Resampler::~Resampler()
{
	if (mContext != nullptr)
	{
		swr_free(&mContext);
	}
	if (mBuffer != nullptr)
	{
		av_freep(&mBuffer[0]);
	}
}





bool Resampler::init(
	uint64_t aSrcChannelLayout,
	int aSrcSampleRate,
	AVSampleFormat aSrcSampleFormat,
	const QAudioFormat & aOutputFormat
)
{
	try
	{
		// Translate QAudioFormat into LibAV/SWR format:
		auto dstChannelLayout = channelLayoutFromChannelCount(aOutputFormat.channelCount());
		assert(av_get_channel_layout_nb_channels(dstChannelLayout) == aOutputFormat.channelCount());
		AVSampleFormat dstSampleFormat = sampleFormatFromSampleType(aOutputFormat.sampleType());

		return init(
			aSrcChannelLayout, aSrcSampleRate, aSrcSampleFormat,
			dstChannelLayout, aOutputFormat.sampleRate(), dstSampleFormat
		);
	}
	catch (const std::exception & exc)
	{
		qDebug() << "Cannot initialize SWR: " << exc.what();
		return false;
	}
}





bool Resampler::init(
	uint64_t aSrcChannelLayout,
	int aSrcSampleRate,
	AVSampleFormat aSrcSampleFormat,
	uint64_t aDstChannelLayout,
	int aDstSampleRate,
	AVSampleFormat aDstSampleFormat
)
{
	// Create and init the context:
	mContext = swr_alloc_set_opts(
		nullptr,
		static_cast<int64_t>(aDstChannelLayout),
		aDstSampleFormat,
		aDstSampleRate,
		static_cast<int64_t>(aSrcChannelLayout),
		aSrcSampleFormat,
		aSrcSampleRate,
		0, nullptr  // logging - unused
	);
	if (mContext == nullptr)
	{
		qWarning() << "Failed to create SWResampler context.";
		return false;
	}
	auto err = swr_init(mContext);
	if (err != 0)
	{
		qWarning() << "Failed to initialize SWResampler context: " << err;
		return false;
	}

	// Create the output buffer structure:
	mDstChannelCount = av_get_channel_layout_nb_channels(aDstChannelLayout);
	err = av_samples_alloc_array_and_samples(
		&mBuffer, &mBufferLineSize, mDstChannelCount,
		1000, aDstSampleFormat, 0
	);
	if (err < 0)
	{
		qWarning() << "Could not allocate conversion buffer structure: " << err;
		return false;
	}

	mBufferSampleFormat = aDstSampleFormat;
	return true;
}





std::pair<uint8_t *, size_t> Resampler::convert(const uint8_t ** aBuffers, int aLen)
{
	assert(mContext != nullptr);

	int outLen = swr_get_out_samples(mContext, aLen);
	if (outLen < 0)
	{
		qWarning() << "Cannot query conversion buffer size: " << outLen;
		outLen = 64 * 1024;  // Take a wild guess
	}
	if (outLen > mBufferMaxNumSamples)
	{
		mBufferMaxNumSamples = outLen;
		av_freep(&mBuffer[0]);
		auto err = av_samples_alloc(
			mBuffer, &mBufferLineSize, mDstChannelCount,
			mBufferMaxNumSamples, mBufferSampleFormat, 1
		);
		if (err < 0)
		{
			qWarning() << "Failed to realloc sample buffer: " << err;
			return std::make_pair(nullptr, 0);
		}
	}
	auto numOutputSamples = swr_convert(mContext, mBuffer, mBufferMaxNumSamples, aBuffers, aLen);
	if (numOutputSamples < 0)
	{
		qWarning() << "Sample conversion failed: " << numOutputSamples;
		return std::make_pair(nullptr, 0);
	}

	// Output through to AudioOutput:
	return std::make_pair(mBuffer[0], static_cast<size_t>(numOutputSamples));
}





////////////////////////////////////////////////////////////////////////////////
// Format:

FormatPtr Format::createContext(const QString & aFileName)
{
	Initializer::init();

	// Create an IO wrapper:
	auto io = FileIO::createContext(aFileName);
	if (io == nullptr)
	{
		qWarning() << "IO creation failed (" << aFileName << ")";
		return nullptr;
	}

	// Create the context:
	auto res = std::unique_ptr<Format>(new Format(io));
	if ((res == nullptr) || (res->mContext == nullptr))
	{
		qWarning() << "Creation failed (" << aFileName << ")";
		return nullptr;
	}

	// Open and detect format:
	auto ret = avformat_open_input(&res->mContext, nullptr, nullptr, nullptr);
	if (ret != 0)
	{
		// User-supplied AVFormatContext is freed upon failure, need to un-bind:
		res->mContext = nullptr;
		qWarning() << "Opening failed: " << ret << " (" << aFileName << ")";
		return nullptr;
	}

	// Find the stream info:
	ret = avformat_find_stream_info(res->mContext, nullptr);
	if (ret < 0)
	{
		qWarning() << "Cannot find stream info: " << ret << " (" << aFileName << ")";
	}

	return res;
}





Format::Format(std::shared_ptr<FileIO> aIO):
	mContext(avformat_alloc_context()),
	mIO(aIO),
	mAudioOutput(nullptr),
	mAudioStreamIdx(-1)
{
	if (mContext != nullptr)
	{
		mContext->pb = mIO->mContext;
	}
}





Format::~Format()
{
	if (mContext != nullptr)
	{
		av_freep(mContext->pb);
		avformat_close_input(&mContext);
	}
}





bool Format::routeAudioTo(PlaybackBuffer * aPlaybackBuffer)
{
	assert(mAudioOutput == nullptr);  // Only one stream can be decoded into audio within a session

	AVCodec * audioDecoder = nullptr;
	mAudioStreamIdx = av_find_best_stream(mContext, AVMEDIA_TYPE_AUDIO, -1, -1, &audioDecoder, 0);
	if (mAudioStreamIdx < 0)
	{
		qWarning() << "Failed to find an audio stream in the file: " << mAudioStreamIdx;
		mAudioStreamIdx = -1;
		return false;
	}

	// Create a decoding context:
	mAudioDecoderContext = CodecContext::create(audioDecoder);
	if (mAudioDecoderContext == nullptr)
	{
		qWarning() << "Failed to allocate decoder context.";
		return false;
	}
	avcodec_parameters_to_context(mAudioDecoderContext->mContext, mContext->streams[mAudioStreamIdx]->codecpar);
	av_opt_set_int(mAudioDecoderContext->mContext, "refcounted_frames", 1, 0);

	// Open the audio decoder:
	if (!mAudioDecoderContext->open(audioDecoder))
	{
		qWarning() << "Cannot open decoder context.";
		mAudioDecoderContext.reset();
		return false;
	}
	mAudioOutput = aPlaybackBuffer;
	aPlaybackBuffer->setDuration(static_cast<double>(mContext->duration) / 1000000);
	return true;
}





bool Format::feedRawAudioDataTo(
	std::function<void (const void * /* aData */, int /* aSize */)> aFunction,
	double & aLengthSec
)
{
	assert(mAudioOutput == nullptr);  // Cannot work as both decoder and feeder

	// Find the stream to feed:
	AVCodec * audioDecoder = nullptr;
	mAudioStreamIdx = av_find_best_stream(mContext, AVMEDIA_TYPE_AUDIO, -1, -1, &audioDecoder, 0);
	if (mAudioStreamIdx < 0)
	{
		qWarning() << "Failed to find an audio stream in the file: " << mAudioStreamIdx;
		mAudioStreamIdx = -1;
		return false;
	}

	// Feed the stream:
	AVPacket packet;
	qint64 maxPts = 0;
	while (true)
	{
		auto ret = av_read_frame(mContext, &packet);
		if (ret < 0)
		{
			break;
		}

		if (packet.stream_index == mAudioStreamIdx)
		{
			if (packet.size >= 0)
			{
				aFunction(packet.data, packet.size);
			}
			else
			{
				qDebug() << "Negative packet size.";
			}
		}
		if (packet.pts != AV_NOPTS_VALUE)
		{
			maxPts = std::max<qint64>(maxPts, packet.pts);
		}
		av_packet_unref(&packet);
	}
	if (maxPts > 0)
	{
		auto stream = mContext->streams[mAudioStreamIdx];
		aLengthSec = static_cast<double>(maxPts) * stream->time_base.num / stream->time_base.den;
	}
	qDebug() << "Feeding done.";
	return true;
}





void Format::decode()
{
	assert(mAudioOutput != nullptr);
	assert(mAudioDecoderContext != nullptr);

	AVPacket packet;
	AVFrame * frame = av_frame_alloc();
	mShouldTerminate = false;
	while (!mShouldTerminate)
	{
		auto ret = av_read_frame(mContext, &packet);
		if (ret < 0)
		{
			// Normal condition on EOF
			// qWarning() << "Failed to read frame: " << ret;
			return;
		}

		if (packet.stream_index == mAudioStreamIdx)
		{
			ret = avcodec_send_packet(mAudioDecoderContext->mContext, &packet);
			if (ret < 0)
			{
				qWarning() << "Error while sending a packet to the decoder: " << ret;
				// return;
			}

			while (ret >= 0)
			{
				ret = avcodec_receive_frame(mAudioDecoderContext->mContext, frame);
				if ((ret == AVERROR(EAGAIN)) || (ret == AVERROR_EOF))
				{
					// Need more data / explicit EOF found
					break;
				}
				else if (ret < 0)
				{
					qWarning() << "Error while receiving a frame from the decoder: " << ret;
					return;
				}
				outputAudioData(frame);
			}
		}
		av_packet_unref(&packet);
	}
	qDebug() << "Decoding done.";
}





std::shared_ptr<PlaybackBuffer> Format::decodeEntireAudio(const QAudioFormat & aFormat)
{
	auto res = std::make_shared<PlaybackBuffer>(aFormat);
	routeAudioTo(res.get());
	decode();
	return res;
}





void Format::outputAudioData(AVFrame * aFrame)
{
	if (mResampler == nullptr)
	{
		mResampler.reset(Resampler::create(
			aFrame->channel_layout,
			aFrame->sample_rate,
			static_cast<AVSampleFormat>(aFrame->format),
			mAudioOutput->format()
		));
		if (mResampler == nullptr)
		{
			qWarning() << "Cannot create audio resampler.";
			mShouldTerminate = true;
			mAudioOutput->abortWithError();
			return;
		}
	}
	auto out = mResampler->convert(const_cast<const uint8_t **>(aFrame->data), aFrame->nb_samples);
	auto numBytes = out.second * static_cast<size_t>(mAudioOutput->format().bytesPerFrame());
	mShouldTerminate = !mAudioOutput->writeDecodedAudio(out.first, numBytes);
}





////////////////////////////////////////////////////////////////////////////////
// InputFormats:

/** Singleton that provides the list of all supported input formats. */
class InputFormats
{
	InputFormats()
	{
		Initializer::init();
		AVInputFormat * fmt = nullptr;
		while ((fmt = av_iformat_next(fmt)) != nullptr)
		{
			if (fmt->extensions != nullptr)
			{
				auto ext = QString::fromLocal8Bit(fmt->extensions);
				for (const auto & e: ext.split(','))
				{
					mExtensions.push_back(e.toLower());
				}
			}
		}
	}

	std::vector<QString> mExtensions;

public:

	static InputFormats & get()
	{
		static InputFormats instance;
		return instance;
	}

	/** Returns true iff the extension is supported by the input formats. */
	bool isExtensionSupported(const QString & aExtension)
	{
		auto lcExt = aExtension.toLower();
		for (const auto & ext: mExtensions)
		{
			if (lcExt == ext)
			{
				return true;
			}
		}
		return false;
	}
};





////////////////////////////////////////////////////////////////////////////////
// AVPP namespace:

bool isExtensionSupported(const QString & aExtension)
{
	Q_UNUSED(aExtension);
	return InputFormats::get().isExtensionSupported(aExtension);
}





}  // namespace AVPP

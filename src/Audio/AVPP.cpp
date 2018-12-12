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


	static void qtLogger(void *, int, const char * a_Format, va_list a_Params)
	{
		// Ignore LibAV logging for now
		Q_UNUSED(a_Format);
		Q_UNUSED(a_Params);
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

std::shared_ptr<FileIO> FileIO::createContext(const QString & a_FileName)
{
	Initializer::init();
	auto res = std::shared_ptr<FileIO>(new FileIO);
	if (res == nullptr)
	{
		return nullptr;
	}
	if (!res->Open(a_FileName))
	{
		return nullptr;
	}
	static const int bufferSize = 4096;
	auto buffer = reinterpret_cast<unsigned char *>(av_malloc(bufferSize));
	res->m_Context = avio_alloc_context(buffer, bufferSize, 0, res.get(), &FileIO::read, nullptr, &FileIO::seek);
	if (res->m_Context == nullptr)
	{
		av_free(buffer);
		return nullptr;
	}
	return res;
}





FileIO::FileIO():
	m_Context(nullptr)
{
}





FileIO::~FileIO()
{
	if (m_Context != nullptr)
	{
		avio_context_free(&m_Context);
	}
}





bool FileIO::Open(const QString & a_FileName)
{
	std::unique_ptr<QFile> f(new QFile(a_FileName));
	if (!f->open(QIODevice::ReadOnly))
	{
		return false;
	}

	std::swap(m_File, f);
	return true;
}





int FileIO::read(void * a_This, uint8_t * a_Dst, int a_Size)
{
	auto This = reinterpret_cast<FileIO *>(a_This);
	return static_cast<int>(This->m_File->read(reinterpret_cast<char *>(a_Dst), a_Size));
}





int64_t FileIO::seek(void * a_This, int64_t a_Offset, int a_Whence)
{
	// qDebug() << "Seek to offset " << a_Offset << " from " << a_Whence;
	auto This = reinterpret_cast<FileIO *>(a_This);
	switch (a_Whence)
	{
		case SEEK_CUR:
		{
			if (!This->m_File->seek(This->m_File->pos() + a_Offset))
			{
				qWarning() << "Relative-Seek failed.";
				return -1;
			}
			return This->m_File->pos();
		}
		case SEEK_SET:
		{
			if (!This->m_File->seek(a_Offset))
			{
				qWarning() << "Absolute-Seek failed.";
				return -1;
			}
			return a_Offset;
		}
		case SEEK_END:
		{
			if (!This->m_File->seek(This->m_File->size() - a_Offset))
			{
				qWarning() << "End-Seek failed.";
				return -1;
			}
			return This->m_File->pos();
		}
	}
	// _X 2018-05-11: This seems to happen way too often upon opening a new file, disabling.
	// qWarning() << "Unexpected seek operation: " << a_Whence << ", offset " << a_Offset;
	return -1;
}





////////////////////////////////////////////////////////////////////////////////
// CodecContext:

std::shared_ptr<CodecContext> CodecContext::create(AVCodec * a_Codec)
{
	Initializer::init();
	auto res = std::shared_ptr<CodecContext>(new CodecContext(a_Codec));
	if (res->m_Context == nullptr)
	{
		return nullptr;
	}
	return res;
}





CodecContext::~CodecContext()
{
	if (m_Context != nullptr)
	{
		avcodec_free_context(&m_Context);
	}
}





bool CodecContext::open(AVCodec * a_Codec)
{
	auto ret = avcodec_open2(m_Context, a_Codec, nullptr);
	if (ret < 0)
	{
		qWarning() << "Cannot open codec: " << ret;
		return false;
	}
	return true;
}





CodecContext::CodecContext(AVCodec * a_Codec):
	m_Context(avcodec_alloc_context3(a_Codec))
{
}





////////////////////////////////////////////////////////////////////////////////
// Resampler:

AVSampleFormat Resampler::sampleFormatFromSampleType(QAudioFormat::SampleType a_SampleType)
{
	switch (a_SampleType)
	{
		case QAudioFormat::SignedInt: return AV_SAMPLE_FMT_S16;
		case QAudioFormat::Float:     return AV_SAMPLE_FMT_FLT;
		default:
		{
			qWarning() << "Unhandled sample type: " << a_SampleType;
			throw RuntimeError("Unhandled sample type: %1", a_SampleType);
		}
	}
}





uint64_t Resampler::channelLayoutFromChannelCount(int a_ChannelCount)
{
	switch (a_ChannelCount)
	{
		case 1: return AV_CH_LAYOUT_MONO;
		case 2: return AV_CH_LAYOUT_STEREO;
		case 4: return AV_CH_LAYOUT_3POINT1;
		case 5: return AV_CH_LAYOUT_5POINT0;
		case 6: return AV_CH_LAYOUT_5POINT1;
		default:
		{
			qWarning() << "Unhandled ChannelCount:" << a_ChannelCount;
			throw RuntimeError("Unhandled ChannelCount: %1: ", a_ChannelCount);
		}
	}
}





Resampler * Resampler::create(
	uint64_t a_SrcChannelLayout,
	int a_SrcSampleRate,
	AVSampleFormat a_SrcSampleFormat,
	const QAudioFormat & a_OutputFormat
)
{
	Initializer::init();
	auto res = std::unique_ptr<Resampler>(new Resampler);
	if (!res->init(a_SrcChannelLayout, a_SrcSampleRate, a_SrcSampleFormat, a_OutputFormat))
	{
		return nullptr;
	}
	return res.release();
}





Resampler * Resampler::create(
	int a_SrcSampleRate,
	int a_DstSampleRate,
	uint64_t a_ChannelLayout,
	AVSampleFormat a_SampleFormat
)
{
	Initializer::init();
	auto res = std::unique_ptr<Resampler>(new Resampler);
	if (!res->init(
		a_ChannelLayout,
		a_SrcSampleRate,
		a_SampleFormat,
		a_ChannelLayout,
		a_DstSampleRate,
		a_SampleFormat
	))
	{
		return nullptr;
	}
	return res.release();
}





Resampler::Resampler():
	m_Context(nullptr),
	m_Buffer(nullptr),
	m_BufferMaxNumSamples(0),
	m_BufferLineSize(0),
	m_BufferSampleFormat(AV_SAMPLE_FMT_S16P)
{
}





Resampler::~Resampler()
{
	if (m_Context != nullptr)
	{
		swr_free(&m_Context);
	}
	if (m_Buffer != nullptr)
	{
		av_freep(&m_Buffer[0]);
	}
}





bool Resampler::init(
	uint64_t a_SrcChannelLayout,
	int a_SrcSampleRate,
	AVSampleFormat a_SrcSampleFormat,
	const QAudioFormat & a_OutputFormat
)
{
	try
	{
		// Translate QAudioFormat into LibAV/SWR format:
		auto dstChannelLayout = channelLayoutFromChannelCount(a_OutputFormat.channelCount());
		assert(av_get_channel_layout_nb_channels(dstChannelLayout) == a_OutputFormat.channelCount());
		AVSampleFormat dstSampleFormat = sampleFormatFromSampleType(a_OutputFormat.sampleType());

		return init(
			a_SrcChannelLayout, a_SrcSampleRate, a_SrcSampleFormat,
			dstChannelLayout, a_OutputFormat.sampleRate(), dstSampleFormat
		);
	}
	catch (const std::exception & exc)
	{
		qDebug() << "Cannot initialize SWR: " << exc.what();
		return false;
	}
}





bool Resampler::init(
	uint64_t a_SrcChannelLayout,
	int a_SrcSampleRate,
	AVSampleFormat a_SrcSampleFormat,
	uint64_t a_DstChannelLayout,
	int a_DstSampleRate,
	AVSampleFormat a_DstSampleFormat
)
{
	// Create and init the context:
	m_Context = swr_alloc_set_opts(
		nullptr,
		static_cast<int64_t>(a_DstChannelLayout),
		a_DstSampleFormat,
		a_DstSampleRate,
		static_cast<int64_t>(a_SrcChannelLayout),
		a_SrcSampleFormat,
		a_SrcSampleRate,
		0, nullptr  // logging - unused
	);
	if (m_Context == nullptr)
	{
		qWarning() << "Failed to create SWResampler context.";
		return false;
	}
	auto err = swr_init(m_Context);
	if (err != 0)
	{
		qWarning() << "Failed to initialize SWResampler context: " << err;
		return false;
	}

	// Create the output buffer structure:
	m_DstChannelCount = av_get_channel_layout_nb_channels(a_DstChannelLayout);
	err = av_samples_alloc_array_and_samples(
		&m_Buffer, &m_BufferLineSize, m_DstChannelCount,
		1000, a_DstSampleFormat, 0
	);
	if (err < 0)
	{
		qWarning() << "Could not allocate conversion buffer structure: " << err;
		return false;
	}

	m_BufferSampleFormat = a_DstSampleFormat;
	return true;
}





std::pair<uint8_t *, size_t> Resampler::convert(const uint8_t ** a_Buffers, int a_Len)
{
	assert(m_Context != nullptr);

	int outLen = swr_get_out_samples(m_Context, a_Len);
	if (outLen < 0)
	{
		qWarning() << "Cannot query conversion buffer size: " << outLen;
		outLen = 64 * 1024;  // Take a wild guess
	}
	if (outLen > m_BufferMaxNumSamples)
	{
		m_BufferMaxNumSamples = outLen;
		av_freep(&m_Buffer[0]);
		auto err = av_samples_alloc(
			m_Buffer, &m_BufferLineSize, m_DstChannelCount,
			m_BufferMaxNumSamples, m_BufferSampleFormat, 1
		);
		if (err < 0)
		{
			qWarning() << "Failed to realloc sample buffer: " << err;
			return std::make_pair(nullptr, 0);
		}
	}
	auto numOutputSamples = swr_convert(m_Context, m_Buffer, m_BufferMaxNumSamples, a_Buffers, a_Len);
	if (numOutputSamples < 0)
	{
		qWarning() << "Sample conversion failed: " << numOutputSamples;
		return std::make_pair(nullptr, 0);
	}

	// Output through to AudioOutput:
	return std::make_pair(m_Buffer[0], static_cast<size_t>(numOutputSamples));
}





////////////////////////////////////////////////////////////////////////////////
// Format:

FormatPtr Format::createContext(const QString & a_FileName)
{
	Initializer::init();

	// Create an IO wrapper:
	auto io = FileIO::createContext(a_FileName);
	if (io == nullptr)
	{
		qWarning() << "IO creation failed (" << a_FileName << ")";
		return nullptr;
	}

	// Create the context:
	auto res = std::unique_ptr<Format>(new Format(io));
	if ((res == nullptr) || (res->m_Context == nullptr))
	{
		qWarning() << "Creation failed (" << a_FileName << ")";
		return nullptr;
	}

	// Open and detect format:
	auto ret = avformat_open_input(&res->m_Context, nullptr, nullptr, nullptr);
	if (ret != 0)
	{
		// User-supplied AVFormatContext is freed upon failure, need to un-bind:
		res->m_Context = nullptr;
		qWarning() << "Opening failed: " << ret << " (" << a_FileName << ")";
		return nullptr;
	}

	// Find the stream info:
	ret = avformat_find_stream_info(res->m_Context, nullptr);
	if (ret < 0)
	{
		qWarning() << "Cannot find stream info: " << ret << " (" << a_FileName << ")";
	}

	return res;
}





Format::Format(std::shared_ptr<FileIO> a_IO):
	m_Context(avformat_alloc_context()),
	m_IO(a_IO),
	m_AudioOutput(nullptr),
	m_AudioStreamIdx(-1)
{
	if (m_Context != nullptr)
	{
		m_Context->pb = m_IO->m_Context;
	}
}





Format::~Format()
{
	if (m_Context != nullptr)
	{
		av_freep(m_Context->pb);
		avformat_close_input(&m_Context);
	}
}





bool Format::routeAudioTo(PlaybackBuffer * a_PlaybackBuffer)
{
	assert(m_AudioOutput == nullptr);  // Only one stream can be decoded into audio within a session

	AVCodec * audioDecoder = nullptr;
	m_AudioStreamIdx = av_find_best_stream(m_Context, AVMEDIA_TYPE_AUDIO, -1, -1, &audioDecoder, 0);
	if (m_AudioStreamIdx < 0)
	{
		qWarning() << "Failed to find an audio stream in the file: " << m_AudioStreamIdx;
		m_AudioStreamIdx = -1;
		return false;
	}

	// Create a decoding context:
	m_AudioDecoderContext = CodecContext::create(audioDecoder);
	if (m_AudioDecoderContext == nullptr)
	{
		qWarning() << "Failed to allocate decoder context.";
		return false;
	}
	avcodec_parameters_to_context(m_AudioDecoderContext->m_Context, m_Context->streams[m_AudioStreamIdx]->codecpar);
	av_opt_set_int(m_AudioDecoderContext->m_Context, "refcounted_frames", 1, 0);

	// Open the audio decoder:
	if (!m_AudioDecoderContext->open(audioDecoder))
	{
		qWarning() << "Cannot open decoder context.";
		m_AudioDecoderContext.reset();
		return false;
	}
	m_AudioOutput = a_PlaybackBuffer;
	a_PlaybackBuffer->setDuration(static_cast<double>(m_Context->duration) / 1000000);
	qDebug() << "Audio routing established, stream index is " << m_AudioStreamIdx;
	return true;
}





bool Format::feedRawAudioDataTo(
	std::function<void (const void * /* a_Data */, int /* a_Size */)> a_Function,
	double & a_LengthSec
)
{
	assert(m_AudioOutput == nullptr);  // Cannot work as both decoder and feeder

	// Find the stream to feed:
	AVCodec * audioDecoder = nullptr;
	m_AudioStreamIdx = av_find_best_stream(m_Context, AVMEDIA_TYPE_AUDIO, -1, -1, &audioDecoder, 0);
	if (m_AudioStreamIdx < 0)
	{
		qWarning() << "Failed to find an audio stream in the file: " << m_AudioStreamIdx;
		m_AudioStreamIdx = -1;
		return false;
	}

	// Feed the stream:
	AVPacket packet;
	qint64 maxPts = 0;
	while (true)
	{
		auto ret = av_read_frame(m_Context, &packet);
		if (ret < 0)
		{
			break;
		}

		if (packet.stream_index == m_AudioStreamIdx)
		{
			if (packet.size >= 0)
			{
				a_Function(packet.data, packet.size);
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
		auto stream = m_Context->streams[m_AudioStreamIdx];
		a_LengthSec = static_cast<double>(maxPts) * stream->time_base.num / stream->time_base.den;
	}
	qDebug() << "Feeding done.";
	return true;
}





void Format::decode()
{
	assert(m_AudioOutput != nullptr);
	assert(m_AudioDecoderContext != nullptr);

	AVPacket packet;
	AVFrame * frame = av_frame_alloc();
	m_ShouldTerminate = false;
	while (!m_ShouldTerminate)
	{
		auto ret = av_read_frame(m_Context, &packet);
		if (ret < 0)
		{
			// Normal condition on EOF
			// qWarning() << "Failed to read frame: " << ret;
			return;
		}

		if (packet.stream_index == m_AudioStreamIdx)
		{
			ret = avcodec_send_packet(m_AudioDecoderContext->m_Context, &packet);
			if (ret < 0)
			{
				qWarning() << "Error while sending a packet to the decoder: " << ret;
				return;
			}

			while (ret >= 0)
			{
				ret = avcodec_receive_frame(m_AudioDecoderContext->m_Context, frame);
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





void Format::outputAudioData(AVFrame * a_Frame)
{
	if (m_Resampler == nullptr)
	{
		qDebug() << "Creating audio resampler...";
		m_Resampler.reset(Resampler::create(
			a_Frame->channel_layout,
			a_Frame->sample_rate,
			static_cast<AVSampleFormat>(a_Frame->format),
			m_AudioOutput->format()
		));
		if (m_Resampler == nullptr)
		{
			qWarning() << "Cannot create audio resampler.";
			m_ShouldTerminate = true;
			return;
		}
	}
	auto out = m_Resampler->convert(const_cast<const uint8_t **>(a_Frame->data), a_Frame->nb_samples);
	auto numBytes = out.second * static_cast<size_t>(m_AudioOutput->format().bytesPerFrame());
	m_ShouldTerminate = !m_AudioOutput->writeDecodedAudio(out.first, numBytes);
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
					m_Extensions.push_back(e.toLower());
				}
			}
		}
	}

	std::vector<QString> m_Extensions;

public:

	static InputFormats & get()
	{
		static InputFormats instance;
		return instance;
	}

	/** Returns true iff the extension is supported by the input formats. */
	bool isExtensionSupported(const QString & a_Extension)
	{
		auto lcExt = a_Extension.toLower();
		for (const auto & ext: m_Extensions)
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

bool isExtensionSupported(const QString & a_Extension)
{
	Q_UNUSED(a_Extension);
	return InputFormats::get().isExtensionSupported(a_Extension);
}





}  // namespace AVPP

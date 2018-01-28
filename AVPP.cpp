#include "AVPP.h"
#include <assert.h>
#include <QDebug>

extern "C"
{
	#include <libavutil/opt.h>
}

#include "PlaybackBuffer.h"





namespace AVPP
{





////////////////////////////////////////////////////////////////////////////////
// FileIO:

std::shared_ptr<FileIO> FileIO::createContext(const QString & a_FileName)
{
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
	return This->m_File->read(reinterpret_cast<char *>(a_Dst), a_Size);
}





int64_t FileIO::seek(void * a_This, int64_t a_Offset, int a_Whence)
{
	qDebug() << __FUNCTION__ << ": Seek to offset " << a_Offset << " from " << a_Whence;
	auto This = reinterpret_cast<FileIO *>(a_This);
	switch (a_Whence)
	{
		case SEEK_CUR:
		{
			if (!This->m_File->seek(This->m_File->pos() + a_Offset))
			{
				qWarning() << __FUNCTION__ << ": Relative-Seek failed.";
				return -1;
			}
			return This->m_File->pos();
		}
		case SEEK_SET:
		{
			if (!This->m_File->seek(a_Offset))
			{
				qWarning() << __FUNCTION__ << ": Absolute-Seek failed.";
				return -1;
			}
			return a_Offset;
		}
		case SEEK_END:
		{
			if (!This->m_File->seek(This->m_File->size() - a_Offset))
			{
				qWarning() << __FUNCTION__ << ": End-Seek failed.";
				return -1;
			}
			return This->m_File->pos();
		}
	}
	qWarning() << __FUNCTION__ << ": Unexpected seek operation: " << a_Whence << ", offset " << a_Offset;
	return -1;
}





////////////////////////////////////////////////////////////////////////////////
// CodecContext:

std::shared_ptr<CodecContext> CodecContext::create(AVCodec * a_Codec)
{
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
		qWarning() << __FUNCTION__ << ": Cannot open codec: " << ret;
		return false;
	}
	qDebug() << __FUNCTION__ << ": Codec opened.";
	return true;
}





CodecContext::CodecContext(AVCodec * a_Codec):
	m_Context(avcodec_alloc_context3(a_Codec))
{
}





////////////////////////////////////////////////////////////////////////////////
// Resampler:

Resampler * Resampler::create(
	int a_SrcChannelMap,
	int a_SrcSampleRate,
	AVSampleFormat a_SrcSampleFormat,
	PlaybackBuffer * a_Output
)
{
	auto res = std::unique_ptr<Resampler>(new Resampler);
	if (!res->init(a_SrcChannelMap, a_SrcSampleRate, a_SrcSampleFormat, a_Output))
	{
		return nullptr;
	}
	return res.release();
}





Resampler::Resampler():
	m_Output(nullptr),
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
		free(m_Buffer);
	}
}





bool Resampler::init(int a_SrcChannelLayout, int a_SrcSampleRate, AVSampleFormat a_SrcSampleFormat, PlaybackBuffer * a_Output)
{
	// Translate QAudioFormat into LibAV/SWR format:
	int dstChannelLayout;
	switch (a_Output->format().channelCount())
	{
		case 1: dstChannelLayout = AV_CH_LAYOUT_MONO;    break;
		case 2: dstChannelLayout = AV_CH_LAYOUT_STEREO;  break;
		case 4: dstChannelLayout = AV_CH_LAYOUT_3POINT1; break;
		case 5: dstChannelLayout = AV_CH_LAYOUT_5POINT0; break;
		case 6: dstChannelLayout = AV_CH_LAYOUT_5POINT1; break;
		default:
		{
			qWarning() << __FUNCTION__ << ": Cannot create SWR context, unhandled output format's ChannelCount:"
				<< a_Output->format().channelCount();
			return false;
		}
	}
	auto dstNumChannels = av_get_channel_layout_nb_channels(dstChannelLayout);
	assert(dstNumChannels = a_Output->format().channelCount());

	switch (a_Output->format().sampleType())
	{
		case QAudioFormat::SignedInt: m_BufferSampleFormat = AV_SAMPLE_FMT_S16; break;
		case QAudioFormat::Float:     m_BufferSampleFormat = AV_SAMPLE_FMT_FLT; break;
		case QAudioFormat::UnSignedInt:  // No SWR equivalent, raise error:
		default:
		{
			qWarning() << __FUNCTION__ << ": Cannot create SWR context, unhandled sample format: "
				<< a_Output->format().sampleType();
			m_BufferSampleFormat = AV_SAMPLE_FMT_S16P;
			return false;
		}
	}

	// Create and init the context:
	m_Context = swr_alloc_set_opts(
		nullptr,
		dstChannelLayout,
		m_BufferSampleFormat,
		a_Output->format().sampleRate(),
		a_SrcChannelLayout,
		a_SrcSampleFormat,
		a_SrcSampleRate,
		0, nullptr  // logging - unused
	);
	if (m_Context == nullptr)
	{
		qWarning() << __FUNCTION__ << ": Failed to create SWResampler context.";
		return false;
	}
	auto err = swr_init(m_Context);
	if (err != 0)
	{
		qWarning() << __FUNCTION__ << ": Failed to initialize SWResampler context: " << err;
		return false;
	}

	// Create the output buffer structure:
	err = av_samples_alloc_array_and_samples(
		&m_Buffer, &m_BufferLineSize, dstNumChannels,
		1000, m_BufferSampleFormat, 0
	);
	if (err < 0)
	{
		qWarning() << __FUNCTION__ << ": Could not allocate conversion buffer structure: " << err;
		return false;
	}

	m_Output = a_Output;
	return true;
}





bool Resampler::push(const uint8_t ** a_Buffers, int a_Len)
{
	assert(m_Context != nullptr);
	assert(m_Output != nullptr);

	int outLen = swr_get_out_samples(m_Context, a_Len);
	if (outLen < 0)
	{
		qWarning() << __FUNCTION__ << ": Cannot query conversion buffer size: " << outLen;
		outLen = 64 * 1024;  // Take a wild guess
	}
	if (outLen > m_BufferMaxNumSamples)
	{
		qDebug() << __FUNCTION__ << ": Reallocating output sample buffer.";
		m_BufferMaxNumSamples = outLen;
		av_freep(&m_Buffer[0]);
		auto err = av_samples_alloc(
			m_Buffer, &m_BufferLineSize, m_Output->format().channelCount(),
			m_BufferMaxNumSamples, m_BufferSampleFormat, 1
		);
		if (err < 0)
		{
			qWarning() << __FUNCTION__ << ": Failed to realloc sample buffer: " << err;
			return true;  // Abort
		}
	}
	auto numOutputSamples = swr_convert(m_Context, m_Buffer, m_BufferMaxNumSamples, a_Buffers, a_Len);
	if (numOutputSamples < 0)
	{
		qWarning() << __FUNCTION__ << ": Sample conversion failed: " << numOutputSamples;
		return true;  // Abort
	}

	// Output through to AudioOutput:
	auto bytesPerSample = m_Output->format().channelCount() * (m_Output->format().sampleSize() / 8);
	return m_Output->writeDecodedAudio(m_Buffer[0], numOutputSamples * bytesPerSample);
}





////////////////////////////////////////////////////////////////////////////////
// Format:

Format * Format::createContext(const QString & a_FileName)
{
	// Create an IO wrapper:
	auto io = FileIO::createContext(a_FileName);
	if (io == nullptr)
	{
		qWarning() << __FUNCTION__ << ": IO creation failed.";
		return nullptr;
	}

	// Create the context:
	auto res = std::unique_ptr<Format>(new Format(io));
	if ((res == nullptr) || (res->m_Context == nullptr))
	{
		qWarning() << __FUNCTION__ << ": Creation failed";
		return nullptr;
	}

	// Open and detect format:
	auto ret = avformat_open_input(&res->m_Context, nullptr, nullptr, nullptr);
	if (ret != 0)
	{
		// User-supplied AVFormatContext is freed upon failure, need to un-bind:
		res->m_Context = nullptr;
		qWarning() << __FUNCTION__ << ": Opening failed: " << ret;
		return nullptr;
	}

	// Find the stream info:
	ret = avformat_find_stream_info(res->m_Context, nullptr);
	if (ret < 0)
	{
		qWarning() << __FUNCTION__ << ": Cannot find stream info: " << ret;
	}

	qDebug() << __FUNCTION__ << ": Format context initialized.";
	return res.release();
}





Format::Format(std::shared_ptr<FileIO> a_IO):
	m_Context(avformat_alloc_context()),
	m_IO(a_IO),
	m_AudioOutput(nullptr),
	m_AudioStreamIdx(-1),
	m_Buffer(nullptr),
	m_BufferSize(0)
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
	if (m_Buffer != nullptr)
	{
		free(m_Buffer);
	}
}





bool Format::routeAudioTo(PlaybackBuffer * a_PlaybackBuffer)
{
	assert(m_AudioOutput == nullptr);  // Only one stream can be decoded into audio within a session

	AVCodec * audioDecoder = nullptr;
	m_AudioStreamIdx = av_find_best_stream(m_Context, AVMEDIA_TYPE_AUDIO, -1, -1, &audioDecoder, 0);
	if (m_AudioStreamIdx < 0)
	{
		qWarning() << __FUNCTION__ << ": Failed to find an audio stream in the file: " << m_AudioStreamIdx;
		m_AudioStreamIdx = -1;
		return false;
	}

	// Create a decoding context:
	m_AudioDecoderContext = CodecContext::create(audioDecoder);
	if (m_AudioDecoderContext == nullptr)
	{
		qWarning() << __FUNCTION__ << ": Failed to allocate decoder context.";
		return false;
	}
	avcodec_parameters_to_context(m_AudioDecoderContext->m_Context, m_Context->streams[m_AudioStreamIdx]->codecpar);
	av_opt_set_int(m_AudioDecoderContext->m_Context, "refcounted_frames", 1, 0);

	// Open the audio decoder:
	if (!m_AudioDecoderContext->open(audioDecoder))
	{
		qWarning() << __FUNCTION__ << ": Cannot open decoder context.";
		m_AudioDecoderContext.reset();
		return false;
	}
	m_AudioOutput = a_PlaybackBuffer;
	qDebug() << __FUNCTION__ << ": Audio routing established, stream index is " << m_AudioStreamIdx;
	return true;
}





void Format::decode()
{
	assert(m_AudioOutput != nullptr);
	assert(m_AudioDecoderContext != nullptr);

	qDebug() << __FUNCTION__ << ": Decoding...";

	AVPacket packet;
	AVFrame * frame = av_frame_alloc();
	m_ShouldTerminate = false;
	while (!m_ShouldTerminate)
	{
		auto ret = av_read_frame(m_Context, &packet);
		if (ret < 0)
		{
			qWarning() << __FUNCTION__ << ": Failed to read frame: " << ret;
			return;
		}

		if (packet.stream_index == m_AudioStreamIdx)
		{
			ret = avcodec_send_packet(m_AudioDecoderContext->m_Context, &packet);
			if (ret < 0)
			{
				qWarning() << __FUNCTION__ << ": Error while sending a packet to the decoder: " << ret;
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
					qWarning() << __FUNCTION__ << ": Error while receiving a frame from the decoder: " << ret;
					return;
				}
				outputAudioData(frame);
			}
		}
		av_packet_unref(&packet);
	}
	qDebug() << __FUNCTION__ << ": Decoding done.";
}





void Format::outputAudioData(AVFrame * a_Frame)
{
	if (m_Resampler == nullptr)
	{
		qDebug() << __FUNCTION__ << ": Creating audio resampler...";
		m_Resampler.reset(Resampler::create(
			a_Frame->channel_layout,
			a_Frame->sample_rate,
			static_cast<AVSampleFormat>(a_Frame->format),
			m_AudioOutput
		));
		if (m_Resampler == nullptr)
		{
			qWarning() << __FUNCTION__ << ": Cannot create audio resampler.";
			m_ShouldTerminate = true;
			return;
		}
	}
	m_ShouldTerminate = !m_Resampler->push(const_cast<const uint8_t **>(a_Frame->data), a_Frame->nb_samples);
}





}  // namespace AVPP

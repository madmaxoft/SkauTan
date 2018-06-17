#ifndef AVPP_H
#define AVPP_H





#include <atomic>
#include <memory>
#include <functional>
#include <QFile>
#include <QAudioFormat>
extern "C"
{
	#include <libavformat/avformat.h>
	#include <libavcodec/avcodec.h>
	#include <libswresample/swresample.h>
}





// fwd:
class PlaybackBuffer;





/** Namespace for C++ wrappers over AV library. */
namespace AVPP
{
	/** Represents an AVIOContext tied to a specific file. */
	class FileIO
	{
	public:

		/** Creates a new AVIOContext tied to the specified file.
		Returns nullptr if the file cannot be opened or upon any error. */
		static std::shared_ptr<FileIO> createContext(const QString & a_FileName);

		/** Destroyes the instance, freeing up what needs to be freed. */
		virtual ~FileIO();


	protected:
		friend class Format;


		/** The context itself. */
		AVIOContext * m_Context;

		/** The file to which the IO is bound. */
		std::unique_ptr<QFile> m_File;


		/** Creates a new empty instance. */
		FileIO();

		/** Opens the specified file.
		Returns true on success, false on failure. */
		bool Open(const QString & a_FileName);

		/** The IO reading function (AVIO signature). */
		static int read(void * a_This, uint8_t * a_Dst, int a_Size);

		/** The IO seeking function (AVIO signature). */
		static int64_t seek(void * a_This, int64_t a_Offset, int a_Whence);
	};  // class FileIO




	class CodecContext
	{
	public:
		/** Creates a new Context for the specified format's codec information (as extracted from one of the streams). */
		static std::shared_ptr<CodecContext> create(AVCodec * a_Codec);

		~CodecContext();

		/** Opens the codec instance, based on the given parameters.
		Returns true on success, false on failure. */
		bool open(AVCodec * a_Codec);

	protected:

		friend class Format;


		/** The context itself. */
		AVCodecContext * m_Context;


		/** Creates a new instance. */
		CodecContext(AVCodec * a_Codec);
	};  // class CodecContext





	/** Wraps the SWR library into a C++ object. */
	class Resampler
	{
	public:

		/** Returns the LibAV's AVSampleFormat representing the specified QAudioFormat's sample format.
		Throws a std::runtime_error if the conversion fails. */
		static AVSampleFormat sampleFormatFromSampleType(QAudioFormat::SampleType a_SampleType);

		/** Returns the best guess of LibAV's channel layout representing the specified QAudioFormat's channel count.
		Throws a std::runtime_error if the conversion fails. */
		static uint64_t channelLayoutFromChannelCount(int a_ChannelCount);

		/** Creates a new Resampler instance that converts the Src format into a_OutputFormat.
		Returns nullptr on error.
		The input format is specified in LIBAV* terms. */
		static Resampler * create(
			uint64_t a_SrcChannelLayout,
			int a_SrcSampleRate,
			AVSampleFormat a_SrcSampleFormat,
			const QAudioFormat & a_OutputFormat
		);

		/** Creates a new Resampler instance that only changes the sample rate.
		Returns nullptr on error. */
		static Resampler * create(
			int a_SrcSampleRate,
			int a_DstSampleRate,
			uint64_t a_ChannelLayout,
			AVSampleFormat a_SampleFormat
		);

		/** Converts the specified input data into output format.
		a_Buffers is a LibAV structure for the input data stream(s)
		a_Len is the input data length, in frames
		Returns the pointer to converted data (in m_Buffer) and its size, in frames.
		The returned pointer is valid until the next call to convert().
		Returns <nullptr, 0> if the conversion fails. */
		std::pair<uint8_t *, size_t> convert(const uint8_t ** a_Buffers, int a_Len);

		~Resampler();

	protected:

		/** The SWR context. */
		SwrContext * m_Context;

		/** Buffer structure for the outgoing data. */
		uint8_t ** m_Buffer;

		/** Current allocated size of m_Buffer, in samples. */
		int m_BufferMaxNumSamples;

		int m_BufferLineSize;
		AVSampleFormat m_BufferSampleFormat;
		int m_DstChannelCount;


		/** Creates a new instance of the resampler.
		The resampler is not prepared, need to call init() to set the formats and initialize. */
		Resampler();

		/** Initializes the resampler.
		The input format is specified in LIBAV* terms, the output format is deduced from a_Output.
		Returns true on success, false on error. */
		bool init(
			uint64_t a_SrcChannelLayout,
			int a_SrcSampleRate,
			AVSampleFormat a_SrcSampleFormat,
			const QAudioFormat & a_OutputFormat
		);

		/** Initializes the resampler.
		Both formats are specified in LIBAV* terms.
		Returns true on success, false on error. */
		bool init(
			uint64_t a_SrcChannelLayout,
			int a_SrcSampleRate,
			AVSampleFormat a_SrcSampleFormat,
			uint64_t a_DstChannelLayout,
			int a_DstSampleRate,
			AVSampleFormat a_DstSampleFormat
		);
	};




	// fwd:
	class Format;
	using FormatPtr = std::unique_ptr<Format>;





	class Format
	{
	public:
		/** Creates a new AVFormatContext instance tied to the specified input file.
		Returns nullptr on error. */
		static FormatPtr createContext(const QString & a_FileName);

		~Format();

		/** Finds the best audio stream and allocates the decoder for it internally.
		When the data is processed, the decoder will output the audio into the specified PlaybackBuffer.
		Only one audio stream can be decoded from the input data.
		Returns true if an audio decoder was successfully initialized for the decoding. */
		bool routeAudioTo(PlaybackBuffer * a_PlaybackBuffer);

		/** Finds the best audio stream and passes all raw (compressed) audio data from that stream
		to the specified callback function.
		a_LengthSec gets filled with the audio data length in seconds, if available.
		Returns true if feeding was successful, false on error. */
		bool feedRawAudioDataTo(
			std::function<void (const void * /*a_Data */, int /* a_Size */)> a_Function,
			double & a_LengthSec
		);

		/** Reads the input and decodes any data found in it, according to routing set with routeAudioTo().
		Blocks until the entire input is decoded (or seeked out of, using seekTo() from another thread). */
		void decode();

	protected:

		/** The context itself. */
		AVFormatContext * m_Context;

		/** The IO object used for accessing files. */
		std::shared_ptr<FileIO> m_IO;

		/** Where the audio data should be output after decoding and resampling. */
		PlaybackBuffer * m_AudioOutput;

		/** Index of the audio stream processed into m_AudioOutput.
		-1 if not valid yet. */
		int m_AudioStreamIdx;

		/** The decoding context for the audio stream (set by routeAudioTo()). */
		std::shared_ptr<CodecContext> m_AudioDecoderContext;

		/** When set to true, the decoding loop will terminate. */
		bool m_ShouldTerminate;

		/** The resampler used to conver audio data from LibAV output to m_AudioOutput. */
		std::unique_ptr<Resampler> m_Resampler;


		/** Creates a new instance and binds it to the specified IO. */
		Format(std::shared_ptr<FileIO> a_IO);

		/** Outputs the specified audio data frame into m_AudioOutput. */
		void outputAudioData(AVFrame * a_Frame);
	};  // class Format





	bool isExtensionSupported(const QString & a_Extension);
}  // namespace AVPP






#endif // AVPP_H

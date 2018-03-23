#ifndef AVPP_H
#define AVPP_H





#include <memory>
#include <QFile>
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
		/** Creates a new Resampler instance that converts the Src format into a_Output's format.
		Returns nullptr on error.
		The input format is specified in LIBAV* terms, the output format is deduced from a_Output. */
		static Resampler * create(
			int a_SrcChannelMap,
			int a_SrcSampleRate,
			AVSampleFormat a_SrcSampleFormat,
			PlaybackBuffer * a_Output
		);

		/** Pushes the specified input data through the resampler and into the output.
		Returns false if the audio output indicated termination, true on success and continuation. */
		bool push(const uint8_t ** a_Buffers, int a_Len);

		~Resampler();

	protected:

		/** The buffer into which the output data is sent after resampling. */
		PlaybackBuffer * m_Output;

		/** The SWR context. */
		SwrContext * m_Context;

		/** Buffer structure for the outgoing data. */
		uint8_t ** m_Buffer;

		/** Current allocated size of m_Buffer, in samples. */
		int m_BufferMaxNumSamples;

		int m_BufferLineSize;
		AVSampleFormat m_BufferSampleFormat;


		/** Creates a new instance of the resampler.
		The resampler is not prepared, need to call init() to set the formats and initialize. */
		Resampler();

		/** Initializes the resampler.
		The input format is specified in LIBAV* terms, the output format is deduced from a_Output.
		Returns true on success, false on error. */
		bool init(
			int a_SrcChannelLayout,
			int a_SrcSampleRate,
			AVSampleFormat a_SrcSampleFormat,
			PlaybackBuffer * a_Output
		);
	};





	class Format
	{
	public:
		/** Creates a new AVFormatContext instance tied to the specified input file.
		Returns nullptr on error. */
		static Format * createContext(const QString & a_FileName);

		~Format();

		/** Finds the best audio stream and allocates the decoder for it internally.
		When the data is processed, the decoder will output the audio into the specified PlaybackBuffer.
		Only one audio stream can be decoded from the input data.
		Returns true if an audio decoder was successfully initialized for the decoding. */
		bool routeAudioTo(PlaybackBuffer * a_PlaybackBuffer);

		/** Reads the input and decodes any data found in it, according to routing set with routeAudioTo(). */
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

		/** The buffer used for transforming data sample formats. */
		int16_t * m_Buffer;

		/** The allocated size of m_Buffer, in bytes. */
		size_t m_BufferSize;

		/** The resampler used to conver audio data from LibAV output to m_AudioOutput. */
		std::unique_ptr<Resampler> m_Resampler;


		/** Creates a new instance and binds it to the specified IO. */
		Format(std::shared_ptr<FileIO> a_IO);

		/** Outputs the specified audio data frame into m_AudioOutput. */
		void outputAudioData(AVFrame * a_Frame);
	};  // class Format



	bool isExtensionSupported(const QString & a_Extension);
}

#endif // AVPP_H

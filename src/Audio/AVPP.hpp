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
		static std::shared_ptr<FileIO> createContext(const QString & aFileName);

		/** Destroyes the instance, freeing up what needs to be freed. */
		virtual ~FileIO();


	protected:
		friend class Format;


		/** The context itself. */
		AVIOContext * mContext;

		/** The file to which the IO is bound. */
		std::unique_ptr<QFile> mFile;


		/** Creates a new empty instance. */
		FileIO();

		/** Opens the specified file.
		Returns true on success, false on failure. */
		bool Open(const QString & aFileName);

		/** The IO reading function (AVIO signature). */
		static int read(void * aThis, uint8_t * aDst, int aSize);

		/** The IO seeking function (AVIO signature). */
		static int64_t seek(void * aThis, int64_t aOffset, int aWhence);
	};  // class FileIO




	class CodecContext
	{
	public:
		/** Creates a new Context for the specified format's codec information (as extracted from one of the streams). */
		static std::shared_ptr<CodecContext> create(AVCodec * aCodec);

		~CodecContext();

		/** Opens the codec instance, based on the given parameters.
		Returns true on success, false on failure. */
		bool open(AVCodec * aCodec);

	protected:

		friend class Format;


		/** The context itself. */
		AVCodecContext * mContext;


		/** Creates a new instance. */
		CodecContext(AVCodec * aCodec);
	};  // class CodecContext





	/** Wraps the SWR library into a C++ object. */
	class Resampler
	{
	public:

		/** Returns the LibAV's AVSampleFormat representing the specified QAudioFormat's sample format.
		Throws a RuntimeError if the conversion fails. */
		static AVSampleFormat sampleFormatFromSampleType(QAudioFormat::SampleType aSampleType);

		/** Returns the best guess of LibAV's channel layout representing the specified QAudioFormat's channel count.
		Throws a RuntimeError if the conversion fails. */
		static uint64_t channelLayoutFromChannelCount(int aChannelCount);

		/** Creates a new Resampler instance that converts the Src format into aOutputFormat.
		Returns nullptr on error.
		The input format is specified in LIBAV* terms. */
		static Resampler * create(
			uint64_t aSrcChannelLayout,
			int aSrcSampleRate,
			AVSampleFormat aSrcSampleFormat,
			const QAudioFormat & aOutputFormat
		);

		/** Creates a new Resampler instance that only changes the sample rate.
		Returns nullptr on error. */
		static Resampler * create(
			int aSrcSampleRate,
			int aDstSampleRate,
			uint64_t aChannelLayout,
			AVSampleFormat aSampleFormat
		);

		/** Converts the specified input data into output format.
		aBuffers is a LibAV structure for the input data stream(s)
		aLen is the input data length, in frames
		Returns the pointer to converted data (in mBuffer) and its size, in frames.
		The returned pointer is valid until the next call to convert().
		Returns <nullptr, 0> if the conversion fails. */
		std::pair<uint8_t *, size_t> convert(const uint8_t ** aBuffers, int aLen);

		~Resampler();

	protected:

		/** The SWR context. */
		SwrContext * mContext;

		/** Buffer structure for the outgoing data. */
		uint8_t ** mBuffer;

		/** Current allocated size of mBuffer, in samples. */
		int mBufferMaxNumSamples;

		int mBufferLineSize;
		AVSampleFormat mBufferSampleFormat;
		int mDstChannelCount;


		/** Creates a new instance of the resampler.
		The resampler is not prepared, need to call init() to set the formats and initialize. */
		Resampler();

		/** Initializes the resampler.
		The input format is specified in LIBAV* terms, the output format is deduced from aOutput.
		Returns true on success, false on error. */
		bool init(
			uint64_t aSrcChannelLayout,
			int aSrcSampleRate,
			AVSampleFormat aSrcSampleFormat,
			const QAudioFormat & aOutputFormat
		);

		/** Initializes the resampler.
		Both formats are specified in LIBAV* terms.
		Returns true on success, false on error. */
		bool init(
			uint64_t aSrcChannelLayout,
			int aSrcSampleRate,
			AVSampleFormat aSrcSampleFormat,
			uint64_t aDstChannelLayout,
			int aDstSampleRate,
			AVSampleFormat aDstSampleFormat
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
		static FormatPtr createContext(const QString & aFileName);

		~Format();

		/** Finds the best audio stream and allocates the decoder for it internally.
		When the data is processed, the decoder will output the audio into the specified PlaybackBuffer.
		Only one audio stream can be decoded from the input data.
		Returns true if an audio decoder was successfully initialized for the decoding. */
		bool routeAudioTo(PlaybackBuffer * aPlaybackBuffer);

		/** Finds the best audio stream and passes all raw (compressed) audio data from that stream
		to the specified callback function.
		aLengthSec gets filled with the audio data length in seconds, if available.
		Returns true if feeding was successful, false on error. */
		bool feedRawAudioDataTo(
			std::function<void (const void * /*aData */, int /* aSize */)> aFunction,
			double & aLengthSec
		);

		/** Reads the input and decodes any data found in it, according to routing set with routeAudioTo().
		Blocks until the entire input is decoded (or seeked out of, using seekTo() from another thread). */
		void decode();

		/** Decodes the entire audio data in the input file and returns it as a playable buffer.
		aFormat specifies the output format for the buffer. */
		std::shared_ptr<PlaybackBuffer> decodeEntireAudio(const QAudioFormat & aFormat);


	protected:

		/** The context itself. */
		AVFormatContext * mContext;

		/** The IO object used for accessing files. */
		std::shared_ptr<FileIO> mIO;

		/** Where the audio data should be output after decoding and resampling. */
		PlaybackBuffer * mAudioOutput;

		/** Index of the audio stream processed into mAudioOutput.
		-1 if not valid yet. */
		int mAudioStreamIdx;

		/** The decoding context for the audio stream (set by routeAudioTo()). */
		std::shared_ptr<CodecContext> mAudioDecoderContext;

		/** When set to true, the decoding loop will terminate. */
		bool mShouldTerminate;

		/** The resampler used to conver audio data from LibAV output to mAudioOutput. */
		std::unique_ptr<Resampler> mResampler;


		/** Creates a new instance and binds it to the specified IO. */
		Format(std::shared_ptr<FileIO> aIO);

		/** Outputs the specified audio data frame into mAudioOutput. */
		void outputAudioData(AVFrame * aFrame);
	};  // class Format





	bool isExtensionSupported(const QString & aExtension);
}  // namespace AVPP






#endif // AVPP_H

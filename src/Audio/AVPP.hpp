#pragma once

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
	/** Represents a generic IO interface passed to the AV library.
	Wraps the AVIOContext and maps the AV callbacks to virtual functions.
	Descendants are expected to implement the actual reading (from file, memory, ...) */
	class IO
	{
	public:
		/** When seeking, what is the offset relative to. */
		enum class Whence
		{
			fromStart,
			fromCurrent,
			fromEnd,
		};

		virtual ~IO();

		/** Reads the specified amount of data.
		Returns the number of bytes read, or <0 for error. */
		virtual int read(void * aDst, int aNumBytes) = 0;

		/** Seeks to the specified aOffset, based on aWhence.
		Returns the current position in the datastream after seeking, <0 on error. */
		virtual qint64 seek(qint64 aOffset, Whence aWhence) = 0;

	protected:
		friend class Format;

		/** The AV library's IO context wrapped in this class. */
		AVIOContext * mContext;


		/** Creates a new instance with an nullptr mContext. */
		IO();

		/** Allocates the mContext and sets the callbacks.
		Returns true on success, false on failure. */
		bool allocContext();

		/** The IO reading function (AVIO signature). */
		static int read(void * aThis, uint8_t * aDst, int aSize);

		/** The IO seeking function (AVIO signature). */
		static int64_t seek(void * aThis, int64_t aOffset, int aWhence);
	};  // class IO





	/** Represents an AVIOContext tied to a specific file. */
	class FileIO:
		public IO
	{
	public:

		/** Creates a new AVIOContext tied to the specified file.
		Returns nullptr if the file cannot be opened or upon any error. */
		static std::shared_ptr<FileIO> createContext(const QString & aFileName);

		// IO overrides:
		virtual int read(void * aDst, int aNumBytes) override;
		virtual qint64 seek(qint64 aOffset, Whence aWhence) override;

	protected:

		/** The file to which the IO is bound. */
		std::unique_ptr<QFile> mFile;


		/** Opens the specified file.
		Returns true on success, false on failure. */
		bool Open(const QString & aFileName);
	};  // class FileIO




	/** Represents an AVIOContext tied to in-memory data. */
	class MemoryIO:
		public IO
	{
	public:

		/** Creates a new AVIOContext tied to the specified file.
		Returns nullptr if the file cannot be opened or upon any error. */
		static std::shared_ptr<MemoryIO> createContext(const QByteArray & aData);

		// IO overrides:
		virtual int read(void * aDst, int aNumBytes) override;
		virtual qint64 seek(qint64 aOffset, Whence aWhence) override;

	protected:

		/** The data that this IO should work on. */
		const QByteArray & mData;

		/** The position within mData where the next read() operation will start. */
		int mCurrentPosition;


		/** Creates a new instance. */
		MemoryIO(const QByteArray & aData);
	};  // class MemoryIO





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

		/** Creates a new AVFormatContext instance tied to the specified AV data (contents of a file).
		Returns nullptr on error. */
		static FormatPtr createContextFromData(const QByteArray & aFileData);

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
		std::shared_ptr<IO> mIO;

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
		Format(std::shared_ptr<IO> aIO);

		/** Outputs the specified audio data frame into mAudioOutput. */
		void outputAudioData(AVFrame * aFrame);
	};  // class Format





	bool isExtensionSupported(const QString & aExtension);
}  // namespace AVPP

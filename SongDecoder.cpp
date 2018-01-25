#include "SongDecoder.h"
#include <assert.h>
#include <QAudioDecoder>
#include <QEventLoop>
#include <QDebug>
#include "Song.h"
#include "PlaybackBuffer.h"





SongDecoder::SongDecoder()
{
	m_DecoderThread.start();
	m_Decoder.moveToThread(&m_DecoderThread);
}





std::shared_ptr<PlaybackBuffer> SongDecoder::start(const QAudioFormat & a_Format, const Song & a_Song)
{
	qDebug() << "Starting song decoder for song " << a_Song.fileName();
	assert(m_Decoder.state() == QAudioDecoder::StoppedState);  // Only one "start" per instance
	QEventLoop el;
	el.connect(&m_Decoder, &QAudioDecoder::durationChanged,
		[&](qint64 a_Duration)
		{
			qDebug() << "Format: " << a_Format;
			qDebug() << "Duration: " << a_Duration << " usec";
			qDebug() << "Total bytes: " << a_Format.bytesForDuration(a_Duration);
			auto byteSize = a_Format.bytesForDuration(a_Duration);
			m_Output = std::make_shared<PlaybackBuffer>(a_Format, byteSize);
			m_Output->setDecoder(this);
			el.quit();
		}
	);
	m_Decoder.connect(&m_Decoder, &QAudioDecoder::bufferReady,
		[&]()
		{
			if (m_Output == nullptr)
			{
				qDebug() << "No output buffer, audio data lost.";
				return;
			}
			const auto & buf = m_Decoder.read();
			qDebug() << "Decoded an audio buffer, " << buf.byteCount() << " bytes (" << buf.byteCount() / 1024 << " KiB)";
			m_Output->write(reinterpret_cast<const char *>(buf.constData()), buf.byteCount());
		}
	);
	m_Decoder.setSourceFilename(a_Song.fileName());
	m_Decoder.setAudioFormat(a_Format);
	m_Decoder.start();
	el.exec();
	qDebug() << "Song decoder fully started";
	return m_Output;
}

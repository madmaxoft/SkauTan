#include "PlaybackBuffer.h"
#include <assert.h>
#include <QDebug>





PlaybackBuffer::PlaybackBuffer(const QAudioFormat & a_Format, int a_ByteSize):
	m_AudioFormat(a_Format),
	m_CurrentReadPos(0),
	m_FadeoutEnd(a_ByteSize),
	m_Decoder(nullptr)
{
	m_Audio.reserve(a_ByteSize);
	open(QIODevice::ReadOnly);
}





void PlaybackBuffer::write(const char * a_Data, int a_Size)
{
	m_Audio.append(a_Data, a_Size);

	emit dataReady();
	// TODO: Wake up reading, if needed
}





void PlaybackBuffer::fadeOut()
{
	m_FadeoutEnd = m_CurrentReadPos + m_AudioFormat.bytesForDuration(1000);
	// TODO: The actual fade-out
}





qint64 PlaybackBuffer::readData(char * a_Data, qint64 a_MaxLen)
{
	int end = std::min(m_Audio.size(), m_FadeoutEnd);
	int numBytesToRead = std::min(static_cast<int>(a_MaxLen), end - m_CurrentReadPos);
	if (numBytesToRead > 0)
	{
		memcpy(a_Data, m_Audio.constData() + m_CurrentReadPos, numBytesToRead);
		m_CurrentReadPos += numBytesToRead;
	}
	qDebug() << "PlaybackBuffer: requested " << a_MaxLen << " bytes, returning " << numBytesToRead << " bytes";
	return numBytesToRead;
}





qint64 PlaybackBuffer::writeData(const char * a_Data, qint64 a_Len)
{
	Q_UNUSED(a_Data);
	Q_UNUSED(a_Len);

	assert(!"Writing data is not supported");
	return -1;
}





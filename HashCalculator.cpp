#include "HashCalculator.h"
#include <QFile>
#include <QDebug>
#include <QCryptographicHash>
#include "Song.h"





HashCalculator::HashCalculator(Song & a_Song):
	m_Song(a_Song)
{

}





void HashCalculator::calc(Song & a_Song)
{
	HashCalculator hc(a_Song);
	return hc.calc();
}





void HashCalculator::calc()
{
	QFile srcFile(m_Song.fileName());
	if (!srcFile.open(QIODevice::ReadOnly))
	{
		// File not available, nothing to do:
		return;
	}
	auto fileSize = srcFile.size();
	qint64 curPos = 128 * 1024;
	srcFile.seek(curPos);  // Skip the first 128 KiB (possible ID3v2 tag)
	QCryptographicHash ch(QCryptographicHash::Sha1);
	auto wantedEnd = fileSize - 128;  // Skip the last 128 bytes (possible ID3v1 tag)
	while (curPos < wantedEnd)
	{
		char buffer[65536];
		auto numBytesToRead = std::min<qint64>(sizeof(buffer), wantedEnd - curPos);
		auto numBytesRead = srcFile.read(buffer, numBytesToRead);
		if (numBytesRead != numBytesToRead)
		{
			qDebug() << "Unexpected end of file found while hashing song " << m_Song.fileName();
			return;
		}
		ch.addData(buffer, numBytesRead);
		curPos += numBytesRead;
	}
	m_Song.setHash(ch.result());
}

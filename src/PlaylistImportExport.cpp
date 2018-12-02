#include "PlaylistImportExport.hpp"
#include <cassert>
#include <QFile>
#include "Playlist.hpp"
#include "PlaylistItemSong.hpp"
#include "DB/Database.hpp"





void PlaylistImportExport::doExport(const Playlist & a_Playlist, const QString & a_FileName)
{
	QFile f(a_FileName);
	if (!f.open(QFile::ReadWrite))
	{
		throw RuntimeError(tr("Cannot write to file"));
	}
	f.write("#EXTM3U\n");
	for (const auto & i: a_Playlist.items())
	{
		auto si = std::dynamic_pointer_cast<PlaylistItemSong>(i);
		if (si == nullptr)
		{
			continue;
		}
		auto filter = si->filter();
		if (filter != nullptr)
		{
			f.write(QString("#SKAUTAN:TMPL:%1:%2\n")
				.arg(QString::fromUtf8(filter->hash().toHex()))
				.arg(filter->displayName())
				.toUtf8()
			);
		}
		f.write(QString("#SKAUTAN:HASH:%1\n").arg(QString::fromUtf8(si->song()->hash().toHex())).toUtf8());
		f.write(si->song()->fileName().toUtf8());
		f.write("\n\n");
	}
}





int PlaylistImportExport::doImport(
	Playlist & a_Playlist,
	Database & a_DB,
	int a_AfterPos,
	const QString & a_FileName
)
{
	QFile f(a_FileName);
	if (!f.open(QFile::ReadOnly | QFile::Text))
	{
		throw RuntimeError(tr("Cannot read from file"));
	}
	qint64 numBytes;
	char line[10000];
	QByteArray songHash, filterHash;
	Playlist tmp;
	while ((numBytes = f.readLine(line, sizeof(line))) > 0)
	{
		if (numBytes == 1)
		{
			// Empty line, ignore
			continue;
		}
		if (line[0] == '#')
		{
			// A flag, store it for later
			if (memcmp(line + 1, "SKAUTAN:", 8) != 0)
			{
				// Not a SkauTan flag, ignore
				continue;
			}
			if (memcmp(line + 9, "TMPL:", 5) == 0)
			{
				int len = std::min(40, static_cast<int>(numBytes - 15));
				filterHash = QByteArray::fromHex(QByteArray(line + 14, len));
			}
			else if (memcmp(line + 9, "HASH:", 5) == 0)
			{
				int len = std::min(40, static_cast<int>(numBytes - 15));
				songHash = QByteArray::fromHex(QByteArray(line + 14, len));
			}
			else
			{
				assert(!"Unknown SkauTan flag");
			}
		}
		else
		{
			// A file entry, add to playlist with all the flags gathered so far:
			auto song = a_DB.songFromHash(songHash);
			if (song == nullptr)
			{
				auto songFileName = QString::fromUtf8(line, static_cast<int>(numBytes));
				song = a_DB.songFromFileName(songFileName);
				if (song == nullptr)
				{
					qDebug() << "Song not found: hash " << songHash << ", filename " << songFileName;
					continue;
				}
			}
			auto filter = a_DB.filterFromHash(filterHash);
			tmp.addItem(std::make_shared<PlaylistItemSong>(song, filter));
			filterHash.clear();
			songHash.clear();
		}
	}
	auto err = f.error();
	if ((numBytes < 0) && (err != QFile::NoError))
	{
		// EOF causes numBytes == -1 and err == QFile::NoError, so we don't want to report an error in such a case
		throw RuntimeError(tr("Reading from file failed: %1, %2, %3")
			.arg(numBytes)
			.arg(err)
			.arg(f.errorString()));
	}

	// Insert the items into the playlist:
	a_Playlist.insertItems(a_AfterPos + 1, tmp.items());
	return static_cast<int>(tmp.items().size());
}

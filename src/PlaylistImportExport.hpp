#pragma once

#include <QObject>





// fwd:
class Database;
class Playlist;





/** Imports and exports playlists to / from M3U files.
The playlist tries to preserve SkauTan-specific information, such as the filter that was used to choose the song. */
class PlaylistImportExport:
	public QObject
{
	Q_OBJECT


public:

	/** Exports the entire playlist to the specified file.
	Throws an Exception on failure. */
	static void doExport(const Playlist & aPlaylist, const QString & aFileName);

	/** Imports the specified file to the specified playlist, after the specified position.
	Songs are picked from aDB; if a song is not found, it is silently dropped.
	Returns the number of songs inserted into the playlist.
	Throws an Exception on failure. */
	static int doImport(
		Playlist & aPlaylist,
		Database & aDB,
		int aAfterPos,
		const QString & aFileName
	);
};

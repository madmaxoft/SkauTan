#ifndef PLAYLISTIMPORTEXPORT_HPP
#define PLAYLISTIMPORTEXPORT_HPP





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
	static void doExport(const Playlist & a_Playlist, const QString & a_FileName);

	/** Imports the specified file to the specified playlist, after the specified position.
	Songs are picked from a_DB; if a song is not found, it is silently dropped.
	Returns the number of songs inserted into the playlist.
	Throws an Exception on failure. */
	static int doImport(
		Playlist & a_Playlist,
		Database & a_DB,
		int a_AfterPos,
		const QString & a_FileName
	);
};





#endif // PLAYLISTIMPORTEXPORT_HPP

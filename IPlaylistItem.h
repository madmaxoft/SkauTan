#ifndef IPLAYLISTITEM_H
#define IPLAYLISTITEM_H




#include <memory>
#include <QString>





// fwd:
class PlaybackBuffer;
class QAudioFormat;





/** Interface that is common for all items that can be queued and played in a playlist. */
class IPlaylistItem
{
public:
	virtual ~IPlaylistItem() {}

	// Playlist-related functions:

	/** Returns the display name of the item. */
	virtual QString displayName() const = 0;

	/** Returns the author to display. */
	virtual QString displayAuthor() const = 0;

	/** Returns the title to display. */
	virtual QString displayTitle() const = 0;

	/** Returns the display length, in seconds.
	The returned length is relevant for the current tempo adjustment. */
	virtual double displayLength() const = 0;

	/** Returns the genre to display. */
	virtual QString displayGenre() const = 0;

	/** Returns the display tempo of the item, <0 if not available. */
	virtual double displayTempo() const = 0;

	// Playback-related functions:

	/** Starts decoding the item into the specified audio format.
	Returns a PlaybackBuffer-derived class that is expected to provide the audio output data. */
	virtual PlaybackBuffer * startDecoding(const QAudioFormat & a_Format) = 0;
};

using IPlaylistItemPtr = std::shared_ptr<IPlaylistItem>;





#endif // IPLAYLISTITEM_H

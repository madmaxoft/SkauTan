#ifndef IPLAYLISTITEM_H
#define IPLAYLISTITEM_H




#include <memory>
#include <QString>





// fwd:
class QAudioFormat;
class PlaybackBuffer;





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

	/** Starts decoding the item's audio, using the specified format.
	Returns a PlaybackBuffer instance that the caller uses to read the decoded data.
	Is expected to fill in that buffer asynchronously.
	Multiple concurrent decoding operations are to be supported.
	If the a_Format cannot be output, returns a nullptr.
	The a_Format will generally be a playback-optimized format, such as 16-bit 44.1kHz raw.
	Note that implementations should try to support the format, such as extending mono-to-stereo, or up-sampling etc. */
	virtual std::shared_ptr<PlaybackBuffer> startDecoding(const QAudioFormat & a_Format) = 0;

	/** Stops the decoding operation on the specified playback buffer. */
	virtual void stopDecoding(PlaybackBuffer * a_Playback) = 0;

	// TODO
};

using IPlaylistItemPtr = std::shared_ptr<IPlaylistItem>;





#endif // IPLAYLISTITEM_H

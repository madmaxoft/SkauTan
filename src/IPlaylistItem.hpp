#ifndef IPLAYLISTITEM_H
#define IPLAYLISTITEM_H




#include <memory>
#include <QString>
#include <QObject>
#include <QDateTime>
#include <QDebug>





// fwd:
class PlaybackBuffer;
class QAudioFormat;





/** Interface that is common for all items that can be queued and played in a playlist. */
class IPlaylistItem
{
public:
	/** The datetime when the playback has started / will start.
	Is null when the item is in the future and the playback hasn't started. */
	QDateTime m_PlaybackStarted;

	/** The datetime when the playback has ended / will end.
	Is null when the item is in the future and the playback hasn't started. */
	QDateTime m_PlaybackEnded;

	/** The tempo coefficient associated with this track. */
	qreal m_TempoCoeff;


	IPlaylistItem():
		m_TempoCoeff(1)
	{
	}


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

	/** Returns the duration limit assigned to the item, or <0 if no such limit. */
	virtual double durationLimit() const = 0;

	/** Sets a new duration limit, or removes the current limit if <0. */
	virtual void setDurationLimit(double a_Seconds) = 0;


	/** Returns the number of seconds that this track would play for.
	Accounts for tempo changes and duration limit.
	Used for playlist wall-clock time updates. */
	virtual double totalPlaybackDuration() const
	{
		auto dur = displayLength();
		auto lim = durationLimit();
		if (lim > 0)
		{
			dur = std::min(dur, lim);
		}
		return dur;
	}


	/** Returns the tempo coefficient currently associated with this track. */
	virtual double tempoCoeff() const
	{
		return m_TempoCoeff;
	}


	/** Sets the tempo coefficient associated with this track. */
	virtual void setTempoCoeff(double a_TempoCoeff)
	{
		m_TempoCoeff = a_TempoCoeff;
	}


	/** Updates m_PlaybackEnd based on the current remaining time for playback.
	Takes into account duration limit.
	Returns true if the end was changed from its previous value, false if not. */
	virtual bool updateEndTimeFromRemainingTime(double a_RemainingTime)
	{
		auto remainingMSecs = static_cast<qint64>(a_RemainingTime * 1000);
		auto end = QDateTime::currentDateTimeUtc().addMSecs(remainingMSecs);
		auto lim = durationLimit();
		if (
			(lim > 0) &&
			(m_PlaybackStarted.msecsTo(end) > lim * 1000)
		)
		{
			end = m_PlaybackStarted.addMSecs(static_cast<qint64>(lim * 1000));
		}
		if (m_PlaybackEnded != end)
		{
			m_PlaybackEnded = end;
			return true;
		}
		return false;
	}

	/** Starts decoding the item into the specified audio format.
	Returns a PlaybackBuffer-derived class that is expected to provide the audio output data. */
	virtual PlaybackBuffer * startDecoding(const QAudioFormat & a_Format) = 0;
};

using IPlaylistItemPtr = std::shared_ptr<IPlaylistItem>;

Q_DECLARE_METATYPE(IPlaylistItemPtr);





#endif // IPLAYLISTITEM_H

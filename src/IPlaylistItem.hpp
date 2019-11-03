#pragma once

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
	QDateTime mPlaybackStarted;

	/** The datetime when the playback has ended / will end.
	Is null when the item is in the future and the playback hasn't started. */
	QDateTime mPlaybackEnded;

	/** The tempo coefficient associated with this track. */
	qreal mTempoCoeff;

	/** Set to true if the player fails to play this track. */
	bool mIsMarkedUnplayable;


	IPlaylistItem():
		mTempoCoeff(1),
		mIsMarkedUnplayable(false)
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
	virtual void setDurationLimit(double aSeconds) = 0;


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
		return mTempoCoeff;
	}


	/** Sets the tempo coefficient associated with this track. */
	virtual void setTempoCoeff(double aTempoCoeff)
	{
		mTempoCoeff = aTempoCoeff;
	}


	/** Updates mPlaybackEnd based on the current remaining time for playback.
	Takes into account duration limit.
	Returns true if the end was changed from its previous value, false if not. */
	virtual bool updateEndTimeFromRemainingTime(double aRemainingTime)
	{
		auto remainingMSecs = static_cast<qint64>(aRemainingTime * 1000);
		auto end = QDateTime::currentDateTimeUtc().addMSecs(remainingMSecs);
		auto lim = durationLimit();
		if (
			(lim > 0) &&
			(mPlaybackStarted.msecsTo(end) > lim * 1000)
		)
		{
			end = mPlaybackStarted.addMSecs(static_cast<qint64>(lim * 1000));
		}
		if (mPlaybackEnded != end)
		{
			mPlaybackEnded = end;
			return true;
		}
		return false;
	}

	/** Starts decoding the item into the specified audio format.
	Returns a PlaybackBuffer-derived class that is expected to provide the audio output data. */
	virtual PlaybackBuffer * startDecoding(const QAudioFormat & aFormat) = 0;

	/** Returns whether the player has marked the track as unplayable. */
	virtual bool isMarkedUnplayable() const { return mIsMarkedUnplayable; }

	/** Marks the track as unplayable.
	Called by the Player on tracks that fail to play. */
	virtual void markAsUnplayable() { mIsMarkedUnplayable = true; }
};

using IPlaylistItemPtr = std::shared_ptr<IPlaylistItem>;

Q_DECLARE_METATYPE(IPlaylistItemPtr);

# SkauTan development notes
These are the development notes regarding this project.

## High-level architecture overview
SkauTan consists of three main parts: a song metadata DB, a high-IQ playlist editor and an integrated player.

The song metadata is stored in a SQLite DB either next to the player ("portable" version) or in the user's home folder
("installed" version). For each song, some basic metadata is tracked - length, MPM, last used volume and tempo
adjustments, last playback time, last update, rating, ...).
The song metadata has three variants: Manual, Id3 and FileName. The Manual metadata is the info entered by SkauTan
users and has the highest priority. The ID3 metadata is the info read from the file's ID3 tag or its appropriate counterparts in other file formats. The FileName metadata is guessed from the file's name. The ID3 and
FileName metadata is not user-editable in SkauTan; changes in ID3 metadata can be rescanned (periodically
automatically, and manually upon request).

The playlist editor can add either individual songs on demand (classic behavior) or it can auto-compose "rounds" of
songs, using Templates. Each template has a fixed number of items with predefined criteria. A random song matching the
criteria is selected for each item in the template when adding it to the playlist. The randomness takes into account
how long since the song was last played in history and how well its rating is. Any song in the playlist that is a part
of a "round" (has been selected by a Template) can be quick-swapped for another song matching the criteria.

Historical playlists are remembered by remembering each song played back. This data is currently not used, but there
are plans for the future for including this into the Template song selection mechanism.

The player uses FFMPEG's LibAV libraries to decode audio files and play them using Qt's platform-independent
QAudioOutput. This has been chosen because Qt's audio decoding doesn't support seeking.

A local webserver is embedded into the application, when turned on, it allows web-based rating of the songs that have been played.

## Object types
 - Database provides DB linkage and in-memory storage of all persistent data: songs, templates, history.
 - Song is an individual song in the DB, with its metadata
 - Template represents the "recipe" for choosing songs randomly, with the criteria matcher
 - IPlaylistItem is an interface for all playlist items. Multiple descendants are expected - Songs, Pauses, Chimes
 - PlaylistItemSong binds together an IPlaylistItem with a Song and possibly a Template

## Vote Webserver details

The LocalVoteServer runs on port 7880 and uses JavaScript to generate most of the page. It can serve static
files and provides API to query the playlist and cast votes. The static files come from SkauTan's internal
resources by default, but they can be overridden by storing a file in the VoteServer folder next to the
executable:

| URL            | Notes |
|----------------|-------|
| /              | Special built-in case for the main page, serves the VoteServer/index.html file |
| /static/{file} | Sends a static file, VoteServer/static/{file} |
| /api/{fn}      | Calls the specified API function |

### API functions

The `playlist` API function returns the list of songs that have been played since SkauTan
has started, as a JSON array:

```json
[
{
	"hash": "0123456789abcdef",
	"author": "Some guy",
	"title": "Some song",
	"fileName": "d:\\mp3\\Some guy - some song.mp3",
	"genre": "SF",
	"mpm": 28.37827417847,
	"index": 0,
	"ratingRC": 4.5,
	"ratingGT": 2.5,
	"ratingPop": 4.891734647892,
}, ...
]
```
Note that numbers may come encoded either as numbers or as strings. The "index" value specifies the index of
the song within the history, the first song played has index 0. The API call reads a HTTP header `X-SkauTan-
Playlist-Start` and only lists songs with index higher than or equal to its value, if present.

The `vote` function is used to cast a single vote. It reads the parameters of the vote from POSTed values,
encoded as HTML forms with either the `application/x-www-form-urlencoded` or `multipart/form-data` encoding.
It requires the following values:

| Value | Notes |
| ----  | ----- |
| songHash | The song's hash, as returned from the `playlist` API call; hex-encoded SHA1 of the song data |
| voteType | The category  in which the vote is made, one of `rhythmClarity`, `genreTypicality` or `popularity`; case-sensitive |
| voteValue | A number between 1 to 5, indicating the actual vote |


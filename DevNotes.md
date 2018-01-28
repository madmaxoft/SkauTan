# SkauTan development notes
These are the development notes regarding this project.

## High-level architecture overview
SkauTan consists of three main parts: a song metadata DB, a high-IQ playlist editor and an integrated player.

The song metadata is stored in a SQLite DB either next to the player ("portable" version) or in the user's home folder
("installed" version). For each song, some basic metadata is tracked - length, MPM, last used volume and tempo
adjustments, last playback time, last update, rating, ...) The DB may contain data for files that are no longer present
in the filesystem, or may have been changed in the meantime. There's a rescan mechanism to gradually update changed
files; deleted files are not removed from the DB (TODO: Maybe add a cleanup option?)

The playlist editor can add either individual songs on demand (classic behavior) or it can auto-compose "rounds" of
songs, where rounds have a fixed number of items with predefined criteria, a random song matching the criteria is
selected for each. The randomness takes into account how long a song wasn't played in history and how well its rating
is. Any song in the playlist that is a part of a "round" can be quick-swapped for another song matching the criteria.

Historical playlists are remembered and can be recalled at any time. Each playlist is assigned a datetime of when it
was created. Empty playlists are not stored.

The player uses FFMPEG's LibAV libraries to decode audio files and play them using Qt's platform-independent
QAudioOutput. This has been chosen because Qt's audio decoding doesn't support seeking.

## Object types
 - SongDatabase is the root object, it contains all the other objects for the player and also provides DB linkage
 - Song is an individual song in the DB, with its metadata
 - Round represents the "recipe" for choosing songs randomly, with the criteria matcher
 - IPlaylistItem is an interface for all playlist items. Multiple descendants are expected - Songs, Pauses, Chimes
 - PlaylistItemSong binds together an IPlaylistItem with a Song and possibly a Round

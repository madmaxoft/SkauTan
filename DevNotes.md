# SkauTan development notes
These are the development notes regarding this project.

## High-level architecture overview
SkauTan consists of three main parts: a song metadata DB, a high-IQ playlist editor and an integrated player.

The song metadata is stored in a SQLite DB either next to the player ("portable" version) or in the user's home folder
("installed" version). For each song, some basic metadata is tracked - length, MPM, last used volume and tempo
adjustments, last playback time, last update, rating, ...).
The song metadata has three variants: Manual, Id3 and FileName. The Manual metadata is the info entered by SkauTan
users and has the highest priority. The ID3 metadata is the info read from the file using TagLib, from the ID3 tag or
its appropriate counterparts in other file formats. The FileName metadata is guessed from the file's name. The ID3 and
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

## Object types
 - Database is the root object, it contains all the other objects' instances for the player (MetadataScanner etc.)
and also provides DB linkage
 - Song is an individual song in the DB, with its metadata
 - Template represents the "recipe" for choosing songs randomly, with the criteria matcher
 - IPlaylistItem is an interface for all playlist items. Multiple descendants are expected - Songs, Pauses, Chimes
 - PlaylistItemSong binds together an IPlaylistItem with a Song and possibly a Template

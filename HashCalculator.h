#ifndef HASHCALCULATOR_H
#define HASHCALCULATOR_H





#include "Song.h"





/** Calculates the hash of a song file.
The hash used is a SHA1 checksum of the entire file, except for its starting 128 KiB and ending 128 bytes, which are
assumed to comprise the ID3 tags (which can change without affecting the song). */
class HashCalculator
{
public:

	/** Calculates the hash for the specified song and updates the song object directly. */
	static void calc(Song & a_Song);

protected:

	Song & m_Song;


	/** Creates a new instance that is bound to the specified song. */
	HashCalculator(Song & a_Song);

	/** Calculates the hash and updates m_Song directly. */
	void calc();
};

#endif // HASHCALCULATOR_H

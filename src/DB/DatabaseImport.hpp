#ifndef DATABASEIMPORT_H
#define DATABASEIMPORT_H





#include <QString>
#include "../DatedOptional.hpp"
#include "Database.hpp"





class DatabaseImport
{
public:
	struct Options
	{
		bool mShouldImportManualTag;
		bool mShouldImportDetectedTempo;
		bool mShouldImportLastPlayedDate;
		bool mShouldImportLocalRating;
		bool mShouldImportCommunityRating;
		bool mShouldImportPlaybackHistory;
		bool mShouldImportSkipStart;
		bool mShouldImportDeletionHistory;
		bool mShouldImportSongColors;
	};


	/** Imports data from aFrom into aTo.
	aOptions specifies what data to import. */
	DatabaseImport(const Database & aFrom, Database & aTo, const Options & aOptions);


private:

	/** The DB that is the source of the data. */
	const Database & mFrom;

	/** The DB that is the destination for the data. */
	Database & mTo;

	/** The options for the import. */
	const Options & mOptions;


	void importManualTag();
	void importDetectedTempo();
	void importLastPlayedDate();
	void importLocalRating();
	void importCommunityRating();
	void importPlaybackHistory();
	void importSkipStart();
	void importDeletionHistory();
	void importSongColors();

	/** Imports the votes from aFrom to aTo, regarding the specified votes DB table. */
	void importVotes(const QString & aTableName);

	/** Returns an average of the votes that are for the specified song hash.
	If no votes are for the hash, returns an empty DatedOptional. */
	DatedOptional<double> averageVotes(
		const std::vector<Database::Vote> & aVotes,
		const QByteArray & aSongHash
	);
};





#endif // DATABASEIMPORT_H

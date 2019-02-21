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
		bool m_ShouldImportManualTag;
		bool m_ShouldImportDetectedTempo;
		bool m_ShouldImportLastPlayedDate;
		bool m_ShouldImportLocalRating;
		bool m_ShouldImportCommunityRating;
		bool m_ShouldImportPlaybackHistory;
		bool m_ShouldImportSkipStart;
		bool m_ShouldImportDeletionHistory;
		bool m_ShouldImportSongColors;
	};


	/** Imports data from a_From into a_To.
	a_Options specifies what data to import. */
	DatabaseImport(const Database & a_From, Database & a_To, const Options & a_Options);


private:

	/** The DB that is the source of the data. */
	const Database & m_From;

	/** The DB that is the destination for the data. */
	Database & m_To;

	/** The options for the import. */
	const Options & m_Options;


	void importManualTag();
	void importDetectedTempo();
	void importLastPlayedDate();
	void importLocalRating();
	void importCommunityRating();
	void importPlaybackHistory();
	void importSkipStart();
	void importDeletionHistory();
	void importSongColors();

	/** Imports the votes from a_From to a_To, regarding the specified votes DB table. */
	void importVotes(const QString & a_TableName);

	/** Returns an average of the votes that are for the specified song hash.
	If no votes are for the hash, returns an empty DatedOptional. */
	DatedOptional<double> averageVotes(
		const std::vector<Database::Vote> & a_Votes,
		const QByteArray & a_SongHash
	);
};





#endif // DATABASEIMPORT_H

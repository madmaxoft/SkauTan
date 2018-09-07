#ifndef DATABASEIMPORT_H
#define DATABASEIMPORT_H





class Database;





class DatabaseImport
{
public:
	struct Options
	{
		bool m_ShouldImportManualTag;
		bool m_ShouldImportLastPlayedDate;
		bool m_ShouldImportLocalRating;
		bool m_ShouldImportCommunityRating;
		bool m_ShouldImportPlaybackHistory;
		bool m_ShouldImportSkipStart;
		bool m_ShouldImportDeletionHistory;
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
	void importLastPlayedDate();
	void importLocalRating();
	void importCommunityRating();
	void importPlaybackHistory();
	void importSkipStart();
	void importDeletionHistory();
};





#endif // DATABASEIMPORT_H

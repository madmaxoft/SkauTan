#ifndef DLGSONGPROPERTIES_H
#define DLGSONGPROPERTIES_H





#include <memory>
#include <QDialog>
#include <Song.h>





// fwd:
class Database;
namespace Ui
{
	class DlgSongProperties;
}





class DlgSongProperties:
	public QDialog
{
	Q_OBJECT

	using Super = QDialog;


public:

	/** Creates a new instance of the dialog.
	a_Song is the song that the user requested to edit. */
	explicit DlgSongProperties(
		Database & a_DB,
		SongPtr a_Song,
		QWidget * a_Parent = nullptr
	);

	~DlgSongProperties();


private:

	/** Stores values that the user is allowed to change.
	Once the user accepts the dialog, these changes are written to the songs.
	Each song out of m_Duplicates has a separate ChangeSet instance. */
	struct ChangeSet
	{
		QVariant m_ManualAuthor;
		QVariant m_ManualTitle;
		QVariant m_ManualGenre;
		QVariant m_ManualMeasuresPerMinute;
		QVariant m_Notes;
	};


	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgSongProperties> m_UI;

	/** The database containing the songs. */
	Database & m_DB;

	/** The song being currently displayed. */
	SongPtr m_Song;

	/** All songs that have the same hash as m_Song. */
	std::vector<SongPtr> m_Duplicates;

	/** Set to true before the code changes any values.
	Used to determine whether a change-event from a control was generated by the user or by code.
	Changes generated by the user are stored for saving later, when requested. */
	bool m_IsInternalChange;

	/** The user-made changes to be made to each song out of m_Duplicates when accepting the dialog. */
	std::map<const Song *, ChangeSet> m_ChangeSets;


	/** Fills in all the duplicates into tblDuplicates. */
	void fillDuplicates();

	/** Updates the UI with the specified song.
	Used when switching duplicates. Updates m_Song. */
	void selectSong(const Song & a_Song);

	/** Returns the SongPtr out of m_Duplicates representing the specified song. */
	SongPtr songPtrFromRef(const Song & a_Song);


private slots:

	/** Applies all changesets in m_ChageSets and closes the dialog. */
	void applyAndClose();

	/** The user has edited the manual author entry, update the current changeset. */
	void authorTextEdited(const QString & a_NewText);

	/** The user has edited the manual title entry, update the current changeset. */
	void titleTextEdited(const QString & a_NewText);

	/** The user has selected a manual genre, update the current changeset. */
	void genreSelected(const QString & a_NewGenre);

	/** The user has edited the manual MPM entry, update the current changeset. */
	void measuresPerMinuteTextEdited(const QString & a_NewText);

	/** The user has edited the notes, update the current changeset. */
	void notesChanged();

	/** The user has selected the duplicate at the specified row, switch data to that duplicate. */
	void switchDuplicate(int a_Row);
};





#endif // DLGSONGPROPERTIES_H

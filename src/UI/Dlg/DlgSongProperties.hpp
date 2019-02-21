#ifndef DLGSONGPROPERTIES_H
#define DLGSONGPROPERTIES_H





#include <memory>
#include <QDialog>
#include "../../Song.hpp"
#include "../../MetadataScanner.hpp"





// fwd:
class ComponentCollection;
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
		ComponentCollection & a_Components,
		SongPtr a_Song,
		QWidget * a_Parent = nullptr
	);

	~DlgSongProperties();


private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgSongProperties> m_UI;

	/** The components of the entire program. */
	ComponentCollection & m_Components;

	/** The song being currently displayed. */
	SongPtr m_Song;

	/** All songs that have the same hash as m_Song. */
	std::vector<Song *> m_Duplicates;

	/** Set to true before the code changes any values.
	Used to determine whether a change-event from a control was generated by the user or by code.
	Changes generated by the user are stored for saving later, when requested. */
	bool m_IsInternalChange;

	/** The ID3 tag contents, as read from the file.
	Filled in the background while opening the dialog.
	The bool part indicates whether the tag reading was successful (when false, ID3 tag cannot be written back). */
	std::map<const Song *, std::pair<bool, MetadataScanner::Tag>> m_OriginalID3;

	/** The user-made changes to be made to each song out of m_Duplicates when accepting the dialog.
	Each member of the tag is independently tracked, if present, then it will be updated upon accepting the dialog.
	If there are no changes for a song, it will not have an entry in this map. */
	std::map<const Song *, MetadataScanner::Tag> m_TagID3Changes;

	/** The changes to the manual tag to be applied to the SharedData when accepting the dialog. */
	Song::Tag m_TagManual;

	/** The changes to Notes to be applied to the SharedData when accepting the dialog. */
	DatedOptional<QString> m_Notes;


	/** Fills in all the duplicates into tblDuplicates. */
	void fillDuplicates();

	/** Updates the UI with the specified song.
	Used when switching duplicates. Updates m_Song. */
	void selectSong(const Song & a_Song);

	/** Returns the Song pointer out of m_Duplicates representing the specified song. */
	SongPtr songPtrFromRef(const Song & a_Song);

	/** Updates the ParsedID3 values from the current ID3 values (originals + user edits). */
	void updateParsedId3();


public slots:

	/** Updates the leDetectedMeasuresPerMinute text to reflect the current detected tempo (or lack there-of).
	Invoked after the TempoDetector detects the tempo. */
	void updateDetectedMpm();


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

	/** The user has edited the ID3 author, update the current changeset and parsed ID3 values. */
	void id3AuthorEdited(const QString & a_NewText);

	/** The user has edited the ID3 title, update the current changeset and parsed ID3 values. */
	void id3TitleEdited(const QString & a_NewText);

	/** The user has edited the ID3 genre, update the current changeset and parsed ID3 values. */
	void id3GenreEdited(const QString & a_NewText);

	/** The user has edited the ID3 comment, update the current changeset and parsed ID3 values. */
	void id3CommentEdited(const QString & a_NewText);

	/** The user has edited the ID3 mpm, update the current changeset and parsed ID3 values. */
	void id3MeasuresPerMinuteEdited(const QString & a_NewText);

	/** Asks for confirmation, then removes the selected duplicate from the library. */
	void removeFromLibrary();

	/** Asks for confirmation, then deletes the selected duplicate from the disk. */
	void deleteFromDisk();

	/** Copies to clipboard the ID3 tag contents. */
	void copyId3Tag();

	/** Copies to clipboard the Parsed tag contents. */
	void copyPid3Tag();

	/** Copies to clipboard the Filename-based tag contents. */
	void copyFilenameTag();
};





#endif // DLGSONGPROPERTIES_H

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
	aSong is the song that the user requested to edit. */
	explicit DlgSongProperties(
		ComponentCollection & aComponents,
		SongPtr aSong,
		QWidget * aParent = nullptr
	);

	~DlgSongProperties();


private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgSongProperties> mUI;

	/** The components of the entire program. */
	ComponentCollection & mComponents;

	/** The song being currently displayed. */
	SongPtr mSong;

	/** All songs that have the same hash as mSong. */
	std::vector<Song *> mDuplicates;

	/** Set to true before the code changes any values.
	Used to determine whether a change-event from a control was generated by the user or by code.
	Changes generated by the user are stored for saving later, when requested. */
	bool mIsInternalChange;

	/** The ID3 tag contents, as read from the file.
	Filled in the background while opening the dialog.
	The bool part indicates whether the tag reading was successful (when false, ID3 tag cannot be written back). */
	std::map<const Song *, std::pair<bool, MetadataScanner::Tag>> mOriginalID3;

	/** The user-made changes to be made to each song out of mDuplicates when accepting the dialog.
	Each member of the tag is independently tracked, if present, then it will be updated upon accepting the dialog.
	If there are no changes for a song, it will not have an entry in this map. */
	std::map<const Song *, MetadataScanner::Tag> mTagID3Changes;

	/** The changes to the manual tag to be applied to the SharedData when accepting the dialog. */
	Song::Tag mTagManual;

	/** The changes to Notes to be applied to the SharedData when accepting the dialog. */
	DatedOptional<QString> mNotes;


	/** Fills in all the duplicates into tblDuplicates. */
	void fillDuplicates();

	/** Updates the UI with the specified song.
	Used when switching duplicates. Updates mSong. */
	void selectSong(const Song & aSong);

	/** Returns the Song pointer out of mDuplicates representing the specified song. */
	SongPtr songPtrFromRef(const Song & aSong);

	/** Updates the ParsedID3 values from the current ID3 values (originals + user edits). */
	void updateParsedId3();


private slots:

	/** Applies all changesets in mChageSets and closes the dialog. */
	void applyAndClose();

	/** The user has edited the manual author entry, update the current changeset. */
	void authorTextEdited(const QString & aNewText);

	/** The user has edited the manual title entry, update the current changeset. */
	void titleTextEdited(const QString & aNewText);

	/** The user has selected a manual genre, update the current changeset. */
	void genreSelected(const QString & aNewGenre);

	/** The user has edited the manual MPM entry, update the current changeset. */
	void measuresPerMinuteTextEdited(const QString & aNewText);

	/** The user has edited the notes, update the current changeset. */
	void notesChanged();

	/** The user has selected the duplicate at the specified row, switch data to that duplicate. */
	void switchDuplicate(int aRow);

	/** The user has edited the ID3 author, update the current changeset and parsed ID3 values. */
	void id3AuthorEdited(const QString & aNewText);

	/** The user has edited the ID3 title, update the current changeset and parsed ID3 values. */
	void id3TitleEdited(const QString & aNewText);

	/** The user has edited the ID3 genre, update the current changeset and parsed ID3 values. */
	void id3GenreEdited(const QString & aNewText);

	/** The user has edited the ID3 comment, update the current changeset and parsed ID3 values. */
	void id3CommentEdited(const QString & aNewText);

	/** The user has edited the ID3 mpm, update the current changeset and parsed ID3 values. */
	void id3MeasuresPerMinuteEdited(const QString & aNewText);

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

	/** Shows the TapTempo dialog, and if saved, updates the manual tempo. */
	void showTapTempo();

	/** The specified song has had its detected tempo updated.
	If this is the shown song, updates the UI to show the tempo. */
	void songTempoDetected(Song::SharedDataPtr aSongSD);
};





#endif // DLGSONGPROPERTIES_H

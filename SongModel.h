#ifndef SONGMODEL_H
#define SONGMODEL_H




#include <QAbstractTableModel>
#include "Song.h"





// fwd:
class SongDatabase;





/** Provides a Qt model-view architecture's Model implementation for displaying all songs in a SongDatabase. */
class SongModel:
	public QAbstractTableModel
{
	Q_OBJECT
	using Super = QAbstractTableModel;


public:

	/** Symbolic names for the individual columns. */
	enum EColumn
	{
		colRating,
		colAuthor,
		colTitle,
		colGenre,
		colMeasuresPerMinute,
		colLength,
		colLastPlayed,
		colFileName,

		colMax,
	};


	SongModel(SongDatabase & a_DB);

	/** Returns the song represented by the specified model index. */
	SongPtr songFromIndex(const QModelIndex & a_Idx) const;

protected:

	/** The DB on which the model is based. */
	SongDatabase & m_DB;


	// QAbstractTableModel overrides:
	virtual int rowCount(const QModelIndex & a_Parent) const override;
	virtual int columnCount(const QModelIndex & a_Parent) const override;
	virtual QVariant data(const QModelIndex & a_Index, int a_Role) const override;
	virtual QVariant headerData(int a_Section, Qt::Orientation a_Orientation, int a_Role) const override;
	virtual Qt::ItemFlags flags(const QModelIndex & a_Index) const override;
	virtual bool setData(const QModelIndex & a_Index, const QVariant & a_Value, int a_Role) override;

protected slots:

	/** Called by m_DB when a new song is added to it. */
	void addSongFile(Song * a_NewSong);


signals:

	/** Emitted after the specified song has been edited by the user. */
	void songEdited(Song * a_Song);
};





#endif // SONGMODEL_H

#ifndef SONGMODEL_H
#define SONGMODEL_H




#include <QAbstractTableModel>





// fwd:
class Song;
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
		colFileName = 0,
		colLength = 1,
		colGenre = 2,
		colMeasuresPerMinute = 3,
		colLastPlayed = 4,
		colRating = 5,
		colAuthor = 6,
		colTitle = 7,

		colMax,
	};


	SongModel(SongDatabase & a_DB);


protected:

	/** The DB on which the model is based. */
	SongDatabase & m_DB;


	// QAbstractTableModel overrides:
	virtual int rowCount(const QModelIndex & a_Parent) const override;
	virtual int columnCount(const QModelIndex & a_Parent) const override;
	virtual QVariant data(const QModelIndex & a_Index, int a_Role) const override;
	virtual QVariant headerData(int a_Section, Qt::Orientation a_Orientation, int a_Role) const override;

protected slots:

	/** Called by m_DB when a new song is added to it. */
	void addSongFile(Song * a_NewSong);
};





#endif // SONGMODEL_H

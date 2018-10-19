#ifndef COLORDELEGATE_H
#define COLORDELEGATE_H





#include <QItemDelegate>





/** A simple ItemDelegate descendant for editing colors in a QTableView.
Provides a button for displaying a color picker next to the color value. */
class ColorDelegate:
	public QItemDelegate
{
	using Super = QItemDelegate;

	Q_OBJECT

public:

	/** Creates a new delegate.
	a_ColorDialogCaption specifies the caption to use for the color picker dialog, once the user clicks the button. */
	ColorDelegate(const QString & a_ColorDialogCaption, QObject * a_Parent = nullptr);


	// QItemDelegate overrides:
	virtual void paint(
		QPainter * a_Painter,
		const QStyleOptionViewItem & a_Option,
		const QModelIndex & a_Index
	) const override;

	virtual bool editorEvent(
		QEvent * a_Event,
		QAbstractItemModel * a_Model,
		const QStyleOptionViewItem & a_Option,
		const QModelIndex & a_Index
	) override;


protected:

	/** The caption to be used for the color picker dialog. */
	QString m_ColorDialogCaption;


	/** Returns the rectangle for the button in the specified item's bounding rectangle. */
	QRect buttonRectFromItemRect(const QRect & a_ItemRect) const;
};





#endif // COLORDELEGATE_H

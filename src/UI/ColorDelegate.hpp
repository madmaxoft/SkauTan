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
	aColorDialogCaption specifies the caption to use for the color picker dialog, once the user clicks the button. */
	ColorDelegate(const QString & aColorDialogCaption, QObject * aParent = nullptr);


	// QItemDelegate overrides:
	virtual void paint(
		QPainter * aPainter,
		const QStyleOptionViewItem & aOption,
		const QModelIndex & aIndex
	) const override;

	virtual bool editorEvent(
		QEvent * aEvent,
		QAbstractItemModel * aModel,
		const QStyleOptionViewItem & aOption,
		const QModelIndex & aIndex
	) override;


protected:

	/** The caption to be used for the color picker dialog. */
	QString mColorDialogCaption;


	/** Returns the rectangle for the button in the specified item's bounding rectangle. */
	QRect buttonRectFromItemRect(const QRect & aItemRect) const;
};





#endif // COLORDELEGATE_H

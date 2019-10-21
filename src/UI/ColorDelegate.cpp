#include "ColorDelegate.hpp"
#include <QApplication>
#include <QMouseEvent>
#include <QColorDialog>





ColorDelegate::ColorDelegate(const QString & aColorDialogCaption, QObject * aParent):
	Super(aParent),
	mColorDialogCaption(aColorDialogCaption)
{
}





void ColorDelegate::paint(
	QPainter * aPainter,
	const QStyleOptionViewItem & aOption,
	const QModelIndex & aIndex
) const
{
	// Draw the button:
	QStyleOptionButton button;
	button.rect = buttonRectFromItemRect(aOption.rect);
	button.text = "...";
	button.state = QStyle::State_Enabled;
	QApplication::style()->drawControl(QStyle::CE_PushButton, &button, aPainter);

	// Draw the text, using the original delegate:
	QStyleOptionViewItem txt(aOption);
	txt.rect.setWidth(txt.rect.width() - txt.rect.height() - 1);
	Super::paint(aPainter, txt,aIndex);
}





bool ColorDelegate::editorEvent(
	QEvent * aEvent,
	QAbstractItemModel * aModel,
	const QStyleOptionViewItem & aOption,
	const QModelIndex & aIndex
)
{
	if (aEvent->type() == QEvent::MouseButtonRelease)
	{
		auto evt = reinterpret_cast<QMouseEvent *>(aEvent);
		auto btnRect = buttonRectFromItemRect(aOption.rect);
		if (btnRect.contains(evt->pos()))
		{
			auto c = QColorDialog::getColor(
				QColor(aModel->data(aIndex).toString()),
				nullptr,  // TODO: A proper parent
				mColorDialogCaption
			);
			if (c.isValid())
			{
				aModel->setData(aIndex, c.name());
			}
			return true;
		}
	}

	return false;
}





QRect ColorDelegate::buttonRectFromItemRect(const QRect & aItemRect) const
{
	return QRect (
		aItemRect.right() - aItemRect.height(),  // Left
		aItemRect.top(),                          // Top
		aItemRect.height(),                       // Width
		aItemRect.height()                        // Height
	);
}

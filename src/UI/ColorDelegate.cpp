#include "ColorDelegate.hpp"
#include <QApplication>
#include <QMouseEvent>
#include <QColorDialog>





ColorDelegate::ColorDelegate(const QString & a_ColorDialogCaption, QObject * a_Parent):
	Super(a_Parent),
	m_ColorDialogCaption(a_ColorDialogCaption)
{
}





void ColorDelegate::paint(
	QPainter * a_Painter,
	const QStyleOptionViewItem & a_Option,
	const QModelIndex & a_Index
) const
{
	// Draw the button:
	QStyleOptionButton button;
	button.rect = buttonRectFromItemRect(a_Option.rect);
	button.text = "...";
	button.state = QStyle::State_Enabled;
	QApplication::style()->drawControl(QStyle::CE_PushButton, &button, a_Painter);

	// Draw the text, using the original delegate:
	QStyleOptionViewItem txt(a_Option);
	txt.rect.setWidth(txt.rect.width() - txt.rect.height() - 1);
	Super::paint(a_Painter, txt,a_Index);
}





bool ColorDelegate::editorEvent(
	QEvent * a_Event,
	QAbstractItemModel * a_Model,
	const QStyleOptionViewItem & a_Option,
	const QModelIndex & a_Index
)
{
	if (a_Event->type() == QEvent::MouseButtonRelease)
	{
		auto evt = reinterpret_cast<QMouseEvent *>(a_Event);
		auto btnRect = buttonRectFromItemRect(a_Option.rect);
		if (btnRect.contains(evt->pos()))
		{
			auto c = QColorDialog::getColor(
				QColor(a_Model->data(a_Index).toString()),
				nullptr,  // TODO: A proper parent
				m_ColorDialogCaption
			);
			if (c.isValid())
			{
				a_Model->setData(a_Index, c.name());
			}
			return true;
		}
	}

	return false;
}





QRect ColorDelegate::buttonRectFromItemRect(const QRect & a_ItemRect) const
{
	return QRect (
		a_ItemRect.right() - a_ItemRect.height(),  // Left
		a_ItemRect.top(),                          // Top
		a_ItemRect.height(),                       // Width
		a_ItemRect.height()                        // Height
	);
}

#ifndef DLGDEBUGLOG_HPP
#define DLGDEBUGLOG_HPP





#include <memory>
#include <QDialog>





namespace Ui
{
	class DlgDebugLog;
}





/** Dialog that displays the DebugLogger's messages in a table. */
class DlgDebugLog:
	public QDialog
{
	using Super = QDialog;

	Q_OBJECT

public:

	explicit DlgDebugLog(QWidget * a_Parent = nullptr);

	~DlgDebugLog();


private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgDebugLog> m_UI;


	/** Adds the specified message to twMessages. */
	void addMessage(
		const QDateTime & a_DateTime,
		const QtMsgType a_Type,
		const QString & a_FileName,
		const QString & a_Function,
		int a_LineNum,
		const QString & a_Message
	);
};





#endif // DLGDEBUGLOG_HPP

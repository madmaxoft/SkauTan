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

	explicit DlgDebugLog(QWidget * aParent = nullptr);

	~DlgDebugLog();


private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgDebugLog> mUI;


	/** Adds the specified message to twMessages. */
	void addMessage(
		const QDateTime & aDateTime,
		const QtMsgType aType,
		const QString & aFileName,
		const QString & aFunction,
		int aLineNum,
		const QString & aMessage
	);
};





#endif // DLGDEBUGLOG_HPP

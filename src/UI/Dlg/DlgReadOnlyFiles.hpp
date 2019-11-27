#pragma once

#include <memory>
#include <QDialog>





namespace Ui
{
	class DlgReadOnlyFiles;
}





/** Dialog to be shown when attempting to change files that are marked ReadOnly in the filesystem.
SkauTan can automatically drop the attribute before the operation, this dialog asks the user for permission. */
class DlgReadOnlyFiles:
	public QDialog
{
	using super = QDialog;
	Q_OBJECT

public:

	explicit DlgReadOnlyFiles(const QStringList & aFileNames, QWidget * aParent = nullptr);

	virtual ~DlgReadOnlyFiles() override;


private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgReadOnlyFiles> mUI;
};

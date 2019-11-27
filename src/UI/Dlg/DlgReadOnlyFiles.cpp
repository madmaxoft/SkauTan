#include "DlgReadOnlyFiles.hpp"
#include "ui_DlgReadOnlyFiles.h"





DlgReadOnlyFiles::DlgReadOnlyFiles(const QStringList & aFileNames, QWidget * aParent):
	super(aParent),
	mUI(new Ui::DlgReadOnlyFiles)
{
	mUI->setupUi(this);

	connect(mUI->btnYes, &QPushButton::clicked, this, &QDialog::accept);
	connect(mUI->btnNo,  &QPushButton::clicked, this, &QDialog::reject);

	mUI->lblFileNames->setText(aFileNames.join("\n"));
}





DlgReadOnlyFiles::~DlgReadOnlyFiles()
{
	// Nothing explicit needed yet
}

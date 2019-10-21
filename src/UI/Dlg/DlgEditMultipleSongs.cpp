#include "DlgEditMultipleSongs.hpp"
#include <cassert>
#include "ui_DlgEditMultipleSongs.h"
#include "../../Settings.hpp"
#include "../../Song.hpp"
#include "../../ComponentCollection.hpp"
#include "../../DB/Database.hpp"





DlgEditMultipleSongs::DlgEditMultipleSongs(
	ComponentCollection & aComponents,
	std::vector<std::shared_ptr<Song>> && aSongs,
	QWidget * aParent
):
	Super(aParent),
	mComponents(aComponents),
	mSongs(std::move(aSongs)),
	mUI(new Ui::DlgEditMultipleSongs)
{
	assert(!mSongs.empty());  // Need at least one song to edit

	Settings::loadWindowPos("DlgEditMultipleSongs", *this);
	mUI->setupUi(this);
	connect(mUI->btnOK,     &QPushButton::clicked,  this, &DlgEditMultipleSongs::applyChangesAndClose);
	connect(mUI->btnCancel, &QPushButton::clicked,  this, &QDialog::reject);
	connect(mUI->leMPM,     &QLineEdit::textEdited, this, &DlgEditMultipleSongs::mpmTextEdited);
	connect(mUI->leAuthor, &QLineEdit::textEdited, [this](){ mUI->chbAuthor->setChecked(true); });
	connect(mUI->leTitle,  &QLineEdit::textEdited, [this](){ mUI->chbTitle->setChecked(true); });
	connect(mUI->leGenre,  &QLineEdit::textEdited, [this](){ mUI->chbGenre->setChecked(true); });

	// Insert the songs into the list widget:
	for (const auto & song: mSongs)
	{
		new QListWidgetItem(song->fileName(), mUI->lwSongs);
	}
}





DlgEditMultipleSongs::~DlgEditMultipleSongs()
{
	Settings::saveWindowPos("DlgEditMultipleSongs", *this);
}





void DlgEditMultipleSongs::applyChangesAndClose()
{
	auto db = mComponents.get<Database>();
	auto shouldSetAuthor = mUI->chbAuthor->isChecked();
	auto shouldSetTitle  = mUI->chbTitle->isChecked();
	auto shouldSetGenre  = mUI->chbGenre->isChecked();
	auto shouldSetMPM    = mUI->chbMPM->isChecked();
	auto author = mUI->leAuthor->text();
	auto title  = mUI->leTitle->text();
	bool isOK;
	auto genre  = mUI->leGenre->text();
	auto mpm    = QLocale::system().toDouble(mUI->leMPM->text(), &isOK);
	if (!isOK)
	{
		shouldSetMPM = false;
	}
	for (auto & song: mSongs)
	{
		if (shouldSetAuthor)
		{
			song->setManualAuthor(author);
		}
		if (shouldSetTitle)
		{
			song->setManualTitle(title);
		}
		if (shouldSetGenre)
		{
			song->setManualGenre(genre);
		}
		if (shouldSetMPM)
		{
			song->setManualMeasuresPerMinute(mpm);
		}
		db->saveSong(song);
	}

	accept();
}





void DlgEditMultipleSongs::mpmTextEdited(const QString & aNewText)
{
	mUI->chbMPM->setChecked(true);
	if (aNewText.isEmpty())
	{
		mUI->leMPM->setStyleSheet("");
		return;
	}
	bool isOK;
	QLocale::system().toDouble(aNewText, &isOK);
	if (isOK)
	{
		mUI->leMPM->setStyleSheet("");
	}
	else
	{
		mUI->leMPM->setStyleSheet("background-color:#fcc");
	}
}

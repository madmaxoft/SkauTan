#include "DlgEditMultipleSongs.hpp"
#include <cassert>
#include "ui_DlgEditMultipleSongs.h"
#include "../../Settings.hpp"
#include "../../Song.hpp"
#include "../../ComponentCollection.hpp"
#include "../../DB/Database.hpp"





DlgEditMultipleSongs::DlgEditMultipleSongs(
	ComponentCollection & a_Components,
	std::vector<std::shared_ptr<Song>> && a_Songs,
	QWidget * a_Parent
):
	Super(a_Parent),
	m_Components(a_Components),
	m_Songs(std::move(a_Songs)),
	m_UI(new Ui::DlgEditMultipleSongs)
{
	assert(!m_Songs.empty());  // Need at least one song to edit

	Settings::loadWindowPos("DlgEditMultipleSongs", *this);
	m_UI->setupUi(this);
	connect(m_UI->btnOK,     &QPushButton::clicked,  this, &DlgEditMultipleSongs::applyChangesAndClose);
	connect(m_UI->btnCancel, &QPushButton::clicked,  this, &QDialog::reject);
	connect(m_UI->leMPM,     &QLineEdit::textEdited, this, &DlgEditMultipleSongs::mpmTextEdited);
	connect(m_UI->leAuthor, &QLineEdit::textEdited, [this](){ m_UI->chbAuthor->setChecked(true); });
	connect(m_UI->leTitle,  &QLineEdit::textEdited, [this](){ m_UI->chbTitle->setChecked(true); });
	connect(m_UI->leGenre,  &QLineEdit::textEdited, [this](){ m_UI->chbGenre->setChecked(true); });

	// Insert the songs into the list widget:
	for (const auto & song: m_Songs)
	{
		new QListWidgetItem(song->fileName(), m_UI->lwSongs);
	}
}





DlgEditMultipleSongs::~DlgEditMultipleSongs()
{
	Settings::saveWindowPos("DlgEditMultipleSongs", *this);
}





void DlgEditMultipleSongs::applyChangesAndClose()
{
	auto db = m_Components.get<Database>();
	auto shouldSetAuthor = m_UI->chbAuthor->isChecked();
	auto shouldSetTitle  = m_UI->chbTitle->isChecked();
	auto shouldSetGenre  = m_UI->chbGenre->isChecked();
	auto shouldSetMPM    = m_UI->chbMPM->isChecked();
	auto author = m_UI->leAuthor->text();
	auto title  = m_UI->leTitle->text();
	bool isOK;
	auto genre  = m_UI->leGenre->text();
	auto mpm    = QLocale::system().toDouble(m_UI->leMPM->text(), &isOK);
	if (!isOK)
	{
		shouldSetMPM = false;
	}
	for (auto & song: m_Songs)
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





void DlgEditMultipleSongs::mpmTextEdited(const QString & a_NewText)
{
	m_UI->chbMPM->setChecked(true);
	if (a_NewText.isEmpty())
	{
		m_UI->leMPM->setStyleSheet("");
		return;
	}
	bool isOK;
	QLocale::system().toDouble(a_NewText, &isOK);
	if (isOK)
	{
		m_UI->leMPM->setStyleSheet("");
	}
	else
	{
		m_UI->leMPM->setStyleSheet("background-color:#fcc");
	}
}

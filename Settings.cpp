#include "Settings.h"
#include <assert.h>
#include <QSettings>
#include <QHeaderView>
#include <QDebug>





std::unique_ptr<QSettings> Settings::m_Settings;





void Settings::init(const QString & a_IniFileName)
{
	m_Settings = std::make_unique<QSettings>(a_IniFileName, QSettings::IniFormat);
}





void Settings::saveHeaderView(
	const char * a_WindowName,
	const char * a_HeaderViewName,
	const QHeaderView & a_HeaderView
)
{
	assert(m_Settings != nullptr);
	if (m_Settings == nullptr)
	{
		qWarning() << "Cannot save header column widths, not initialized.";
		return;
	}

	m_Settings->beginGroup(a_WindowName);
		m_Settings->beginGroup(a_HeaderViewName);
			m_Settings->beginWriteArray("columns");
				int numColumns = a_HeaderView.count();
				for (int i = 0; i < numColumns; ++i)
				{
					m_Settings->setArrayIndex(i);
					m_Settings->setValue("sectionSize",a_HeaderView.sectionSize(i));
				}
			m_Settings->endArray();
		m_Settings->endGroup();
	m_Settings->endGroup();
	m_Settings->sync();
}





void Settings::loadHeaderView(
	const char * a_WindowName,
	const char * a_HeaderViewName,
	QHeaderView & a_HeaderView
)
{
	assert(m_Settings != nullptr);
	if (m_Settings == nullptr)
	{
		qWarning() << "Cannot save header column widths, not initialized.";
		return;
	}

	m_Settings->beginGroup(a_WindowName);
		m_Settings->beginGroup(a_HeaderViewName);
			int numInSettings = m_Settings->beginReadArray("columns");
				int numInControl = a_HeaderView.count();
				int numColumns = std::min(numInSettings, numInControl);
				for (int i = 0; i < numColumns; ++i)
				{
					m_Settings->setArrayIndex(i);
					auto s = m_Settings->value("sectionSize").toInt();
					if (s > 0)
					{
						a_HeaderView.resizeSection(i, s);
					}
				}
			m_Settings->endArray();
		m_Settings->endGroup();
	m_Settings->endGroup();
}





void Settings::saveWindowPos(const char * a_WindowName, const QWidget & a_Window)
{
	assert(m_Settings != nullptr);
	if (m_Settings == nullptr)
	{
		qWarning() << "Cannot save window pos, not initialized.";
		return;
	}

	m_Settings->beginGroup(a_WindowName);
		m_Settings->setValue("pos", a_Window.pos());
		m_Settings->setValue("size", a_Window.size());
	m_Settings->endGroup();
	m_Settings->sync();
}





void Settings::loadWindowPos(const char * a_WindowName, QWidget & a_Window)
{
	assert(m_Settings != nullptr);
	if (m_Settings == nullptr)
	{
		qWarning() << "Cannot save window pos, not initialized.";
		return;
	}

	m_Settings->beginGroup(a_WindowName);
		a_Window.resize(m_Settings->value("size", a_Window.size()).toSize());
		a_Window.move(  m_Settings->value("pos",  a_Window.pos()).toPoint());
	m_Settings->endGroup();
}

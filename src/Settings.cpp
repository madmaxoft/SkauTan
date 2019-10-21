#include "Settings.hpp"
#include <cassert>
#include <QSettings>
#include <QHeaderView>
#include <QSplitter>
#include <QDebug>





std::unique_ptr<QSettings> Settings::mSettings;





#define CHECK_VALID \
	assert(mSettings != nullptr); \
	if (mSettings == nullptr) \
	{ \
		qWarning() << "The Settings object has not been initialized."; \
		return; \
	} \





void Settings::init(const QString & aIniFileName)
{
	mSettings = std::make_unique<QSettings>(aIniFileName, QSettings::IniFormat);
}





void Settings::saveHeaderView(
	const char * aWindowName,
	const char * aHeaderViewName,
	const QHeaderView & aHeaderView
)
{
	CHECK_VALID;

	mSettings->beginGroup(aWindowName);
		mSettings->beginGroup(aHeaderViewName);
			mSettings->beginWriteArray("columns");
				int numColumns = aHeaderView.count();
				for (int i = 0; i < numColumns; ++i)
				{
					mSettings->setArrayIndex(i);
					mSettings->setValue("sectionSize",aHeaderView.sectionSize(i));
				}
			mSettings->endArray();
		mSettings->endGroup();
	mSettings->endGroup();
	mSettings->sync();
}





void Settings::loadHeaderView(
	const char * aWindowName,
	const char * aHeaderViewName,
	QHeaderView & aHeaderView
)
{
	CHECK_VALID;

	mSettings->beginGroup(aWindowName);
		mSettings->beginGroup(aHeaderViewName);
			int numInSettings = mSettings->beginReadArray("columns");
				int numInControl = aHeaderView.count();
				int numColumns = std::min(numInSettings, numInControl);
				for (int i = 0; i < numColumns; ++i)
				{
					mSettings->setArrayIndex(i);
					auto s = mSettings->value("sectionSize").toInt();
					if (s > 0)
					{
						aHeaderView.resizeSection(i, s);
					}
				}
			mSettings->endArray();
		mSettings->endGroup();
	mSettings->endGroup();
}





void Settings::saveWindowPos(const char * aWindowName, const QWidget & aWindow)
{
	CHECK_VALID;

	mSettings->beginGroup(aWindowName);
		mSettings->setValue("pos", aWindow.pos());
		mSettings->setValue("size", aWindow.size());
	mSettings->endGroup();
	mSettings->sync();
}





void Settings::loadWindowPos(const char * aWindowName, QWidget & aWindow)
{
	CHECK_VALID;

	mSettings->beginGroup(aWindowName);
		aWindow.resize(mSettings->value("size", aWindow.size()).toSize());
		aWindow.move(  mSettings->value("pos",  aWindow.pos()).toPoint());
	mSettings->endGroup();
}





void Settings::saveSplitterSizes(const char * aWindowName, const char * aSplitterName, const QSplitter & aSplitter)
{
	CHECK_VALID;

	mSettings->beginGroup(aWindowName);
		mSettings->beginGroup(aSplitterName);
			mSettings->beginWriteArray("sectionSizes");
				const auto & sizes = aSplitter.sizes();
				int numSections = sizes.count();
				for (int i = 0; i < numSections; ++i)
				{
					mSettings->setArrayIndex(i);
					mSettings->setValue("sectionSize", sizes[i]);
				}
			mSettings->endArray();
		mSettings->endGroup();
	mSettings->endGroup();
	mSettings->sync();
}





void Settings::saveSplitterSizes(const char * aWindowName, const char * aSplitterName, const QSplitter * aSplitter)
{
	assert(aSplitter != nullptr);
	saveSplitterSizes(aWindowName, aSplitterName, *aSplitter);
}





void Settings::loadSplitterSizes(const char * aWindowName, const char * aSplitterName, QSplitter & aSplitter)
{
	CHECK_VALID;

	mSettings->beginGroup(aWindowName);
		mSettings->beginGroup(aSplitterName);
			int numSections = mSettings->beginReadArray("sectionSizes");
				QList<int> sectionSizes;
				for (int i = 0; i < numSections; ++i)
				{
					mSettings->setArrayIndex(i);
					sectionSizes.append(mSettings->value("sectionSize").toInt());
				}
				if (!sectionSizes.isEmpty())
				{
					aSplitter.setSizes(sectionSizes);
				}
			mSettings->endArray();
		mSettings->endGroup();
	mSettings->endGroup();
}





void Settings::loadSplitterSizes(const char * aWindowName, const char * aSplitterName, QSplitter * aSplitter)
{
	assert(aSplitter != nullptr);
	loadSplitterSizes(aWindowName, aSplitterName, *aSplitter);
}





void Settings::saveValue(const char * aWindowName, const char * aValueName, const QVariant & aValue)
{
	CHECK_VALID;

	mSettings->beginGroup(aWindowName);
		mSettings->setValue(aValueName, aValue);
	mSettings->endGroup();
	mSettings->sync();
}





QVariant Settings::loadValue(const char * aWindowName, const char * aValueName, const QVariant & aDefault)
{
	assert(mSettings != nullptr);
	if (mSettings == nullptr)
	{
		qWarning() << "The Settings object has not been initialized.";
		return aDefault;
	}

	mSettings->beginGroup(aWindowName);
		auto res = mSettings->value(aValueName, aDefault);
	mSettings->endGroup();

	return res;
}

#include "Stopwatch.hpp"
#include <QDebug>





/** Initializer for the TimeSinceStart class.
Creates the singleton instance and sets its reference timepoint. */
static class Initializer
{
public:
	Initializer()
	{
		TimeSinceStart::nsecElapsed();
	}
} init;





////////////////////////////////////////////////////////////////////////////////
// TimeSinceStart:

TimeSinceStart::TimeSinceStart()
{
	mTimer.start();
}





TimeSinceStart &TimeSinceStart::get()
{
	static TimeSinceStart instance;
	return instance;
}





qint64 TimeSinceStart::nsecElapsed()
{
	return get().mTimer.nsecsElapsed();
}





qint64 TimeSinceStart::msecElapsed()
{
	return get().mTimer.elapsed();
}





////////////////////////////////////////////////////////////////////////////////
// Stopwatch:

Stopwatch::Stopwatch(const char * aSrcFile, int aSrcLine, const char * aAction):
	mSrcFile(aSrcFile),
	mSrcLine(aSrcLine),
	mAction(aAction)
{
	mTimer.start();
}





Stopwatch::~Stopwatch()
{
	qDebug() << mAction.c_str() << " took " << mTimer.elapsed() << "msec.";
}





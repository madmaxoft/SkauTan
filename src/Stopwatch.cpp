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
	m_Timer.start();
}





TimeSinceStart &TimeSinceStart::get()
{
	static TimeSinceStart instance;
	return instance;
}





qint64 TimeSinceStart::nsecElapsed()
{
	return get().m_Timer.nsecsElapsed();
}





qint64 TimeSinceStart::msecElapsed()
{
	return get().m_Timer.elapsed();
}





////////////////////////////////////////////////////////////////////////////////
// Stopwatch:

Stopwatch::Stopwatch(const char * a_SrcFile, int a_SrcLine, const char * a_Action):
	m_SrcFile(a_SrcFile),
	m_SrcLine(a_SrcLine),
	m_Action(a_Action)
{
	m_Timer.start();
}





Stopwatch::~Stopwatch()
{
	qDebug() << m_Action.c_str() << " took " << m_Timer.elapsed() << "msec.";
}





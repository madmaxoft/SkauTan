#ifndef STOPWATCH_H
#define STOPWATCH_H





#include <string>
#include <QElapsedTimer>





/** Provides a high-resolution time difference between the program start and the current moment.
Use the nsecElapsed() or msecElapsed() functions to query the time difference. */
class TimeSinceStart
{
public:

	/** Returns the number of nanoseconds elapsed from program start. */
	static qint64 nsecElapsed();

	/** Returns the number of micro-seconds elapsed from program start. */
	static qint64 msecElapsed();


protected:

	/** Constructs the single instance of this timer. Do not use anywhere but in get(). */
	TimeSinceStart();

	/** Returns the single instance of this timer. */
	static TimeSinceStart & get();


	QElapsedTimer mTimer;
};





/** Used as a simple RAII-style stopwatch - takes note of its creation time and upon destruction, logs the
elapsed time to the log. */
class Stopwatch
{
public:
	Stopwatch(const char * aSrcFile, int aSrcLine, const char * aAction);

	~Stopwatch();


protected:

	const std::string mSrcFile;
	const int mSrcLine;
	const std::string mAction;
	QElapsedTimer mTimer;
};

#define STOPWATCH(X) Stopwatch sw(__FILE__, __LINE__, X);





#endif // STOPWATCH_H

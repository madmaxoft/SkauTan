#ifndef STOPWATCH_H
#define STOPWATCH_H





#include <string>
#include <QElapsedTimer>
#include <QDebug>





class Stopwatch
{
public:
	Stopwatch(const char * a_SrcFile, int a_SrcLine, const char * a_Action):
		m_SrcFile(a_SrcFile),
		m_SrcLine(a_SrcLine),
		m_Action(a_Action)
	{
		m_Timer.start();
	}


	~Stopwatch()
	{
		qDebug() << m_Action.c_str() << " took " << m_Timer.elapsed() << "msec.";
	}


protected:

	const std::string m_SrcFile;
	const int m_SrcLine;
	const std::string m_Action;
	QElapsedTimer m_Timer;
};

#define STOPWATCH(X) Stopwatch sw(__FILE__, __LINE__, X);





#endif // STOPWATCH_H

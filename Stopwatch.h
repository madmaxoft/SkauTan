#ifndef STOPWATCH_H
#define STOPWATCH_H





#include <string>
#include <QElapsedTimer>
#include <QDebug>





class Stopwatch
{
public:
	Stopwatch(const char * a_SrcFile, int a_SrcLine, const char * a_Function):
		m_SrcFile(a_SrcFile),
		m_SrcLine(a_SrcLine),
		m_Function(a_Function)
	{
		m_Timer.start();
	}


	~Stopwatch()
	{
		qDebug() << "Function " << m_Function.c_str() << " took " << m_Timer.elapsed() << "msec.";
	}


protected:

	const std::string m_SrcFile;
	const int m_SrcLine;
	const std::string m_Function;
	QElapsedTimer m_Timer;
};

#define STOPWATCH Stopwatch sw(__FILE__, __LINE__, __FUNCTION__);





#endif // STOPWATCH_H

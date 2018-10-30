#ifndef EXCEPTION_H
#define EXCEPTION_H





#include <QString>
#include <QDebug>





/** A class that is the base of all exceptions used in the program.
Provides parametrical description message formatting.
Usage:
throw Exception(formatString, argValue1, argValue2);
*/
class Exception:
	public std::runtime_error
{
public:

	/** Creates an exception with the specified values concatenated as the description. */
	template <typename... OtherTs>
	Exception(const QString & a_FormatString, const OtherTs &... a_ArgValues):
		std::runtime_error(format(a_FormatString, a_ArgValues...).toStdString())
	{
		qWarning() << "Created an Exception: " << what();
	}


protected:


	/** Helper function for formatting a single value into a string (like QString::arg).
	Overload for handling all arithmetic types. */
	template <typename T>
	QString formatSingle(
		const QString & a_FormatString,
		T a_Value,
		typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0
	)
	{
		return a_FormatString.arg(a_Value);
	}


	/** Helper function for formatting a single value into a string (like QString::arg).
	Overload for handling QString values. */
	QString formatSingle(const QString & a_FormatString, const QString & a_Value)
	{
		return a_FormatString.arg(a_Value);
	}


	/** Helper function for formatting a single value into a string (like QString::arg).
	Overload for handling std::string values. */
	QString formatSingle(const QString & a_FormatString, const std::string & a_Value)
	{
		return a_FormatString.arg(QString::fromStdString(a_Value));
	}


	/** Helper function for formatting a single value into a string (like QString::arg).
	Overload for handling all other types - pass them through QDebug for more informational formatting. */
	template <typename T>
	QString formatSingle(const QString & a_FormatString, T a_Value,
		typename std::enable_if<
			!std::is_same<T, std::string>::value &&
			!std::is_same<T, QString>::value &&
			!std::is_arithmetic<T>::value
		, int>::type = 0
	)
	{
		QString tmp;
		QDebug(&tmp).nospace() << a_Value;
		return a_FormatString.arg(tmp);
	}


	/** Recursively formats the given format string with the given arguments.
	First converts the value into a string representation using QDebug. */
	template <typename T, typename... OtherTs>
	QString format(const QString & a_FormatString, const T & a_Value, const OtherTs & ... a_Values)
	{
		auto msg = formatSingle(a_FormatString, a_Value);
		return format(msg, a_Values...);
	}


	/** Terminator for the format() expansion. */
	QString format(const QString & a_FormatString)
	{
		return a_FormatString;
	}
};





/** Descendant to be used for errors in the runtime. */
class RuntimeError: public Exception
{
public:
	using Exception::Exception;
};





/** Descendant to be used for errors that indicate a bug in the program. */
class LogicError: public Exception
{
public:
	using Exception::Exception;
};





#endif // EXCEPTION_H

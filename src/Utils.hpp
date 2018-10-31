#ifndef UTILS_H
#define UTILS_H





// Fwd:
class QString;
class QListWidget;
class QVariant;
class QByteArray;





namespace Utils
{




/** Returns the value, clamped to the range provided. */
template <typename T> T clamp(T a_Value, T a_Min, T a_Max)
{
	if (a_Value < a_Min)
	{
		return a_Min;
	}
	if (a_Value > a_Max)
	{
		return a_Max;
	}
	return a_Value;
}





/** Returns true if the specified value lies within the specified range. Min and Max are inclusive. */
template <typename T> bool isInRange(T a_Value, T a_Min, T a_Max)
{
	return (a_Value >= a_Min) && (a_Value <= a_Max);
}





/** Parses the string in the format "[[hh:]mm:]ss.fff" or "[hh:]mmm:ss.fff" into fractional seconds.
If the string cannot be parsed, isOK is set to false and returns -1. */
double parseTime(const QString & a_TimeString, bool & isOK);

/** Formats the time in seconds into string "mmm:ss".
Fractional second part is rounded and otherwise ignored. */
QString formatTime(double a_Seconds);

/** Formats the time in fractional seconds into string "mmm:ss.fff".
a_Decimals specifies the number of decimal places after the decimal dot ("f"). */
QString formatFractionalTime(double a_Seconds, int a_NumDecimals);

/** Finds the first item in the widget that has the specified data set as its Qt::UserRole,
and selects the item's row.
Returns true if item found, false otherwise. */
bool selectItemWithData(QListWidget * a_ListWidget, const QVariant & a_Data);

/** Returns a string that represents the data in hex. */
QString toHex(const QByteArray & a_Data);

};





#endif  // UTILS_H

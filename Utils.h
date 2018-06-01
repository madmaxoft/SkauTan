#ifndef UTILS_H
#define UTILS_H





// Fwd:
class QString;





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





/** Parses the string in the format "[[hh:]mm:]ss.fff" or "[hh:]mmm:ss.fff" into fractional seconds.
If the string cannot be parsed, isOK is set to false and returns -1. */
double parseTime(const QString & a_TimeString, bool & isOK);

/** Formats the time in seconds into string "mmm:ss".
Fractional second part is rounded and otherwise ignored. */
QString formatTime(double a_Seconds);

/** Formats the time in fractional seconds into string "mmm:ss.fff". */
QString formatFractionalTime(double a_Seconds);




};





#endif  // UTILS_H

#pragma once

#include <cmath>





// Fwd:
class QString;
class QListWidget;
class QVariant;
class QByteArray;
class QPainter;





namespace Utils
{




/** Returns the value, clamped to the range provided. */
template <typename T> T clamp(T aValue, T aMin, T aMax)
{
	if (aValue < aMin)
	{
		return aMin;
	}
	if (aValue > aMax)
	{
		return aMax;
	}
	return aValue;
}





/** Returns true if the specified value lies within the specified range. Min and Max are inclusive. */
template <typename T> bool isInRange(T aValue, T aMin, T aMax)
{
	return (aValue >= aMin) && (aValue <= aMax);
}





/** Performs floor and static_cast at the same time. */
template <typename T> T floor(double aValue)
{
	return static_cast<T>(std::floor(aValue));
}





/** Performs ceil and static_cast at the same time. */
template <typename T> T ceil(double aValue)
{
	return static_cast<T>(std::ceil(aValue));
}





/** Parses the string in the format "[[hh:]mm:]ss.fff" or "[hh:]mmm:ss.fff" into fractional seconds.
If the string cannot be parsed, isOK is set to false and returns -1. */
double parseTime(const QString & aTimeString, bool & isOK);

/** Formats the time in seconds into string "mmm:ss".
Fractional second part is rounded and otherwise ignored. */
QString formatTime(double aSeconds);

/** Formats the time in fractional seconds into string "mmm:ss.fff".
a_Decimals specifies the number of decimal places after the decimal dot ("f"). */
QString formatFractionalTime(double aSeconds, int aNumDecimals);

/** Finds the first item in the widget that has the specified data set as its Qt::UserRole,
and selects the item's row.
Returns true if item found, false otherwise. */
bool selectItemWithData(QListWidget * aListWidget, const QVariant & aData);

/** Returns a string that represents the data in hex. */
QString toHex(const QByteArray & aData);






/** RAII class for saving and restoring QPainter state. */
class QPainterSaver
{
public:

	/** Creates a new instance, saves the QPainter state. */
	QPainterSaver(QPainter & aPainter);

	/** Creates a new instance, saves the QPainter state. */
	QPainterSaver(QPainter * aPainter);

	/** Restores the QPainter state. */
	~QPainterSaver();


protected:

	/** The painter whose state will be restored on exit. */
	QPainter * mPainter;
};





};  // namespace Utils

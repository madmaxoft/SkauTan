#include "Utils.h"
#include <QString>
#include <QDebug>
#include <QListWidget>
#include <QByteArray>
#include <cmath>





namespace Utils
{





double parseTime(const QString & a_TimeString, bool & isOK)
{
	double res = 0, minutes = 0, hours = 0;
	int numColons = 0;
	bool isAfterDecimal = false;
	double decimalValue = 0.1;
	for (const auto & ch: a_TimeString.trimmed())
	{
		if (ch.isDigit())
		{
			if (isAfterDecimal)
			{
				res = res + ch.digitValue() * decimalValue;
				decimalValue /= 10;
			}
			else
			{
				res = res * 10 + ch.digitValue();
			}
		}
		else if (ch == ':')
		{
			if (numColons == 2)
			{
				qWarning() << "Too manu colons in the input string: " << a_TimeString;
				isOK = false;
				return -1;
			}
			if (isAfterDecimal)
			{
				qWarning() << "Colon after decimal is invalid; input string: " << a_TimeString;
				isOK = false;
				return -1;
			}
			numColons += 1;
			hours = minutes;
			minutes = res;
			res = 0;
		}
		else if ((ch == '.') || (ch == ','))  // Consider both comma and dot a decimal
		{
			if (isAfterDecimal)
			{
				qWarning() << "A second decimal found in the input string: " << a_TimeString;
				isOK = false;
				return -1;
			}
			isAfterDecimal = true;
		}
		else
		{
			qWarning() << "Unexpected character in the input string: " << ch << " (" << a_TimeString << ")";
			isOK = false;
			return -1;
		}
	}
	isOK = true;
	return res + 60 * (minutes + 60 * hours);
}





QString formatTime(double a_Seconds)
{
	auto seconds = static_cast<int>(std::round(a_Seconds));
	auto minutes = seconds / 60;
	return QString("%1:%2").arg(minutes).arg(QString::number(seconds % 60), 2, '0');
}





QString formatFractionalTime(double a_Seconds, int a_NumDecimals)
{
	auto seconds = static_cast<int>(std::floor(a_Seconds));
	auto minutes = seconds / 60;
	QString fractionalSecondsStr;
	auto fractionalSeconds = (a_Seconds - seconds) * 10;
	for (int i = 0; i < a_NumDecimals; ++i)
	{
		fractionalSecondsStr.append(QChar('0' + static_cast<int>(fractionalSeconds)));
		fractionalSeconds = (fractionalSeconds - static_cast<int>(fractionalSeconds)) * 10;
	}
	return QString("%1:%2.%3")
		.arg(minutes)
		.arg(QString::number(seconds % 60), 2,'0')
		.arg(fractionalSecondsStr);
}





bool selectItemWithData(QListWidget * a_ListWidget, const QVariant & a_Data)
{
	int numRows = a_ListWidget->count();
	for (int row = 0; row < numRows; ++row)
	{
		if (a_ListWidget->item(row)->data(Qt::UserRole) == a_Data)
		{
			a_ListWidget->setCurrentRow(row);
			return true;
		}
	}
	return false;
}





QString toHex(const QByteArray & a_Data)
{
	QString res;
	auto len = a_Data.size();
	res.resize(len * 2);
	static const char hexChar[] = "0123456789abcdef";
	for (auto i = 0; i < len; ++i)
	{
		auto val = a_Data[i];
		res[2 * i] = hexChar[(val >> 4) & 0x0f];
		res[2 * i + 1] = hexChar[val & 0x0f];
	}
	return res;
}





}

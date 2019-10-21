#include "Utils.hpp"
#include <QString>
#include <QDebug>
#include <QListWidget>
#include <QByteArray>
#include <QPainter>





namespace Utils
{





double parseTime(const QString & aTimeString, bool & isOK)
{
	double res = 0, minutes = 0, hours = 0;
	int numColons = 0;
	bool isAfterDecimal = false;
	double decimalValue = 0.1;
	for (const auto & ch: aTimeString.trimmed())
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
				qWarning() << "Too manu colons in the input string: " << aTimeString;
				isOK = false;
				return -1;
			}
			if (isAfterDecimal)
			{
				qWarning() << "Colon after decimal is invalid; input string: " << aTimeString;
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
				qWarning() << "A second decimal found in the input string: " << aTimeString;
				isOK = false;
				return -1;
			}
			isAfterDecimal = true;
		}
		else
		{
			qWarning() << "Unexpected character in the input string: " << ch << " (" << aTimeString << ")";
			isOK = false;
			return -1;
		}
	}
	isOK = true;
	return res + 60 * (minutes + 60 * hours);
}





QString formatTime(double aSeconds)
{
	auto seconds = static_cast<int>(std::round(aSeconds));
	auto minutes = seconds / 60;
	return QString("%1:%2").arg(minutes).arg(QString::number(seconds % 60), 2, '0');
}





QString formatFractionalTime(double aSeconds, int aNumDecimals)
{
	auto seconds = static_cast<int>(std::floor(aSeconds));
	auto minutes = seconds / 60;
	QString fractionalSecondsStr;
	auto fractionalSeconds = (aSeconds - seconds) * 10;
	for (int i = 0; i < aNumDecimals; ++i)
	{
		fractionalSecondsStr.append(QChar('0' + static_cast<int>(fractionalSeconds)));
		fractionalSeconds = (fractionalSeconds - static_cast<int>(fractionalSeconds)) * 10;
	}
	return QString("%1:%2.%3")
		.arg(minutes)
		.arg(QString::number(seconds % 60), 2,'0')
		.arg(fractionalSecondsStr);
}





bool selectItemWithData(QListWidget * aListWidget, const QVariant & aData)
{
	int numRows = aListWidget->count();
	for (int row = 0; row < numRows; ++row)
	{
		if (aListWidget->item(row)->data(Qt::UserRole) == aData)
		{
			aListWidget->setCurrentRow(row);
			return true;
		}
	}
	return false;
}





QString toHex(const QByteArray & aData)
{
	QString res;
	auto len = aData.size();
	res.resize(len * 2);
	static const char hexChar[] = "0123456789abcdef";
	for (auto i = 0; i < len; ++i)
	{
		auto val = aData[i];
		res[2 * i] = hexChar[(val >> 4) & 0x0f];
		res[2 * i + 1] = hexChar[val & 0x0f];
	}
	return res;
}





QPainterSaver::QPainterSaver(QPainter & aPainter):
	mPainter(&aPainter)
{
	aPainter.save();
}





QPainterSaver::QPainterSaver(QPainter * aPainter):
	mPainter(aPainter)
{
	aPainter->save();
}





QPainterSaver::~QPainterSaver()
{
	mPainter->restore();
}





}  // namespace Utils

#pragma once

#include <QDateTime>
#include <QVariant>
#include <QDebug>
#include "Exception.hpp"





/** Generic helper class that can provide additional functions for specific types, that will be accessible in
DatedOptional for that specific type. */
template <typename DatedOptionalT, typename T> class DatedOptionalExtra
{
public:
	bool isEmpty() const
	{
		auto self = static_cast<const DatedOptionalT *>(this);
		return !self->mIsPresent;
	}
};





/** For strings, the isEmpty() function returns true both when the optional itself is empty, or
the contained string is empty. */
template <typename DatedOptionalT> class DatedOptionalExtra<DatedOptionalT, QString>
{
public:
	bool isEmpty() const
	{
		auto self = static_cast<const DatedOptionalT *>(this);
		return !self->mIsPresent || self->mValue.isEmpty();
	}
};






/** A container that has an optional value and its last modification timestamp.
Used for storing values in the DB, together with their timestamp. */
template <typename T> class DatedOptional:
	public DatedOptionalExtra<DatedOptional<T>, T>  // Provides extra functions
{
	friend class DatedOptionalExtra<DatedOptional<T>, T>;

public:


	/** Constructs a valid DatedOptional out of the provided value.
	Sets the last modification time to epoch start. */
	DatedOptional(const T & aValue):
		mIsPresent(true),
		mValue(aValue)
	{
	}

	/** Constructs a valid DatedOptional out of the provided value and the last modification time. */
	DatedOptional(const T & aValue, const QDateTime & aLastModification):
		mIsPresent(true),
		mValue(aValue),
		mLastModification(aLastModification)
	{
	}

	/** Constructs an empty DatedOptional, with last modification time set to epoch start. */
	DatedOptional():
		mIsPresent(false)
	{
	}

	/** Constructs a copy of the source object. */
	DatedOptional(const DatedOptional<T> & aSrc):
		mIsPresent(aSrc.mIsPresent),
		mValue(aSrc.mValue),
		mLastModification(aSrc.mLastModification)
	{
	}

	/** Move-constructs an object based on the source. */
	DatedOptional(DatedOptional<T> && aSrc):
		mIsPresent(std::move(aSrc.mIsPresent)),
		mValue(std::move(aSrc.mValue)),
		mLastModification(std::move(aSrc.mLastModification))
	{
	}

	/** Constructs a DatedOptional from a variant and a timestamp.
	If the variant is empty or non-convertible, the resulting DatedOptional is empty. */
	DatedOptional(const QVariant & aValue, const QDateTime & aLastModification):
		mIsPresent(aValue.isValid() && !aValue.isNull() && aValue.canConvert<T>()),
		mValue(mIsPresent ? aValue.value<T>() : T()),
		mLastModification(aLastModification)
	{
	}

	constexpr explicit operator bool() const noexcept
	{
		return mIsPresent;
	}

	constexpr bool operator == (const DatedOptional<T> & aOther) const noexcept
	{
		return (
			(mIsPresent == aOther.mIsPresent) &&
			(
				!mIsPresent ||
				(mValue == aOther.mValue)
			)
		);
	}

	constexpr bool operator != (const DatedOptional<T> & aOther) const noexcept
	{
		return !(operator==(aOther));
	}

	constexpr bool isPresent() const noexcept
	{
		return mIsPresent;
	}

	constexpr T & value() &
	{
		if (mIsPresent)
		{
			return mValue;
		}
		else
		{
			throw LogicError("Optional value not present");
		}
	}

	constexpr const T & value() const &
	{
		if (mIsPresent)
		{
			return mValue;
		}
		else
		{
			throw LogicError("Optional value not present");
		}
	}


	/** Returns the stored value if it is present, or the specified value if not present. */
	T valueOr(const T & aDefault) const
	{
		if (mIsPresent)
		{
			return mValue;
		}
		else
		{
			return aDefault;
		}
	}


	/** Returns the stored value if it is present, or the default-contructed value if not present. */
	T valueOrDefault() const
	{
		if (mIsPresent)
		{
			return mValue;
		}
		else
		{
			return T();
		}
	}

	DatedOptional<T> & operator = (const DatedOptional<T> & aOther)
	{
		if (&aOther == this)
		{
			return *this;
		}
		mIsPresent = aOther.mIsPresent;
		mValue = aOther.mValue;
		mLastModification = aOther.mLastModification;
		return *this;
	}

	DatedOptional<T> & operator = (DatedOptional<T> && aOther)
	{
		mIsPresent = std::move(aOther.mIsPresent);
		mValue = std::move(aOther.mValue);
		mLastModification = std::move(aOther.mLastModification);
		return *this;
	}

	/** Assigns the new value to the container, and updates the last modification date to now. */
	DatedOptional<T> & operator = (const T & aValue)
	{
		mIsPresent = true;
		mValue = aValue;
		mLastModification = QDateTime::currentDateTime();
		return *this;
	}

	/** Move-assigns the new value to the container, and updates the last modification date to now. */
	DatedOptional<T> & operator = (T && aValue)
	{
		mIsPresent = true;
		mValue = std::move(aValue);
		mLastModification = QDateTime::currentDateTime();
		return *this;
	}

	DatedOptional<T> & operator = (const QVariant & aValue)
	{
		// Invalid variant produces an empty Optional:
		if (!aValue.isValid() || aValue.isNull())
		{
			mIsPresent = false;
			return *this;
		}

		// Check if conversion is possible:
		if (!aValue.canConvert<T>())
		{
			qDebug() << "Bad conversion from QVariant " << aValue;
			throw LogicError("Incompatible QVariant type");
		}

		// Produce a valid Optional:
		mIsPresent = true;
		mValue = aValue.value<T>();
		mLastModification = QDateTime::currentDateTime();
		return *this;
	}

	QVariant toVariant() const
	{
		return mIsPresent ? mValue : QVariant();
	}

	const QDateTime & lastModification() const { return mLastModification; }

	/** Resets the DatedOptional to "empty" state. */
	void reset()
	{
		mIsPresent = false;
	}


	/** If the data in the parameter is newer than what is contained, updates self with that data. */
	void updateIfNewer(const DatedOptional<T> & aOther)
	{
		if (aOther.mLastModification.isNull())
		{
			return;
		}
		if (mLastModification > aOther.mLastModification)
		{
			return;
		}
		mIsPresent = true;
		mValue = aOther.mValue;
		mLastModification = aOther.mLastModification;
	}

protected:

	/** If true, the value in mValue is valid.
	If false, the container should be considered empty (mValue is undefined). */
	bool mIsPresent;

	/** The actual value stored in the container, if any. */
	T mValue;

	/** Timestamp of the last modification. */
	QDateTime mLastModification;
};

#ifndef DATEDOPTIONAL_H
#define DATEDOPTIONAL_H





#include <QDateTime>
#include <QVariant>
#include <QDebug>
#include "Exception.h"





/** Generic helper class that can provide additional functions for specific types, that will be accessible in
DatedOptional for that specific type. */
template <typename DatedOptionalT, typename T> class DatedOptionalExtra
{
public:
	bool isEmpty() const
	{
		auto self = static_cast<const DatedOptionalT *>(this);
		return !self->m_IsPresent;
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
		return !self->m_IsPresent || self->m_Value.isEmpty();
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
	DatedOptional(const T & a_Value):
		m_IsPresent(true),
		m_Value(a_Value)
	{
	}

	/** Constructs a valid DatedOptional out of the provided value and the last modification time. */
	DatedOptional(const T & a_Value, const QDateTime & a_LastModification):
		m_IsPresent(true),
		m_Value(a_Value),
		m_LastModification(a_LastModification)
	{
	}

	/** Constructs an empty DatedOptional, with last modification time set to epoch start. */
	DatedOptional():
		m_IsPresent(false)
	{
	}

	/** Constructs a copy of the source object. */
	DatedOptional(const DatedOptional<T> & a_Src):
		m_IsPresent(a_Src.m_IsPresent),
		m_Value(a_Src.m_Value),
		m_LastModification(a_Src.m_LastModification)
	{
	}

	/** Move-constructs an object based on the source. */
	DatedOptional(DatedOptional<T> && a_Src):
		m_IsPresent(std::move(a_Src.m_IsPresent)),
		m_Value(std::move(a_Src.m_Value)),
		m_LastModification(std::move(a_Src.m_LastModification))
	{
	}

	/** Constructs a DatedOptional from a variant and a timestamp.
	If the variant is empty or non-convertible, the resulting DatedOptional is empty. */
	DatedOptional(const QVariant & a_Value, const QDateTime & a_LastModification):
		m_IsPresent(a_Value.isValid() && !a_Value.isNull() && a_Value.canConvert<T>()),
		m_Value(m_IsPresent ? a_Value.value<T>() : T()),
		m_LastModification(a_LastModification)
	{
	}

	constexpr explicit operator bool() const noexcept
	{
		return m_IsPresent;
	}

	constexpr bool isPresent() const noexcept
	{
		return m_IsPresent;
	}

	constexpr T & value() &
	{
		if (m_IsPresent)
		{
			return m_Value;
		}
		else
		{
			throw LogicError("Optional value not present");
		}
	}

	constexpr const T & value() const &
	{
		if (m_IsPresent)
		{
			return m_Value;
		}
		else
		{
			throw LogicError("Optional value not present");
		}
	}


	/** Returns the stored value if it is present, or the specified value if not present. */
	T valueOr(const T & a_Default) const
	{
		if (m_IsPresent)
		{
			return m_Value;
		}
		else
		{
			return a_Default;
		}
	}


	/** Returns the stored value if it is present, or the default-contructed value if not present. */
	T valueOrDefault() const
	{
		if (m_IsPresent)
		{
			return m_Value;
		}
		else
		{
			return T();
		}
	}

	DatedOptional<T> & operator = (const DatedOptional<T> & a_Other)
	{
		if (&a_Other == this)
		{
			return *this;
		}
		m_IsPresent = a_Other.m_IsPresent;
		m_Value = a_Other.m_Value;
		m_LastModification = a_Other.m_LastModification;
		return *this;
	}

	DatedOptional<T> & operator = (DatedOptional<T> && a_Other)
	{
		m_IsPresent = std::move(a_Other.m_IsPresent);
		m_Value = std::move(a_Other.m_Value);
		m_LastModification = std::move(a_Other.m_LastModification);
		return *this;
	}

	/** Assigns the new value to the container, and updates the last modification date to now. */
	DatedOptional<T> & operator = (const T & a_Value)
	{
		m_IsPresent = true;
		m_Value = a_Value;
		m_LastModification = QDateTime::currentDateTime();
		return *this;
	}

	/** Move-assigns the new value to the container, and updates the last modification date to now. */
	DatedOptional<T> & operator = (T && a_Value)
	{
		m_IsPresent = true;
		m_Value = std::move(a_Value);
		m_LastModification = QDateTime::currentDateTime();
		return *this;
	}

	DatedOptional<T> & operator = (const QVariant & a_Value)
	{
		// Invalid variant produces an empty Optional:
		if (!a_Value.isValid() || a_Value.isNull())
		{
			m_IsPresent = false;
			return *this;
		}

		// Check if conversion is possible:
		if (!a_Value.canConvert<T>())
		{
			qDebug() << "Bad conversion from QVariant " << a_Value;
			throw LogicError("Incompatible QVariant type");
		}

		// Produce a valid Optional:
		m_IsPresent = true;
		m_Value = a_Value.value<T>();
		m_LastModification = QDateTime::currentDateTime();
		return *this;
	}

	QVariant toVariant() const
	{
		return m_IsPresent ? m_Value : QVariant();
	}

	const QDateTime & lastModification() const { return m_LastModification; }

	/** Resets the DatedOptional to "empty" state. */
	void reset()
	{
		m_IsPresent = false;
	}


	/** If the data in the parameter is newer than what is contained, updates self with that data. */
	void updateIfNewer(const DatedOptional<T> & a_Other)
	{
		if (a_Other.m_LastModification.isNull())
		{
			return;
		}
		if (m_LastModification > a_Other.m_LastModification)
		{
			return;
		}
		m_IsPresent = true;
		m_Value = a_Other.m_Value;
		m_LastModification = a_Other.m_LastModification;
	}

protected:

	/** If true, the value in m_Value is valid.
	If false, the container should be considered empty (m_Value is undefined). */
	bool m_IsPresent;

	/** The actual value stored in the container, if any. */
	T m_Value;

	/** Timestamp of the last modification. */
	QDateTime m_LastModification;
};





#endif // DATEDOPTIONAL_H

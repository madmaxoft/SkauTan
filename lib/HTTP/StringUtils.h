#pragma once

#include <string>
#include <vector>





namespace Http { namespace Utils {





/** Splits the specified string on each occurrence of the specified separator. */
std::vector<std::string> stringSplit(const std::string & a_Input, const std::string & a_Separator);

/** Output the formatted text into string
Returns the formatted string by value. */
extern std::string printf(const char * format, ...);

/** Returns a lower-cased copy of the string */
extern std::string strToLower(const std::string & s);

/** Case-insensitive string comparison.
Returns 0 if the strings are the same, <0 if s1 < s2 and >0 if s1 > s2. */
extern int noCaseCompare(const std::string & s1, const std::string & s2);

/** Decodes a Base64-encoded string into the raw data */
extern std::string base64Decode(const std::string & a_Base64String);

/** Replaces all occurrences of char a_From inside a_String with char a_To. */
extern std::string replaceAllCharOccurrences(const std::string & a_String, char a_From, char a_To);

/** URL-Decodes the given string.
The first value specifies whether the decoding was successful.
The second value is the decoded string, if successful. */
extern std::pair<bool, std::string> urlDecode(const std::string & a_String);





/** Parses any integer type. Checks bounds and returns errors out of band. */
template <class T>
bool stringToInteger(const std::string & a_str, T & a_Num)
{
	size_t i = 0;
	bool positive = true;
	T result = 0;
	if (a_str[0] == '+')
	{
		i++;
	}
	else if (a_str[0] == '-')
	{
		i++;
		positive = false;
	}
	if (positive)
	{
		for (size_t size = a_str.size(); i < size; i++)
		{
			if ((a_str[i] < '0') || (a_str[i] > '9'))
			{
				return false;
			}
			if (std::numeric_limits<T>::max() / 10 < result)
			{
				return false;
			}
			result *= 10;
			T digit = static_cast<T>(a_str[i] - '0');
			if (std::numeric_limits<T>::max() - digit < result)
			{
				return false;
			}
			result += digit;
		}
	}
	else
	{
		// Unsigned result cannot be signed!
		if (!std::numeric_limits<T>::is_signed)
		{
			return false;
		}

		for (size_t size = a_str.size(); i < size; i++)
		{
			if ((a_str[i] < '0') || (a_str[i] > '9'))
			{
				return false;
			}
			if (std::numeric_limits<T>::min() / 10 > result)
			{
				return false;
			}
			result *= 10;
			T digit = static_cast<T>(a_str[i] - '0');
			if (std::numeric_limits<T>::min() + digit > result)
			{
				return false;
			}
			result -= digit;
		}
	}
	a_Num = result;
	return true;
}





}}  // namespace Http::Utils

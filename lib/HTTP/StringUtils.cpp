#include "StringUtils.h"
#include <algorithm>
#include <cassert>
#include <cstdarg>





#define ARRAYCOUNT(X) (sizeof(X) / sizeof(*(X)))





namespace Http { namespace Utils {





/** Returns the value of the single hex digit.
Returns 0xff on failure. */
static unsigned char hexToDec(char a_HexChar)
{
	switch (a_HexChar)
	{
		case '0': return 0;
		case '1': return 1;
		case '2': return 2;
		case '3': return 3;
		case '4': return 4;
		case '5': return 5;
		case '6': return 6;
		case '7': return 7;
		case '8': return 8;
		case '9': return 9;
		case 'a': return 10;
		case 'b': return 11;
		case 'c': return 12;
		case 'd': return 13;
		case 'e': return 14;
		case 'f': return 15;
		case 'A': return 10;
		case 'B': return 11;
		case 'C': return 12;
		case 'D': return 13;
		case 'E': return 14;
		case 'F': return 15;
	}
	return 0xff;
}





std::vector<std::string> stringSplit(const std::string & a_Input, const std::string & a_Separator)
{
	std::vector<std::string> results;
	size_t cutAt = 0;
	size_t prev = 0;
	while ((cutAt = a_Input.find_first_of(a_Separator, prev)) != std::string::npos)
	{
		results.push_back(a_Input.substr(prev, cutAt - prev));
		prev = cutAt + 1;
	}
	if (prev < a_Input.length())
	{
		results.push_back(a_Input.substr(prev));
	}
	return results;
}





static std::string & appendVPrintf(std::string & str, const char * format, va_list args)
{
	assert(format != nullptr);

	char buffer[2048];
	int len;
	#ifdef va_copy
	va_list argsCopy;
	va_copy(argsCopy, args);
	#else
	#define argsCopy args
	#endif
	#ifdef _MSC_VER
	// MS CRT provides secure printf that doesn't behave like in the C99 standard
	if ((len = _vsnprintf_s(buffer, ARRAYCOUNT(buffer), _TRUNCATE, format, argsCopy)) != -1)
	#else  // _MSC_VER
	if ((len = vsnprintf(buffer, ARRAYCOUNT(buffer), format, argsCopy)) < static_cast<int>(ARRAYCOUNT(buffer)))
	#endif  // else _MSC_VER
	{
		// The result did fit into the static buffer
		#ifdef va_copy
		va_end(argsCopy);
		#endif
		str.append(buffer, static_cast<size_t>(len));
		return str;
	}
	#ifdef va_copy
	va_end(argsCopy);
	#endif

	// The result did not fit into the static buffer, use a dynamic buffer:
	#ifdef _MSC_VER
	// for MS CRT, we need to calculate the result length
	len = _vscprintf(format, args);
	if (len == -1)
	{
		return str;
	}
	#endif  // _MSC_VER

	// Allocate a buffer and printf into it:
	#ifdef va_copy
	va_copy(argsCopy, args);
	#endif
	std::vector<char> Buffer(static_cast<size_t>(len) + 1);
	#ifdef _MSC_VER
	vsprintf_s(&(Buffer.front()), Buffer.size(), format, argsCopy);
	#else  // _MSC_VER
	vsnprintf(&(Buffer.front()), Buffer.size(), format, argsCopy);
	#endif  // else _MSC_VER
	str.append(&(Buffer.front()), Buffer.size() - 1);
	#ifdef va_copy
	va_end(argsCopy);
	#endif
	return str;
}





std::string printf(const char * format, ...)
{
	std::string res;
	va_list args;
	va_start(args, format);
	appendVPrintf(res, format, args);
	va_end(args);
	return res;
}





std::string strToLower(const std::string & s)
{
	std::string res;
	res.resize(s.size());
	std::transform(s.begin(), s.end(), res.begin(), ::tolower);
	return res;
}





int noCaseCompare(const std::string & s1, const std::string & s2)
{
	#ifdef _MSC_VER
		// MSVC has stricmp that compares case-insensitive:
		return _stricmp(s1.c_str(), s2.c_str());
	#else
		// Do it the hard way - convert both strings to lowercase:
		return StrToLower(s1).compare(StrToLower(s2));
	#endif  // else _MSC_VER
}





/** Converts one Hex character in a Base64 encoding into the data value */
static inline int UnBase64(char c)
{
	if ((c >='A') && (c <= 'Z'))
	{
		return c - 'A';
	}
	if ((c >='a') && (c <= 'z'))
	{
		return c - 'a' + 26;
	}
	if ((c >= '0') && (c <= '9'))
	{
		return c - '0' + 52;
	}
	if (c == '+')
	{
		return 62;
	}
	if (c == '/')
	{
		return 63;
	}
	if (c == '=')
	{
		return -1;
	}
	return -2;
}





std::string base64Decode(const std::string & a_Base64String)
{
	std::string res;
	size_t i, len = a_Base64String.size();
	size_t o;
	int c;
	res.resize((len * 4) / 3 + 5, 0);  // Approximate the upper bound on the result length
	for (o = 0, i = 0; i < len; i++)
	{
		c = UnBase64(a_Base64String[i]);
		if (c >= 0)
		{
			switch (o & 7)
			{
				case 0: res[o >> 3] |= (c << 2); break;
				case 6: res[o >> 3] |= (c >> 4); res[(o >> 3) + 1] |= (c << 4); break;
				case 4: res[o >> 3] |= (c >> 2); res[(o >> 3) + 1] |= (c << 6); break;
				case 2: res[o >> 3] |= c; break;
			}
			o += 6;
		}
		if (c == -1)
		{
			// Error while decoding, invalid input. Return as much as we've decoded:
			res.resize(o >> 3);
			return res;
		}
	}
	res.resize(o >> 3);
	return res;
}





std::string replaceAllCharOccurrences(const std::string & a_String, char a_From, char a_To)
{
	std::string res(a_String);
	std::replace(res.begin(), res.end(), a_From, a_To);
	return res;
}





static std::string UnicodeCharToUtf8(unsigned a_UnicodeChar)
{
	if (a_UnicodeChar < 0x80)
	{
		return std::string{static_cast<char>(a_UnicodeChar)};
	}
	else if (a_UnicodeChar < 0x800)
	{
		return std::string
		{
			static_cast<char>(192 + a_UnicodeChar / 64),
			static_cast<char>(128 + a_UnicodeChar % 64),
		};
	}
	else if (a_UnicodeChar - 0xd800 < 0x800)
	{
		// Error
		return std::string();
	}
	else if (a_UnicodeChar < 0x10000)
	{
		return std::string
		{
			static_cast<char>(224 + a_UnicodeChar / 4096),
			static_cast<char>(128 + (a_UnicodeChar / 64) % 64),
			static_cast<char>(128 + a_UnicodeChar % 64)
		};
	}
	else if (a_UnicodeChar < 0x110000)
	{
		return std::string
		{
			static_cast<char>(240 + a_UnicodeChar / 262144),
			static_cast<char>(128 + (a_UnicodeChar / 4096) % 64),
			static_cast<char>(128 + (a_UnicodeChar / 64) % 64),
			static_cast<char>(128 + a_UnicodeChar % 64),
		};
	}
	else
	{
		// Error
		return std::string();
	}
}





std::pair<bool, std::string> urlDecode(const std::string & a_Text)
{
	std::string res;
	auto len = a_Text.size();
	res.reserve(len);
	for (size_t i = 0; i < len; i++)
	{
		if (a_Text[i] == '+')
		{
			res.push_back(' ');
			continue;
		}
		if (a_Text[i] != '%')
		{
			res.push_back(a_Text[i]);
			continue;
		}
		if (i + 1 >= len)
		{
			// String too short for an encoded value
			return std::make_pair(false, std::string());
		}
		if ((a_Text[i + 1] == 'u') || (a_Text[i + 1] == 'U'))
		{
			// Unicode char "%u0xxxx"
			if (i + 6 >= len)
			{
				return std::make_pair(false, std::string());
			}
			if (a_Text[i + 2] != '0')
			{
				return std::make_pair(false, std::string());
			}
			unsigned v1 = hexToDec(a_Text[i + 3]);
			unsigned v2 = hexToDec(a_Text[i + 4]);
			unsigned v3 = hexToDec(a_Text[i + 5]);
			unsigned v4 = hexToDec(a_Text[i + 6]);
			if ((v1 == 0xff) || (v2 == 0xff) || (v4 == 0xff) || (v3 == 0xff))
			{
				// Invalid hex numbers
				return std::make_pair(false, std::string());
			}
			res.append(UnicodeCharToUtf8((v1 << 12) | (v2 << 8) | (v3 << 4) | v4));
			i = i + 6;
		}
		else
		{
			// Regular char "%xx":
			if (i + 2 >= len)
			{
				return std::make_pair(false, std::string());
			}
			auto v1 = hexToDec(a_Text[i + 1]);
			auto v2 = hexToDec(a_Text[i + 2]);
			if ((v1 == 0xff) || (v2 == 0xff))
			{
				// Invalid hex numbers
				return std::make_pair(false, std::string());
			}
			res.push_back(static_cast<char>((v1 << 4) | v2));
			i = i + 2;
		}
	}  // for i - a_Text[i]
	return std::make_pair(true, res);
}





}}  // namespace Http::Utils

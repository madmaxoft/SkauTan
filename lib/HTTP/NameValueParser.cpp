#include "NameValueParser.h"
#include <cassert>





namespace Http {





NameValueParser::NameValueParser(bool a_AllowsKeyOnly) :
	m_State(psKeySpace),
	m_AllowsKeyOnly(a_AllowsKeyOnly)
{
}





NameValueParser::NameValueParser(const char * a_Data, size_t a_Size, bool a_AllowsKeyOnly) :
	m_State(psKeySpace),
	m_AllowsKeyOnly(a_AllowsKeyOnly)
{
	parse(a_Data, a_Size);
}





void NameValueParser::parse(const char * a_Data, size_t a_Size)
{
	assert(m_State != psFinished);  // Calling Parse() after Finish() is wrong!

	size_t Last = 0;
	for (size_t i = 0; i < a_Size;)
	{
		switch (m_State)
		{
			case psInvalid:
			case psFinished:
			{
				return;
			}

			case psKeySpace:
			{
				// Skip whitespace until a non-whitespace is found, then start the key:
				while ((i < a_Size) && (a_Data[i] <= ' '))
				{
					i++;
				}
				if ((i < a_Size) && (a_Data[i] > ' '))
				{
					m_State = psKey;
					Last = i;
				}
				break;
			}

			case psKey:
			{
				// Read the key until whitespace or an equal sign:
				while (i < a_Size)
				{
					if (a_Data[i] == '=')
					{
						m_CurrentKey.append(a_Data + Last, i - Last);
						i++;
						Last = i;
						m_State = psEqual;
						break;
					}
					else if (a_Data[i] <= ' ')
					{
						m_CurrentKey.append(a_Data + Last, i - Last);
						i++;
						Last = i;
						m_State = psEqualSpace;
						break;
					}
					else if (a_Data[i] == ';')
					{
						if (!m_AllowsKeyOnly)
						{
							m_State = psInvalid;
							return;
						}
						m_CurrentKey.append(a_Data + Last, i - Last);
						i++;
						Last = i;
						(*this)[m_CurrentKey] = "";
						m_CurrentKey.clear();
						m_State = psKeySpace;
						break;
					}
					else if ((a_Data[i] == '\"') || (a_Data[i] == '\''))
					{
						m_State = psInvalid;
						return;
					}
					i++;
				}  // while (i < a_Size)
				if (i == a_Size)
				{
					// Still the key, ran out of data to parse, store the part of the key parsed so far:
					m_CurrentKey.append(a_Data + Last, a_Size - Last);
					return;
				}
				break;
			}

			case psEqualSpace:
			{
				// The space before the expected equal sign; the current key is already assigned
				while (i < a_Size)
				{
					if (a_Data[i] == '=')
					{
						m_State = psEqual;
						i++;
						Last = i;
						break;
					}
					else if (a_Data[i] == ';')
					{
						// Key-only
						if (!m_AllowsKeyOnly)
						{
							m_State = psInvalid;
							return;
						}
						i++;
						Last = i;
						(*this)[m_CurrentKey] = "";
						m_CurrentKey.clear();
						m_State = psKeySpace;
						break;
					}
					else if (a_Data[i] > ' ')
					{
						m_State = psInvalid;
						return;
					}
					i++;
				}  // while (i < a_Size)
				break;
			}  // case psEqualSpace

			case psEqual:
			{
				// just parsed the equal-sign
				while (i < a_Size)
				{
					if (a_Data[i] == ';')
					{
						if (!m_AllowsKeyOnly)
						{
							m_State = psInvalid;
							return;
						}
						i++;
						Last = i;
						(*this)[m_CurrentKey] = "";
						m_CurrentKey.clear();
						m_State = psKeySpace;
						break;
					}
					else if (a_Data[i] == '\"')
					{
						i++;
						Last = i;
						m_State = psValueInDQuotes;
						break;
					}
					else if (a_Data[i] == '\'')
					{
						i++;
						Last = i;
						m_State = psValueInSQuotes;
						break;
					}
					else
					{
						m_CurrentValue.push_back(a_Data[i]);
						i++;
						Last = i;
						m_State = psValueRaw;
						break;
					}
				}  // while (i < a_Size)
				break;
			}  // case psEqual

			case psValueInDQuotes:
			{
				while (i < a_Size)
				{
					if (a_Data[i] == '\"')
					{
						m_CurrentValue.append(a_Data + Last, i - Last);
						(*this)[m_CurrentKey] = m_CurrentValue;
						m_CurrentKey.clear();
						m_CurrentValue.clear();
						m_State = psAfterValue;
						i++;
						Last = i;
						break;
					}
					i++;
				}  // while (i < a_Size)
				if (i == a_Size)
				{
					m_CurrentValue.append(a_Data + Last, a_Size - Last);
				}
				break;
			}  // case psValueInDQuotes

			case psValueInSQuotes:
			{
				while (i < a_Size)
				{
					if (a_Data[i] == '\'')
					{
						m_CurrentValue.append(a_Data + Last, i - Last);
						(*this)[m_CurrentKey] = m_CurrentValue;
						m_CurrentKey.clear();
						m_CurrentValue.clear();
						m_State = psAfterValue;
						i++;
						Last = i;
						break;
					}
					i++;
				}  // while (i < a_Size)
				if (i == a_Size)
				{
					m_CurrentValue.append(a_Data + Last, a_Size - Last);
				}
				break;
			}  // case psValueInSQuotes

			case psValueRaw:
			{
				while (i < a_Size)
				{
					if (a_Data[i] == ';')
					{
						m_CurrentValue.append(a_Data + Last, i - Last);
						(*this)[m_CurrentKey] = m_CurrentValue;
						m_CurrentKey.clear();
						m_CurrentValue.clear();
						m_State = psKeySpace;
						i++;
						Last = i;
						break;
					}
					i++;
				}
				if (i == a_Size)
				{
					m_CurrentValue.append(a_Data + Last, a_Size - Last);
				}
				break;
			}  // case psValueRaw

			case psAfterValue:
			{
				// Between the closing DQuote or SQuote and the terminating semicolon
				while (i < a_Size)
				{
					if (a_Data[i] == ';')
					{
						m_State = psKeySpace;
						i++;
						Last = i;
						break;
					}
					else if (a_Data[i] < ' ')
					{
						i++;
						continue;
					}
					m_State = psInvalid;
					return;
				}  // while (i < a_Size)
				break;
			}
		}  // switch (m_State)
	}  // for i - a_Data[]
}





bool NameValueParser::finish(void)
{
	switch (m_State)
	{
		case psInvalid:
		{
			return false;
		}
		case psFinished:
		{
			return true;
		}
		case psKey:
		case psEqualSpace:
		case psEqual:
		{
			if ((m_AllowsKeyOnly) && !m_CurrentKey.empty())
			{
				(*this)[m_CurrentKey] = "";
				m_State = psFinished;
				return true;
			}
			m_State = psInvalid;
			return false;
		}
		case psValueRaw:
		{
			(*this)[m_CurrentKey] = m_CurrentValue;
			m_State = psFinished;
			return true;
		}
		case psValueInDQuotes:
		case psValueInSQuotes:
		{
			// Missing the terminating quotes, this is an error
			m_State = psInvalid;
			return false;
		}
		case psKeySpace:
		case psAfterValue:
		{
			m_State = psFinished;
			return true;
		}
	}
	assert(!"Unhandled parser state!");
	#ifndef __clang__
		return false;
	#endif
}





}  // namespace Http

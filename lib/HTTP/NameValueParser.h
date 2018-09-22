#pragma once

#include <string>
#include <map>





namespace Http {





/** Parses strings in the "name=value;name2=value2" format into a stringmap.
The data is pushed into the parser and it keeps internal parsing state, until finish() is called. */
class NameValueParser:
	public std::map<std::string, std::string>
{
public:

	/** Creates an empty parser */
	NameValueParser(bool a_AllowsKeyOnly = true);

	/** Creates an empty parser, then parses the data given. Doesn't call Finish(), so more data can be parsed later */
	NameValueParser(const char * a_Data, size_t a_Size, bool a_AllowsKeyOnly = true);

	/** Parses the data given */
	void parse(const char * a_Data, size_t a_Size);

	/** Notifies the parser that no more data will be coming. Returns true if the parser state is valid */
	bool finish(void);

	/** Returns true if the data parsed so far was valid */
	bool isValid(void) const { return (m_State != psInvalid); }

	/** Returns true if the parser expects no more data */
	bool isFinished(void) const { return ((m_State == psInvalid) || (m_State == psFinished)); }


protected:

	enum State
	{
		psKeySpace,        ///< Parsing the space in front of the next key
		psKey,             ///< Currently adding more chars to the key in m_CurrentKey
		psEqualSpace,      ///< Space after m_CurrentKey
		psEqual,           ///< Just parsed the = sign after a name
		psValueInSQuotes,  ///< Just parsed a Single-quote sign after the Equal sign
		psValueInDQuotes,  ///< Just parsed a Double-quote sign after the Equal sign
		psValueRaw,        ///< Just parsed a raw value without a quote
		psAfterValue,      ///< Just finished parsing the value, waiting for semicolon or data end
		psInvalid,         ///< The parser has encountered an invalid input; further parsing is skipped
		psFinished,        ///< The parser has already been instructed to finish and doesn't expect any more data
	};


	/** The current state of the parser */
	State m_State;

	/** If true, the parser will accept keys without an equal sign and the value */
	bool m_AllowsKeyOnly;

	/** Buffer for the current Key */
	std::string m_CurrentKey;

	/** Buffer for the current Value; */
	std::string m_CurrentValue;
} ;





}  // namespace Http

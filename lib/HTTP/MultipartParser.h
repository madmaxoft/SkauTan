#pragma once

#include <string>
#include "EnvelopeParser.h"





namespace Http {





/** Implements a SAX-like parser for MIME-encoded messages.
The user of this class provides callbacks, then feeds data into this class, and it calls the various
callbacks upon encountering the data. */
class MultipartParser:
	protected EnvelopeParser::Callbacks
{
public:

	class Callbacks
	{
	public:
		// Force a virtual destructor in descendants:
		virtual ~Callbacks() {}

		/** Called when a new part starts */
		virtual void onPartStart(void) = 0;

		/** Called when a complete header line is received for a part */
		virtual void onPartHeader(const std::string & a_Key, const std::string & a_Value) = 0;

		/** Called when body for a part is received */
		virtual void onPartData(const char * a_Data, size_t a_Size) = 0;

		/** Called when the current part ends */
		virtual void onPartEnd(void) = 0;
	};

	/** Creates the parser, expects to find the boundary in a_ContentType */
	MultipartParser(const std::string & a_ContentType, Callbacks & a_Callbacks);

	/** Parses more incoming data */
	void parse(const char * a_Data, size_t a_Size);


protected:

	/** The callbacks to call for various parsing events */
	Callbacks & m_Callbacks;

	/** True if the data parsed so far is valid; if false, further parsing is skipped */
	bool m_IsValid;

	/** Parser for each part's envelope */
	EnvelopeParser m_EnvelopeParser;

	/** Buffer for the incoming data until it is parsed */
	std::string m_IncomingData;

	/** The boundary, excluding both the initial "--" and the terminating CRLF */
	std::string m_Boundary;

	/** Set to true if some data for the current part has already been signalized to m_Callbacks. Used for proper CRLF inserting. */
	bool m_HasHadData;


	/** Parse one line of incoming data. The CRLF has already been stripped from a_Data / a_Size */
	void parseLine(const char * a_Data, size_t a_Size);

	/** Parse one line of incoming data in the headers section of a part. The CRLF has already been stripped from a_Data / a_Size */
	void parseHeaderLine(const char * a_Data, size_t a_Size);

	// EnvelopeParser::Callbacks overrides:
	virtual void onHeaderLine(const std::string & a_Key, const std::string & a_Value) override;
} ;





}  // namespace Http

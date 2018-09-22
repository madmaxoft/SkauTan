#pragma once

#include "EnvelopeParser.h"
#include "TransferEncodingParser.h"





namespace Http {





/** Parses HTTP messages (request or response) being pushed into the parser, and reports the individual parts
via callbacks. */
class MessageParser:
	protected EnvelopeParser::Callbacks,
	protected TransferEncodingParser::Callbacks
{
public:

	class Callbacks
	{
	public:
		// Force a virtual destructor in descendants:
		virtual ~Callbacks() {}

		/** Called when an error has occured while parsing. */
		virtual void onError(const std::string & a_ErrorDescription) = 0;

		/** Called when the first line of the request or response is fully parsed.
		Doesn't check the validity of the line, only extracts the first complete line. */
		virtual void onFirstLine(const std::string & a_FirstLine) = 0;

		/** Called when a single header line is parsed. */
		virtual void onHeaderLine(const std::string & a_Key, const std::string & a_Value) = 0;

		/** Called when all the headers have been parsed. */
		virtual void onHeadersFinished(void) = 0;

		/** Called for each chunk of the incoming body data. */
		virtual void onBodyData(const void * a_Data, size_t a_Size) = 0;

		/** Called when the entire body has been reported by OnBodyData(). */
		virtual void onBodyFinished(void) = 0;
	};


	/** Creates a new parser instance that will use the specified callbacks for reporting. */
	MessageParser(Callbacks & a_Callbacks);

	/** Parses the incoming data and calls the appropriate callbacks.
	Returns the number of bytes consumed or std::string::npos number for error. */
	size_t parse(const char * a_Data, size_t a_Size);

	/** Called when the server indicates no more data will be sent (HTTP 1.0 socket closed).
	Finishes all parsing and calls apropriate callbacks (error if incomplete response). */
	void finish(void);

	/** Returns true if the entire response has been already parsed. */
	bool isFinished(void) const { return m_IsFinished; }

	/** Resets the parser to the initial state, so that a new request can be parsed. */
	void reset(void);


protected:

	/** The callbacks used for reporting. */
	Callbacks & m_Callbacks;

	/** Set to true if an error has been encountered by the parser. */
	bool m_HasHadError;

	/** True if the response has been fully parsed. */
	bool m_IsFinished;

	/** The complete first line of the response. Empty if not parsed yet. */
	std::string m_FirstLine;

	/** Buffer for the incoming data until the status line is parsed. */
	std::string m_Buffer;

	/** Parser for the envelope data (headers) */
	EnvelopeParser m_EnvelopeParser;

	/** The specific parser for the transfer encoding used by this response. */
	TransferEncodingParserPtr m_TransferEncodingParser;

	/** The transfer encoding to be used by the parser.
	Filled while parsing headers, used when headers are finished. */
	std::string m_TransferEncoding;

	/** The content length, parsed from the headers, if available.
	Unused for chunked encoding.
	Filled while parsing headers, used when headers are finished. */
	size_t m_ContentLength;


	/** Parses the first line out of m_Buffer.
	Removes the first line from m_Buffer, if appropriate.
	Returns the number of bytes consumed out of m_Buffer, or std::string::npos number for error. */
	size_t parseFirstLine(void);

	/** Parses the message body.
	Processes transfer encoding and calls the callbacks for body data.
	Returns the number of bytes consumed or std::string::npos number for error. */
	size_t parseBody(const char * a_Data, size_t a_Size);

	/** Called internally when the headers-parsing has just finished. */
	void headersFinished(void);

	// EnvelopeParser::Callbacks overrides:
	virtual void onHeaderLine(const std::string & a_Key, const std::string & a_Value) override;

	// TransferEncodingParser::Callbacks overrides:
	virtual void onError(const std::string & a_ErrorDescription) override;
	virtual void onBodyData(const void * a_Data, size_t a_Size) override;
	virtual void onBodyFinished(void) override;
};





}  // namespace Http

#pragma once

#include <memory>
#include <string>





namespace Http {





// fwd:
class TransferEncodingParser;
typedef std::shared_ptr<TransferEncodingParser> TransferEncodingParserPtr;





/** Used as both the interface that all the parsers share and the (static) factory creating such parsers.
The parsers convert data from raw incoming stream to a processed steram, based on the Transfer-Encoding. */
class TransferEncodingParser
{
public:

	class Callbacks
	{
	public:
		// Force a virtual destructor in descendants
		virtual ~Callbacks() {}

		/** Called when an error has occured while parsing. */
		virtual void onError(const std::string & a_ErrorDescription) = 0;

		/** Called for each chunk of the incoming body data. */
		virtual void onBodyData(const void * a_Data, size_t a_Size) = 0;

		/** Called when the entire body has been reported by OnBodyData(). */
		virtual void onBodyFinished(void) = 0;
	};


	////////////////////////////////////////////////////////////////////////////////
	// The interface:

	// Force a virtual destructor in all descendants
	virtual ~TransferEncodingParser() {}

	/** Parses the incoming data and calls the appropriate callbacks.
	Returns the number of bytes from the end of a_Data that is already not part of this message (if the parser can detect it).
	Returns std::string::npos on an error. */
	virtual size_t parse(const char * a_Data, size_t a_Size) = 0;

	/** To be called when the stream is terminated from the source (connection closed).
	Flushes any buffers and calls appropriate callbacks. */
	virtual void finish(void) = 0;


	////////////////////////////////////////////////////////////////////////////////
	// The factory:

	/** Creates a new parser for the specified encoding (case-insensitive).
	If the encoding is not known, returns a nullptr.
	a_ContentLength is the length of the content, received in a Content-Length header, it is used for
	the Identity encoding, it is ignored for the Chunked encoding. */
	static TransferEncodingParserPtr create(
		Callbacks & a_Callbacks,
		const std::string & a_TransferEncoding,
		size_t a_ContentLength
	);


protected:

	/** The callbacks used to report progress. */
	Callbacks & m_Callbacks;


	TransferEncodingParser(Callbacks & a_Callbacks):
		m_Callbacks(a_Callbacks)
	{
	}
};





}  // namespace Http

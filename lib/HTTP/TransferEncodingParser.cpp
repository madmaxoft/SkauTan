
// TransferEncodingParser.cpp

// Implements the TransferEncodingParser class and its descendants representing the parsers for the various transfer encodings (chunked etc.)

#include "TransferEncodingParser.h"
#include <cassert>
#include <algorithm>
#include "EnvelopeParser.h"
#include "StringUtils.h"





namespace Http {





////////////////////////////////////////////////////////////////////////////////
// ChunkedTEParser:

class ChunkedTEParser:
	public TransferEncodingParser,
	public EnvelopeParser::Callbacks
{
	typedef TransferEncodingParser Super;

public:
	ChunkedTEParser(Super::Callbacks & a_Callbacks):
		Super(a_Callbacks),
		m_State(psChunkLength),
		m_ChunkDataLengthLeft(0),
		m_TrailerParser(*this)
	{
	}


protected:
	enum eState
	{
		psChunkLength,         ///< Parsing the chunk length hex number
		psChunkLengthTrailer,  ///< Any trailer (chunk extension) specified after the chunk length
		psChunkLengthLF,       ///< The LF character after the CR character terminating the chunk length
		psChunkData,           ///< Relaying chunk data
		psChunkDataCR,         ///< Skipping the extra CR character after chunk data
		psChunkDataLF,         ///< Skipping the extra LF character after chunk data
		psTrailer,             ///< Received an empty chunk, parsing the trailer (through the envelope parser)
		psFinished,            ///< The parser has finished parsing, either successfully or with an error
	};

	/** The current state of the parser (parsing chunk length / chunk data). */
	eState m_State;

	/** Number of bytes that still belong to the chunk currently being parsed.
	When in psChunkLength, the value is the currently parsed length digits. */
	size_t m_ChunkDataLengthLeft;

	/** The parser used for the last (empty) chunk's trailer data */
	EnvelopeParser m_TrailerParser;


	/** Calls the onError callback and sets parser state to finished. */
	void error(const std::string & a_ErrorMsg)
	{
		m_State = psFinished;
		m_Callbacks.onError(a_ErrorMsg);
	}


	/** Parses the incoming data, the current state is psChunkLength.
	Stops parsing when either the chunk length has been read, or there is no more data in the input.
	Returns the number of bytes consumed from the input, or std::string::npos on error (calls the Error handler). */
	size_t parseChunkLength(const char * a_Data, size_t a_Size)
	{
		// Expected input: <hexnumber>[;<trailer>]<CR><LF>
		// Only the hexnumber is parsed into m_ChunkDataLengthLeft, the rest is postponed into psChunkLengthTrailer or psChunkLengthLF
		for (size_t i = 0; i < a_Size; i++)
		{
			switch (a_Data[i])
			{
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				{
					m_ChunkDataLengthLeft = m_ChunkDataLengthLeft * 16 + static_cast<decltype(m_ChunkDataLengthLeft)>(a_Data[i] - '0');
					break;
				}
				case 'a':
				case 'b':
				case 'c':
				case 'd':
				case 'e':
				case 'f':
				{
					m_ChunkDataLengthLeft = m_ChunkDataLengthLeft * 16 + static_cast<decltype(m_ChunkDataLengthLeft)>(a_Data[i] - 'a' + 10);
					break;
				}
				case 'A':
				case 'B':
				case 'C':
				case 'D':
				case 'E':
				case 'F':
				{
					m_ChunkDataLengthLeft = m_ChunkDataLengthLeft * 16 + static_cast<decltype(m_ChunkDataLengthLeft)>(a_Data[i] - 'A' + 10);
					break;
				}
				case '\r':
				{
					m_State = psChunkLengthLF;
					return i + 1;
				}
				case ';':
				{
					m_State = psChunkLengthTrailer;
					return i + 1;
				}
				default:
				{
					error(Utils::printf("Invalid character in chunk length line: 0x%x", a_Data[i]));
					return std::string::npos;
				}
			}  // switch (a_Data[i])
		}  // for i - a_Data[]
		return a_Size;
	}


	/** Parses the incoming data, the current state is psChunkLengthTrailer.
	Stops parsing when either the chunk length trailer has been read, or there is no more data in the input.
	Returns the number of bytes consumed from the input, or std::string::npos on error (calls the Error handler). */
	size_t parseChunkLengthTrailer(const char * a_Data, size_t a_Size)
	{
		// Expected input: <trailer><CR><LF>
		// The LF itself is not parsed, it is instead postponed into psChunkLengthLF
		for (size_t i = 0; i < a_Size; i++)
		{
			switch (a_Data[i])
			{
				case '\r':
				{
					m_State = psChunkLengthLF;
					return i;
				}
				default:
				{
					if (a_Data[i] < 32)
					{
						// Only printable characters are allowed in the trailer
						error(Utils::printf("Invalid character in chunk length line: 0x%x", a_Data[i]));
						return std::string::npos;
					}
				}
			}  // switch (a_Data[i])
		}  // for i - a_Data[]
		return a_Size;
	}


	/** Parses the incoming data, the current state is psChunkLengthLF.
	Only the LF character is expected, if found, moves to psChunkData, otherwise issues an error.
	If the chunk length that just finished reading is equal to 0, signals the end of stream (via psTrailer).
	Returns the number of bytes consumed from the input, or std::string::npos on error (calls the Error handler). */
	size_t parseChunkLengthLF(const char * a_Data, size_t a_Size)
	{
		// Expected input: <LF>
		if (a_Size == 0)
		{
			return 0;
		}
		if (a_Data[0] == '\n')
		{
			if (m_ChunkDataLengthLeft == 0)
			{
				m_State = psTrailer;
			}
			else
			{
				m_State = psChunkData;
			}
			return 1;
		}
		error(Utils::printf("Invalid character past chunk length's CR: 0x%x", a_Data[0]));
		return std::string::npos;
	}


	/** Consumes as much chunk data from the input as possible.
	Returns the number of bytes consumed from the input, or std::string::npos on error (calls the Error() handler). */
	size_t parseChunkData(const char * a_Data, size_t a_Size)
	{
		assert(m_ChunkDataLengthLeft > 0);
		auto bytes = std::min(a_Size, m_ChunkDataLengthLeft);
		m_ChunkDataLengthLeft -= bytes;
		m_Callbacks.onBodyData(a_Data, bytes);
		if (m_ChunkDataLengthLeft == 0)
		{
			m_State = psChunkDataCR;
		}
		return bytes;
	}


	/** Parses the incoming data, the current state is psChunkDataCR.
	Only the CR character is expected, if found, moves to psChunkDataLF, otherwise issues an error.
	Returns the number of bytes consumed from the input, or std::string::npos on error (calls the Error handler). */
	size_t parseChunkDataCR(const char * a_Data, size_t a_Size)
	{
		// Expected input: <CR>
		if (a_Size == 0)
		{
			return 0;
		}
		if (a_Data[0] == '\r')
		{
			m_State = psChunkDataLF;
			return 1;
		}
		error(Utils::printf("Invalid character past chunk data: 0x%x", a_Data[0]));
		return std::string::npos;
	}




	/** Parses the incoming data, the current state is psChunkDataCR.
	Only the CR character is expected, if found, moves to psChunkDataLF, otherwise issues an error.
	Returns the number of bytes consumed from the input, or std::string::npos on error (calls the Error handler). */
	size_t parseChunkDataLF(const char * a_Data, size_t a_Size)
	{
		// Expected input: <LF>
		if (a_Size == 0)
		{
			return 0;
		}
		if (a_Data[0] == '\n')
		{
			m_State = psChunkLength;
			return 1;
		}
		error(Utils::printf("Invalid character past chunk data's CR: 0x%x", a_Data[0]));
		return std::string::npos;
	}


	/** Parses the incoming data, the current state is psChunkDataCR.
	The trailer is normally a set of "Header: Value" lines, terminated by an empty line. Use the m_TrailerParser for that.
	Returns the number of bytes consumed from the input, or std::string::npos on error (calls the Error handler). */
	size_t parseTrailer(const char * a_Data, size_t a_Size)
	{
		auto res = m_TrailerParser.parse(a_Data, a_Size);
		if (res == std::string::npos)
		{
			error("Error while parsing the trailer");
		}
		if ((res < a_Size) || !m_TrailerParser.isInHeaders())
		{
			m_Callbacks.onBodyFinished();
			m_State = psFinished;
		}
		return res;
	}


	// TransferEncodingParser overrides:
	virtual size_t parse(const char * a_Data, size_t a_Size) override
	{
		while ((a_Size > 0) && (m_State != psFinished))
		{
			size_t consumed = 0;
			switch (m_State)
			{
				case psChunkLength:        consumed = parseChunkLength       (a_Data, a_Size); break;
				case psChunkLengthTrailer: consumed = parseChunkLengthTrailer(a_Data, a_Size); break;
				case psChunkLengthLF:      consumed = parseChunkLengthLF     (a_Data, a_Size); break;
				case psChunkData:          consumed = parseChunkData         (a_Data, a_Size); break;
				case psChunkDataCR:        consumed = parseChunkDataCR       (a_Data, a_Size); break;
				case psChunkDataLF:        consumed = parseChunkDataLF       (a_Data, a_Size); break;
				case psTrailer:            consumed = parseTrailer           (a_Data, a_Size); break;
				case psFinished:           consumed = 0;                                       break;  // Not supposed to happen, but Clang complains without it
			}
			if (consumed == std::string::npos)
			{
				return std::string::npos;
			}
			a_Data += consumed;
			a_Size -= consumed;
		}
		return a_Size;
	}

	virtual void finish(void) override
	{
		if (m_State != psFinished)
		{
			error(Utils::printf("ChunkedTransferEncoding: Finish signal received before the data stream ended (state: %d)", m_State));
		}
		m_State = psFinished;
	}


	// EnvelopeParser::Callbacks overrides:
	virtual void onHeaderLine(const std::string & /* a_Key */, const std::string & /* a_Value */) override
	{
		// Ignored
	}
};





////////////////////////////////////////////////////////////////////////////////
// IdentityTEParser:

class IdentityTEParser:
	public TransferEncodingParser
{
	typedef TransferEncodingParser Super;

public:
	IdentityTEParser(Callbacks & a_Callbacks, size_t a_ContentLength):
		Super(a_Callbacks),
		m_BytesLeft(a_ContentLength)
	{
	}


protected:

	/** How many bytes of content are left before the message ends. */
	size_t m_BytesLeft;

	// TransferEncodingParser overrides:
	virtual size_t parse(const char * a_Data, size_t a_Size) override
	{
		auto size = std::min(a_Size, m_BytesLeft);
		if (size > 0)
		{
			m_Callbacks.onBodyData(a_Data, size);
		}
		m_BytesLeft -= size;
		if (m_BytesLeft == 0)
		{
			m_Callbacks.onBodyFinished();
		}
		return a_Size - size;
	}

	virtual void finish(void) override
	{
		if (m_BytesLeft > 0)
		{
			m_Callbacks.onError("IdentityTransferEncoding: body was truncated");
		}
		else
		{
			// BodyFinished has already been called, just bail out
		}
	}
};





////////////////////////////////////////////////////////////////////////////////
// TransferEncodingParser:

TransferEncodingParserPtr TransferEncodingParser::create(
	Callbacks & a_Callbacks,
	const std::string & a_TransferEncoding,
	size_t a_ContentLength
)
{
	auto transferEncoding = Utils::strToLower(a_TransferEncoding);
	if (transferEncoding == "chunked")
	{
		return std::make_shared<ChunkedTEParser>(a_Callbacks);
	}
	if (transferEncoding == "identity")
	{
		return std::make_shared<IdentityTEParser>(a_Callbacks, a_ContentLength);
	}
	return nullptr;
}





}  // namespace Http

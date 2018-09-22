#include "MessageParser.h"
#include <cassert>
#include "StringUtils.h"





namespace Http {





MessageParser::MessageParser(MessageParser::Callbacks & a_Callbacks):
	m_Callbacks(a_Callbacks),
	m_EnvelopeParser(*this)
{
	reset();
}





size_t MessageParser::parse(const char * a_Data, size_t a_Size)
{
	// If parsing already finished or errorred, let the caller keep all the data:
	if (m_IsFinished || m_HasHadError)
	{
		return 0;
	}

	// If still waiting for the status line, add to buffer and try parsing it:
	auto inBufferSoFar = m_Buffer.size();
	if (m_FirstLine.empty())
	{
		m_Buffer.append(a_Data, a_Size);
		auto bytesConsumedFirstLine = parseFirstLine();
		assert(bytesConsumedFirstLine <= inBufferSoFar + a_Size);  // Haven't consumed more data than there is in the buffer
		assert(bytesConsumedFirstLine > inBufferSoFar);  // Have consumed at least the previous buffer contents
		if (m_FirstLine.empty())
		{
			// All data used, but not a complete status line yet.
			return a_Size;
		}
		if (m_HasHadError)
		{
			return std::string::npos;
		}
		// Status line completed, feed the rest of the buffer into the envelope parser:
		auto bytesConsumedEnvelope = m_EnvelopeParser.parse(m_Buffer.data(), m_Buffer.size());
		if (bytesConsumedEnvelope == std::string::npos)
		{
			m_HasHadError = true;
			m_Callbacks.onError("Failed to parse the envelope");
			return std::string::npos;
		}
		assert(bytesConsumedEnvelope <= bytesConsumedFirstLine + a_Size);  // Haven't consumed more data than there was in the buffer
		m_Buffer.erase(0, bytesConsumedEnvelope);
		if (!m_EnvelopeParser.isInHeaders())
		{
			headersFinished();
			// Process any data still left in the buffer as message body:
			auto bytesConsumedBody = parseBody(m_Buffer.data(), m_Buffer.size());
			if (bytesConsumedBody == std::string::npos)
			{
				// Error has already been reported by ParseBody, just bail out:
				return std::string::npos;
			}
			return bytesConsumedBody + bytesConsumedEnvelope + bytesConsumedFirstLine - inBufferSoFar;
		}
		return a_Size;
	}  // if (m_FirstLine.empty())

	// If still parsing headers, send them to the envelope parser:
	if (m_EnvelopeParser.isInHeaders())
	{
		auto bytesConsumed = m_EnvelopeParser.parse(a_Data, a_Size);
		if (bytesConsumed == std::string::npos)
		{
			m_HasHadError = true;
			m_Callbacks.onError("Failed to parse the envelope");
			return std::string::npos;
		}
		if (!m_EnvelopeParser.isInHeaders())
		{
			headersFinished();
			// Process any data still left as message body:
			auto bytesConsumedBody = parseBody(a_Data + bytesConsumed, a_Size - bytesConsumed);
			if (bytesConsumedBody == std::string::npos)
			{
				// Error has already been reported by ParseBody, just bail out:
				return std::string::npos;
			}
		}
		return a_Size;
	}

	// Already parsing the body
	return parseBody(a_Data, a_Size);
}





void MessageParser::reset(void)
{
	m_HasHadError = false;
	m_IsFinished = false;
	m_FirstLine.clear();
	m_Buffer.clear();
	m_EnvelopeParser.reset();
	m_TransferEncodingParser.reset();
	m_TransferEncoding.clear();
	m_ContentLength = 0;
}




size_t MessageParser::parseFirstLine(void)
{
	auto idxLineEnd = m_Buffer.find("\r\n");
	if (idxLineEnd == std::string::npos)
	{
		// Not a complete line yet
		return m_Buffer.size();
	}
	m_FirstLine = m_Buffer.substr(0, idxLineEnd);
	m_Buffer.erase(0, idxLineEnd + 2);
	m_Callbacks.onFirstLine(m_FirstLine);
	return idxLineEnd + 2;
}





size_t MessageParser::parseBody(const char * a_Data, size_t a_Size)
{
	if (m_TransferEncodingParser == nullptr)
	{
		// We have no Transfer-encoding parser assigned. This should have happened when finishing the envelope
		onError("No transfer encoding parser");
		return std::string::npos;
	}

	// Parse the body using the transfer encoding parser:
	// (Note that TE parser returns the number of bytes left, while we return the number of bytes consumed)
	return a_Size - m_TransferEncodingParser->parse(a_Data, a_Size);
}





void MessageParser::headersFinished(void)
{
	m_Callbacks.onHeadersFinished();
	if (m_TransferEncoding.empty())
	{
		m_TransferEncoding = "Identity";
	}
	m_TransferEncodingParser = TransferEncodingParser::create(*this, m_TransferEncoding, m_ContentLength);
	if (m_TransferEncodingParser == nullptr)
	{
		onError(Utils::printf("Unknown transfer encoding: %s", m_TransferEncoding.c_str()));
		return;
	}
}





void MessageParser::onHeaderLine(const std::string & a_Key, const std::string & a_Value)
{
	m_Callbacks.onHeaderLine(a_Key, a_Value);
	auto key = Utils::strToLower(a_Key);
	if (key == "content-length")
	{
		if (!Utils::stringToInteger(a_Value, m_ContentLength))
		{
			onError(Utils::printf("Invalid content length header value: \"%s\"", a_Value.c_str()));
		}
		return;
	}
	if (key == "transfer-encoding")
	{
		m_TransferEncoding = a_Value;
		return;
	}
}





void MessageParser::onError(const std::string & a_ErrorDescription)
{
	m_HasHadError = true;
	m_Callbacks.onError(a_ErrorDescription);
}





void MessageParser::onBodyData(const void * a_Data, size_t a_Size)
{
	m_Callbacks.onBodyData(a_Data, a_Size);
}





void MessageParser::onBodyFinished(void)
{
	m_IsFinished = true;
	m_Callbacks.onBodyFinished();
}





}  // namespace Http

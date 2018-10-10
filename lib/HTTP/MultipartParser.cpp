#include "MultipartParser.h"
#include <cstring>
#include "NameValueParser.h"





namespace Http {





MultipartParser::MultipartParser(const std::string & a_ContentType, Callbacks & a_Callbacks) :
	m_Callbacks(a_Callbacks),
	m_IsValid(true),
	m_EnvelopeParser(*this),
	m_HasHadData(false)
{
	// Check that the content type is multipart:
	std::string ContentType(a_ContentType);
	if (strncmp(ContentType.c_str(), "multipart/", 10) != 0)
	{
		m_IsValid = false;
		return;
	}
	size_t idxSC = ContentType.find(';', 10);
	if (idxSC == std::string::npos)
	{
		m_IsValid = false;
		return;
	}

	// Find the multipart boundary:
	ContentType.erase(0, idxSC + 1);
	NameValueParser CTParser(ContentType.c_str(), ContentType.size());
	CTParser.finish();
	if (!CTParser.isValid())
	{
		m_IsValid = false;
		return;
	}
	m_Boundary = CTParser["boundary"];
	m_IsValid = !m_Boundary.empty();
	if (!m_IsValid)
	{
		return;
	}

	// Set the envelope parser for parsing the body, so that our Parse() function parses the ignored prefix data as a body
	m_EnvelopeParser.setIsInHeaders(false);

	// Append an initial CRLF to the incoming data, so that a body starting with the boundary line will get caught
	m_IncomingData.assign("\r\n");

	/*
	m_Boundary = std::string("\r\n--") + m_Boundary
	m_BoundaryEnd = m_Boundary + "--\r\n";
	m_Boundary = m_Boundary + "\r\n";
	*/
}





void MultipartParser::parse(const char * a_Data, size_t a_Size)
{
	// Skip parsing if invalid
	if (!m_IsValid)
	{
		return;
	}

	// Append to buffer, then parse it:
	m_IncomingData.append(a_Data, a_Size);
	for (;;)
	{
		if (m_EnvelopeParser.isInHeaders())
		{
			size_t BytesConsumed = m_EnvelopeParser.parse(m_IncomingData.data(), m_IncomingData.size());
			if (BytesConsumed == std::string::npos)
			{
				m_IsValid = false;
				return;
			}
			if ((BytesConsumed == a_Size) && m_EnvelopeParser.isInHeaders())
			{
				// All the incoming data has been consumed and still waiting for more
				return;
			}
			m_IncomingData.erase(0, BytesConsumed);
		}

		// Search for boundary / boundary end:
		size_t idxBoundary = m_IncomingData.find("\r\n--");
		if (idxBoundary == std::string::npos)
		{
			// Boundary string start not present, present as much data to the part callback as possible
			if (m_IncomingData.size() > m_Boundary.size() + 8)
			{
				size_t BytesToReport = m_IncomingData.size() - m_Boundary.size() - 8;
				m_Callbacks.onPartData(m_IncomingData.data(), BytesToReport);
				m_IncomingData.erase(0, BytesToReport);
			}
			return;
		}
		if (idxBoundary > 0)
		{
			m_Callbacks.onPartData(m_IncomingData.data(), idxBoundary);
			m_IncomingData.erase(0, idxBoundary);
		}
		idxBoundary = 4;
		size_t LineEnd = m_IncomingData.find("\r\n", idxBoundary);
		if (LineEnd == std::string::npos)
		{
			// Not a complete line yet, present as much data to the part callback as possible
			if (m_IncomingData.size() > m_Boundary.size() + 8)
			{
				size_t BytesToReport = m_IncomingData.size() - m_Boundary.size() - 8;
				m_Callbacks.onPartData(m_IncomingData.data(), BytesToReport);
				m_IncomingData.erase(0, BytesToReport);
			}
			return;
		}
		if (
			(LineEnd - idxBoundary != m_Boundary.size()) &&  // Line length not equal to boundary
			(LineEnd - idxBoundary != m_Boundary.size() + 2)  // Line length not equal to boundary end
		)
		{
			// Got a line, but it's not a boundary, report it as data:
			m_Callbacks.onPartData(m_IncomingData.data(), LineEnd);
			m_IncomingData.erase(0, LineEnd);
			continue;
		}

		if (strncmp(m_IncomingData.c_str() + idxBoundary, m_Boundary.c_str(), m_Boundary.size()) == 0)
		{
			// Boundary or BoundaryEnd found:
			m_Callbacks.onPartEnd();
			size_t idxSlash = idxBoundary + m_Boundary.size();
			if ((m_IncomingData[idxSlash] == '-') && (m_IncomingData[idxSlash + 1] == '-'))
			{
				// This was the last part
				m_Callbacks.onPartData(m_IncomingData.data() + idxSlash + 4, m_IncomingData.size() - idxSlash - 4);
				m_IncomingData.clear();
				return;
			}
			m_Callbacks.onPartStart();
			m_IncomingData.erase(0, LineEnd + 2);

			// Keep parsing for the headers that may have come with this data:
			m_EnvelopeParser.reset();
			continue;
		}

		// It's a line, but not a boundary. It can be fully sent to the data receiver, since a boundary cannot cross lines
		m_Callbacks.onPartData(m_IncomingData.c_str(), LineEnd);
		m_IncomingData.erase(0, LineEnd);
	}  // while (true)
}





void MultipartParser::onHeaderLine(const std::string & a_Key, const std::string & a_Value)
{
	m_Callbacks.onPartHeader(a_Key, a_Value);
}





}  // namespace Http

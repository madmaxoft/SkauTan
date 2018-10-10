#include "Message.h"
#include <cstring>
#include "StringUtils.h"





namespace Http {





////////////////////////////////////////////////////////////////////////////////
// Message:

Message::Message(Kind a_Kind) :
	m_Kind(a_Kind),
	m_ContentLength(std::string::npos)
{
}





void Message::addHeader(const std::string & a_Key, const std::string & a_Value)
{
	auto Key = Utils::strToLower(a_Key);
	auto itr = m_Headers.find(Key);
	if (itr == m_Headers.end())
	{
		m_Headers[Key] = a_Value;
	}
	else
	{
		// The header-field key is specified multiple times, combine into comma-separated list (RFC 2616 @ 4.2)
		itr->second.append(", ");
		itr->second.append(a_Value);
	}

	// Special processing for well-known headers:
	if (Key == "content-type")
	{
		m_ContentType = m_Headers[Key];
	}
	else if (Key == "content-length")
	{
		if (!Utils::stringToInteger(m_Headers[Key], m_ContentLength))
		{
			m_ContentLength = 0;
		}
	}
}





std::string Message::headerToValue(const std::string & a_Key, const std::string & a_Default) const
{
	auto itr = m_Headers.find(Utils::strToLower(a_Key));
	if (itr == m_Headers.cend())
	{
		return a_Default;
	}
	return itr->second;
}





void Message::setContentType(const std::string & a_ContentType)
{
	m_Headers["content-type"] = a_ContentType;
	m_ContentType = a_ContentType;
}





void Message::setContentLength(size_t a_ContentLength)
{
	m_Headers["content-length"] = Utils::printf("%llu", (static_cast<unsigned long long>(a_ContentLength)));
	m_ContentLength = a_ContentLength;
}





////////////////////////////////////////////////////////////////////////////////
// OutgoingResponse:

OutgoingResponse::OutgoingResponse(void) :
	Super(mkResponse)
{
}





std::string OutgoingResponse::serialize(int a_StatusCode, const std::string & a_StatusText) const
{
	// Status line:
	auto res = Utils::printf("HTTP/1.1 %d %s\r\n",
		a_StatusCode, a_StatusText.c_str()
	);

	// Headers:
	for (const auto & hdr: m_Headers)
	{
		res.append(hdr.first);
		res.append(": ");
		res.append(hdr.second);
		res.append("\r\n");
	}  // for itr - m_Headers[]
	res.append("\r\n");

	return res;
}





////////////////////////////////////////////////////////////////////////////////
// SimpleOutgoingResponse:

std::string SimpleOutgoingResponse::serialize(
	int a_StatusCode,
	const std::string & a_StatusText,
	const std::string & a_Body
)
{
	return serialize(
		a_StatusCode,
		a_StatusText,
		{ {"Content-Length", Utils::printf("%llu", a_Body.size())} },
		a_Body
	);
}





std::string SimpleOutgoingResponse::serialize(
	int a_StatusCode,
	const std::string & a_StatusText,
	const std::string & a_ContentType,
	const std::string & a_Body
)
{
	std::map<std::string, std::string> headers =
	{
		{ "Content-Type", a_ContentType },
		{ "Content-Length", Utils::printf("%llu", a_Body.size()) },
	};
	return serialize(
		a_StatusCode,
		a_StatusText,
		headers,
		a_Body
	);
}





std::string SimpleOutgoingResponse::serialize(
	int a_StatusCode,
	const std::string & a_StatusText,
	const std::map<std::string, std::string> & a_Headers,
	const std::string & a_Body
)
{
	std::string res = Utils::printf("HTTP/1.1 %d %s\r\n", a_StatusCode, a_StatusText.c_str());
	for (const auto & hdr: a_Headers)
	{
		res.append(Utils::printf("%s: %s\r\n", hdr.first.c_str(), hdr.second.c_str()));
	}
	res.append("\r\n");
	res.append(a_Body);
	return res;
}





////////////////////////////////////////////////////////////////////////////////
// IncomingRequest:

IncomingRequest::IncomingRequest(const std::string & a_Method, const std::string & a_URL):
	Super(mkRequest),
	m_Method(a_Method),
	m_URL(a_URL),
	m_HasAuth(false)
{
}





std::string IncomingRequest::urlPath(void) const
{
	auto idxQuestionMark = m_URL.find('?');
	if (idxQuestionMark == std::string::npos)
	{
		return m_URL;
	}
	else
	{
		return m_URL.substr(0, idxQuestionMark);
	}
}





void IncomingRequest::addHeader(const std::string & a_Key, const std::string & a_Value)
{
	if (
		(Utils::noCaseCompare(a_Key, "Authorization") == 0) &&
		(strncmp(a_Value.c_str(), "Basic ", 6) == 0)
	)
	{
		std::string UserPass = Utils::base64Decode(a_Value.substr(6));
		size_t idxCol = UserPass.find(':');
		if (idxCol != std::string::npos)
		{
			m_AuthUsername = UserPass.substr(0, idxCol);
			m_AuthPassword = UserPass.substr(idxCol + 1);
			m_HasAuth = true;
		}
	}
	if ((a_Key == "Connection") && (Utils::noCaseCompare(a_Value, "keep-alive") == 0))
	{
		m_AllowKeepAlive = true;
	}
	Super::addHeader(a_Key, a_Value);
}





}  // namespace Http

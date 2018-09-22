#pragma once

#include <string>
#include <map>
#include <memory>
#include "EnvelopeParser.h"
#include "StringUtils.h"





namespace Http {





/** Base for all HTTP messages.
Provides storage and basic handling for headers.
Note that the headers are stored / compared with their keys lowercased.
Multiple header values are concatenated using commas (RFC 2616 @ 4.2) uppon addition. */
class Message
{
public:

	/** Values of the various HTTP status codes. */
	static const int HTTP_OK = 200;
	static const int HTTP_BAD_REQUEST = 400;
	static const int HTTP_NOT_FOUND = 404;


	/** Signals whether the message is a HTTP request or HTTP response. */
	enum Kind
	{
		mkRequest,
		mkResponse,
	};

	typedef std::map<std::string, std::string> NameValueMap;


	Message(Kind a_Kind);

	// Force a virtual destructor in all descendants
	virtual ~Message() {}

	/** Adds a header into the internal map of headers.
	The header key is lowercase before processing the header.
	Recognizes special headers: Content-Type and Content-Length.
	Descendants may override to recognize and process other headers. */
	virtual void addHeader(const std::string & a_Key, const std::string & a_Value);

	/** Returns all the headers within the message.
	The header keys are in lowercase. */
	const NameValueMap & headers() const { return m_Headers; }

	/** If the specified header key is found (case-insensitive), returns the header's value.
	Returns the default when header the key is not found. */
	std::string headerToValue(const std::string & a_Key, const std::string & a_Default) const;

	/** Returns the value of the specified header as number. The header key is compared case-insensitive.
	If the specified header key is not found, returns the default.
	If the conversion from the header value to a number fails, returns the default. */
	template <typename T>
	T headerToNumber(const std::string & a_Key, T a_Default) const
	{
		auto v = headerToValue(a_Key, std::string());
		if (v.empty())
		{
			return a_Default;
		}
		T out;
		if (Utils::stringToInteger(v, out))
		{
			return out;
		}
		return a_Default;
	}

	void setContentType  (const std::string & a_ContentType);
	void setContentLength(size_t a_ContentLength);

	const std::string & contentType  (void) const { return m_ContentType; }
	size_t              contentLength(void) const { return m_ContentLength; }


protected:

	Kind m_Kind;

	/** Map of headers, with their keys lowercased. */
	NameValueMap m_Headers;

	/** Type of the content; parsed by addHeader(), set directly by setContentLength() */
	std::string m_ContentType;

	/** Length of the content that is to be received.
	std::string::npos when the object is created.
	Parsed by addHeader() or set directly by setContentLength() */
	size_t m_ContentLength;
} ;





/** Stores outgoing response headers and serializes them to an HTTP data stream. */
class OutgoingResponse:
	public Message
{
	typedef Message Super;

public:

	OutgoingResponse(void);

	/** Returns the beginning of a response datastream, containing the specified status code, text, and all
	serialized headers.
	The users should send this, then the actual body of the response. */
	std::string serialize(int a_StatusCode, const std::string & StatusText) const;
} ;





/** Serializer for simple outgoing responses - those that have a fixed known status
line, headers, and a short body. */
class SimpleOutgoingResponse
{
public:

	/** Returns HTTP response data that represents the specified parameters.
	This overload provides only the Content-Length header. */
	static std::string serialize(
		int a_StatusCode,
		const std::string & a_StatusText,
		const std::string & a_Body
	);


	/** Returns HTTP response data that represents the specified parameters.
	This overload provides only the Content-Type and Content-Length headers. */
	static std::string serialize(
		int a_StatusCode,
		const std::string & a_StatusText,
		const std::string & a_ContentType,
		const std::string & a_Body
	);

	/** Returns HTTP response data that represents the specified parameters. */
	static std::string serialize(
		int a_StatusCode,
		const std::string & a_StatusText,
		const std::map<std::string, std::string> & a_Headers,
		const std::string & a_Body
	);
};





/** Provides storage for an incoming HTTP request. */
class IncomingRequest:
	public Message
{
	typedef Message Super;


public:

	/** Base class for anything that can be used as the UserData for the request. */
	class UserData
	{
	public:
		// Force a virtual destructor in descendants:
		virtual ~UserData() {}
	};
	typedef std::shared_ptr<UserData> UserDataPtr;


	/** Creates a new instance of the class, containing the method and URL provided by the client. */
	IncomingRequest(const std::string & a_Method, const std::string & a_URL);

	/** Returns the method used in the request */
	const std::string & method(void) const { return m_Method; }

	/** Returns the entire URL used in the request, including the parameters after '?'. */
	const std::string & url(void) const { return m_URL; }

	/** Returns the path part of the URL (without the parameters after '?'). */
	std::string urlPath(void) const;

	/** Returns true if the request has had the Auth header present. */
	bool hasAuth(void) const { return m_HasAuth; }

	/** Returns the username that the request presented. Only valid if hasAuth() is true */
	const std::string & authUsername(void) const { return m_AuthUsername; }

	/** Returns the password that the request presented. Only valid if hasAuth() is true */
	const std::string & authPassword(void) const { return m_AuthPassword; }

	bool doesAllowKeepAlive(void) const { return m_AllowKeepAlive; }

	/** Attaches any kind of data to this request, to be later retrieved by userData(). */
	void setUserData(UserDataPtr a_UserData) { m_UserData = a_UserData; }

	/** Returns the data attached to this request by the class client. */
	UserDataPtr userData(void) { return m_UserData; }

	/** Adds the specified header into the internal list of headers.
	Overrides the parent to add recognizing additional headers: auth and keepalive. */
	virtual void addHeader(const std::string & a_Key, const std::string & a_Value) override;


protected:

	/** Method of the request (GET / PUT / POST / ...) */
	std::string m_Method;

	/** Full URL of the request */
	std::string m_URL;

	/** Set to true if the request contains auth data that was understood by the parser */
	bool m_HasAuth;

	/** The username used for auth */
	std::string m_AuthUsername;

	/** The password used for auth */
	std::string m_AuthPassword;

	/** Set to true if the request indicated that it supports keepalives.
	If false, the server will close the connection once the request is finished */
	bool m_AllowKeepAlive;

	/** Any data attached to the request by the class client. */
	UserDataPtr m_UserData;
};




}  // namespace Http

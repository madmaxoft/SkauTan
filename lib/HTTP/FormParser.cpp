#include "FormParser.h"
#include <vector>
#include <cassert>
#include "StringUtils.h"
#include "Message.h"
#include "MultipartParser.h"
#include "NameValueParser.h"





namespace Http {





FormParser::FormParser(const IncomingRequest & a_Request, Callbacks & a_Callbacks) :
	m_Callbacks(a_Callbacks),
	m_IsValid(true),
	m_IsCurrentPartFile(false),
	m_FileHasBeenAnnounced(false)
{
	if (a_Request.method() == "GET")
	{
		m_Kind = fpkURL;

		// Directly parse the URL in the request:
		const std::string & URL = a_Request.url();
		size_t idxQM = URL.find('?');
		if (idxQM != std::string::npos)
		{
			parse(URL.c_str() + idxQM + 1, URL.size() - idxQM - 1);
		}
		return;
	}
	if ((a_Request.method() == "POST") || (a_Request.method() == "PUT"))
	{
		if (strncmp(a_Request.contentType().c_str(), "application/x-www-form-urlencoded", 33) == 0)
		{
			m_Kind = fpkFormUrlEncoded;
			return;
		}
		if (strncmp(a_Request.contentType().c_str(), "multipart/form-data", 19) == 0)
		{
			m_Kind = fpkMultipart;
			beginMultipart(a_Request);
			return;
		}
	}
	// Invalid method / content type combination, this is not a HTTP form
	m_IsValid = false;
}





FormParser::FormParser(Kind a_Kind, const char * a_Data, size_t a_Size, Callbacks & a_Callbacks) :
	m_Callbacks(a_Callbacks),
	m_Kind(a_Kind),
	m_IsValid(true),
	m_IsCurrentPartFile(false),
	m_FileHasBeenAnnounced(false)
{
	parse(a_Data, a_Size);
}





void FormParser::parse(const char * a_Data, size_t a_Size)
{
	if (!m_IsValid)
	{
		return;
	}

	switch (m_Kind)
	{
		case fpkURL:
		case fpkFormUrlEncoded:
		{
			// This format is used for smaller forms (not file uploads), so we can delay parsing it until Finish()
			m_IncomingData.append(a_Data, a_Size);
			break;
		}
		case fpkMultipart:
		{
			assert(m_MultipartParser.get() != nullptr);
			m_MultipartParser->parse(a_Data, a_Size);
			break;
		}
	}
}





bool FormParser::finish(void)
{
	switch (m_Kind)
	{
		case fpkURL:
		case fpkFormUrlEncoded:
		{
			// m_IncomingData has all the form data, parse it now:
			parseFormUrlEncoded();
			break;
		}
		case fpkMultipart:
		{
			// Nothing needed for other formats
			break;
		}
	}
	return (m_IsValid && m_IncomingData.empty());
}





bool FormParser::hasFormData(const IncomingRequest & a_Request)
{
	const std::string & ContentType = a_Request.contentType();
	return (
		(ContentType == "application/x-www-form-urlencoded") ||
		(strncmp(ContentType.c_str(), "multipart/form-data", 19) == 0) ||
		(
			(a_Request.method() == "GET") &&
			(a_Request.url().find('?') != std::string::npos)
		)
	);
}





void FormParser::beginMultipart(const IncomingRequest & a_Request)
{
	assert(m_MultipartParser.get() == nullptr);
	m_MultipartParser.reset(new MultipartParser(a_Request.contentType(), *this));
}





void FormParser::parseFormUrlEncoded(void)
{
	// Parse m_IncomingData for all the variables; no more data is incoming, since this is called from Finish()
	// This may not be the most performant version, but we don't care, the form data is small enough and we're not a full-fledged web server anyway
	auto lines = Utils::stringSplit(m_IncomingData, "&");
	for (auto itr = lines.begin(), end = lines.end(); itr != end; ++itr)
	{
		auto Components = Utils::stringSplit(*itr, "=");
		switch (Components.size())
		{
			default:
			{
				// Neither name nor value, or too many "="s, mark this as invalid form:
				m_IsValid = false;
				return;
			}
			case 1:
			{
				// Only name present
				auto name = Utils::urlDecode(Utils::replaceAllCharOccurrences(Components[0], '+', ' '));
				if (name.first)
				{
					(*this)[name.second] = "";
				}
				break;
			}
			case 2:
			{
				// name=value format:
				auto name = Utils::urlDecode(Components[0]);
				auto value = Utils::urlDecode(Components[1]);
				if (name.first && value.first)
				{
					(*this)[name.second] = value.second;
				}
				break;
			}
		}
	}  // for itr - Lines[]
	m_IncomingData.clear();
}





void FormParser::onPartStart(void)
{
	m_CurrentPartFileName.clear();
	m_CurrentPartName.clear();
	m_IsCurrentPartFile = false;
	m_FileHasBeenAnnounced = false;
}





void FormParser::onPartHeader(const std::string & a_Key, const std::string & a_Value)
{
	if (Utils::noCaseCompare(a_Key, "Content-Disposition") == 0)
	{
		size_t len = a_Value.size();
		size_t ParamsStart = std::string::npos;
		for (size_t i = 0; i < len; ++i)
		{
			if (a_Value[i] > ' ')
			{
				if (strncmp(a_Value.c_str() + i, "form-data", 9) != 0)
				{
					// Content disposition is not "form-data", mark the whole form invalid
					m_IsValid = false;
					return;
				}
				ParamsStart = a_Value.find(';', i + 9);
				break;
			}
		}
		if (ParamsStart == std::string::npos)
		{
			// There is data missing in the Content-Disposition field, mark the whole form invalid:
			m_IsValid = false;
			return;
		}

		// Parse the field name and optional filename from this header:
		NameValueParser Parser(a_Value.data() + ParamsStart, a_Value.size() - ParamsStart);
		Parser.finish();
		m_CurrentPartName = Parser["name"];
		if (!Parser.isValid() || m_CurrentPartName.empty())
		{
			// The required parameter "name" is missing, mark the whole form invalid:
			m_IsValid = false;
			return;
		}
		m_CurrentPartFileName = Parser["filename"];
	}
}





void FormParser::onPartData(const char * a_Data, size_t a_Size)
{
	if (m_CurrentPartName.empty())
	{
		// Prologue, epilogue or invalid part
		return;
	}
	if (m_CurrentPartFileName.empty())
	{
		// This is a variable, store it in the map
		iterator itr = find(m_CurrentPartName);
		if (itr == end())
		{
			(*this)[m_CurrentPartName] = std::string(a_Data, a_Size);
		}
		else
		{
			itr->second.append(a_Data, a_Size);
		}
	}
	else
	{
		// This is a file, pass it on through the callbacks
		if (!m_FileHasBeenAnnounced)
		{
			m_Callbacks.onFileStart(*this, m_CurrentPartFileName);
			m_FileHasBeenAnnounced = true;
		}
		m_Callbacks.onFileData(*this, a_Data, a_Size);
	}
}





void FormParser::onPartEnd(void)
{
	if (m_FileHasBeenAnnounced)
	{
		m_Callbacks.onFileEnd(*this);
	}
	m_CurrentPartName.clear();
	m_CurrentPartFileName.clear();
}




}  // namespace Http

#pragma once

#include <map>
#include <string>
#include <memory>
#include "MultipartParser.h"





namespace Http {





// fwd:
class IncomingRequest;





/** Parses the data sent over HTTP from an HTML form, in a SAX-like form.
The form values are stored within the std::map part of the class, the potentially large file parts are
reported using callbacks.
The user of this class provides callbacks and then pushes data into this class, it then calls the various
callbacks upon encountering the data and stores the simple values within. */
class FormParser:
	public std::map<std::string, std::string>,
	public MultipartParser::Callbacks
{
public:
	enum Kind
	{
		fpkURL,             ///< The form has been transmitted as parameters to a GET request
		fpkFormUrlEncoded,  ///< The form has been POSTed or PUT, with Content-Type of "application/x-www-form-urlencoded"
		fpkMultipart,       ///< The form has been POSTed or PUT, with Content-Type of "multipart/form-data"
	};

	class Callbacks
	{
	public:
		// Force a virtual destructor in descendants:
		virtual ~Callbacks() {}

		/** Called when a new file part is encountered in the form data */
		virtual void onFileStart(FormParser & a_Parser, const std::string & a_FileName) = 0;

		/** Called when more file data has come for the current file in the form data */
		virtual void onFileData(FormParser & a_Parser, const char * a_Data, size_t a_Size) = 0;

		/** Called when the current file part has ended in the form data */
		virtual void onFileEnd(FormParser & a_Parser) = 0;
	};


	/** Creates a parser that is tied to a request and notifies of various events using a callback mechanism */
	FormParser(const IncomingRequest & a_Request, Callbacks & a_Callbacks);

	/** Creates a parser with the specified content type that reads data from a string */
	FormParser(Kind a_Kind, const char * a_Data, size_t a_Size, Callbacks & a_Callbacks);

	/** Adds more data into the parser, as the request body is received */
	void parse(const char * a_Data, size_t a_Size);

	/** Notifies that there's no more data incoming and the parser should finish its parsing.
	Returns true if parsing successful. */
	bool finish();

	/** Returns true if the headers suggest the request has form data parseable by this class */
	static bool hasFormData(const IncomingRequest & a_Request);


protected:

	/** The callbacks to call for incoming file data */
	Callbacks & m_Callbacks;

	/** The kind of the parser (decided in the constructor, used in Parse() */
	Kind m_Kind;

	/** Buffer for the incoming data until it's parsed */
	std::string m_IncomingData;

	/** True if the information received so far is a valid form; set to false on first problem. Further parsing is skipped when false. */
	bool m_IsValid;

	/** The parser for the multipart data, if used */
	std::unique_ptr<MultipartParser> m_MultipartParser;

	/** Name of the currently parsed part in multipart data */
	std::string m_CurrentPartName;

	/** True if the currently parsed part in multipart data is a file */
	bool m_IsCurrentPartFile;

	/** Filename of the current parsed part in multipart data (for file uploads) */
	std::string m_CurrentPartFileName;

	/** Set to true after m_Callbacks.OnFileStart() has been called, reset to false on PartEnd */
	bool m_FileHasBeenAnnounced;


	/** Sets up the object for parsing a fpkMultipart request */
	void beginMultipart(const IncomingRequest & a_Request);

	/** Parses m_IncomingData as form-urlencoded data (fpkURL or fpkFormUrlEncoded kinds) */
	void parseFormUrlEncoded(void);

	// cMultipartParser::cCallbacks overrides:
	virtual void onPartStart (void) override;
	virtual void onPartHeader(const std::string & a_Key, const std::string & a_Value) override;
	virtual void onPartData  (const char * a_Data, size_t a_Size) override;
	virtual void onPartEnd   (void) override;
} ;





}  // namespace Http

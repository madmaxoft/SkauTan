#include "LocalVoteServer.h"
#include <QTcpSocket>
#include <QDebug>
#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QNetworkInterface>
#include <QProcessEnvironment>
#include "lib/HTTP/StringUtils.h"
#include "lib/HTTP/Message.h"
#include "lib/HTTP/FormParser.h"
#include "Player.h"
#include "Playlist.h"
#include "PlaylistItemSong.h"





/** Wraps a single TCP connection together with an HTTP parser and buffer for overflowing data. */
class Connection:
	public QObject,
	public Http::MessageParser::Callbacks
{

public:

	Connection(LocalVoteServer & a_Server, QTcpSocket * a_Socket):
		m_Server(a_Server),
		m_Socket(a_Socket),
		m_Parser(*this)
	{
		connect(m_Socket, &QTcpSocket::readyRead,
			[this]()
			{
				while (m_Socket->bytesAvailable() > 0)
				{
					auto data = m_Socket->read(50);
					auto size = static_cast<size_t>(data.size());
					size_t pos = 0;
					while (size > 0)
					{
						auto consumed = m_Parser.parse(data.constData() + pos, size);
						if (consumed == std::string::npos)
						{
							m_Socket->close();
							return;
						}
						size -= consumed;
						pos += consumed;
					}
				}
			}
		);
	}


protected:

	/** The server component that will handle the requests, once received. */
	LocalVoteServer & m_Server;

	/** The socket on which to read / write. */
	QTcpSocket * m_Socket;

	/** The parser that keeps state of the HTTP protocol. */
	Http::MessageParser m_Parser;

	/** The request being currently serviced. */
	std::shared_ptr<Http::IncomingRequest> m_CurrentRequest;

	/** The body of the request currently being serviced. */
	std::string m_CurrentBodyData;





	virtual void onError(const std::string & a_ErrorDescription) override
	{
		qDebug() << "HTTP Connection error: " << a_ErrorDescription.c_str();
		m_Socket->disconnect();
	}




	virtual void onFirstLine(const std::string & a_FirstLine) override
	{
		auto split = Http::Utils::stringSplit(a_FirstLine, " ");
		if (split.size() != 3)
		{
			qDebug() << "Invalid first line: " << a_FirstLine.c_str();
			m_Socket->disconnect();
			return;
		}
		m_CurrentRequest.reset(new Http::IncomingRequest(split[0], split[1]));
		m_CurrentBodyData.clear();
	}




	virtual void onHeaderLine(const std::string & a_Key, const std::string & a_Value) override
	{
		if (m_CurrentRequest != nullptr)
		{
			m_CurrentRequest->addHeader(a_Key, a_Value);
		}
	}





	virtual void onHeadersFinished() override
	{
	}





	virtual void onBodyData(const void * a_Data, size_t a_Size) override
	{
		m_CurrentBodyData.append(reinterpret_cast<const char *>(a_Data), a_Size);
	}





	virtual void onBodyFinished() override
	{
		m_Server.handleRequest(m_Socket, *m_CurrentRequest, m_CurrentBodyData);
		if (!m_CurrentRequest->doesAllowKeepAlive())
		{
			m_Socket->close();
		}
		m_CurrentRequest.reset();
		m_CurrentBodyData.clear();
		m_Parser.reset();
	}
};





LocalVoteServer::LocalVoteServer(ComponentCollection & a_Components, QObject * a_Parent):
	Super(a_Parent),
	m_Components(a_Components),
	m_HashLength(QCryptographicHash::hash("", QCryptographicHash::Sha1).length()),
	m_IsStarted(false)
{
	connect(&m_Server, &QTcpServer::newConnection, this, &LocalVoteServer::onNewConnection);
}





void LocalVoteServer::handleRequest(
	QTcpSocket * a_Socket,
	const Http::IncomingRequest & a_Request,
	const std::string & a_RequestBody
)
{
	Q_UNUSED(a_RequestBody);

	// If the request is for the root, return the root page contents:
	const auto & url = a_Request.url();
	if (url == "/")
	{
		return sendFile(a_Socket, "index.html");
	}

	// If the request is for the static data, return them from the disk / resources:
	if (url.compare(0, 8, "/static/") == 0)
	{
		if (url.find("/../") != std::string::npos)
		{
			return send404(a_Socket);
		}
		return sendFile(a_Socket, url);
	}

	// If the request is for an API, deliver it to the API processor:
	if (url.compare(0, 5, "/api/") == 0)
	{
		return processApi(a_Socket, a_Request, a_RequestBody);
	}

	// Nothing matched, send a 404:
	return send404(a_Socket);
}





quint16 LocalVoteServer::port() const
{
	if (!m_IsStarted)
	{
		throw std::logic_error("Server not running");
	}
	return m_Port;
}





void LocalVoteServer::sendFile(QTcpSocket * a_Socket, const std::string & a_RelativePath)
{
	std::string contents;
	auto webLang = QProcessEnvironment::systemEnvironment().value("SKAUTAN_WEB_LANGUAGE", "").toStdString();
	auto progLang = QProcessEnvironment::systemEnvironment().value("LANG", "").toStdString();
	std::vector<std::string> prefixes;
	if (!webLang.empty())
	{
		prefixes.push_back("VoteServer/" + webLang + "/");
	}
	if (!progLang.empty())
	{
		prefixes.push_back("VoteServer/" + progLang + "/");
	}
	prefixes.push_back("VoteServer/");
	if (!webLang.empty())
	{
		prefixes.push_back(":/VoteServer/" + webLang + "/");
	}
	if (!progLang.empty())
	{
		prefixes.push_back(":/VoteServer/" + progLang + "/");
	}
	prefixes.push_back(":/VoteServer/");
	for (const auto & prefix: prefixes)
	{
		QFile f(QString::fromStdString(prefix + a_RelativePath));
		if (f.open(QIODevice::ReadOnly))
		{
			contents = f.readAll().toStdString();
			auto resp = Http::SimpleOutgoingResponse::serialize(200, "OK", contents);
			a_Socket->write(QByteArray::fromStdString(resp));
			return;
		}
	}
	// File not found, send a 404 error:
	return send404(a_Socket);
}





void LocalVoteServer::send404(QTcpSocket * a_Socket)
{
	a_Socket->write(QByteArray::fromStdString(
		Http::SimpleOutgoingResponse::serialize(
			404, "Not found", "text/plain", "The specified resource was not found"
		)
	));
}





void LocalVoteServer::processApi(
	QTcpSocket * a_Socket,
	const Http::IncomingRequest & a_Request,
	const std::string & a_RequestBody
)
{
	const auto & url = a_Request.urlPath();
	using Handler = std::function<void(LocalVoteServer *, QTcpSocket *, const Http::IncomingRequest &, const std::string &)>;
	static const std::map<std::string, Handler> apiMap =
	{
		{"/api/playlist", &LocalVoteServer::apiPlaylist},
		{"/api/vote",     &LocalVoteServer::apiVote},
	};
	auto handler = apiMap.find(url);
	if (handler == apiMap.end())
	{
		return send404(a_Socket);
	}
	return (handler->second)(this, a_Socket, a_Request, a_RequestBody);
}





void LocalVoteServer::apiPlaylist(
	QTcpSocket * a_Socket,
	const Http::IncomingRequest & a_Request,
	const std::string & a_RequestBody
)
{
	Q_UNUSED(a_Request);
	Q_UNUSED(a_RequestBody);

	// Check the requested start against history size:
	auto start = a_Request.headerToNumber("x-skautan-playlist-start", 0ul);
	if (start >= m_History.size())
	{
		a_Socket->write(QByteArray::fromStdString(Http::SimpleOutgoingResponse::serialize(
			200, "OK", "application/json", "[]"
		)));
		return;
	}

	// Send all songs in the history:
	QByteArray out ("[");
	auto len = m_History.size();
	for (size_t index = start; index < len; ++index)
	{
		auto song = m_History[index];
		QJsonObject entry;
		entry["hash"]      = QString::fromUtf8(song->hash().toHex());
		entry["author"]    = song->primaryAuthor().valueOrDefault();
		entry["title"]     = song->primaryTitle().valueOrDefault();
		entry["fileName"]  = song->fileName();
		entry["genre"]     = song->primaryGenre().valueOrDefault();
		entry["mpm"]       = song->primaryMeasuresPerMinute().valueOr(-1);
		entry["index"]     = static_cast<qlonglong>(index);
		entry["ratingRC"]  = song->rating().m_RhythmClarity.valueOr(2.5);
		entry["ratingGT"]  = song->rating().m_GenreTypicality.valueOr(2.5);
		entry["ratingPop"] = song->rating().m_Popularity.valueOr(2.5);
		QJsonDocument doc(entry);
		out.append(doc.toJson());
		out.append(",\r\n");
	}
	out.append("null]");
	a_Socket->write(QByteArray::fromStdString(Http::SimpleOutgoingResponse::serialize(
		200, "OK", "application/json", out.toStdString()
	)));
}





void LocalVoteServer::apiVote(
	QTcpSocket * a_Socket,
	const Http::IncomingRequest & a_Request,
	const std::string & a_RequestBody
)
{
	// Dummy callbacks - we're not handling files here:
	class DummyCallbacks: public Http::FormParser::Callbacks
	{
	public:
		virtual void onFileStart(Http::FormParser &, const std::string &) override {}
		virtual void onFileData(Http::FormParser &, const char *, size_t) override {}
		virtual void onFileEnd(Http::FormParser &) override {}
	} callbacks;

	// Parse the posted parameters:
	Http::FormParser parser(a_Request, callbacks);
	parser.parse(a_RequestBody.data(), a_RequestBody.size());
	if (!parser.finish())
	{
		return send404(a_Socket);
	}

	// Extract and check the parameters:
	auto songHash = parser.find("songHash");
	if ((songHash == parser.end()) || (songHash->second.size() != static_cast<size_t>(2 * m_HashLength)))
	{
		return send404(a_Socket);
	}
	auto voteType = parser.find("voteType");
	if (voteType == parser.end())
	{
		return send404(a_Socket);
	}
	auto voteValue = parser.find("voteValue");
	if (voteValue == parser.end())
	{
		return send404(a_Socket);
	}
	int value = 0;
	if (!Http::Utils::stringToInteger(voteValue->second, value))
	{
		return send404(a_Socket);
	}
	auto hash = QByteArray::fromHex(QByteArray::fromStdString(songHash->second));
	if (hash.length() != m_HashLength)
	{
		return send404(a_Socket);
	}

	// Dispatch the vote according to type:
	if (voteType->second == "rhythmClarity")
	{
		emit addVoteRhythmClarity(hash, value);
	}
	else if (voteType->second == "genreTypicality")
	{
		emit addVoteGenreTypicality(hash, value);
	}
	else if (voteType->second == "popularity")
	{
		emit addVotePopularity(hash, value);
	}
	else
	{
		return send404(a_Socket);
	}

	// TODO: Send an updated rating back to the client

	a_Socket->write(QByteArray::fromStdString(Http::SimpleOutgoingResponse::serialize(
		200, "OK", "text/plain", ""
	)));
}





void LocalVoteServer::onNewConnection()
{
	auto socket = m_Server.nextPendingConnection();
	auto connection = new Connection(*this, socket);
	connect(socket, &QTcpSocket::disconnected, connection, &QObject::deleteLater);
}





void LocalVoteServer::startServer(quint16 a_Port)
{
	m_Server.close();
	m_Server.listen(QHostAddress::Any, a_Port);
	m_Port = a_Port;
	m_IsStarted = true;
}





void LocalVoteServer::stopServer()
{
	m_IsStarted = false;
	m_Port = 0;
	m_Server.close();
}





void LocalVoteServer::startedPlayback(IPlaylistItemPtr a_Item)
{
	// Only interested in songs:
	auto spi = std::dynamic_pointer_cast<PlaylistItemSong>(a_Item);
	if (spi == nullptr)
	{
		return;
	}

	// Save to history:
	m_History.push_back(spi->song());
}

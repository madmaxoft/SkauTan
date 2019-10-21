#include "LocalVoteServer.hpp"
#include <QTcpSocket>
#include <QDebug>
#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QNetworkInterface>
#include <QProcessEnvironment>
#include <LibCppHttpParser/src/Utils.hpp>
#include <LibCppHttpParser/src/Message.hpp>
#include <LibCppHttpParser/src/FormParser.hpp>
#include "Audio/Player.hpp"
#include "Playlist.hpp"
#include "PlaylistItemSong.hpp"





/** Wraps a single TCP connection together with an HTTP parser and buffer for overflowing data. */
class Connection:
	public QObject,
	public Http::MessageParser::Callbacks
{

public:

	Connection(LocalVoteServer & aServer, QTcpSocket * aSocket):
		mServer(aServer),
		mSocket(aSocket),
		mParser(*this)
	{
		connect(mSocket, &QTcpSocket::readyRead,
			[this]()
			{
				while (mSocket->bytesAvailable() > 0)
				{
					auto data = mSocket->read(50);
					auto size = static_cast<size_t>(data.size());
					size_t pos = 0;
					while (size > 0)
					{
						auto consumed = mParser.parse(data.constData() + pos, size);
						if (consumed == std::string::npos)
						{
							mSocket->close();
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
	LocalVoteServer & mServer;

	/** The socket on which to read / write. */
	QTcpSocket * mSocket;

	/** The parser that keeps state of the HTTP protocol. */
	Http::MessageParser mParser;

	/** The request being currently serviced. */
	std::shared_ptr<Http::IncomingRequest> mCurrentRequest;

	/** The body of the request currently being serviced. */
	std::string mCurrentBodyData;





	virtual void onError(const std::string & aErrorDescription) override
	{
		qDebug() << "HTTP Connection error: " << aErrorDescription.c_str();
		mSocket->disconnect();
	}




	virtual void onFirstLine(const std::string & aFirstLine) override
	{
		auto split = Http::Utils::stringSplit(aFirstLine, " ");
		if (split.size() != 3)
		{
			qDebug() << "Invalid first line: " << aFirstLine.c_str();
			mSocket->disconnect();
			return;
		}
		mCurrentRequest.reset(new Http::IncomingRequest(split[0], split[1]));
		mCurrentBodyData.clear();
	}




	virtual void onHeaderLine(const std::string & aKey, const std::string & aValue) override
	{
		if (mCurrentRequest != nullptr)
		{
			mCurrentRequest->addHeader(aKey, aValue);
		}
	}





	virtual void onHeadersFinished() override
	{
	}





	virtual void onBodyData(const void * aData, size_t aSize) override
	{
		mCurrentBodyData.append(reinterpret_cast<const char *>(aData), aSize);
	}





	virtual void onBodyFinished() override
	{
		mServer.handleRequest(mSocket, *mCurrentRequest, mCurrentBodyData);
		if (!mCurrentRequest->doesAllowKeepAlive())
		{
			mSocket->close();
		}
		mCurrentRequest.reset();
		mCurrentBodyData.clear();
		mParser.reset();
	}
};





////////////////////////////////////////////////////////////////////////////////
// LocalVoteServer:

LocalVoteServer::LocalVoteServer(ComponentCollection & aComponents, QObject * aParent):
	Super(aParent),
	mComponents(aComponents),
	mHashLength(QCryptographicHash::hash("", QCryptographicHash::Sha1).length()),
	mIsStarted(false),
	mNumVotes(0)
{
	connect(&mServer, &QTcpServer::newConnection, this, &LocalVoteServer::onNewConnection);
}





void LocalVoteServer::handleRequest(
	QTcpSocket * aSocket,
	const Http::IncomingRequest & aRequest,
	const std::string & aRequestBody
)
{
	qDebug() << "Request: " << aRequest.method().c_str() << " " << aRequest.url().c_str();

	Q_UNUSED(aRequestBody);

	// If the request is for the root, return the root page contents:
	const auto & url = aRequest.url();
	if (url == "/")
	{
		return sendFile(aSocket, "index.html");
	}

	// If the request is for the static data, return them from the disk / resources:
	if (url.compare(0, 8, "/static/") == 0)
	{
		if (url.find("/../") != std::string::npos)
		{
			return send404(aSocket);
		}
		return sendFile(aSocket, url);
	}

	// If the request is for an API, deliver it to the API processor:
	if (url.compare(0, 5, "/api/") == 0)
	{
		return processApi(aSocket, aRequest, aRequestBody);
	}

	// Nothing matched, send a 404:
	return send404(aSocket);
}





quint16 LocalVoteServer::port() const
{
	if (!mIsStarted)
	{
		throw LogicError("Server not running");
	}
	return mPort;
}





void LocalVoteServer::sendFile(QTcpSocket * aSocket, const std::string & aRelativePath)
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
		QFile f(QString::fromStdString(prefix + aRelativePath));
		if (f.open(QIODevice::ReadOnly))
		{
			contents = f.readAll().toStdString();
			auto resp = Http::SimpleOutgoingResponse::serialize(200, "OK", contents);
			aSocket->write(QByteArray::fromStdString(resp));
			qDebug() << "  Sending file " << f.fileName();
			return;
		}
	}
	// File not found, send a 404 error:
	return send404(aSocket);
}





void LocalVoteServer::send404(QTcpSocket * aSocket)
{
	aSocket->write(QByteArray::fromStdString(
		Http::SimpleOutgoingResponse::serialize(
			404, "Not found", "text/plain", "The specified resource was not found"
		)
	));
	qDebug() << "  Sending 404";
}





void LocalVoteServer::processApi(
	QTcpSocket * aSocket,
	const Http::IncomingRequest & aRequest,
	const std::string & aRequestBody
)
{
	const auto & url = aRequest.urlPath();
	using Handler = std::function<void(LocalVoteServer *, QTcpSocket *, const Http::IncomingRequest &, const std::string &)>;
	static const std::map<std::string, Handler> apiMap =
	{
		{"/api/playlist", &LocalVoteServer::apiPlaylist},
		{"/api/vote",     &LocalVoteServer::apiVote},
	};
	auto handler = apiMap.find(url);
	if (handler == apiMap.end())
	{
		return send404(aSocket);
	}
	return (handler->second)(this, aSocket, aRequest, aRequestBody);
}





void LocalVoteServer::apiPlaylist(
	QTcpSocket * aSocket,
	const Http::IncomingRequest & aRequest,
	const std::string & aRequestBody
)
{
	Q_UNUSED(aRequest);
	Q_UNUSED(aRequestBody);

	// Check the requested start against history size:
	auto start = aRequest.headerToNumber("x-skautan-playlist-start", 0ul);
	if (start >= mHistory.size())
	{
		aSocket->write(QByteArray::fromStdString(Http::SimpleOutgoingResponse::serialize(
			200, "OK", "application/json", "[]"
		)));
		qDebug() << "  Sending empty json";
		return;
	}

	// Send all songs in the history:
	QByteArray out ("[");
	auto len = mHistory.size();
	for (size_t index = start; index < len; ++index)
	{
		auto song = mHistory[index];
		QJsonObject entry;
		entry["hash"]      = QString::fromUtf8(song->hash().toHex());
		entry["author"]    = song->primaryAuthor().valueOrDefault();
		entry["title"]     = song->primaryTitle().valueOrDefault();
		entry["fileName"]  = song->fileName();
		entry["genre"]     = song->primaryGenre().valueOrDefault();
		entry["mpm"]       = song->primaryMeasuresPerMinute().valueOr(-1);
		entry["index"]     = static_cast<qlonglong>(index);
		entry["ratingRC"]  = song->rating().mRhythmClarity.valueOr(2.5);
		entry["ratingGT"]  = song->rating().mGenreTypicality.valueOr(2.5);
		entry["ratingPop"] = song->rating().mPopularity.valueOr(2.5);
		QJsonDocument doc(entry);
		out.append(doc.toJson());
		out.append(",\r\n");
	}
	out.append("null]");
	aSocket->write(QByteArray::fromStdString(Http::SimpleOutgoingResponse::serialize(
		200, "OK", "application/json", out.toStdString()
	)));
	qDebug() << "  Sending playlist json";
}





void LocalVoteServer::apiVote(
	QTcpSocket * aSocket,
	const Http::IncomingRequest & aRequest,
	const std::string & aRequestBody
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
	Http::FormParser parser(aRequest, callbacks);
	parser.parse(aRequestBody.data(), aRequestBody.size());
	if (!parser.finish())
	{
		return send404(aSocket);
	}

	// Extract and check the parameters:
	auto songHash = parser.find("songHash");
	if ((songHash == parser.end()) || (songHash->second.size() != static_cast<size_t>(2 * mHashLength)))
	{
		return send404(aSocket);
	}
	auto voteType = parser.find("voteType");
	if (voteType == parser.end())
	{
		return send404(aSocket);
	}
	auto voteValue = parser.find("voteValue");
	if (voteValue == parser.end())
	{
		return send404(aSocket);
	}
	int value = 0;
	if (!Http::Utils::stringToInteger(voteValue->second, value))
	{
		return send404(aSocket);
	}
	auto hash = QByteArray::fromHex(QByteArray::fromStdString(songHash->second));
	if (hash.length() != mHashLength)
	{
		return send404(aSocket);
	}

	// Dispatch the vote according to type:
	if (voteType->second == "rhythmClarity")
	{
		mNumVotes += 1;
		emit addVoteRhythmClarity(hash, value);
	}
	else if (voteType->second == "genreTypicality")
	{
		mNumVotes += 1;
		emit addVoteGenreTypicality(hash, value);
	}
	else if (voteType->second == "popularity")
	{
		mNumVotes += 1;
		emit addVotePopularity(hash, value);
	}
	else
	{
		return send404(aSocket);
	}

	// TODO: Send an updated rating back to the client

	aSocket->write(QByteArray::fromStdString(Http::SimpleOutgoingResponse::serialize(
		200, "OK", "text/plain", ""
	)));
	qDebug() << "  Sending empty OK";
}





void LocalVoteServer::onNewConnection()
{
	auto socket = mServer.nextPendingConnection();
	auto connection = new Connection(*this, socket);
	connect(socket, &QTcpSocket::disconnected, connection, &QObject::deleteLater);
}





void LocalVoteServer::startServer(quint16 aPort)
{
	mServer.close();
	mServer.listen(QHostAddress::Any, aPort);
	mPort = aPort;
	mIsStarted = true;
}





void LocalVoteServer::stopServer()
{
	mIsStarted = false;
	mPort = 0;
	mServer.close();
}





void LocalVoteServer::startedPlayback(IPlaylistItemPtr aItem)
{
	// Only interested in songs:
	auto spi = std::dynamic_pointer_cast<PlaylistItemSong>(aItem);
	if (spi == nullptr)
	{
		return;
	}

	// Save to history:
	mHistory.push_back(spi->song());
}

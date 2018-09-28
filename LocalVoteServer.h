#ifndef LOCALVOTESERVER_H
#define LOCALVOTESERVER_H





#include <set>
#include <QObject>
#include <QTcpServer>
#include "ComponentCollection.h"
#include "lib/HTTP/Message.h"
#include "lib/HTTP/MessageParser.h"
#include "IPlaylistItem.h"





// fwd:
class Song;
using SongPtr = std::shared_ptr<Song>;





class LocalVoteServer:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckLocalVoteServer>
{
	using Super = QObject;
	Q_OBJECT

public:

	/** The default port on which to listen by default. */
	static const quint16 DefaultPort = 7880;


	explicit LocalVoteServer(ComponentCollection & a_Components, QObject * a_Parent = nullptr);

	/** Returns true if the server is started / listening. */
	bool isStarted() const { return m_IsStarted; }

	void handleRequest(
		QTcpSocket * a_Socket,
		const Http::IncomingRequest & a_Request,
		const std::string & a_RequestBody
	);

	/** Returns the port on which the server is reachable.
	Throws if not started. */
	quint16 port() const;


protected:

	/** The components that are used for the API calls. */
	ComponentCollection m_Components;

	/** The actual low-level TCP server. */
	QTcpServer m_Server;

	/** The history of the song that have been played.
	The most recent song is the last one in the vector. */
	std::vector<SongPtr> m_History;

	/** The expected length of the song hash.
	Pre-calculated by SHA1-ing empty data, to avoid having to use crypto hash on each songHash length verification. */
	int m_HashLength;

	/** Specifies whether the server is started.
	Note that QTcpServer's isListening() returns false shortly after starting and returns true shortly after stopping,
	hence we need a proper status stored here. */
	bool m_IsStarted;

	/** The port on which the server is listening. Only valid when started. */
	quint16 m_Port;


	/** Processes the specified data incoming from the specified socket. */
	void processIncomingData(QTcpSocket & a_Socket, const QByteArray & a_Data);

	/** Sends the file at the specified relative path through the specified socket.
	The path is first checked in the filesystem's VoteServer folder, then in the resources' VoteServer folder.
	If the file is not found, a 404 is sent. */
	void sendFile(QTcpSocket * a_Socket, const std::string & a_RelativePath);

	/** Sends a simple 404 error through the specified socket. */
	void send404(QTcpSocket * a_Socket);

	/** Sends the specified data as a HTTP response through the specified socket. */
	void sendData(QTcpSocket * a_Socket, const std::string & a_ContentType, const QByteArray & a_Data);

	/** Processes the API call requested by the client in the specified request. */
	void processApi(
		QTcpSocket * a_Socket,
		const Http::IncomingRequest & a_Request,
		const std::string & a_RequestBody
	);

	/** The handler for the "playlist" API call. */
	void apiPlaylist(
		QTcpSocket * a_Socket,
		const Http::IncomingRequest & a_Request,
		const std::string & a_RequestBody
	);

	/** The handler for the "vote" API call. */
	void apiVote(
		QTcpSocket * a_Socket,
		const Http::IncomingRequest & a_Request,
		const std::string & a_RequestBody
	);


signals:

	/** Emitted when the user clicks a RhythmClarity vote on the webpage. */
	void addVoteRhythmClarity(QByteArray a_SongHash, int a_VoteValue);

	/** Emitted when the user clicks a GenreTypicality vote on the webpage. */
	void addVoteGenreTypicality(QByteArray a_SongHash, int a_VoteValue);

	/** Emitted when the user clicks a Popularity vote on the webpage. */
	void addVotePopularity(QByteArray a_SongHash, int a_VoteValue);


protected slots:

	/** Emitted by m_Server when there's a new incoming connection. */
	void onNewConnection();


public slots:

	/** Starts the local webserver on the specified port.
	If the server is already running, stops it first. */
	void startServer(quint16 a_Port = DefaultPort);

	/** Stops the local webserver.
	Ignored if the server is not running. */
	void stopServer();

	/** Emitted by the player when it starts playing a new item.
	If it is a song, adds it to the history and sends it to all connected interested API clients. */
	void startedPlayback(IPlaylistItemPtr a_Item);

};





#endif // LOCALVOTESERVER_H

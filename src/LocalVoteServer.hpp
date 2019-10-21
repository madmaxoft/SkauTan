#ifndef LOCALVOTESERVER_H
#define LOCALVOTESERVER_H





#include <set>
#include <atomic>
#include <QObject>
#include <QTcpServer>
#include "ComponentCollection.hpp"
#include <LibCppHttpParser/src/Message.hpp>
#include <LibCppHttpParser/src/MessageParser.hpp>
#include "IPlaylistItem.hpp"





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


	explicit LocalVoteServer(ComponentCollection & aComponents, QObject * aParent = nullptr);

	/** Returns true if the server is started / listening. */
	bool isStarted() const { return mIsStarted; }

	void handleRequest(
		QTcpSocket * aSocket,
		const Http::IncomingRequest & aRequest,
		const std::string & aRequestBody
	);

	/** Returns the port on which the server is reachable.
	Throws if not started. */
	quint16 port() const;

	/** Returns the number of votes that were cast since the program start. */
	unsigned numVotes() const { return mNumVotes.load(); }


protected:

	/** The components that are used for the API calls. */
	ComponentCollection mComponents;

	/** The actual low-level TCP server. */
	QTcpServer mServer;

	/** The history of the song that have been played.
	The most recent song is the last one in the vector. */
	std::vector<SongPtr> mHistory;

	/** The expected length of the song hash.
	Pre-calculated by SHA1-ing empty data, to avoid having to use crypto hash on each songHash length verification. */
	int mHashLength;

	/** Specifies whether the server is started.
	Note that QTcpServer's isListening() returns false shortly after starting and returns true shortly after stopping,
	hence we need a proper status stored here. */
	bool mIsStarted;

	/** The port on which the server is listening. Only valid when started. */
	quint16 mPort;

	/** Number of votes that were cast within this app run. */
	std::atomic<unsigned> mNumVotes;


	/** Processes the specified data incoming from the specified socket. */
	void processIncomingData(QTcpSocket & aSocket, const QByteArray & aData);

	/** Sends the file at the specified relative path through the specified socket.
	The path is first checked in the filesystem's VoteServer folder, then in the resources' VoteServer folder.
	If the file is not found, a 404 is sent. */
	void sendFile(QTcpSocket * aSocket, const std::string & aRelativePath);

	/** Sends a simple 404 error through the specified socket. */
	void send404(QTcpSocket * aSocket);

	/** Sends the specified data as a HTTP response through the specified socket. */
	void sendData(QTcpSocket * aSocket, const std::string & aContentType, const QByteArray & aData);

	/** Processes the API call requested by the client in the specified request. */
	void processApi(
		QTcpSocket * aSocket,
		const Http::IncomingRequest & aRequest,
		const std::string & aRequestBody
	);

	/** The handler for the "playlist" API call. */
	void apiPlaylist(
		QTcpSocket * aSocket,
		const Http::IncomingRequest & aRequest,
		const std::string & aRequestBody
	);

	/** The handler for the "vote" API call. */
	void apiVote(
		QTcpSocket * aSocket,
		const Http::IncomingRequest & aRequest,
		const std::string & aRequestBody
	);


signals:

	/** Emitted when the user clicks a RhythmClarity vote on the webpage. */
	void addVoteRhythmClarity(QByteArray aSongHash, int aVoteValue);

	/** Emitted when the user clicks a GenreTypicality vote on the webpage. */
	void addVoteGenreTypicality(QByteArray aSongHash, int aVoteValue);

	/** Emitted when the user clicks a Popularity vote on the webpage. */
	void addVotePopularity(QByteArray aSongHash, int aVoteValue);


protected slots:

	/** Emitted by mServer when there's a new incoming connection. */
	void onNewConnection();


public slots:

	/** Starts the local webserver on the specified port.
	If the server is already running, stops it first. */
	void startServer(quint16 aPort = DefaultPort);

	/** Stops the local webserver.
	Ignored if the server is not running. */
	void stopServer();

	/** Emitted by the player when it starts playing a new item.
	If it is a song, adds it to the history and sends it to all connected interested API clients. */
	void startedPlayback(IPlaylistItemPtr aItem);

};





#endif // LOCALVOTESERVER_H

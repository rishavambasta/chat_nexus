#include "NetworkManager.h"
#include <QDebug>
#include <QUuid>
#include <QFileInfo>
#include <QJsonArray>
#include "../db/DatabaseManager.h"

NetworkManager &NetworkManager::instance() {
    static NetworkManager s_instance;
    return s_instance;
}

NetworkManager::NetworkManager() {
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &NetworkManager::onNewServerConnection);

    m_pollingTimer = new QTimer(this);
    connect(m_pollingTimer, &QTimer::timeout, this, &NetworkManager::onPollingTimerTimeout);
}

NetworkManager::~NetworkManager() {
    stopServer();
    stopPolling();

    for (PeerConnection *conn : m_connections) {
        delete conn;
    }
    m_connections.clear();
}

bool NetworkManager::startServer(quint16 port) {
    if (m_server->isListening()) {
        m_server->close();
    }

    if (!m_server->listen(QHostAddress::AnyIPv4, port)) {
        qWarning() << "Failed to start TCP server on port" << port << ":" << m_server->errorString();
        // Fallback to any available port if requested port is taken
        if (!m_server->listen(QHostAddress::AnyIPv4, 0)) {
            qWarning() << "Failed to start TCP server on any port:" << m_server->errorString();
            return false;
        }
    }

    qDebug() << "P2P Chat server listening on port" << m_server->serverPort();
    DatabaseManager::instance().setListenPort(m_server->serverPort());
    return true;
}

void NetworkManager::stopServer() {
    if (m_server && m_server->isListening()) {
        m_server->close();
    }
}

quint16 NetworkManager::serverPort() const {
    return m_server ? m_server->serverPort() : 0;
}

QString NetworkManager::makeAddressKey(const QString &ip, quint16 port) const {
    QString cleanIp = ip.trimmed();
    if (cleanIp.startsWith("::ffff:")) {
        cleanIp = cleanIp.mid(7);
    }
    return QString("%1:%2").arg(cleanIp).arg(port);
}

void NetworkManager::setupConnection(PeerConnection *conn) {
    connect(conn, &PeerConnection::handshakeReceived, this, &NetworkManager::onHandshakeReceived);
    connect(conn, &PeerConnection::pingReceived, this, &NetworkManager::onPingReceived);
    connect(conn, &PeerConnection::pongReceived, this, &NetworkManager::onPongReceived);
    connect(conn, &PeerConnection::chatMessageReceived, this, &NetworkManager::onChatMessageReceived);
    connect(conn, &PeerConnection::ackDeliveredReceived, this, &NetworkManager::onAckDeliveredReceived);
    connect(conn, &PeerConnection::ackReadReceived, this, &NetworkManager::onAckReadReceived);
    connect(conn, &PeerConnection::fileOfferReceived, this, &NetworkManager::onFileOfferReceived);
    connect(conn, &PeerConnection::fileProgressUpdated, this, &NetworkManager::onFileProgressUpdated);
    connect(conn, &PeerConnection::fileTransferFinished, this, &NetworkManager::onFileTransferFinished);
    connect(conn, &PeerConnection::connectionClosed, this, &NetworkManager::onPeerConnectionClosed);
}

void NetworkManager::onNewServerConnection() {
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        if (!socket) continue;

        PeerConnection *conn = new PeerConnection(socket, this);
        setupConnection(conn);

        // Expect incoming handshake, or we can send our handshake as well
        QString myAlias = DatabaseManager::instance().getUserAlias();
        quint16 myPort = serverPort();
        conn->sendHandshake(myAlias, myPort);
    }
}

void NetworkManager::addPeerManual(const QString &ip, quint16 port,
                                   std::function<void(bool success, QString alias, QString errorMsg)> callback) {
    QTcpSocket *socket = new QTcpSocket(this);
    PeerConnection *conn = new PeerConnection(socket, this);
    conn->setPeerIp(ip);
    conn->setPeerListenPort(port);
    setupConnection(conn);

    QPointer<QTimer> timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);

    auto finished = std::make_shared<bool>(false);

    auto cleanup = [timeoutTimer, finished]() {
        if (*finished) return;
        *finished = true;
        if (timeoutTimer) {
            timeoutTimer->stop();
            timeoutTimer->deleteLater();
        }
    };

    struct ConnectionsHolder {
        QMetaObject::Connection hsConn;
        QMetaObject::Connection errConn;
        QMetaObject::Connection timeConn;
    };
    auto conns = std::make_shared<ConnectionsHolder>();

    conns->hsConn = connect(conn, &PeerConnection::handshakeReceived, this, [this, conn, ip, port, callback, cleanup, conns](PeerConnection *c, const QString &alias, quint16 listenPort) {
        if (c != conn) return;
        cleanup();
        disconnect(conns->errConn);
        disconnect(conns->timeConn);
        disconnect(conns->hsConn);

        // Update peer in DB
        int peerId = DatabaseManager::instance().addOrUpdatePeer(ip, port, alias, true);
        conn->setPeerId(peerId);
        conn->setPeerListenPort(listenPort);

        QString key = makeAddressKey(ip, port);
        if (m_connections.contains(key) && m_connections[key] != conn) {
            delete m_connections[key];
        }
        m_connections[key] = conn;
        m_peerIdToAddressKey[peerId] = key;

        emit newPeerAdded(peerId);
        emit peerOnlineStatusChanged(peerId, true);

        if (callback) {
            callback(true, alias, QString());
        }

        flushQueuedMessages(peerId, conn);
    });

    conns->errConn = connect(socket, static_cast<void(QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
            this, [conn, ip, port, callback, cleanup, conns](QAbstractSocket::SocketError err) {
        disconnect(conns->errConn);
        disconnect(conns->timeConn);
        disconnect(conns->hsConn);
        cleanup();
        QString errStr = conn->socket() ? conn->socket()->errorString() : "Connection failed";
        qWarning() << "addPeerManual error connecting to" << ip << port << ":" << err << errStr;
        if (callback) {
            callback(false, QString(), errStr);
        }
        conn->deleteLater();
    });

    conns->timeConn = connect(timeoutTimer.data(), &QTimer::timeout, this, [conn, callback, cleanup, conns]() {
        disconnect(conns->errConn);
        disconnect(conns->timeConn);
        disconnect(conns->hsConn);
        cleanup();
        if (callback) {
            callback(false, QString(), "Connection timed out (Peer offline or unreachable)");
        }
        conn->deleteLater();
    });

    timeoutTimer->start(4500); // 4.5s timeout for manual add
    socket->connectToHost(ip, port);

    connect(socket, &QTcpSocket::connected, this, [conn]() {
        QString myAlias = DatabaseManager::instance().getUserAlias();
        quint16 myPort = NetworkManager::instance().serverPort();
        conn->sendHandshake(myAlias, myPort);
    });
}

PeerConnection *NetworkManager::ensureConnection(int peerId) {
    if (m_peerIdToAddressKey.contains(peerId)) {
        QString key = m_peerIdToAddressKey[peerId];
        if (m_connections.contains(key)) {
            PeerConnection *conn = m_connections[key];
            if (conn->socket() && conn->socket()->state() == QAbstractSocket::ConnectedState) {
                return conn;
            }
        }
    }

    Peer peer;
    if (!DatabaseManager::instance().getPeerById(peerId, peer)) {
        return nullptr;
    }

    QString key = makeAddressKey(peer.ip, peer.port);
    if (m_connections.contains(key)) {
        PeerConnection *existing = m_connections[key];
        if (existing->socket() && existing->socket()->state() == QAbstractSocket::ConnectedState) {
            m_peerIdToAddressKey[peerId] = key;
            return existing;
        }
        delete existing;
        m_connections.remove(key);
    }

    QTcpSocket *socket = new QTcpSocket(this);
    PeerConnection *conn = new PeerConnection(socket, this);
    conn->setPeerIp(peer.ip);
    conn->setPeerId(peerId);
    conn->setPeerListenPort(peer.port);
    setupConnection(conn);

    m_connections[key] = conn;
    m_peerIdToAddressKey[peerId] = key;

    socket->connectToHost(peer.ip, peer.port);

    connect(socket, &QTcpSocket::connected, this, [this, conn, peerId]() {
        QString myAlias = DatabaseManager::instance().getUserAlias();
        quint16 myPort = serverPort();
        conn->sendHandshake(myAlias, myPort);
        DatabaseManager::instance().updatePeerStatus(peerId, true);
        emit peerOnlineStatusChanged(peerId, true);
        flushQueuedMessages(peerId, conn);
    });

    connect(socket, static_cast<void(QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
            this, [this, peerId](QAbstractSocket::SocketError) {
        DatabaseManager::instance().updatePeerStatus(peerId, false);
        emit peerOnlineStatusChanged(peerId, false);
    });

    return conn;
}

PeerConnection *NetworkManager::getConnectionForPeer(int peerId) {
    if (m_peerIdToAddressKey.contains(peerId)) {
        QString key = m_peerIdToAddressKey[peerId];
        if (m_connections.contains(key)) {
            PeerConnection *conn = m_connections[key];
            if (conn->socket() && conn->socket()->state() == QAbstractSocket::ConnectedState) {
                return conn;
            }
        }
    }
    return nullptr;
}

PeerConnection *NetworkManager::getConnectionForAddress(const QString &ip, quint16 port) {
    QString key = makeAddressKey(ip, port);
    if (m_connections.contains(key)) {
        PeerConnection *conn = m_connections[key];
        if (conn->socket() && conn->socket()->state() == QAbstractSocket::ConnectedState) {
            return conn;
        }
    }
    return nullptr;
}

bool NetworkManager::sendMessage(int peerId, const QString &content, QString *outMsgUid) {
    QString uid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (outMsgUid) *outMsgUid = uid;

    PeerConnection *conn = getConnectionForPeer(peerId);
    bool isConnected = (conn && conn->socket() && conn->socket()->state() == QAbstractSocket::ConnectedState);

    Message msg;
    msg.msgUid = uid;
    msg.peerId = peerId;
    msg.direction = "sent";
    msg.content = content;
    msg.timestamp = QDateTime::currentDateTime();
    msg.status = isConnected ? "queued" : "queued"; // Saved as queued, transitions to delivered upon ACK

    int msgId = DatabaseManager::instance().insertMessage(msg);
    msg.id = msgId;

    if (isConnected) {
        conn->sendChatMessage(uid, content, msg.timestamp);
    } else {
        // Try connecting in the background
        ensureConnection(peerId);
    }

    return true;
}

bool NetworkManager::sendFile(int peerId, const QString &filePath, QString *outTransferUid) {
    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        qWarning() << "File does not exist:" << filePath;
        return false;
    }

    QString uid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (outTransferUid) *outTransferUid = uid;

    PeerConnection *conn = getConnectionForPeer(peerId);
    bool isConnected = (conn && conn->socket() && conn->socket()->state() == QAbstractSocket::ConnectedState);

    FileTransfer ft;
    ft.transferUid = uid;
    ft.peerId = peerId;
    ft.direction = "send";
    ft.fileName = fi.fileName();
    ft.filePath = fi.absoluteFilePath();
    ft.fileSize = fi.size();
    ft.bytesTransferred = 0;
    ft.status = isConnected ? "transferring" : "queued";
    ft.timestamp = QDateTime::currentDateTime();

    DatabaseManager::instance().insertFileTransfer(ft);

    if (isConnected) {
        conn->sendFileOffer(uid, ft.fileName, ft.fileSize);
        conn->startOutgoingFile(uid, ft.filePath, ft.fileSize);
    } else {
        ensureConnection(peerId);
    }

    return true;
}

void NetworkManager::markChatAsRead(int peerId) {
    QStringList readUids;
    DatabaseManager::instance().markPeerMessagesAsRead(peerId, &readUids);
    DatabaseManager::instance().resetUnreadCount(peerId);

    if (!readUids.isEmpty()) {
        PeerConnection *conn = getConnectionForPeer(peerId);
        if (conn) {
            conn->sendAckRead(readUids);
        }
    }
}

void NetworkManager::flushQueuedMessages(int peerId, PeerConnection *conn) {
    if (!conn || !conn->socket() || conn->socket()->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    // 1. Flush queued chat messages
    QVector<Message> queuedMsgs = DatabaseManager::instance().getQueuedMessagesForPeer(peerId);
    for (const Message &msg : queuedMsgs) {
        conn->sendChatMessage(msg.msgUid, msg.content, msg.timestamp);
    }

    // 2. Flush queued file transfers
    QVector<FileTransfer> queuedFts = DatabaseManager::instance().getQueuedFileTransfersForPeer(peerId);
    for (const FileTransfer &ft : queuedFts) {
        DatabaseManager::instance().updateFileTransferStatus(ft.transferUid, "transferring");
        conn->sendFileOffer(ft.transferUid, ft.fileName, ft.fileSize);
        conn->startOutgoingFile(ft.transferUid, ft.filePath, ft.fileSize);
    }
}

void NetworkManager::startPolling(int intervalSeconds) {
    m_pollingTimer->start(intervalSeconds * 1000);
}

void NetworkManager::stopPolling() {
    m_pollingTimer->stop();
}

void NetworkManager::setPollingInterval(int seconds) {
    DatabaseManager::instance().setPollingInterval(seconds);
    if (m_pollingTimer->isActive()) {
        m_pollingTimer->start(seconds * 1000);
    }
}

void NetworkManager::pollAllPeers() {
    QVector<Peer> peers = DatabaseManager::instance().getAllPeers();
    for (const Peer &peer : peers) {
        QString key = makeAddressKey(peer.ip, peer.port);

        if (m_connections.contains(key)) {
            PeerConnection *conn = m_connections[key];
            if (conn->socket() && conn->socket()->state() == QAbstractSocket::ConnectedState) {
                // Send Ping
                conn->sendPing();
                continue;
            }
        }

        // Connection not active, attempt reconnect
        QTcpSocket *socket = new QTcpSocket(this);
        PeerConnection *conn = new PeerConnection(socket, this);
        conn->setPeerIp(peer.ip);
        conn->setPeerId(peer.id);
        conn->setPeerListenPort(peer.port);
        setupConnection(conn);

        int pId = peer.id;
        QString pIp = peer.ip;
        quint16 pPort = peer.port;

        connect(socket, &QTcpSocket::connected, this, [this, conn, pId, pIp, pPort]() {
            QString myAlias = DatabaseManager::instance().getUserAlias();
            quint16 myPort = serverPort();
            conn->sendHandshake(myAlias, myPort);

            QString key = makeAddressKey(pIp, pPort);
            if (m_connections.contains(key) && m_connections[key] != conn) {
                delete m_connections[key];
            }
            m_connections[key] = conn;
            m_peerIdToAddressKey[pId] = key;

            DatabaseManager::instance().updatePeerStatus(pId, true);
            emit peerOnlineStatusChanged(pId, true);

            flushQueuedMessages(pId, conn);
        });

        connect(socket, static_cast<void(QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
                this, [this, conn, pId](QAbstractSocket::SocketError) {
            DatabaseManager::instance().updatePeerStatus(pId, false);
            emit peerOnlineStatusChanged(pId, false);
            conn->deleteLater();
        });

        socket->connectToHost(peer.ip, peer.port);
    }
}

void NetworkManager::onPollingTimerTimeout() {
    pollAllPeers();
}

void NetworkManager::onPeerConnectionClosed(PeerConnection *conn) {
    if (!conn) return;

    int pId = conn->peerId();
    if (pId > 0) {
        DatabaseManager::instance().updatePeerStatus(pId, false);
        emit peerOnlineStatusChanged(pId, false);
    }

    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        if (it.value() == conn) {
            m_connections.erase(it);
            break;
        }
    }

    conn->deleteLater();
}

void NetworkManager::onHandshakeReceived(PeerConnection *conn, const QString &alias, quint16 listenPort) {
    QString ip = conn->peerIp();
    // Resolve peer by IP and listen port
    Peer peer;
    bool found = DatabaseManager::instance().getPeerByAddress(ip, listenPort, peer);

    int peerId = -1;
    if (found) {
        peerId = peer.id;
        DatabaseManager::instance().updatePeerStatus(peerId, true);
        if (!alias.isEmpty() && peer.alias != alias) {
            DatabaseManager::instance().updatePeerAlias(peerId, alias);
            emit peerAliasUpdated(peerId, alias);
        }
    } else {
        // Auto add or update
        peerId = DatabaseManager::instance().addOrUpdatePeer(ip, listenPort, alias, true);
        emit newPeerAdded(peerId);
    }

    conn->setPeerId(peerId);
    conn->setPeerListenPort(listenPort);

    QString key = makeAddressKey(ip, listenPort);
    m_connections[key] = conn;
    m_peerIdToAddressKey[peerId] = key;

    emit peerOnlineStatusChanged(peerId, true);

    flushQueuedMessages(peerId, conn);
}

void NetworkManager::onPingReceived(PeerConnection *conn) {
    int pId = conn->peerId();
    if (pId > 0) {
        DatabaseManager::instance().updatePeerStatus(pId, true);
    }
}

void NetworkManager::onPongReceived(PeerConnection *conn, const QString &alias) {
    int pId = conn->peerId();
    if (pId > 0) {
        DatabaseManager::instance().updatePeerStatus(pId, true);
        if (!alias.isEmpty()) {
            DatabaseManager::instance().updatePeerAlias(pId, alias);
            emit peerAliasUpdated(pId, alias);
        }
        emit peerOnlineStatusChanged(pId, true);
    }
}

void NetworkManager::onChatMessageReceived(PeerConnection *conn, const QString &msgUid, const QString &content, const QDateTime &timestamp) {
    int peerId = conn->peerId();
    if (peerId <= 0) {
        // Find peer by connection address
        Peer peer;
        if (DatabaseManager::instance().getPeerByAddress(conn->peerIp(), conn->peerListenPort(), peer)) {
            peerId = peer.id;
            conn->setPeerId(peerId);
        }
    }

    if (peerId <= 0) return;

    Message msg;
    msg.msgUid = msgUid;
    msg.peerId = peerId;
    msg.direction = "received";
    msg.content = content;
    msg.timestamp = timestamp;

    bool isViewingChat = (m_activePeerId == peerId);
    msg.status = isViewingChat ? "read" : "delivered";

    int id = DatabaseManager::instance().insertMessage(msg);
    msg.id = id;

    // Send delivery ACK immediately!
    conn->sendAckDelivered(QStringList() << msgUid);

    // If active chat, immediately send read ACK as well!
    if (isViewingChat) {
        conn->sendAckRead(QStringList() << msgUid);
    } else {
        DatabaseManager::instance().incrementUnreadCount(peerId);
    }

    emit messageReceived(peerId, msg);
}

void NetworkManager::onAckDeliveredReceived(PeerConnection *, const QStringList &msgUids) {
    for (const QString &uid : msgUids) {
        DatabaseManager::instance().updateMessageStatus(uid, "delivered");
        emit messageStatusUpdated(uid, "delivered");
    }
}

void NetworkManager::onAckReadReceived(PeerConnection *, const QStringList &msgUids) {
    for (const QString &uid : msgUids) {
        DatabaseManager::instance().updateMessageStatus(uid, "read");
        emit messageStatusUpdated(uid, "read");
    }
}

void NetworkManager::onFileOfferReceived(PeerConnection *conn, const QString &transferUid, const QString &fileName, qint64 fileSize) {
    int peerId = conn->peerId();
    if (peerId <= 0) return;

    FileTransfer ft;
    ft.transferUid = transferUid;
    ft.peerId = peerId;
    ft.direction = "receive";
    ft.fileName = fileName;
    ft.filePath = DatabaseManager::instance().getDownloadDirectory() + "/" + fileName;
    ft.fileSize = fileSize;
    ft.bytesTransferred = 0;
    ft.status = "transferring";
    ft.timestamp = QDateTime::currentDateTime();

    DatabaseManager::instance().insertFileTransfer(ft);
}

void NetworkManager::onFileProgressUpdated(const QString &transferUid, qint64 transferred, qint64 total, bool isSender) {
    DatabaseManager::instance().updateFileTransferProgress(transferUid, transferred, "transferring");
    emit fileTransferProgress(transferUid, transferred, total, isSender);
}

void NetworkManager::onFileTransferFinished(const QString &transferUid, bool success, const QString &savedPath, bool isSender) {
    QString status = success ? "completed" : "failed";
    DatabaseManager::instance().updateFileTransferStatus(transferUid, status);
    emit fileTransferFinished(transferUid, success, savedPath, isSender);
}

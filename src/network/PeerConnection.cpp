#include "PeerConnection.h"
#include <QDataStream>
#include <QJsonDocument>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>
#include "../db/DatabaseManager.h"

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
#define SOCKET_ERROR_SIGNAL &QAbstractSocket::errorOccurred
#else
#define SOCKET_ERROR_SIGNAL static_cast<void(QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error)
#endif

static const qint64 CHUNK_SIZE = 64 * 1024; // 64 KB per chunk
static const qint64 MAX_SOCKET_BUFFER = 256 * 1024; // Throttling threshold

PeerConnection::PeerConnection(QTcpSocket *socket, QObject *parent)
    : QObject(parent)
    , m_socket(socket) {
    if (m_socket) {
        if (m_socket->state() == QAbstractSocket::ConnectedState) {
            m_peerIp = m_socket->peerAddress().toString();
            if (m_peerIp.startsWith("::ffff:")) {
                m_peerIp = m_peerIp.mid(7);
            }
            m_peerPort = m_socket->peerPort();
        }

        connect(m_socket, &QTcpSocket::connected, this, [this]() {
            if (m_peerIp.isEmpty() && m_socket) {
                m_peerIp = m_socket->peerAddress().toString();
                if (m_peerIp.startsWith("::ffff:")) {
                    m_peerIp = m_peerIp.mid(7);
                }
                m_peerPort = m_socket->peerPort();
            }
        });

        connect(m_socket, &QTcpSocket::readyRead, this, &PeerConnection::onReadyRead);
        connect(m_socket, &QTcpSocket::disconnected, this, &PeerConnection::onSocketDisconnected);
        connect(m_socket, SOCKET_ERROR_SIGNAL,
                this, &PeerConnection::onSocketError);
        connect(m_socket, &QTcpSocket::bytesWritten, this, &PeerConnection::onBytesWritten);
    }
}

PeerConnection::~PeerConnection() {
    // Clean up open files
    for (auto it = m_incomingFiles.begin(); it != m_incomingFiles.end(); ++it) {
        if (it.value().file) {
            it.value().file->close();
            delete it.value().file;
        }
    }
    m_incomingFiles.clear();

    for (auto it = m_outgoingFiles.begin(); it != m_outgoingFiles.end(); ++it) {
        if (it.value().file) {
            it.value().file->close();
            delete it.value().file;
        }
    }
    m_outgoingFiles.clear();

    if (m_socket) {
        m_socket->disconnect(this);
        if (m_socket->isOpen()) {
            m_socket->close();
        }
        m_socket->deleteLater();
        m_socket = nullptr;
    }
}

void PeerConnection::sendPacket(Protocol::PacketType type, const QJsonObject &obj) {
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) return;
    QByteArray packet = Protocol::packJson(type, obj);
    m_socket->write(packet);
    m_socket->flush();
}

void PeerConnection::sendPing() {
    QJsonObject obj;
    obj["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    sendPacket(Protocol::TypePing, obj);
}

void PeerConnection::sendHandshake(const QString &myAlias, quint16 myListenPort) {
    QJsonObject obj;
    obj["alias"] = myAlias;
    obj["listen_port"] = static_cast<int>(myListenPort);
    sendPacket(Protocol::TypeHandshakeRequest, obj);
}

void PeerConnection::sendChatMessage(const QString &msgUid, const QString &content, const QDateTime &timestamp) {
    QJsonObject obj;
    obj["msg_uid"] = msgUid;
    obj["content"] = content;
    obj["timestamp"] = timestamp.toString(Qt::ISODate);
    sendPacket(Protocol::TypeChatMessage, obj);
}

void PeerConnection::sendAckDelivered(const QStringList &msgUids) {
    if (msgUids.isEmpty()) return;
    QJsonObject obj;
    QJsonArray arr;
    for (const QString &uid : msgUids) arr.append(uid);
    obj["msg_uids"] = arr;
    sendPacket(Protocol::TypeMessageAckDelivered, obj);
}

void PeerConnection::sendAckRead(const QStringList &msgUids) {
    if (msgUids.isEmpty()) return;
    QJsonObject obj;
    QJsonArray arr;
    for (const QString &uid : msgUids) arr.append(uid);
    obj["msg_uids"] = arr;
    sendPacket(Protocol::TypeMessageAckRead, obj);
}

void PeerConnection::sendFileOffer(const QString &transferUid, const QString &fileName, qint64 fileSize) {
    QJsonObject obj;
    obj["transfer_uid"] = transferUid;
    obj["file_name"] = fileName;
    obj["file_size"] = QString::number(fileSize);
    sendPacket(Protocol::TypeFileOffer, obj);
}

void PeerConnection::startOutgoingFile(const QString &transferUid, const QString &filePath, qint64 totalSize) {
    QFile *file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file for sending:" << filePath << file->errorString();
        delete file;
        emit fileTransferFinished(transferUid, false, filePath, true);
        return;
    }

    OutgoingFileState state;
    state.transferUid = transferUid;
    state.filePath = filePath;
    state.totalSize = totalSize > 0 ? totalSize : file->size();
    state.sentBytes = 0;
    state.file = file;
    state.isSending = true;

    m_outgoingFiles.insert(transferUid, state);

    // Start sending
    pumpOutgoingFiles();
}

void PeerConnection::disconnectPeer() {
    if (m_socket && m_socket->isOpen()) {
        m_socket->disconnectFromHost();
    }
}

void PeerConnection::onReadyRead() {
    if (!m_socket) return;
    m_receiveBuffer.append(m_socket->readAll());

    while (true) {
        if (m_readingHeader) {
            // Need at least 12 bytes for header (MAGIC + TYPE + SIZE)
            if (m_receiveBuffer.size() < 12) {
                break;
            }

            QDataStream stream(m_receiveBuffer);
            stream.setByteOrder(QDataStream::BigEndian);

            quint32 magic = 0;
            stream >> magic;
            if (magic != Protocol::MAGIC) {
                qWarning() << "Protocol magic mismatch:" << Qt::hex << magic << "Dropping byte";
                m_receiveBuffer.remove(0, 1);
                continue;
            }

            stream >> m_expectedPacketType;
            stream >> m_expectedPayloadSize;

            m_receiveBuffer.remove(0, 12);
            m_readingHeader = false;
        }

        if (!m_readingHeader) {
            if (static_cast<quint32>(m_receiveBuffer.size()) < m_expectedPayloadSize) {
                // Wait for rest of payload
                break;
            }

            QByteArray payload = m_receiveBuffer.left(m_expectedPayloadSize);
            m_receiveBuffer.remove(0, m_expectedPayloadSize);
            m_readingHeader = true;

            processPacket(m_expectedPacketType, payload);
        }
    }
}

void PeerConnection::processPacket(quint32 type, const QByteArray &payload) {
    if (type == Protocol::TypeFileChunk) {
        handleFileChunkPayload(payload);
        return;
    }

    // JSON based packet
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "Failed to parse JSON packet type:" << type << parseError.errorString();
        return;
    }

    QJsonObject obj = doc.object();

    switch (type) {
    case Protocol::TypeHandshakeRequest: {
        QString alias = obj["alias"].toString();
        quint16 listenPort = static_cast<quint16>(obj["listen_port"].toInt(42424));
        m_peerAlias = alias;
        m_peerListenPort = listenPort;
        m_handshakeComplete = true;

        // Reply with handshake response
        QString myAlias = DatabaseManager::instance().getUserAlias();
        quint16 myPort = DatabaseManager::instance().getListenPort();
        QJsonObject resp;
        resp["alias"] = myAlias;
        resp["listen_port"] = static_cast<int>(myPort);
        sendPacket(Protocol::TypeHandshakeResponse, resp);

        emit handshakeReceived(this, alias, listenPort);
        break;
    }
    case Protocol::TypeHandshakeResponse: {
        QString alias = obj["alias"].toString();
        quint16 listenPort = static_cast<quint16>(obj["listen_port"].toInt(42424));
        m_peerAlias = alias;
        m_peerListenPort = listenPort;
        m_handshakeComplete = true;
        emit handshakeReceived(this, alias, listenPort);
        break;
    }
    case Protocol::TypePing: {
        emit pingReceived(this);
        // Reply with Pong
        QJsonObject resp;
        resp["alias"] = DatabaseManager::instance().getUserAlias();
        resp["timestamp"] = obj["timestamp"];
        sendPacket(Protocol::TypePong, resp);
        break;
    }
    case Protocol::TypePong: {
        QString alias = obj["alias"].toString();
        if (!alias.isEmpty()) {
            m_peerAlias = alias;
        }
        emit pongReceived(this, m_peerAlias);
        break;
    }
    case Protocol::TypeChatMessage: {
        QString msgUid = obj["msg_uid"].toString();
        QString content = obj["content"].toString();
        QDateTime ts = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
        if (!ts.isValid()) ts = QDateTime::currentDateTime();

        emit chatMessageReceived(this, msgUid, content, ts);
        break;
    }
    case Protocol::TypeMessageAckDelivered: {
        QJsonArray arr = obj["msg_uids"].toArray();
        QStringList uids;
        for (const auto &v : arr) uids.append(v.toString());
        emit ackDeliveredReceived(this, uids);
        break;
    }
    case Protocol::TypeMessageAckRead: {
        QJsonArray arr = obj["msg_uids"].toArray();
        QStringList uids;
        for (const auto &v : arr) uids.append(v.toString());
        emit ackReadReceived(this, uids);
        break;
    }
    case Protocol::TypeFileOffer: {
        QString transferUid = obj["transfer_uid"].toString();
        QString fileName = obj["file_name"].toString();
        qint64 fileSize = obj["file_size"].toString().toLongLong();

        // Emit offer received so NetworkManager automatically handles creation & sending Accept
        emit fileOfferReceived(this, transferUid, fileName, fileSize);

        // Automatically accept file transfer as required!
        QString downloadDir = DatabaseManager::instance().getDownloadDirectory();
        QDir().mkpath(downloadDir);

        // Create unique file name if already exists
        QString destPath = downloadDir + "/" + fileName;
        QFileInfo fi(destPath);
        if (fi.exists()) {
            QString baseName = fi.completeBaseName();
            QString suffix = fi.suffix();
            if (!suffix.isEmpty()) suffix = "." + suffix;
            int counter = 1;
            do {
                destPath = QString("%1/%2 (%3)%4").arg(downloadDir).arg(baseName).arg(counter).arg(suffix);
                counter++;
            } while (QFile::exists(destPath));
        }

        QFile *file = new QFile(destPath);
        if (!file->open(QIODevice::WriteOnly)) {
            qWarning() << "Failed to open destination file for incoming transfer:" << destPath;
            delete file;
            emit fileTransferFinished(transferUid, false, destPath, false);
            return;
        }

        IncomingFileState inState;
        inState.transferUid = transferUid;
        inState.fileName = fileName;
        inState.filePath = destPath;
        inState.totalSize = fileSize;
        inState.receivedBytes = 0;
        inState.file = file;

        m_incomingFiles.insert(transferUid, inState);

        // Send FileAccept
        QJsonObject acceptObj;
        acceptObj["transfer_uid"] = transferUid;
        acceptObj["start_offset"] = 0;
        sendPacket(Protocol::TypeFileAccept, acceptObj);
        break;
    }
    case Protocol::TypeFileAccept: {
        QString transferUid = obj["transfer_uid"].toString();
        if (m_outgoingFiles.contains(transferUid)) {
            pumpOutgoingFiles();
        }
        break;
    }
    case Protocol::TypeFileComplete: {
        QString transferUid = obj["transfer_uid"].toString();
        bool success = obj["success"].toBool(true);

        if (m_incomingFiles.contains(transferUid)) {
            IncomingFileState inState = m_incomingFiles.take(transferUid);
            if (inState.file) {
                inState.file->flush();
                inState.file->close();
                delete inState.file;
            }
            emit fileTransferFinished(transferUid, success, inState.filePath, false);
        }
        break;
    }
    default:
        qWarning() << "Unknown packet type:" << type;
        break;
    }
}

void PeerConnection::handleFileChunkPayload(const QByteArray &payload) {
    if (payload.size() < 36 + 8 + 4) {
        qWarning() << "Corrupt file chunk payload size:" << payload.size();
        return;
    }

    QString transferUid = QString::fromUtf8(payload.left(36)).trimmed();

    QDataStream metaStream(payload.mid(36, 12));
    metaStream.setByteOrder(QDataStream::BigEndian);

    quint64 offset = 0;
    quint32 chunkLen = 0;
    metaStream >> offset >> chunkLen;

    QByteArray chunkData = payload.mid(36 + 12, chunkLen);

    if (!m_incomingFiles.contains(transferUid)) {
        qWarning() << "Received chunk for unknown incoming transfer:" << transferUid;
        return;
    }

    IncomingFileState &state = m_incomingFiles[transferUid];
    if (state.file) {
        state.file->seek(offset);
        state.file->write(chunkData);
        state.receivedBytes += chunkData.size();

        emit fileProgressUpdated(transferUid, state.receivedBytes, state.totalSize, false);

        if (state.totalSize > 0 && state.receivedBytes >= state.totalSize) {
            state.file->flush();
            state.file->close();
            delete state.file;
            state.file = nullptr;

            QString savedPath = state.filePath;
            m_incomingFiles.remove(transferUid);

            emit fileTransferFinished(transferUid, true, savedPath, false);

            // Send TypeFileComplete
            QJsonObject compObj;
            compObj["transfer_uid"] = transferUid;
            compObj["success"] = true;
            sendPacket(Protocol::TypeFileComplete, compObj);
        }
    }
}

void PeerConnection::onBytesWritten(qint64) {
    pumpOutgoingFiles();
}

void PeerConnection::pumpOutgoingFiles() {
    if (m_isPumpingFiles) return;
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) return;

    m_isPumpingFiles = true;

    // Check if socket buffer is too full
    if (m_socket->bytesToWrite() > MAX_SOCKET_BUFFER) {
        m_isPumpingFiles = false;
        return;
    }

    for (auto it = m_outgoingFiles.begin(); it != m_outgoingFiles.end();) {
        OutgoingFileState &state = it.value();
        if (!state.isSending || !state.file) {
            ++it;
            continue;
        }

        while (m_socket->bytesToWrite() <= MAX_SOCKET_BUFFER && state.file && !state.file->atEnd()) {
            QByteArray chunk = state.file->read(CHUNK_SIZE);
            if (chunk.isEmpty()) break;

            quint64 offset = state.sentBytes;
            QByteArray packet = Protocol::packFileChunk(state.transferUid, offset, chunk);
            m_socket->write(packet);
            m_socket->flush();

            state.sentBytes += chunk.size();
            emit fileProgressUpdated(state.transferUid, state.sentBytes, state.totalSize, true);
        }

        if (state.file && (state.file->atEnd() || state.sentBytes >= state.totalSize)) {
            state.file->close();
            delete state.file;
            state.file = nullptr;

            QString uid = state.transferUid;
            QString path = state.filePath;

            it = m_outgoingFiles.erase(it);

            // Send complete notification
            QJsonObject compObj;
            compObj["transfer_uid"] = uid;
            compObj["success"] = true;
            sendPacket(Protocol::TypeFileComplete, compObj);

            emit fileTransferFinished(uid, true, path, true);
        } else {
            ++it;
        }
    }

    m_isPumpingFiles = false;
}

void PeerConnection::onSocketDisconnected() {
    emit connectionClosed(this);
}

void PeerConnection::onSocketError(QAbstractSocket::SocketError socketError) {
    qWarning() << "PeerConnection socket error:" << socketError << (m_socket ? m_socket->errorString() : "");
}

#ifndef PEERCONNECTION_H
#define PEERCONNECTION_H

#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QMap>
#include "Protocol.h"

struct IncomingFileState {
    QString transferUid;
    QString fileName;
    QString filePath;
    qint64 totalSize = 0;
    qint64 receivedBytes = 0;
    QFile *file = nullptr;
};

struct OutgoingFileState {
    QString transferUid;
    QString filePath;
    qint64 totalSize = 0;
    qint64 sentBytes = 0;
    QFile *file = nullptr;
    bool isSending = false;
};

class PeerConnection : public QObject {
    Q_OBJECT

public:
    explicit PeerConnection(QTcpSocket *socket, QObject *parent = nullptr);
    ~PeerConnection();

    QTcpSocket *socket() const { return m_socket; }
    QString peerIp() const { return m_peerIp; }
    quint16 peerPort() const { return m_peerPort; }
    quint16 peerListenPort() const { return m_peerListenPort; }
    QString peerAlias() const { return m_peerAlias; }
    int peerId() const { return m_peerId; }
    bool isHandshakeComplete() const { return m_handshakeComplete; }

    void setPeerIp(const QString &ip) { m_peerIp = ip; }
    void setPeerPort(quint16 port) { m_peerPort = port; }
    void setPeerListenPort(quint16 port) { m_peerListenPort = port; }
    void setPeerAlias(const QString &alias) { m_peerAlias = alias; }
    void setPeerId(int id) { m_peerId = id; }

    void sendPacket(Protocol::PacketType type, const QJsonObject &obj);
    void sendPing();
    void sendHandshake(const QString &myAlias, quint16 myListenPort);
    void sendChatMessage(const QString &msgUid, const QString &content, const QDateTime &timestamp);
    void sendAckDelivered(const QStringList &msgUids);
    void sendAckRead(const QStringList &msgUids);
    void sendFileOffer(const QString &transferUid, const QString &fileName, qint64 fileSize);
    void startOutgoingFile(const QString &transferUid, const QString &filePath, qint64 totalSize);
    void disconnectPeer();

signals:
    void handshakeReceived(PeerConnection *conn, const QString &alias, quint16 listenPort);
    void pingReceived(PeerConnection *conn);
    void pongReceived(PeerConnection *conn, const QString &alias);
    void chatMessageReceived(PeerConnection *conn, const QString &msgUid, const QString &content, const QDateTime &timestamp);
    void ackDeliveredReceived(PeerConnection *conn, const QStringList &msgUids);
    void ackReadReceived(PeerConnection *conn, const QStringList &msgUids);
    void fileOfferReceived(PeerConnection *conn, const QString &transferUid, const QString &fileName, qint64 fileSize);
    void fileProgressUpdated(const QString &transferUid, qint64 transferred, qint64 total, bool isSender);
    void fileTransferFinished(const QString &transferUid, bool success, const QString &savedPath, bool isSender);
    void connectionClosed(PeerConnection *conn);

private slots:
    void onReadyRead();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onBytesWritten(qint64 bytes);
    void pumpOutgoingFiles();

private:
    void processPacket(quint32 type, const QByteArray &payload);
    void handleFileChunkPayload(const QByteArray &payload);

    QTcpSocket *m_socket = nullptr;
    QString m_peerIp;
    quint16 m_peerPort = 0;
    quint16 m_peerListenPort = 42424;
    QString m_peerAlias;
    int m_peerId = -1;
    bool m_handshakeComplete = false;

    QByteArray m_receiveBuffer;
    quint32 m_expectedPayloadSize = 0;
    quint32 m_expectedPacketType = 0;
    bool m_readingHeader = true;

    QMap<QString, IncomingFileState> m_incomingFiles;
    QMap<QString, OutgoingFileState> m_outgoingFiles;
    bool m_isPumpingFiles = false;
};

#endif // PEERCONNECTION_H

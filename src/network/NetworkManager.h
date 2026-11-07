#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpServer>
#include <QHostAddress>
#include <QTimer>
#include <QMap>
#include <QPointer>
#include <memory>
#include <functional>
#include "PeerConnection.h"
#include "../models/Peer.h"
#include "../models/Message.h"
#include "../models/FileTransfer.h"

class NetworkManager : public QObject {
    Q_OBJECT

public:
    static NetworkManager &instance();

    bool startServer(quint16 port = 42424);
    void stopServer();
    quint16 serverPort() const;

    // Manual Peer Addition with Alias Fetching
    void addPeerManual(const QString &ip, quint16 port,
                       std::function<void(bool success, QString alias, QString errorMsg)> callback);

    // Messaging & File Transfer
    bool sendMessage(int peerId, const QString &content, QString *outMsgUid = nullptr);
    bool sendFile(int peerId, const QString &filePath, QString *outTransferUid = nullptr);
    void markChatAsRead(int peerId);

    // Active Chat Tracking (for immediate read receipt)
    void setActivePeerId(int peerId) { m_activePeerId = peerId; }
    int activePeerId() const { return m_activePeerId; }

    // Polling
    void startPolling(int intervalSeconds = 15);
    void stopPolling();
    void setPollingInterval(int seconds);
    void pollAllPeers();

    PeerConnection *getConnectionForPeer(int peerId);
    PeerConnection *getConnectionForAddress(const QString &ip, quint16 port);

signals:
    void peerOnlineStatusChanged(int peerId, bool isOnline);
    void peerAliasUpdated(int peerId, const QString &alias);
    void messageReceived(int peerId, const Message &msg);
    void messageStatusUpdated(const QString &msgUid, const QString &status);
    void fileTransferProgress(const QString &transferUid, qint64 bytes, qint64 total, bool isSender);
    void fileTransferFinished(const QString &transferUid, bool success, const QString &filePath, bool isSender);
    void newPeerAdded(int peerId);

private slots:
    void onNewServerConnection();
    void onPollingTimerTimeout();
    void onPeerConnectionClosed(PeerConnection *conn);
    void onHandshakeReceived(PeerConnection *conn, const QString &alias, quint16 listenPort);
    void onPingReceived(PeerConnection *conn);
    void onPongReceived(PeerConnection *conn, const QString &alias);
    void onChatMessageReceived(PeerConnection *conn, const QString &msgUid, const QString &content, const QDateTime &timestamp);
    void onAckDeliveredReceived(PeerConnection *conn, const QStringList &msgUids);
    void onAckReadReceived(PeerConnection *conn, const QStringList &msgUids);
    void onFileOfferReceived(PeerConnection *conn, const QString &transferUid, const QString &fileName, qint64 fileSize);
    void onFileProgressUpdated(const QString &transferUid, qint64 transferred, qint64 total, bool isSender);
    void onFileTransferFinished(const QString &transferUid, bool success, const QString &savedPath, bool isSender);

private:
    NetworkManager();
    ~NetworkManager();
    NetworkManager(const NetworkManager &) = delete;
    NetworkManager &operator=(const NetworkManager &) = delete;

    void setupConnection(PeerConnection *conn);
    PeerConnection *ensureConnection(int peerId);
    void flushQueuedMessages(int peerId, PeerConnection *conn);
    QString makeAddressKey(const QString &ip, quint16 port) const;

    QTcpServer *m_server = nullptr;
    QTimer *m_pollingTimer = nullptr;
    int m_activePeerId = -1;

    // Key is "ip:port"
    QMap<QString, PeerConnection*> m_connections;
    // Map peerId to "ip:port"
    QMap<int, QString> m_peerIdToAddressKey;
};

#endif // NETWORKMANAGER_H

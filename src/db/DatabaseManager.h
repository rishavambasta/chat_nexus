#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVector>
#include <QDateTime>
#include <QMutex>

#include "../models/Peer.h"
#include "../models/Message.h"
#include "../models/FileTransfer.h"

class DatabaseManager : public QObject {
    Q_OBJECT

public:
    static DatabaseManager &instance();

    bool initialize(const QString &dbPath = "p2p_chat.db");
    void close();

    // User Profile
    bool hasUserProfile();
    QString getUserAlias();
    bool setUserAlias(const QString &alias);
    quint16 getListenPort();
    bool setListenPort(quint16 port);
    int getPollingInterval();
    bool setPollingInterval(int seconds);
    QString getDownloadDirectory();
    bool setDownloadDirectory(const QString &dir);

    // Peers
    int addOrUpdatePeer(const QString &ip, quint16 port, const QString &alias, bool isOnline);
    bool updatePeerStatus(int peerId, bool isOnline, const QDateTime &lastSeen = QDateTime::currentDateTime());
    bool updatePeerStatusByAddress(const QString &ip, quint16 port, bool isOnline, const QString &alias = QString());
    bool updatePeerAlias(int peerId, const QString &alias);
    bool deletePeer(int peerId);
    QVector<Peer> getAllPeers();
    bool getPeerById(int peerId, Peer &outPeer);
    bool getPeerByAddress(const QString &ip, quint16 port, Peer &outPeer);
    bool incrementUnreadCount(int peerId);
    bool resetUnreadCount(int peerId);

    // Messages
    int insertMessage(const Message &msg);
    bool updateMessageStatus(const QString &msgUid, const QString &status);
    bool updateMessageStatusById(int id, const QString &status);
    bool markPeerMessagesAsDelivered(int peerId);
    bool markPeerMessagesAsRead(int peerId, QStringList *markedUids = nullptr);
    QVector<Message> getMessagesForPeer(int peerId, int limit = 200);
    QVector<Message> getQueuedMessagesForPeer(int peerId);

    // File Transfers
    int insertFileTransfer(const FileTransfer &ft);
    bool updateFileTransferProgress(const QString &transferUid, qint64 bytesTransferred, const QString &status);
    bool updateFileTransferStatus(const QString &transferUid, const QString &status);
    bool getFileTransferByUid(const QString &transferUid, FileTransfer &outFt);
    QVector<FileTransfer> getFileTransfersForPeer(int peerId);
    QVector<FileTransfer> getQueuedFileTransfersForPeer(int peerId);

signals:
    void peerUpdated(int peerId);
    void messageStatusChanged(const QString &msgUid, const QString &status);
    void fileTransferUpdated(const QString &transferUid, qint64 bytes, const QString &status);

private:
    DatabaseManager();
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager &) = delete;
    DatabaseManager &operator=(const DatabaseManager &) = delete;

    bool createTables();

    QSqlDatabase m_db;
    QMutex m_mutex;
    QString m_connectionName;
};

#endif // DATABASEMANAGER_H

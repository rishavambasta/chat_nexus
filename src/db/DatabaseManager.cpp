#include "DatabaseManager.h"
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QUuid>

DatabaseManager &DatabaseManager::instance() {
    static DatabaseManager s_instance;
    return s_instance;
}

DatabaseManager::DatabaseManager()
    : m_connectionName("P2PChatDB") {
}

DatabaseManager::~DatabaseManager() {
    close();
}

bool DatabaseManager::initialize(const QString &dbPath) {
    QMutexLocker locker(&m_mutex);

    if (QSqlDatabase::contains(m_connectionName)) {
        m_db = QSqlDatabase::database(m_connectionName);
    } else {
        m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    }

    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qWarning() << "Failed to open SQLite database:" << m_db.lastError().text();
        return false;
    }

    // Enable WAL mode and foreign keys for performance and integrity
    QSqlQuery pragmaQuery(m_db);
    pragmaQuery.exec("PRAGMA journal_mode = WAL;");
    pragmaQuery.exec("PRAGMA foreign_keys = ON;");

    return createTables();
}

void DatabaseManager::close() {
    QMutexLocker locker(&m_mutex);
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DatabaseManager::createTables() {
    QSqlQuery query(m_db);

    // 1. User profile and application settings
    bool ok = query.exec(
        "CREATE TABLE IF NOT EXISTS settings ("
        "  key TEXT PRIMARY KEY,"
        "  value TEXT NOT NULL"
        ");"
    );
    if (!ok) {
        qWarning() << "Failed to create settings table:" << query.lastError().text();
        return false;
    }

    // 2. Peers table
    ok = query.exec(
        "CREATE TABLE IF NOT EXISTS peers ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  ip TEXT NOT NULL,"
        "  port INTEGER NOT NULL DEFAULT 42424,"
        "  alias TEXT NOT NULL,"
        "  is_online INTEGER NOT NULL DEFAULT 0,"
        "  last_seen TEXT,"
        "  unread_count INTEGER NOT NULL DEFAULT 0,"
        "  UNIQUE(ip, port)"
        ");"
    );
    if (!ok) {
        qWarning() << "Failed to create peers table:" << query.lastError().text();
        return false;
    }

    // 3. Messages table
    ok = query.exec(
        "CREATE TABLE IF NOT EXISTS messages ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  msg_uid TEXT UNIQUE NOT NULL,"
        "  peer_id INTEGER NOT NULL,"
        "  direction TEXT NOT NULL,"
        "  content TEXT,"
        "  timestamp TEXT NOT NULL,"
        "  status TEXT NOT NULL,"
        "  FOREIGN KEY(peer_id) REFERENCES peers(id) ON DELETE CASCADE"
        ");"
    );
    if (!ok) {
        qWarning() << "Failed to create messages table:" << query.lastError().text();
        return false;
    }

    // Index for quick peer message retrieval
    query.exec("CREATE INDEX IF NOT EXISTS idx_messages_peer_id ON messages(peer_id);");
    query.exec("CREATE INDEX IF NOT EXISTS idx_messages_status ON messages(status);");

    // 4. File transfers table
    ok = query.exec(
        "CREATE TABLE IF NOT EXISTS file_transfers ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  transfer_uid TEXT UNIQUE NOT NULL,"
        "  peer_id INTEGER NOT NULL,"
        "  direction TEXT NOT NULL,"
        "  file_name TEXT NOT NULL,"
        "  file_path TEXT NOT NULL,"
        "  file_size INTEGER NOT NULL,"
        "  bytes_transferred INTEGER NOT NULL DEFAULT 0,"
        "  status TEXT NOT NULL,"
        "  timestamp TEXT NOT NULL,"
        "  FOREIGN KEY(peer_id) REFERENCES peers(id) ON DELETE CASCADE"
        ");"
    );
    if (!ok) {
        qWarning() << "Failed to create file_transfers table:" << query.lastError().text();
        return false;
    }

    query.exec("CREATE INDEX IF NOT EXISTS idx_file_transfers_peer_id ON file_transfers(peer_id);");

    return true;
}

// User Profile
bool DatabaseManager::hasUserProfile() {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("SELECT value FROM settings WHERE key = 'user_alias';");
    if (query.exec() && query.next()) {
        QString alias = query.value(0).toString().trimmed();
        return !alias.isEmpty();
    }
    return false;
}

QString DatabaseManager::getUserAlias() {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("SELECT value FROM settings WHERE key = 'user_alias';");
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return QString();
}

bool DatabaseManager::setUserAlias(const QString &alias) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES ('user_alias', :val);");
    query.bindValue(":val", alias);
    return query.exec();
}

quint16 DatabaseManager::getListenPort() {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("SELECT value FROM settings WHERE key = 'listen_port';");
    if (query.exec() && query.next()) {
        bool ok = false;
        quint16 port = query.value(0).toUInt(&ok);
        if (ok && port > 0) return port;
    }
    return 42424; // Default port
}

bool DatabaseManager::setListenPort(quint16 port) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES ('listen_port', :val);");
    query.bindValue(":val", QString::number(port));
    return query.exec();
}

int DatabaseManager::getPollingInterval() {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("SELECT value FROM settings WHERE key = 'polling_interval';");
    if (query.exec() && query.next()) {
        bool ok = false;
        int interval = query.value(0).toInt(&ok);
        if (ok && interval > 0) return interval;
    }
    return 15; // Default 15 seconds
}

bool DatabaseManager::setPollingInterval(int seconds) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES ('polling_interval', :val);");
    query.bindValue(":val", QString::number(seconds));
    return query.exec();
}

QString DatabaseManager::getDownloadDirectory() {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("SELECT value FROM settings WHERE key = 'download_directory';");
    if (query.exec() && query.next()) {
        QString dir = query.value(0).toString();
        if (!dir.isEmpty() && QDir(dir).exists()) return dir;
    }
    // Default fallback to Downloads/P2PChat
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/P2PChat";
    QDir().mkpath(defaultPath);
    return defaultPath;
}

bool DatabaseManager::setDownloadDirectory(const QString &dir) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES ('download_directory', :val);");
    query.bindValue(":val", dir);
    return query.exec();
}

// Peers
int DatabaseManager::addOrUpdatePeer(const QString &ip, quint16 port, const QString &alias, bool isOnline) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);

    query.prepare(
        "INSERT INTO peers (ip, port, alias, is_online, last_seen) "
        "VALUES (:ip, :port, :alias, :online, :seen) "
        "ON CONFLICT(ip, port) DO UPDATE SET "
        "  alias = CASE WHEN :alias != '' THEN :alias ELSE peers.alias END, "
        "  is_online = :online, "
        "  last_seen = :seen;"
    );
    query.bindValue(":ip", ip.trimmed());
    query.bindValue(":port", port);
    query.bindValue(":alias", alias.trimmed());
    query.bindValue(":online", isOnline ? 1 : 0);
    query.bindValue(":seen", QDateTime::currentDateTime().toString(Qt::ISODate));

    if (!query.exec()) {
        qWarning() << "addOrUpdatePeer error:" << query.lastError().text();
        return -1;
    }

    // Retrieve peer id
    QSqlQuery idQuery(m_db);
    idQuery.prepare("SELECT id FROM peers WHERE ip = :ip AND port = :port;");
    idQuery.bindValue(":ip", ip.trimmed());
    idQuery.bindValue(":port", port);
    if (idQuery.exec() && idQuery.next()) {
        int peerId = idQuery.value(0).toInt();
        emit peerUpdated(peerId);
        return peerId;
    }

    return -1;
}

bool DatabaseManager::updatePeerStatus(int peerId, bool isOnline, const QDateTime &lastSeen) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("UPDATE peers SET is_online = :online, last_seen = :seen WHERE id = :id;");
    query.bindValue(":online", isOnline ? 1 : 0);
    query.bindValue(":seen", lastSeen.toString(Qt::ISODate));
    query.bindValue(":id", peerId);
    bool ok = query.exec();
    if (ok) {
        emit peerUpdated(peerId);
    }
    return ok;
}

bool DatabaseManager::updatePeerStatusByAddress(const QString &ip, quint16 port, bool isOnline, const QString &alias) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    if (!alias.trimmed().isEmpty()) {
        query.prepare("UPDATE peers SET is_online = :online, last_seen = :seen, alias = :alias WHERE ip = :ip AND port = :port;");
        query.bindValue(":alias", alias.trimmed());
    } else {
        query.prepare("UPDATE peers SET is_online = :online, last_seen = :seen WHERE ip = :ip AND port = :port;");
    }
    query.bindValue(":online", isOnline ? 1 : 0);
    query.bindValue(":seen", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":ip", ip.trimmed());
    query.bindValue(":port", port);

    bool ok = query.exec();
    if (ok) {
        QSqlQuery idQuery(m_db);
        idQuery.prepare("SELECT id FROM peers WHERE ip = :ip AND port = :port;");
        idQuery.bindValue(":ip", ip.trimmed());
        idQuery.bindValue(":port", port);
        if (idQuery.exec() && idQuery.next()) {
            emit peerUpdated(idQuery.value(0).toInt());
        }
    }
    return ok;
}

bool DatabaseManager::updatePeerAlias(int peerId, const QString &alias) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("UPDATE peers SET alias = :alias WHERE id = :id;");
    query.bindValue(":alias", alias.trimmed());
    query.bindValue(":id", peerId);
    bool ok = query.exec();
    if (ok) {
        emit peerUpdated(peerId);
    }
    return ok;
}

bool DatabaseManager::deletePeer(int peerId) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM peers WHERE id = :id;");
    query.bindValue(":id", peerId);
    return query.exec();
}

QVector<Peer> DatabaseManager::getAllPeers() {
    QMutexLocker locker(&m_mutex);
    QVector<Peer> list;
    QSqlQuery query(m_db);

    // Join with latest message snippet and timestamp
    query.exec(
        "SELECT p.id, p.ip, p.port, p.alias, p.is_online, p.last_seen, p.unread_count, "
        "       m.content, m.timestamp "
        "FROM peers p "
        "LEFT JOIN ( "
        "    SELECT peer_id, content, timestamp "
        "    FROM messages "
        "    WHERE id IN (SELECT MAX(id) FROM messages GROUP BY peer_id) "
        ") m ON p.id = m.peer_id "
        "ORDER BY p.is_online DESC, COALESCE(m.timestamp, p.last_seen) DESC;"
    );

    while (query.next()) {
        Peer peer;
        peer.id = query.value(0).toInt();
        peer.ip = query.value(1).toString();
        peer.port = static_cast<quint16>(query.value(2).toUInt());
        peer.alias = query.value(3).toString();
        peer.isOnline = query.value(4).toInt() == 1;
        peer.lastSeen = QDateTime::fromString(query.value(5).toString(), Qt::ISODate);
        peer.unreadCount = query.value(6).toInt();
        peer.lastMessageSnippet = query.value(7).toString();
        peer.lastMessageTime = QDateTime::fromString(query.value(8).toString(), Qt::ISODate);
        list.append(peer);
    }

    return list;
}

bool DatabaseManager::getPeerById(int peerId, Peer &outPeer) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("SELECT id, ip, port, alias, is_online, last_seen, unread_count FROM peers WHERE id = :id;");
    query.bindValue(":id", peerId);
    if (query.exec() && query.next()) {
        outPeer.id = query.value(0).toInt();
        outPeer.ip = query.value(1).toString();
        outPeer.port = static_cast<quint16>(query.value(2).toUInt());
        outPeer.alias = query.value(3).toString();
        outPeer.isOnline = query.value(4).toInt() == 1;
        outPeer.lastSeen = QDateTime::fromString(query.value(5).toString(), Qt::ISODate);
        outPeer.unreadCount = query.value(6).toInt();
        return true;
    }
    return false;
}

bool DatabaseManager::getPeerByAddress(const QString &ip, quint16 port, Peer &outPeer) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("SELECT id, ip, port, alias, is_online, last_seen, unread_count FROM peers WHERE ip = :ip AND port = :port;");
    query.bindValue(":ip", ip.trimmed());
    query.bindValue(":port", port);
    if (query.exec() && query.next()) {
        outPeer.id = query.value(0).toInt();
        outPeer.ip = query.value(1).toString();
        outPeer.port = static_cast<quint16>(query.value(2).toUInt());
        outPeer.alias = query.value(3).toString();
        outPeer.isOnline = query.value(4).toInt() == 1;
        outPeer.lastSeen = QDateTime::fromString(query.value(5).toString(), Qt::ISODate);
        outPeer.unreadCount = query.value(6).toInt();
        return true;
    }
    return false;
}

bool DatabaseManager::incrementUnreadCount(int peerId) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("UPDATE peers SET unread_count = unread_count + 1 WHERE id = :id;");
    query.bindValue(":id", peerId);
    bool ok = query.exec();
    if (ok) emit peerUpdated(peerId);
    return ok;
}

bool DatabaseManager::resetUnreadCount(int peerId) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("UPDATE peers SET unread_count = 0 WHERE id = :id;");
    query.bindValue(":id", peerId);
    bool ok = query.exec();
    if (ok) emit peerUpdated(peerId);
    return ok;
}

// Messages
int DatabaseManager::insertMessage(const Message &msg) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare(
        "INSERT INTO messages (msg_uid, peer_id, direction, content, timestamp, status) "
        "VALUES (:uid, :pid, :dir, :content, :ts, :status);"
    );
    query.bindValue(":uid", msg.msgUid.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : msg.msgUid);
    query.bindValue(":pid", msg.peerId);
    query.bindValue(":dir", msg.direction);
    query.bindValue(":content", msg.content);
    query.bindValue(":ts", msg.timestamp.isValid() ? msg.timestamp.toString(Qt::ISODate) : QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":status", msg.status);

    if (query.exec()) {
        int id = query.lastInsertId().toInt();
        emit peerUpdated(msg.peerId);
        return id;
    }
    qWarning() << "insertMessage error:" << query.lastError().text();
    return -1;
}

bool DatabaseManager::updateMessageStatus(const QString &msgUid, const QString &status) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("UPDATE messages SET status = :status WHERE msg_uid = :uid;");
    query.bindValue(":status", status);
    query.bindValue(":uid", msgUid);
    bool ok = query.exec();
    if (ok) {
        emit messageStatusChanged(msgUid, status);
    }
    return ok;
}

bool DatabaseManager::updateMessageStatusById(int id, const QString &status) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("UPDATE messages SET status = :status WHERE id = :id;");
    query.bindValue(":status", status);
    query.bindValue(":id", id);
    return query.exec();
}

bool DatabaseManager::markPeerMessagesAsDelivered(int peerId) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("UPDATE messages SET status = 'delivered' WHERE peer_id = :pid AND direction = 'sent' AND status = 'queued';");
    query.bindValue(":pid", peerId);
    return query.exec();
}

bool DatabaseManager::markPeerMessagesAsRead(int peerId, QStringList *markedUids) {
    QMutexLocker locker(&m_mutex);
    if (markedUids) {
        QSqlQuery sel(m_db);
        sel.prepare("SELECT msg_uid FROM messages WHERE peer_id = :pid AND direction = 'received' AND status != 'read';");
        sel.bindValue(":pid", peerId);
        if (sel.exec()) {
            while (sel.next()) {
                markedUids->append(sel.value(0).toString());
            }
        }
    }

    QSqlQuery query(m_db);
    query.prepare("UPDATE messages SET status = 'read' WHERE peer_id = :pid AND direction = 'received' AND status != 'read';");
    query.bindValue(":pid", peerId);
    return query.exec();
}

QVector<Message> DatabaseManager::getMessagesForPeer(int peerId, int limit) {
    QMutexLocker locker(&m_mutex);
    QVector<Message> list;
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT id, msg_uid, peer_id, direction, content, timestamp, status "
        "FROM messages WHERE peer_id = :pid ORDER BY id ASC LIMIT :lim;"
    );
    query.bindValue(":pid", peerId);
    query.bindValue(":lim", limit);

    if (query.exec()) {
        while (query.next()) {
            Message m;
            m.id = query.value(0).toInt();
            m.msgUid = query.value(1).toString();
            m.peerId = query.value(2).toInt();
            m.direction = query.value(3).toString();
            m.content = query.value(4).toString();
            m.timestamp = QDateTime::fromString(query.value(5).toString(), Qt::ISODate);
            m.status = query.value(6).toString();
            list.append(m);
        }
    }
    return list;
}

QVector<Message> DatabaseManager::getQueuedMessagesForPeer(int peerId) {
    QMutexLocker locker(&m_mutex);
    QVector<Message> list;
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT id, msg_uid, peer_id, direction, content, timestamp, status "
        "FROM messages WHERE peer_id = :pid AND direction = 'sent' AND status = 'queued' ORDER BY id ASC;"
    );
    query.bindValue(":pid", peerId);

    if (query.exec()) {
        while (query.next()) {
            Message m;
            m.id = query.value(0).toInt();
            m.msgUid = query.value(1).toString();
            m.peerId = query.value(2).toInt();
            m.direction = query.value(3).toString();
            m.content = query.value(4).toString();
            m.timestamp = QDateTime::fromString(query.value(5).toString(), Qt::ISODate);
            m.status = query.value(6).toString();
            list.append(m);
        }
    }
    return list;
}

// File Transfers
int DatabaseManager::insertFileTransfer(const FileTransfer &ft) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare(
        "INSERT INTO file_transfers (transfer_uid, peer_id, direction, file_name, file_path, file_size, bytes_transferred, status, timestamp) "
        "VALUES (:uid, :pid, :dir, :name, :path, :size, :transferred, :status, :ts);"
    );
    query.bindValue(":uid", ft.transferUid.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : ft.transferUid);
    query.bindValue(":pid", ft.peerId);
    query.bindValue(":dir", ft.direction);
    query.bindValue(":name", ft.fileName);
    query.bindValue(":path", ft.filePath);
    query.bindValue(":size", ft.fileSize);
    query.bindValue(":transferred", ft.bytesTransferred);
    query.bindValue(":status", ft.status);
    query.bindValue(":ts", ft.timestamp.isValid() ? ft.timestamp.toString(Qt::ISODate) : QDateTime::currentDateTime().toString(Qt::ISODate));

    if (query.exec()) {
        int id = query.lastInsertId().toInt();
        emit peerUpdated(ft.peerId);
        return id;
    }
    qWarning() << "insertFileTransfer error:" << query.lastError().text();
    return -1;
}

bool DatabaseManager::updateFileTransferProgress(const QString &transferUid, qint64 bytesTransferred, const QString &status) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("UPDATE file_transfers SET bytes_transferred = :b, status = :s WHERE transfer_uid = :uid;");
    query.bindValue(":b", bytesTransferred);
    query.bindValue(":s", status);
    query.bindValue(":uid", transferUid);
    bool ok = query.exec();
    if (ok) {
        emit fileTransferUpdated(transferUid, bytesTransferred, status);
    }
    return ok;
}

bool DatabaseManager::updateFileTransferStatus(const QString &transferUid, const QString &status) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("UPDATE file_transfers SET status = :s WHERE transfer_uid = :uid;");
    query.bindValue(":s", status);
    query.bindValue(":uid", transferUid);
    bool ok = query.exec();
    if (ok) {
        emit fileTransferUpdated(transferUid, 0, status);
    }
    return ok;
}

bool DatabaseManager::getFileTransferByUid(const QString &transferUid, FileTransfer &outFt) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("SELECT id, transfer_uid, peer_id, direction, file_name, file_path, file_size, bytes_transferred, status, timestamp "
                  "FROM file_transfers WHERE transfer_uid = :uid;");
    query.bindValue(":uid", transferUid);
    if (query.exec() && query.next()) {
        outFt.id = query.value(0).toInt();
        outFt.transferUid = query.value(1).toString();
        outFt.peerId = query.value(2).toInt();
        outFt.direction = query.value(3).toString();
        outFt.fileName = query.value(4).toString();
        outFt.filePath = query.value(5).toString();
        outFt.fileSize = query.value(6).toLongLong();
        outFt.bytesTransferred = query.value(7).toLongLong();
        outFt.status = query.value(8).toString();
        outFt.timestamp = QDateTime::fromString(query.value(9).toString(), Qt::ISODate);
        return true;
    }
    return false;
}

QVector<FileTransfer> DatabaseManager::getFileTransfersForPeer(int peerId) {
    QMutexLocker locker(&m_mutex);
    QVector<FileTransfer> list;
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT id, transfer_uid, peer_id, direction, file_name, file_path, file_size, bytes_transferred, status, timestamp "
        "FROM file_transfers WHERE peer_id = :pid ORDER BY id ASC;"
    );
    query.bindValue(":pid", peerId);

    if (query.exec()) {
        while (query.next()) {
            FileTransfer ft;
            ft.id = query.value(0).toInt();
            ft.transferUid = query.value(1).toString();
            ft.peerId = query.value(2).toInt();
            ft.direction = query.value(3).toString();
            ft.fileName = query.value(4).toString();
            ft.filePath = query.value(5).toString();
            ft.fileSize = query.value(6).toLongLong();
            ft.bytesTransferred = query.value(7).toLongLong();
            ft.status = query.value(8).toString();
            ft.timestamp = QDateTime::fromString(query.value(9).toString(), Qt::ISODate);
            list.append(ft);
        }
    }
    return list;
}

QVector<FileTransfer> DatabaseManager::getQueuedFileTransfersForPeer(int peerId) {
    QMutexLocker locker(&m_mutex);
    QVector<FileTransfer> list;
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT id, transfer_uid, peer_id, direction, file_name, file_path, file_size, bytes_transferred, status, timestamp "
        "FROM file_transfers WHERE peer_id = :pid AND direction = 'send' AND status = 'queued' ORDER BY id ASC;"
    );
    query.bindValue(":pid", peerId);

    if (query.exec()) {
        while (query.next()) {
            FileTransfer ft;
            ft.id = query.value(0).toInt();
            ft.transferUid = query.value(1).toString();
            ft.peerId = query.value(2).toInt();
            ft.direction = query.value(3).toString();
            ft.fileName = query.value(4).toString();
            ft.filePath = query.value(5).toString();
            ft.fileSize = query.value(6).toLongLong();
            ft.bytesTransferred = query.value(7).toLongLong();
            ft.status = query.value(8).toString();
            ft.timestamp = QDateTime::fromString(query.value(9).toString(), Qt::ISODate);
            list.append(ft);
        }
    }
    return list;
}

#include <QCoreApplication>
#include <QTest>
#include <QSignalSpy>
#include <QFile>
#include <QCryptographicHash>
#include <QTemporaryDir>
#include <QDebug>
#include "../src/db/DatabaseManager.h"
#include "../src/network/NetworkManager.h"
#include "../src/network/Protocol.h"

class TestP2P : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void testDatabaseOperations();
    void testOfflineQueueingInDatabase();
    void testTicksStatusTransitions();
    void testNetworkHandshakeAndMessaging();
    void testFileTransferStream();
    void testTwoNodeLiveP2PExchange();
    void cleanupTestCase();

private:
    QTemporaryDir m_tempDir;
    QString m_dbPath;
};

void TestP2P::initTestCase() {
    QVERIFY(m_tempDir.isValid());
    m_dbPath = m_tempDir.filePath("test_chat.db");
    QVERIFY(DatabaseManager::instance().initialize(m_dbPath));
}

void TestP2P::testDatabaseOperations() {
    // 1. User Profile with Spaces
    QString myAlias = "Rishav The Explorer";
    QVERIFY(DatabaseManager::instance().setUserAlias(myAlias));
    QCOMPARE(DatabaseManager::instance().getUserAlias(), myAlias);
    QVERIFY(DatabaseManager::instance().hasUserProfile());

    // 2. Add Peer
    QString peerIp = "192.168.1.50";
    quint16 peerPort = 42424;
    QString peerAlias = "Captain Nemo";
    int peerId = DatabaseManager::instance().addOrUpdatePeer(peerIp, peerPort, peerAlias, true);
    QVERIFY(peerId > 0);

    Peer fetchedPeer;
    QVERIFY(DatabaseManager::instance().getPeerById(peerId, fetchedPeer));
    QCOMPARE(fetchedPeer.alias, peerAlias);
    QCOMPARE(fetchedPeer.ip, peerIp);
    QCOMPARE(fetchedPeer.port, peerPort);
    QVERIFY(fetchedPeer.isOnline);

    // 3. Update status to offline
    QVERIFY(DatabaseManager::instance().updatePeerStatus(peerId, false));
    QVERIFY(DatabaseManager::instance().getPeerById(peerId, fetchedPeer));
    QVERIFY(!fetchedPeer.isOnline);

    // 4. Update alias
    QVERIFY(DatabaseManager::instance().updatePeerAlias(peerId, "Captain Nemo Updated"));
    QVERIFY(DatabaseManager::instance().getPeerById(peerId, fetchedPeer));
    QCOMPARE(fetchedPeer.alias, QString("Captain Nemo Updated"));
}

void TestP2P::testOfflineQueueingInDatabase() {
    Peer peer;
    QVERIFY(DatabaseManager::instance().getPeerByAddress("192.168.1.50", 42424, peer));

    // Insert queued message while peer is offline
    Message m1;
    m1.msgUid = "msg-uuid-1001";
    m1.peerId = peer.id;
    m1.direction = "sent";
    m1.content = "Message queued while offline";
    m1.timestamp = QDateTime::currentDateTime();
    m1.status = "queued";

    int id = DatabaseManager::instance().insertMessage(m1);
    QVERIFY(id > 0);

    // Verify queued messages retrieval
    QVector<Message> queued = DatabaseManager::instance().getQueuedMessagesForPeer(peer.id);
    QCOMPARE(queued.size(), 1);
    QCOMPARE(queued.first().msgUid, QString("msg-uuid-1001"));
    QCOMPARE(queued.first().status, QString("queued"));
    QVERIFY(queued.first().isQueued());
}

void TestP2P::testTicksStatusTransitions() {
    Peer peer;
    QVERIFY(DatabaseManager::instance().getPeerByAddress("192.168.1.50", 42424, peer));

    QString uid = "msg-ticks-test";
    Message m;
    m.msgUid = uid;
    m.peerId = peer.id;
    m.direction = "sent";
    m.content = "Testing WhatsApp Ticks";
    m.timestamp = QDateTime::currentDateTime();
    m.status = "queued";

    DatabaseManager::instance().insertMessage(m);

    // Transition 1: Peer receives message -> 2 grey ticks ("delivered")
    QVERIFY(DatabaseManager::instance().updateMessageStatus(uid, "delivered"));
    QVector<Message> msgs = DatabaseManager::instance().getMessagesForPeer(peer.id);
    bool foundDelivered = false;
    for (const Message &msg : msgs) {
        if (msg.msgUid == uid) {
            QCOMPARE(msg.status, QString("delivered"));
            QVERIFY(msg.isDelivered());
            QVERIFY(!msg.isRead());
            foundDelivered = true;
            break;
        }
    }
    QVERIFY(foundDelivered);

    // Transition 2: Peer reads message -> 2 blue ticks ("read")
    QVERIFY(DatabaseManager::instance().updateMessageStatus(uid, "read"));
    msgs = DatabaseManager::instance().getMessagesForPeer(peer.id);
    bool foundRead = false;
    for (const Message &msg : msgs) {
        if (msg.msgUid == uid) {
            QCOMPARE(msg.status, QString("read"));
            QVERIFY(msg.isRead());
            foundRead = true;
            break;
        }
    }
    QVERIFY(foundRead);
}

void TestP2P::testNetworkHandshakeAndMessaging() {
    // Start NetworkManager server on ephemeral port
    QVERIFY(NetworkManager::instance().startServer(0));
    quint16 srvPort = NetworkManager::instance().serverPort();
    QVERIFY(srvPort > 0);

    // Simulate peer handshake connection via TCP socket
    QTcpSocket client;
    client.connectToHost("127.0.0.1", srvPort);
    QVERIFY(client.waitForConnected(2000));
    QTest::qWait(100); // Allow QTcpServer to process newConnection()

    // Send Handshake from simulated peer "Bob Peer"
    QJsonObject hsObj;
    hsObj["alias"] = "Bob The Peer";
    hsObj["listen_port"] = 55555;
    QByteArray packet = Protocol::packJson(Protocol::TypeHandshakeRequest, hsObj);
    client.write(packet);
    QVERIFY(client.waitForBytesWritten(1000));

    // Wait for Handshake response from NetworkManager
    if (client.bytesAvailable() == 0) {
        client.waitForReadyRead(2000);
    }
    QByteArray respData = client.readAll();
    QVERIFY(respData.size() >= 12);

    // Allow event loop to process incoming handshake on server side
    QTest::qWait(150);

    // Send a chat message from client to server
    QJsonObject msgObj;
    msgObj["msg_uid"] = "net-test-msg-01";
    msgObj["content"] = "Hello from network test!";
    msgObj["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    client.write(Protocol::packJson(Protocol::TypeChatMessage, msgObj));
    client.waitForBytesWritten(1000);

    // Allow server to process chat message and send delivery ACK
    QTest::qWait(150);

    // Wait for delivery ACK from server
    QVERIFY(client.waitForReadyRead(3000) || client.bytesAvailable() > 0);
    QByteArray ackData = client.readAll();
    QVERIFY(ackData.contains("net-test-msg-01"));

    client.disconnectFromHost();
}

void TestP2P::testFileTransferStream() {
    // Test file transfer DB model and size formatting
    FileTransfer ft;
    ft.transferUid = "file-test-uid";
    ft.peerId = 1;
    ft.direction = "send";
    ft.fileName = "test_document.txt";
    ft.filePath = "/tmp/test_document.txt";
    ft.fileSize = 1024 * 1024; // 1 MB
    ft.bytesTransferred = 512 * 1024; // 50%
    ft.status = "transferring";
    ft.timestamp = QDateTime::currentDateTime();

    QCOMPARE(ft.progressPercentage(), 50);
    QCOMPARE(FileTransfer::formatSize(1024 * 1024), QString("1.0 MB"));

    int id = DatabaseManager::instance().insertFileTransfer(ft);
    QVERIFY(id > 0);

    QVERIFY(DatabaseManager::instance().updateFileTransferProgress("file-test-uid", 1024 * 1024, "completed"));
    FileTransfer outFt;
    QVERIFY(DatabaseManager::instance().getFileTransferByUid("file-test-uid", outFt));
    QCOMPARE(outFt.progressPercentage(), 100);
    QCOMPARE(outFt.status, QString("completed"));
}

void TestP2P::testTwoNodeLiveP2PExchange() {
    // 1. Create a simulated peer node (Bob) using a standalone QTcpServer
    QTcpServer bobServer;
    QVERIFY(bobServer.listen(QHostAddress::LocalHost, 0));
    quint16 bobPort = bobServer.serverPort();

    QString bobAlias = "Bob Marley";
    QTcpSocket *bobSocket = nullptr;
    QString receivedMsgUid;
    QString receivedMsgContent;
    QString receivedFileUid;
    QByteArray receivedFileData;
    QByteArray bobReceiveBuffer;
    qint64 expectedFileSize = 0;

    connect(&bobServer, &QTcpServer::newConnection, [&]() {
        bobSocket = bobServer.nextPendingConnection();
        connect(bobSocket, &QTcpSocket::readyRead, [&]() {
            bobReceiveBuffer.append(bobSocket->readAll());
            while (bobReceiveBuffer.size() >= 12) {
                QDataStream ds(bobReceiveBuffer);
                ds.setByteOrder(QDataStream::BigEndian);
                quint32 magic = 0, type = 0, size = 0;
                ds >> magic >> type >> size;
                if (magic != Protocol::MAGIC) {
                    bobReceiveBuffer.remove(0, 1);
                    continue;
                }
                if (static_cast<quint32>(bobReceiveBuffer.size()) < 12 + size) break;

                QByteArray payload = bobReceiveBuffer.mid(12, size);
                bobReceiveBuffer.remove(0, 12 + size);

                if (type == Protocol::TypeHandshakeRequest) {
                    // Send Handshake response with Bob's alias
                    QJsonObject resp;
                    resp["alias"] = bobAlias;
                    resp["listen_port"] = static_cast<int>(bobPort);
                    bobSocket->write(Protocol::packJson(Protocol::TypeHandshakeResponse, resp));
                    bobSocket->flush();
                } else if (type == Protocol::TypeChatMessage) {
                    QJsonObject obj = QJsonDocument::fromJson(payload).object();
                    receivedMsgUid = obj["msg_uid"].toString();
                    receivedMsgContent = obj["content"].toString();

                    // Send Delivery ACK (2 ticks)
                    QJsonObject delAck;
                    QJsonArray arr;
                    arr.append(receivedMsgUid);
                    delAck["msg_uids"] = arr;
                    bobSocket->write(Protocol::packJson(Protocol::TypeMessageAckDelivered, delAck));
                    bobSocket->flush();

                    // Send Read ACK (2 blue ticks)
                    QJsonObject readAck;
                    readAck["msg_uids"] = arr;
                    bobSocket->write(Protocol::packJson(Protocol::TypeMessageAckRead, readAck));
                    bobSocket->flush();
                } else if (type == Protocol::TypeFileOffer) {
                    QJsonObject obj = QJsonDocument::fromJson(payload).object();
                    receivedFileUid = obj["transfer_uid"].toString();
                    expectedFileSize = obj["file_size"].toString().toLongLong();

                    // Automatically accept file transfer
                    QJsonObject acc;
                    acc["transfer_uid"] = receivedFileUid;
                    acc["start_offset"] = 0;
                    bobSocket->write(Protocol::packJson(Protocol::TypeFileAccept, acc));
                    bobSocket->flush();
                } else if (type == Protocol::TypeFileChunk) {
                    if (payload.size() >= 36 + 12) {
                        QByteArray chunk = payload.mid(36 + 12);
                        receivedFileData.append(chunk);
                    }
                }
            }
        });
    });

    // 2. Alice adds Bob via manual IPv4 address addition
    bool addFinished = false;
    QString fetchedAlias;
    NetworkManager::instance().addPeerManual("127.0.0.1", bobPort, [&](bool success, QString alias, QString) {
        addFinished = true;
        fetchedAlias = alias;
    });

    // Wait for handshake completion
    int waitLoops = 0;
    while (!addFinished && waitLoops < 50) {
        QTest::qWait(50);
        waitLoops++;
    }
    QVERIFY(addFinished);
    QCOMPARE(fetchedAlias, bobAlias);

    Peer bobPeer;
    QVERIFY(DatabaseManager::instance().getPeerByAddress("127.0.0.1", bobPort, bobPeer));
    QCOMPARE(bobPeer.alias, bobAlias);
    QVERIFY(bobPeer.isOnline);

    // 3. Alice sends a chat message
    QString msgUid;
    NetworkManager::instance().sendMessage(bobPeer.id, "Hello Bob from Alice!", &msgUid);

    // Wait for delivery & read ACKs
    waitLoops = 0;
    while (receivedMsgUid != msgUid && waitLoops < 50) {
        QTest::qWait(50);
        waitLoops++;
    }
    QCOMPARE(receivedMsgContent, QString("Hello Bob from Alice!"));

    // Allow Alice to process Read ACK (2 blue ticks)
    QTest::qWait(150);
    QVector<Message> msgs = DatabaseManager::instance().getMessagesForPeer(bobPeer.id);
    bool isReadInDb = false;
    for (const Message &m : msgs) {
        if (m.msgUid == msgUid && m.status == "read") {
            isReadInDb = true;
            break;
        }
    }
    QVERIFY(isReadInDb);

    // 4. Automatic File Transfer Test
    QString tempTestFile = m_tempDir.filePath("sample_send.dat");
    QByteArray sampleData(128 * 1024, 'A'); // 128 KB sample data
    for (int i = 0; i < sampleData.size(); ++i) {
        sampleData[i] = static_cast<char>(i % 256);
    }
    {
        QFile f(tempTestFile);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(sampleData);
        f.close();
    }

    QString ftUid;
    QVERIFY(NetworkManager::instance().sendFile(bobPeer.id, tempTestFile, &ftUid));

    // Wait for file streaming
    waitLoops = 0;
    while (receivedFileData.size() < sampleData.size() && waitLoops < 100) {
        QTest::qWait(50);
        waitLoops++;
    }
    QCOMPARE(receivedFileData.size(), sampleData.size());
    QCOMPARE(QCryptographicHash::hash(receivedFileData, QCryptographicHash::Sha256),
             QCryptographicHash::hash(sampleData, QCryptographicHash::Sha256));

    // 5. Offline Queuing Test: Bob disconnects
    if (bobSocket) {
        bobSocket->close();
        QTest::qWait(100);
    }
    DatabaseManager::instance().updatePeerStatus(bobPeer.id, false);

    // Alice sends while Bob is offline
    QString queuedMsgUid;
    NetworkManager::instance().sendMessage(bobPeer.id, "Message while Bob was offline!", &queuedMsgUid);

    QVector<Message> queuedMsgs = DatabaseManager::instance().getQueuedMessagesForPeer(bobPeer.id);
    bool foundInQueued = false;
    for (const Message &qm : queuedMsgs) {
        if (qm.msgUid == queuedMsgUid) {
            foundInQueued = true;
            break;
        }
    }
    QVERIFY(foundInQueued);

    bobServer.close();
}

void TestP2P::cleanupTestCase() {
    NetworkManager::instance().stopServer();
    DatabaseManager::instance().close();
}

QTEST_MAIN(TestP2P)
#include "test_p2p.moc"

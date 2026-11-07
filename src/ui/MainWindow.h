#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include "PeerListWidget.h"
#include "ChatAreaWidget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void checkFirstLaunch();

private slots:
    void onPeerSelected(int peerId);
    void onAddPeerRequested();
    void onSettingsRequested();

    void onPeerOnlineStatusChanged(int peerId, bool isOnline);
    void onPeerAliasUpdated(int peerId, const QString &alias);
    void onMessageReceived(int peerId, const Message &msg);
    void onMessageStatusUpdated(const QString &msgUid, const QString &status);
    void onFileTransferProgress(const QString &transferUid, qint64 bytes, qint64 total, bool isSender);
    void onFileTransferFinished(const QString &transferUid, bool success, const QString &filePath, bool isSender);

private:
    void setupUi();
    void connectSignals();

    QSplitter *m_splitter = nullptr;
    PeerListWidget *m_peerListWidget = nullptr;
    ChatAreaWidget *m_chatAreaWidget = nullptr;
};

#endif // MAINWINDOW_H

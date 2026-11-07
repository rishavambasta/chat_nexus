#include "MainWindow.h"
#include <QApplication>
#include "FirstLaunchDialog.h"
#include "AddPeerDialog.h"
#include "SettingsDialog.h"
#include "../db/DatabaseManager.h"
#include "../network/NetworkManager.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    setupUi();
    connectSignals();
}

MainWindow::~MainWindow() {
}

void MainWindow::setupUi() {
    setWindowTitle("P2P Chat & File Exchange");
    resize(1000, 700);
    setMinimumSize(780, 500);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);

    m_peerListWidget = new PeerListWidget(m_splitter);
    m_peerListWidget->setMinimumWidth(260);

    m_chatAreaWidget = new ChatAreaWidget(m_splitter);
    m_chatAreaWidget->setMinimumWidth(480);

    m_splitter->addWidget(m_peerListWidget);
    m_splitter->addWidget(m_chatAreaWidget);

    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes(QList<int>() << 320 << 680);

    setCentralWidget(m_splitter);
}

void MainWindow::connectSignals() {
    connect(m_peerListWidget, &PeerListWidget::peerSelected, this, &MainWindow::onPeerSelected);
    connect(m_peerListWidget, &PeerListWidget::addPeerRequested, this, &MainWindow::onAddPeerRequested);
    connect(m_peerListWidget, &PeerListWidget::settingsRequested, this, &MainWindow::onSettingsRequested);

    connect(&NetworkManager::instance(), &NetworkManager::peerOnlineStatusChanged,
            this, &MainWindow::onPeerOnlineStatusChanged);
    connect(&NetworkManager::instance(), &NetworkManager::peerAliasUpdated,
            this, &MainWindow::onPeerAliasUpdated);
    connect(&NetworkManager::instance(), &NetworkManager::messageReceived,
            this, &MainWindow::onMessageReceived);
    connect(&NetworkManager::instance(), &NetworkManager::messageStatusUpdated,
            this, &MainWindow::onMessageStatusUpdated);
    connect(&NetworkManager::instance(), &NetworkManager::fileTransferProgress,
            this, &MainWindow::onFileTransferProgress);
    connect(&NetworkManager::instance(), &NetworkManager::fileTransferFinished,
            this, &MainWindow::onFileTransferFinished);
    connect(&NetworkManager::instance(), &NetworkManager::newPeerAdded,
            m_peerListWidget, &PeerListWidget::reloadPeers);

    connect(&DatabaseManager::instance(), &DatabaseManager::peerUpdated,
            m_peerListWidget, &PeerListWidget::updatePeerItem);
}

void MainWindow::checkFirstLaunch() {
    if (!DatabaseManager::instance().hasUserProfile()) {
        FirstLaunchDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            QString alias = dialog.enteredAlias();
            DatabaseManager::instance().setUserAlias(alias);
            m_peerListWidget->reloadPeers();
        } else {
            // User cancelled initial setup, set default alias
            DatabaseManager::instance().setUserAlias("Anonymous User");
            m_peerListWidget->reloadPeers();
        }
    }
}

void MainWindow::onPeerSelected(int peerId) {
    m_chatAreaWidget->setPeer(peerId);
}

void MainWindow::onAddPeerRequested() {
    AddPeerDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        m_peerListWidget->reloadPeers();
        if (dialog.addedPeerId() > 0) {
            m_peerListWidget->selectPeer(dialog.addedPeerId());
            m_chatAreaWidget->setPeer(dialog.addedPeerId());
        }
    }
}

void MainWindow::onSettingsRequested() {
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        m_peerListWidget->reloadPeers();
    }
}

void MainWindow::onPeerOnlineStatusChanged(int peerId, bool isOnline) {
    m_peerListWidget->updatePeerItem(peerId);
    m_chatAreaWidget->updatePeerStatus(peerId, isOnline);
}

void MainWindow::onPeerAliasUpdated(int peerId, const QString &alias) {
    m_peerListWidget->updatePeerItem(peerId);
    m_chatAreaWidget->updatePeerAlias(peerId, alias);
}

void MainWindow::onMessageReceived(int peerId, const Message &msg) {
    m_peerListWidget->updatePeerItem(peerId);
    m_chatAreaWidget->handleMessageReceived(peerId, msg);
}

void MainWindow::onMessageStatusUpdated(const QString &msgUid, const QString &status) {
    m_chatAreaWidget->handleMessageStatusUpdated(msgUid, status);
}

void MainWindow::onFileTransferProgress(const QString &transferUid, qint64 bytes, qint64 total, bool isSender) {
    m_chatAreaWidget->handleFileProgress(transferUid, bytes, total, isSender);
}

void MainWindow::onFileTransferFinished(const QString &transferUid, bool success, const QString &filePath, bool isSender) {
    m_chatAreaWidget->handleFileFinished(transferUid, success, filePath, isSender);
}

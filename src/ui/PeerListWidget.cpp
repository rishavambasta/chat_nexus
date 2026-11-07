#include "PeerListWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QNetworkInterface>
#include "../db/DatabaseManager.h"
#include "../network/NetworkManager.h"

PeerListWidget::PeerListWidget(QWidget *parent)
    : QWidget(parent) {
    setObjectName("peerListContainer");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. User Profile Header
    QWidget *headerWidget = new QWidget(this);
    headerWidget->setObjectName("leftHeader");

    QVBoxLayout *headerLayout = new QVBoxLayout(headerWidget);
    headerLayout->setContentsMargins(14, 12, 14, 12);
    headerLayout->setSpacing(6);

    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->setSpacing(8);

    QLabel *avatarLabel = new QLabel("👤", headerWidget);
    avatarLabel->setStyleSheet("font-size: 20px; background: transparent;");
    topRow->addWidget(avatarLabel);

    QVBoxLayout *nameLayout = new QVBoxLayout();
    nameLayout->setSpacing(1);

    m_myAliasLabel = new QLabel(headerWidget);
    m_myAliasLabel->setObjectName("myAliasLabel");
    nameLayout->addWidget(m_myAliasLabel);

    m_myInfoLabel = new QLabel(headerWidget);
    m_myInfoLabel->setObjectName("myInfoLabel");
    nameLayout->addWidget(m_myInfoLabel);

    topRow->addLayout(nameLayout);
    topRow->addStretch();

    m_settingsBtn = new QPushButton("⚙", headerWidget);
    m_settingsBtn->setToolTip("Settings");
    m_settingsBtn->setCursor(Qt::PointingHandCursor);
    m_settingsBtn->setStyleSheet("font-size: 16px; padding: 4px 8px; border-radius: 4px;");
    topRow->addWidget(m_settingsBtn);

    headerLayout->addLayout(topRow);

    // "+ Add Peer" button
    m_addPeerBtn = new QPushButton("+ Add Peer", headerWidget);
    m_addPeerBtn->setObjectName("addPeerBtn");
    m_addPeerBtn->setCursor(Qt::PointingHandCursor);
    m_addPeerBtn->setMinimumHeight(32);
    headerLayout->addWidget(m_addPeerBtn);

    mainLayout->addWidget(headerWidget);

    // 2. Search Box
    QWidget *searchContainer = new QWidget(this);
    searchContainer->setStyleSheet("background-color: #111b21; padding: 8px 12px;");
    QHBoxLayout *searchLayout = new QHBoxLayout(searchContainer);
    searchLayout->setContentsMargins(0, 0, 0, 0);

    m_searchEdit = new QLineEdit(searchContainer);
    m_searchEdit->setObjectName("searchPeerEdit");
    m_searchEdit->setPlaceholderText("🔍  Search peers...");
    searchLayout->addWidget(m_searchEdit);

    mainLayout->addWidget(searchContainer);

    // 3. Peer List Widget
    m_listWidget = new QListWidget(this);
    m_listWidget->setObjectName("peerList");
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mainLayout->addWidget(m_listWidget);

    connect(m_settingsBtn, &QPushButton::clicked, this, &PeerListWidget::settingsRequested);
    connect(m_addPeerBtn, &QPushButton::clicked, this, &PeerListWidget::addPeerRequested);
    connect(m_listWidget, &QListWidget::itemSelectionChanged, this, &PeerListWidget::onItemSelectionChanged);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &PeerListWidget::onSearchTextChanged);

    updateUserInfo();
    reloadPeers();
}

void PeerListWidget::updateUserInfo() {
    QString alias = DatabaseManager::instance().getUserAlias();
    quint16 port = NetworkManager::instance().serverPort();

    // Find local primary IPv4
    QString localIp = "127.0.0.1";
    const auto addresses = QNetworkInterface::allAddresses();
    for (const auto &addr : addresses) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback()) {
            localIp = addr.toString();
            break;
        }
    }

    m_myAliasLabel->setText(alias.isEmpty() ? "Anonymous" : alias);
    m_myInfoLabel->setText(QString("🟢 Listening on %1:%2").arg(localIp).arg(port));
}

QWidget *PeerListWidget::createPeerItemWidget(const Peer &peer) {
    QWidget *itemWidget = new QWidget();
    itemWidget->setStyleSheet("background: transparent;");

    QHBoxLayout *layout = new QHBoxLayout(itemWidget);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(10);

    // Status Indicator dot
    QLabel *statusDot = new QLabel(itemWidget);
    if (peer.isOnline) {
        statusDot->setText("●");
        statusDot->setStyleSheet("color: #00a884; font-size: 14px;");
        statusDot->setToolTip("Online");
    } else {
        statusDot->setText("○");
        statusDot->setStyleSheet("color: #8696a0; font-size: 14px;");
        statusDot->setToolTip("Offline");
    }
    layout->addWidget(statusDot);

    // Details column
    QVBoxLayout *detailsLayout = new QVBoxLayout();
    detailsLayout->setSpacing(2);

    QHBoxLayout *titleRow = new QHBoxLayout();
    titleRow->setSpacing(6);

    QLabel *nameLabel = new QLabel(peer.displayName(), itemWidget);
    nameLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #e9edef;");
    titleRow->addWidget(nameLabel);
    titleRow->addStretch();

    if (peer.lastMessageTime.isValid()) {
        QLabel *timeLabel = new QLabel(peer.lastMessageTime.toString("hh:mm"), itemWidget);
        timeLabel->setStyleSheet("font-size: 11px; color: #8696a0;");
        titleRow->addWidget(timeLabel);
    }
    detailsLayout->addLayout(titleRow);

    QHBoxLayout *subRow = new QHBoxLayout();
    subRow->setSpacing(6);

    QString snippet = peer.lastMessageSnippet;
    if (snippet.isEmpty()) {
        snippet = peer.addressString();
    } else if (snippet.length() > 28) {
        snippet = snippet.left(28) + "...";
    }

    QLabel *subLabel = new QLabel(snippet, itemWidget);
    subLabel->setStyleSheet("font-size: 12px; color: #8696a0;");
    subRow->addWidget(subLabel);
    subRow->addStretch();

    // Unread count badge
    if (peer.unreadCount > 0) {
        QLabel *badge = new QLabel(QString::number(peer.unreadCount), itemWidget);
        badge->setStyleSheet(
            "background-color: #00a884;"
            "color: #111b21;"
            "font-weight: bold;"
            "font-size: 11px;"
            "border-radius: 9px;"
            "padding: 2px 6px;"
            "min-width: 14px;"
            "alignment: center;"
        );
        subRow->addWidget(badge);
    }

    detailsLayout->addLayout(subRow);
    layout->addLayout(detailsLayout);

    return itemWidget;
}

void PeerListWidget::reloadPeers() {
    updateUserInfo();
    int currentId = currentSelectedPeerId();

    m_allPeers = DatabaseManager::instance().getAllPeers();
    m_listWidget->clear();

    QString filter = m_searchEdit->text().trimmed().toLower();

    for (const Peer &peer : m_allPeers) {
        if (!filter.isEmpty()) {
            if (!peer.displayName().toLower().contains(filter) &&
                !peer.addressString().toLower().contains(filter)) {
                continue;
            }
        }

        QListWidgetItem *item = new QListWidgetItem(m_listWidget);
        item->setData(Qt::UserRole, peer.id);
        item->setSizeHint(QSize(200, 64));

        QWidget *widget = createPeerItemWidget(peer);
        m_listWidget->setItemWidget(item, widget);

        if (peer.id == currentId) {
            m_listWidget->setCurrentItem(item);
        }
    }
}

void PeerListWidget::selectPeer(int peerId) {
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (item->data(Qt::UserRole).toInt() == peerId) {
            m_listWidget->setCurrentItem(item);
            return;
        }
    }
}

int PeerListWidget::currentSelectedPeerId() const {
    QListWidgetItem *item = m_listWidget->currentItem();
    if (item) {
        return item->data(Qt::UserRole).toInt();
    }
    return -1;
}

void PeerListWidget::updatePeerItem(int peerId) {
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (item->data(Qt::UserRole).toInt() == peerId) {
            Peer peer;
            if (DatabaseManager::instance().getPeerById(peerId, peer)) {
                QWidget *widget = createPeerItemWidget(peer);
                m_listWidget->setItemWidget(item, widget);
            }
            return;
        }
    }
    // If not found in list, reload
    reloadPeers();
}

void PeerListWidget::onItemSelectionChanged() {
    int id = currentSelectedPeerId();
    if (id > 0) {
        emit peerSelected(id);
    }
}

void PeerListWidget::onSearchTextChanged(const QString &) {
    reloadPeers();
}

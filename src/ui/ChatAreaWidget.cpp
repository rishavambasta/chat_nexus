#include "ChatAreaWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QFileDialog>
#include <QKeyEvent>
#include <QTimer>
#include <algorithm>
#include "../db/DatabaseManager.h"
#include "../network/NetworkManager.h"

// ChatInputEdit
ChatInputEdit::ChatInputEdit(QWidget *parent)
    : QTextEdit(parent) {
    setObjectName("messageInput");
    setAcceptRichText(false);
    setFixedHeight(44);
    setPlaceholderText("Type a message (Enter to send, Shift+Enter for newline)...");
}

void ChatInputEdit::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        if (e->modifiers() & Qt::ShiftModifier || e->modifiers() & Qt::ControlModifier) {
            QTextEdit::keyPressEvent(e);
        } else {
            emit submitRequested();
            e->accept();
            return;
        }
    } else {
        QTextEdit::keyPressEvent(e);
    }
}

// ChatAreaWidget
ChatAreaWidget::ChatAreaWidget(QWidget *parent)
    : QWidget(parent) {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. Empty State Widget
    m_emptyStateWidget = new QWidget(this);
    QVBoxLayout *emptyLayout = new QVBoxLayout(m_emptyStateWidget);
    emptyLayout->setContentsMargins(40, 40, 40, 40);
    emptyLayout->setSpacing(16);
    emptyLayout->addStretch();

    QLabel *emptyIcon = new QLabel("💬", m_emptyStateWidget);
    emptyIcon->setStyleSheet("font-size: 48px; background: transparent;");
    emptyIcon->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyIcon);

    QLabel *emptyTitle = new QLabel("P2P Secure Chat & File Transfer", m_emptyStateWidget);
    emptyTitle->setStyleSheet("font-size: 20px; font-weight: bold; color: #e9edef; background: transparent;");
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyTitle);

    QLabel *emptySub = new QLabel(
        "Select a peer from the left pane to start chatting or sharing files.\n"
        "Direct peer-to-peer connection without central servers.",
        m_emptyStateWidget
    );
    emptySub->setStyleSheet("font-size: 13px; color: #8696a0; background: transparent;");
    emptySub->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptySub);

    emptyLayout->addStretch();
    mainLayout->addWidget(m_emptyStateWidget);

    // 2. Chat Container Widget
    m_chatContainerWidget = new QWidget(this);
    QVBoxLayout *chatLayout = new QVBoxLayout(m_chatContainerWidget);
    chatLayout->setContentsMargins(0, 0, 0, 0);
    chatLayout->setSpacing(0);

    // Header Bar
    QWidget *headerWidget = new QWidget(m_chatContainerWidget);
    headerWidget->setObjectName("chatHeader");
    headerWidget->setFixedHeight(60);

    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(16, 8, 16, 8);
    headerLayout->setSpacing(12);

    QVBoxLayout *headerTextLayout = new QVBoxLayout();
    headerTextLayout->setSpacing(2);

    m_peerAliasLabel = new QLabel(headerWidget);
    m_peerAliasLabel->setObjectName("chatPeerAlias");
    headerTextLayout->addWidget(m_peerAliasLabel);

    m_peerStatusLabel = new QLabel(headerWidget);
    m_peerStatusLabel->setObjectName("chatPeerStatus");
    headerTextLayout->addWidget(m_peerStatusLabel);

    headerLayout->addLayout(headerTextLayout);
    headerLayout->addStretch();

    m_attachHeaderBtn = new QPushButton("📎 Send File", headerWidget);
    m_attachHeaderBtn->setCursor(Qt::PointingHandCursor);
    m_attachHeaderBtn->setStyleSheet("padding: 6px 12px; font-size: 12px;");
    headerLayout->addWidget(m_attachHeaderBtn);

    m_pingBtn = new QPushButton("🔄 Refresh", headerWidget);
    m_pingBtn->setCursor(Qt::PointingHandCursor);
    m_pingBtn->setStyleSheet("padding: 6px 12px; font-size: 12px;");
    headerLayout->addWidget(m_pingBtn);

    chatLayout->addWidget(headerWidget);

    // Messages Scroll Area
    m_scrollArea = new QScrollArea(m_chatContainerWidget);
    m_scrollArea->setObjectName("chatScrollArea");
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_scrollContent = new QWidget();
    m_scrollContent->setObjectName("chatScrollContent");

    m_messagesLayout = new QVBoxLayout(m_scrollContent);
    m_messagesLayout->setContentsMargins(16, 16, 16, 16);
    m_messagesLayout->setSpacing(8);
    m_messagesLayout->addStretch();

    m_scrollArea->setWidget(m_scrollContent);
    chatLayout->addWidget(m_scrollArea);

    // Bottom Input Container
    QWidget *inputContainer = new QWidget(m_chatContainerWidget);
    inputContainer->setObjectName("inputContainer");

    QHBoxLayout *inputLayout = new QHBoxLayout(inputContainer);
    inputLayout->setContentsMargins(12, 8, 12, 8);
    inputLayout->setSpacing(10);

    m_attachBtn = new QPushButton("📎", inputContainer);
    m_attachBtn->setObjectName("attachBtn");
    m_attachBtn->setToolTip("Attach File");
    m_attachBtn->setCursor(Qt::PointingHandCursor);
    inputLayout->addWidget(m_attachBtn);

    m_inputEdit = new ChatInputEdit(inputContainer);
    inputLayout->addWidget(m_inputEdit);

    m_sendBtn = new QPushButton("Send ➤", inputContainer);
    m_sendBtn->setObjectName("sendBtn");
    m_sendBtn->setCursor(Qt::PointingHandCursor);
    m_sendBtn->setMinimumSize(80, 42);
    inputLayout->addWidget(m_sendBtn);

    chatLayout->addWidget(inputContainer);

    mainLayout->addWidget(m_chatContainerWidget);

    m_emptyStateWidget->setVisible(true);
    m_chatContainerWidget->setVisible(false);

    connect(m_sendBtn, &QPushButton::clicked, this, &ChatAreaWidget::onSendMessage);
    connect(m_inputEdit, &ChatInputEdit::submitRequested, this, &ChatAreaWidget::onSendMessage);
    connect(m_attachBtn, &QPushButton::clicked, this, &ChatAreaWidget::onAttachFile);
    connect(m_attachHeaderBtn, &QPushButton::clicked, this, &ChatAreaWidget::onAttachFile);
    connect(m_pingBtn, &QPushButton::clicked, this, &ChatAreaWidget::onPingPeer);
}

void ChatAreaWidget::setPeer(int peerId) {
    m_currentPeerId = peerId;
    NetworkManager::instance().setActivePeerId(peerId);

    if (peerId <= 0) {
        m_emptyStateWidget->setVisible(true);
        m_chatContainerWidget->setVisible(false);
        return;
    }

    if (!DatabaseManager::instance().getPeerById(peerId, m_currentPeer)) {
        m_emptyStateWidget->setVisible(true);
        m_chatContainerWidget->setVisible(false);
        return;
    }

    m_emptyStateWidget->setVisible(false);
    m_chatContainerWidget->setVisible(true);

    m_peerAliasLabel->setText(m_currentPeer.displayName());
    updatePeerStatus(peerId, m_currentPeer.isOnline);

    clearChat();
    loadChatHistory();

    // Mark messages as read now that chat is open
    NetworkManager::instance().markChatAsRead(peerId);

    m_inputEdit->setFocus();
}

void ChatAreaWidget::updatePeerStatus(int peerId, bool isOnline) {
    if (m_currentPeerId != peerId) return;

    m_currentPeer.isOnline = isOnline;
    if (isOnline) {
        m_peerStatusLabel->setText(QString("🟢 Online • %1:%2").arg(m_currentPeer.ip).arg(m_currentPeer.port));
        m_peerStatusLabel->setStyleSheet("color: #00a884; font-size: 12px;");
    } else {
        QString seenStr = m_currentPeer.lastSeen.isValid() ? m_currentPeer.lastSeen.toString("hh:mm") : "recently";
        m_peerStatusLabel->setText(QString("⚪ Offline (last seen %1) • %2:%3").arg(seenStr).arg(m_currentPeer.ip).arg(m_currentPeer.port));
        m_peerStatusLabel->setStyleSheet("color: #8696a0; font-size: 12px;");
    }
}

void ChatAreaWidget::updatePeerAlias(int peerId, const QString &alias) {
    if (m_currentPeerId != peerId) return;
    m_currentPeer.alias = alias;
    m_peerAliasLabel->setText(m_currentPeer.displayName());
}

void ChatAreaWidget::clearChat() {
    m_messageBubbles.clear();
    m_fileBubbles.clear();

    // Remove widgets from layout
    QLayoutItem *item;
    while ((item = m_messagesLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    m_messagesLayout->addStretch();
}

struct ChatHistoryItem {
    bool isFile = false;
    QDateTime timestamp;
    int id = 0;
    Message msg;
    FileTransfer ft;
};

void ChatAreaWidget::loadChatHistory() {
    if (m_currentPeerId <= 0) return;

    QVector<Message> msgs = DatabaseManager::instance().getMessagesForPeer(m_currentPeerId);
    QVector<FileTransfer> fts = DatabaseManager::instance().getFileTransfersForPeer(m_currentPeerId);

    QVector<ChatHistoryItem> combined;
    for (const Message &m : msgs) {
        ChatHistoryItem item;
        item.isFile = false;
        item.timestamp = m.timestamp;
        item.id = m.id;
        item.msg = m;
        combined.append(item);
    }

    for (const FileTransfer &f : fts) {
        ChatHistoryItem item;
        item.isFile = true;
        item.timestamp = f.timestamp;
        item.id = f.id;
        item.ft = f;
        combined.append(item);
    }

    // Sort by timestamp then id
    std::sort(combined.begin(), combined.end(), [](const ChatHistoryItem &a, const ChatHistoryItem &b) {
        if (a.timestamp == b.timestamp) return a.id < b.id;
        return a.timestamp < b.timestamp;
    });

    for (const ChatHistoryItem &item : combined) {
        if (item.isFile) {
            addFileTransferBubble(item.ft);
        } else {
            addMessageBubble(item.msg);
        }
    }

    scrollToBottom();
}

void ChatAreaWidget::addMessageBubble(const Message &msg) {
    MessageBubbleWidget *bubble = new MessageBubbleWidget(msg, m_scrollContent);
    // Insert before the trailing stretch
    int index = qMax(0, m_messagesLayout->count() - 1);
    m_messagesLayout->insertWidget(index, bubble);
    m_messageBubbles.insert(msg.msgUid, bubble);
}

void ChatAreaWidget::addFileTransferBubble(const FileTransfer &ft) {
    FileTransferBubbleWidget *bubble = new FileTransferBubbleWidget(ft, m_scrollContent);
    int index = qMax(0, m_messagesLayout->count() - 1);
    m_messagesLayout->insertWidget(index, bubble);
    m_fileBubbles.insert(ft.transferUid, bubble);
}

void ChatAreaWidget::scrollToBottom() {
    QTimer::singleShot(50, this, [this]() {
        if (m_scrollArea && m_scrollArea->verticalScrollBar()) {
            m_scrollArea->verticalScrollBar()->setValue(m_scrollArea->verticalScrollBar()->maximum());
        }
    });
}

void ChatAreaWidget::onSendMessage() {
    if (m_currentPeerId <= 0) return;

    QString content = m_inputEdit->toPlainText().trimmed();
    if (content.isEmpty()) return;

    QString msgUid;
    NetworkManager::instance().sendMessage(m_currentPeerId, content, &msgUid);

    // Add sent bubble immediately
    Message sentMsg;
    sentMsg.msgUid = msgUid;
    sentMsg.peerId = m_currentPeerId;
    sentMsg.direction = "sent";
    sentMsg.content = content;
    sentMsg.timestamp = QDateTime::currentDateTime();
    sentMsg.status = "queued"; // Starts as queued, updates to delivered/read

    addMessageBubble(sentMsg);
    m_inputEdit->clear();
    scrollToBottom();
}

void ChatAreaWidget::onAttachFile() {
    if (m_currentPeerId <= 0) return;

    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Select File to Send",
        QDir::homePath()
    );

    if (filePath.isEmpty()) return;

    QString transferUid;
    bool ok = NetworkManager::instance().sendFile(m_currentPeerId, filePath, &transferUid);
    if (ok) {
        FileTransfer ft;
        if (DatabaseManager::instance().getFileTransferByUid(transferUid, ft)) {
            addFileTransferBubble(ft);
            scrollToBottom();
        }
    }
}

void ChatAreaWidget::onPingPeer() {
    if (m_currentPeerId <= 0) return;
    PeerConnection *conn = NetworkManager::instance().getConnectionForPeer(m_currentPeerId);
    if (conn) {
        conn->sendPing();
    } else {
        NetworkManager::instance().pollAllPeers();
    }
}

void ChatAreaWidget::handleMessageReceived(int peerId, const Message &msg) {
    if (m_currentPeerId != peerId) return;

    addMessageBubble(msg);
    scrollToBottom();
}

void ChatAreaWidget::handleMessageStatusUpdated(const QString &msgUid, const QString &status) {
    if (m_messageBubbles.contains(msgUid)) {
        m_messageBubbles[msgUid]->updateStatus(status);
    }
}

void ChatAreaWidget::handleFileProgress(const QString &transferUid, qint64 bytes, qint64 total, bool) {
    if (m_fileBubbles.contains(transferUid)) {
        m_fileBubbles[transferUid]->updateProgress(bytes, total, "transferring");
    } else {
        // Bubble might not have been added yet if incoming
        FileTransfer ft;
        if (DatabaseManager::instance().getFileTransferByUid(transferUid, ft)) {
            if (ft.peerId == m_currentPeerId) {
                addFileTransferBubble(ft);
                scrollToBottom();
            }
        }
    }
}

void ChatAreaWidget::handleFileFinished(const QString &transferUid, bool success, const QString &filePath, bool) {
    if (m_fileBubbles.contains(transferUid)) {
        m_fileBubbles[transferUid]->setFinished(success, filePath);
    }
}

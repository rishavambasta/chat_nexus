#ifndef CHATAREAWIDGET_H
#define CHATAREAWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QMap>
#include "../models/Peer.h"
#include "../models/Message.h"
#include "../models/FileTransfer.h"
#include "MessageBubbleWidget.h"
#include "FileTransferBubbleWidget.h"

class ChatInputEdit : public QTextEdit {
    Q_OBJECT

public:
    explicit ChatInputEdit(QWidget *parent = nullptr);

signals:
    void submitRequested();

protected:
    void keyPressEvent(QKeyEvent *e) override;
};

class ChatAreaWidget : public QWidget {
    Q_OBJECT

public:
    explicit ChatAreaWidget(QWidget *parent = nullptr);

    void setPeer(int peerId);
    int currentPeerId() const { return m_currentPeerId; }
    void updatePeerStatus(int peerId, bool isOnline);
    void updatePeerAlias(int peerId, const QString &alias);

    void handleMessageReceived(int peerId, const Message &msg);
    void handleMessageStatusUpdated(const QString &msgUid, const QString &status);
    void handleFileProgress(const QString &transferUid, qint64 bytes, qint64 total, bool isSender);
    void handleFileFinished(const QString &transferUid, bool success, const QString &filePath, bool isSender);

private slots:
    void onSendMessage();
    void onAttachFile();
    void onPingPeer();

private:
    void clearChat();
    void loadChatHistory();
    void scrollToBottom();
    void addMessageBubble(const Message &msg);
    void addFileTransferBubble(const FileTransfer &ft);

    int m_currentPeerId = -1;
    Peer m_currentPeer;

    QWidget *m_emptyStateWidget = nullptr;
    QWidget *m_chatContainerWidget = nullptr;

    // Header
    QLabel *m_peerAliasLabel = nullptr;
    QLabel *m_peerStatusLabel = nullptr;
    QPushButton *m_attachHeaderBtn = nullptr;
    QPushButton *m_pingBtn = nullptr;

    // Message list
    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_scrollContent = nullptr;
    QVBoxLayout *m_messagesLayout = nullptr;

    // Input area
    QPushButton *m_attachBtn = nullptr;
    ChatInputEdit *m_inputEdit = nullptr;
    QPushButton *m_sendBtn = nullptr;

    QMap<QString, MessageBubbleWidget*> m_messageBubbles; // keyed by msgUid
    QMap<QString, FileTransferBubbleWidget*> m_fileBubbles; // keyed by transferUid
};

#endif // CHATAREAWIDGET_H

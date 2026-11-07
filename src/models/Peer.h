#ifndef PEER_H
#define PEER_H

#include <QString>
#include <QDateTime>

struct Peer {
    int id = -1;
    QString ip;
    quint16 port = 42424;
    QString alias;
    bool isOnline = false;
    QDateTime lastSeen;
    int unreadCount = 0;
    QString lastMessageSnippet;
    QDateTime lastMessageTime;

    QString displayName() const {
        if (!alias.trimmed().isEmpty()) {
            return alias;
        }
        return QString("%1:%2").arg(ip).arg(port);
    }

    QString addressString() const {
        return QString("%1:%2").arg(ip).arg(port);
    }
};

#endif // PEER_H

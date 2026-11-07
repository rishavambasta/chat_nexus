#ifndef MESSAGE_H
#define MESSAGE_H

#include <QString>
#include <QDateTime>

struct Message {
    int id = -1;
    QString msgUid;
    int peerId = -1;
    QString direction; // "sent" or "received"
    QString content;
    QDateTime timestamp;
    QString status; // "queued", "delivered", "read"

    bool isSent() const {
        return direction.toLower() == "sent";
    }

    bool isDelivered() const {
        return status == "delivered" || status == "read";
    }

    bool isRead() const {
        return status == "read";
    }

    bool isQueued() const {
        return status == "queued";
    }
};

#endif // MESSAGE_H

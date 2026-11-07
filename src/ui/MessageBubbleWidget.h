#ifndef MESSAGEBUBBLEWIDGET_H
#define MESSAGEBUBBLEWIDGET_H

#include <QWidget>
#include <QLabel>
#include "../models/Message.h"

class MessageBubbleWidget : public QWidget {
    Q_OBJECT

public:
    explicit MessageBubbleWidget(const Message &msg, QWidget *parent = nullptr);

    QString msgUid() const { return m_msg.msgUid; }
    void updateStatus(const QString &newStatus);

private:
    void updateTicksDisplay();

    Message m_msg;
    QLabel *m_ticksLabel = nullptr;
    QLabel *m_contentLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
};

#endif // MESSAGEBUBBLEWIDGET_H

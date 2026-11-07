#include "MessageBubbleWidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>

MessageBubbleWidget::MessageBubbleWidget(const Message &msg, QWidget *parent)
    : QWidget(parent)
    , m_msg(msg) {
    bool isSent = msg.isSent();

    QHBoxLayout *outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(12, 3, 12, 3);
    outerLayout->setSpacing(0);

    QFrame *bubbleFrame = new QFrame(this);
    bubbleFrame->setObjectName("bubbleFrame");

    // Colors matching WhatsApp Dark
    if (isSent) {
        bubbleFrame->setStyleSheet(
            "QFrame#bubbleFrame {"
            "  background-color: #005c4b;"
            "  border-radius: 8px;"
            "  border-top-right-radius: 2px;"
            "  padding: 6px 10px;"
            "}"
        );
    } else {
        bubbleFrame->setStyleSheet(
            "QFrame#bubbleFrame {"
            "  background-color: #202c33;"
            "  border-radius: 8px;"
            "  border-top-left-radius: 2px;"
            "  padding: 6px 10px;"
            "}"
        );
    }

    QVBoxLayout *bubbleLayout = new QVBoxLayout(bubbleFrame);
    bubbleLayout->setContentsMargins(6, 4, 6, 4);
    bubbleLayout->setSpacing(4);

    m_contentLabel = new QLabel(msg.content, bubbleFrame);
    m_contentLabel->setWordWrap(true);
    m_contentLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_contentLabel->setStyleSheet("color: #e9edef; font-size: 14px; background: transparent;");
    bubbleLayout->addWidget(m_contentLabel);

    QHBoxLayout *metaLayout = new QHBoxLayout();
    metaLayout->setContentsMargins(0, 0, 0, 0);
    metaLayout->setSpacing(4);
    metaLayout->addStretch();

    m_timeLabel = new QLabel(msg.timestamp.toString("hh:mm"), bubbleFrame);
    m_timeLabel->setStyleSheet("color: #8696a0; font-size: 11px; background: transparent;");
    metaLayout->addWidget(m_timeLabel);

    if (isSent) {
        m_ticksLabel = new QLabel(bubbleFrame);
        m_ticksLabel->setStyleSheet("font-size: 12px; background: transparent;");
        updateTicksDisplay();
        metaLayout->addWidget(m_ticksLabel);
    }

    bubbleLayout->addLayout(metaLayout);

    if (isSent) {
        outerLayout->addStretch();
        outerLayout->addWidget(bubbleFrame);
    } else {
        outerLayout->addWidget(bubbleFrame);
        outerLayout->addStretch();
    }
}

void MessageBubbleWidget::updateStatus(const QString &newStatus) {
    m_msg.status = newStatus;
    updateTicksDisplay();
}

void MessageBubbleWidget::updateTicksDisplay() {
    if (!m_ticksLabel) return;

    if (m_msg.status == "queued") {
        // Clock icon for queued/offline
        m_ticksLabel->setText("🕒");
        m_ticksLabel->setStyleSheet("color: #8696a0; font-size: 11px; background: transparent;");
        m_ticksLabel->setToolTip("Queued (Peer offline)");
    } else if (m_msg.status == "delivered") {
        // Two grey ticks for delivered
        m_ticksLabel->setText("✓✓");
        m_ticksLabel->setStyleSheet("color: #8696a0; font-weight: bold; font-size: 12px; background: transparent;");
        m_ticksLabel->setToolTip("Delivered");
    } else if (m_msg.status == "read") {
        // Two blue ticks for read!
        m_ticksLabel->setText("✓✓");
        m_ticksLabel->setStyleSheet("color: #34b7f1; font-weight: bold; font-size: 12px; background: transparent;");
        m_ticksLabel->setToolTip("Read");
    } else {
        m_ticksLabel->setText("");
    }
}

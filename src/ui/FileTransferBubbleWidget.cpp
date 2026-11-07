#include "FileTransferBubbleWidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>

FileTransferBubbleWidget::FileTransferBubbleWidget(const FileTransfer &ft, QWidget *parent)
    : QWidget(parent)
    , m_ft(ft) {
    bool isSender = ft.isSender();

    QHBoxLayout *outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(12, 4, 12, 4);
    outerLayout->setSpacing(0);

    QFrame *bubbleFrame = new QFrame(this);
    bubbleFrame->setObjectName("fileBubbleFrame");
    bubbleFrame->setMinimumWidth(280);
    bubbleFrame->setMaximumWidth(360);

    if (isSender) {
        bubbleFrame->setStyleSheet(
            "QFrame#fileBubbleFrame {"
            "  background-color: #005c4b;"
            "  border-radius: 8px;"
            "  border-top-right-radius: 2px;"
            "  padding: 8px 12px;"
            "}"
        );
    } else {
        bubbleFrame->setStyleSheet(
            "QFrame#fileBubbleFrame {"
            "  background-color: #202c33;"
            "  border-radius: 8px;"
            "  border-top-left-radius: 2px;"
            "  padding: 8px 12px;"
            "}"
        );
    }

    QVBoxLayout *bubbleLayout = new QVBoxLayout(bubbleFrame);
    bubbleLayout->setContentsMargins(6, 4, 6, 4);
    bubbleLayout->setSpacing(6);

    // Top row: File icon + Name & Size
    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->setSpacing(10);

    QLabel *fileIcon = new QLabel("📄", bubbleFrame);
    fileIcon->setStyleSheet("font-size: 26px; background: transparent;");
    topRow->addWidget(fileIcon);

    QVBoxLayout *nameSizeLayout = new QVBoxLayout();
    nameSizeLayout->setSpacing(2);

    m_nameLabel = new QLabel(ft.fileName, bubbleFrame);
    m_nameLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #e9edef; background: transparent;");
    m_nameLabel->setToolTip(ft.fileName);
    nameSizeLayout->addWidget(m_nameLabel);

    m_sizeLabel = new QLabel(FileTransfer::formatSize(ft.fileSize), bubbleFrame);
    m_sizeLabel->setStyleSheet("font-size: 11px; color: #8696a0; background: transparent;");
    nameSizeLayout->addWidget(m_sizeLabel);

    topRow->addLayout(nameSizeLayout);
    topRow->addStretch();
    bubbleLayout->addLayout(topRow);

    // Progress bar
    m_progressBar = new QProgressBar(bubbleFrame);
    m_progressBar->setFixedHeight(12);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(ft.progressPercentage());
    bubbleLayout->addWidget(m_progressBar);

    // Action buttons row (Open File, Show in Folder)
    m_actionsWidget = new QWidget(bubbleFrame);
    QHBoxLayout *actionsLayout = new QHBoxLayout(m_actionsWidget);
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->setSpacing(8);

    m_openFileBtn = new QPushButton("Open File", m_actionsWidget);
    m_openFileBtn->setCursor(Qt::PointingHandCursor);
    m_openFileBtn->setStyleSheet("padding: 4px 10px; font-size: 11px;");
    actionsLayout->addWidget(m_openFileBtn);

    m_openFolderBtn = new QPushButton("Show in Folder", m_actionsWidget);
    m_openFolderBtn->setCursor(Qt::PointingHandCursor);
    m_openFolderBtn->setStyleSheet("padding: 4px 10px; font-size: 11px;");
    actionsLayout->addWidget(m_openFolderBtn);

    actionsLayout->addStretch();
    bubbleLayout->addWidget(m_actionsWidget);

    connect(m_openFileBtn, &QPushButton::clicked, this, &FileTransferBubbleWidget::onOpenFile);
    connect(m_openFolderBtn, &QPushButton::clicked, this, &FileTransferBubbleWidget::onOpenFolder);

    // Bottom row: Status + Time + Ticks
    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 0, 0, 0);
    bottomRow->setSpacing(4);

    m_statusLabel = new QLabel(bubbleFrame);
    m_statusLabel->setStyleSheet("font-size: 11px; color: #8696a0; background: transparent;");
    bottomRow->addWidget(m_statusLabel);

    bottomRow->addStretch();

    m_timeLabel = new QLabel(ft.timestamp.toString("hh:mm"), bubbleFrame);
    m_timeLabel->setStyleSheet("font-size: 11px; color: #8696a0; background: transparent;");
    bottomRow->addWidget(m_timeLabel);

    if (isSender) {
        m_ticksLabel = new QLabel(bubbleFrame);
        m_ticksLabel->setStyleSheet("font-size: 12px; background: transparent;");
        bottomRow->addWidget(m_ticksLabel);
    }

    bubbleLayout->addLayout(bottomRow);

    updateUiForStatus();

    if (isSender) {
        outerLayout->addStretch();
        outerLayout->addWidget(bubbleFrame);
    } else {
        outerLayout->addWidget(bubbleFrame);
        outerLayout->addStretch();
    }
}

void FileTransferBubbleWidget::updateProgress(qint64 transferred, qint64 total, const QString &status) {
    m_ft.bytesTransferred = transferred;
    m_ft.fileSize = total;
    m_ft.status = status;

    int pct = m_ft.progressPercentage();
    m_progressBar->setValue(pct);

    updateUiForStatus();
}

void FileTransferBubbleWidget::setFinished(bool success, const QString &finalPath) {
    m_ft.status = success ? "completed" : "failed";
    if (!finalPath.isEmpty()) {
        m_ft.filePath = finalPath;
    }
    m_progressBar->setValue(success ? 100 : 0);
    updateUiForStatus();
}

void FileTransferBubbleWidget::updateUiForStatus() {
    bool isSender = m_ft.isSender();
    QString status = m_ft.status.toLower();

    if (status == "completed") {
        m_progressBar->setVisible(false);
        m_actionsWidget->setVisible(true);
        m_statusLabel->setText(isSender ? "Sent" : "Downloaded");
        m_statusLabel->setStyleSheet("color: #00a884; font-size: 11px; font-weight: bold; background: transparent;");

        if (m_ticksLabel) {
            m_ticksLabel->setText("✓✓");
            m_ticksLabel->setStyleSheet("color: #34b7f1; font-weight: bold; font-size: 12px; background: transparent;");
        }
    } else if (status == "failed" || status == "cancelled") {
        m_progressBar->setVisible(false);
        m_actionsWidget->setVisible(false);
        m_statusLabel->setText("Failed");
        m_statusLabel->setStyleSheet("color: #ef4444; font-size: 11px; background: transparent;");

        if (m_ticksLabel) {
            m_ticksLabel->setText("⚠");
            m_ticksLabel->setStyleSheet("color: #ef4444; font-size: 11px; background: transparent;");
        }
    } else if (status == "queued") {
        m_progressBar->setVisible(false);
        m_actionsWidget->setVisible(false);
        m_statusLabel->setText("Queued (Peer offline)");
        m_statusLabel->setStyleSheet("color: #8696a0; font-size: 11px; background: transparent;");

        if (m_ticksLabel) {
            m_ticksLabel->setText("🕒");
            m_ticksLabel->setStyleSheet("color: #8696a0; font-size: 11px; background: transparent;");
        }
    } else { // transferring
        m_progressBar->setVisible(true);
        m_actionsWidget->setVisible(false);
        int pct = m_ft.progressPercentage();
        m_statusLabel->setText(QString("%1 %2% (%3 / %4)")
                               .arg(isSender ? "Sending" : "Receiving")
                               .arg(pct)
                               .arg(FileTransfer::formatSize(m_ft.bytesTransferred))
                               .arg(FileTransfer::formatSize(m_ft.fileSize)));
        m_statusLabel->setStyleSheet("color: #8696a0; font-size: 11px; background: transparent;");

        if (m_ticksLabel) {
            m_ticksLabel->setText("...");
            m_ticksLabel->setStyleSheet("color: #8696a0; font-size: 11px; background: transparent;");
        }
    }
}

void FileTransferBubbleWidget::onOpenFile() {
    if (!m_ft.filePath.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_ft.filePath));
    }
}

void FileTransferBubbleWidget::onOpenFolder() {
    if (!m_ft.filePath.isEmpty()) {
        QFileInfo fi(m_ft.filePath);
        QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
    }
}

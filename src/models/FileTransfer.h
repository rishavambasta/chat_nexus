#ifndef FILETRANSFER_H
#define FILETRANSFER_H

#include <QString>
#include <QDateTime>

struct FileTransfer {
    int id = -1;
    QString transferUid;
    int peerId = -1;
    QString direction; // "send" or "receive"
    QString fileName;
    QString filePath;
    qint64 fileSize = 0;
    qint64 bytesTransferred = 0;
    QString status; // "queued", "transferring", "completed", "failed", "cancelled"
    QDateTime timestamp;

    bool isSender() const {
        return direction.toLower() == "send";
    }

    int progressPercentage() const {
        if (fileSize <= 0) return 0;
        int pct = static_cast<int>((bytesTransferred * 100) / fileSize);
        return qBound(0, pct, 100);
    }

    static QString formatSize(qint64 bytes) {
        if (bytes < 1024) {
            return QString("%1 B").arg(bytes);
        } else if (bytes < 1024 * 1024) {
            return QString("%1 KB").arg(QString::number(bytes / 1024.0, 'f', 1));
        } else if (bytes < 1024 * 1024 * 1024) {
            return QString("%1 MB").arg(QString::number(bytes / (1024.0 * 1024.0), 'f', 1));
        } else {
            return QString("%1 GB").arg(QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2));
        }
    }
};

#endif // FILETRANSFER_H

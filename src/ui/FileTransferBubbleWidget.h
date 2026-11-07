#ifndef FILETRANSFERBUBBLEWIDGET_H
#define FILETRANSFERBUBBLEWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include "../models/FileTransfer.h"

class FileTransferBubbleWidget : public QWidget {
    Q_OBJECT

public:
    explicit FileTransferBubbleWidget(const FileTransfer &ft, QWidget *parent = nullptr);

    QString transferUid() const { return m_ft.transferUid; }
    void updateProgress(qint64 transferred, qint64 total, const QString &status);
    void setFinished(bool success, const QString &finalPath);

private slots:
    void onOpenFile();
    void onOpenFolder();

private:
    void updateUiForStatus();

    FileTransfer m_ft;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_sizeLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_ticksLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QWidget *m_actionsWidget = nullptr;
    QPushButton *m_openFileBtn = nullptr;
    QPushButton *m_openFolderBtn = nullptr;
};

#endif // FILETRANSFERBUBBLEWIDGET_H

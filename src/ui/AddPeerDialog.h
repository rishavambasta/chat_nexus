#ifndef ADDPEERDIALOG_H
#define ADDPEERDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>

class AddPeerDialog : public QDialog {
    Q_OBJECT

public:
    explicit AddPeerDialog(QWidget *parent = nullptr);

    int addedPeerId() const { return m_addedPeerId; }

private slots:
    void onConnectAndAdd();
    void onAddOfflineAnyway();

private:
    bool parseAddress(const QString &text, QString &outIp, quint16 &outPort);

    QLineEdit *m_ipEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QProgressBar *m_spinner = nullptr;
    QPushButton *m_connectBtn = nullptr;
    QPushButton *m_addOfflineBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;

    QString m_lastIp;
    quint16 m_lastPort = 42424;
    int m_addedPeerId = -1;
};

#endif // ADDPEERDIALOG_H

#ifndef PEERLISTWIDGET_H
#define PEERLISTWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVector>
#include "../models/Peer.h"

class PeerListWidget : public QWidget {
    Q_OBJECT

public:
    explicit PeerListWidget(QWidget *parent = nullptr);

    void reloadPeers();
    void selectPeer(int peerId);
    int currentSelectedPeerId() const;
    void updatePeerItem(int peerId);

signals:
    void peerSelected(int peerId);
    void addPeerRequested();
    void settingsRequested();

private slots:
    void onItemSelectionChanged();
    void onSearchTextChanged(const QString &text);

private:
    void updateUserInfo();
    QWidget *createPeerItemWidget(const Peer &peer);

    QLabel *m_myAliasLabel = nullptr;
    QLabel *m_myInfoLabel = nullptr;
    QPushButton *m_settingsBtn = nullptr;
    QPushButton *m_addPeerBtn = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QListWidget *m_listWidget = nullptr;

    QVector<Peer> m_allPeers;
};

#endif // PEERLISTWIDGET_H

#include "AddPeerDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QTimer>
#include "../network/NetworkManager.h"
#include "../db/DatabaseManager.h"

AddPeerDialog::AddPeerDialog(QWidget *parent)
    : QDialog(parent) {
    setWindowTitle("Add New Peer");
    setModal(true);
    setFixedSize(460, 280);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 20, 24, 20);
    mainLayout->setSpacing(14);

    QLabel *titleLabel = new QLabel("Add Peer by IPv4 Address", this);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #00a884;");
    mainLayout->addWidget(titleLabel);

    QLabel *descLabel = new QLabel(
        "Enter peer's IPv4 address. If the peer is online, their alias will be fetched automatically.",
        this
    );
    descLabel->setStyleSheet("color: #8696a0; font-size: 12px;");
    descLabel->setWordWrap(true);
    mainLayout->addWidget(descLabel);

    QHBoxLayout *inputLayout = new QHBoxLayout();
    QLabel *ipIcon = new QLabel("🌐", this);
    ipIcon->setStyleSheet("font-size: 16px;");
    inputLayout->addWidget(ipIcon);

    m_ipEdit = new QLineEdit(this);
    m_ipEdit->setPlaceholderText("e.g. 192.168.1.105 or 192.168.1.105:42424");
    m_ipEdit->setStyleSheet("font-size: 13px; padding: 8px 10px;");
    inputLayout->addWidget(m_ipEdit);
    mainLayout->addLayout(inputLayout);

    m_spinner = new QProgressBar(this);
    m_spinner->setRange(0, 0); // Indeterminate
    m_spinner->setFixedHeight(6);
    m_spinner->setTextVisible(false);
    m_spinner->setVisible(false);
    mainLayout->addWidget(m_spinner);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("font-size: 12px; color: #8696a0;");
    m_statusLabel->setWordWrap(true);
    mainLayout->addWidget(m_statusLabel);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    m_addOfflineBtn = new QPushButton("Add Anyway (Offline)", this);
    m_addOfflineBtn->setVisible(false);
    m_addOfflineBtn->setCursor(Qt::PointingHandCursor);
    btnLayout->addWidget(m_addOfflineBtn);

    btnLayout->addStretch();

    m_cancelBtn = new QPushButton("Cancel", this);
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    btnLayout->addWidget(m_cancelBtn);

    m_connectBtn = new QPushButton("Connect & Add", this);
    m_connectBtn->setObjectName("primaryBtn");
    m_connectBtn->setCursor(Qt::PointingHandCursor);
    btnLayout->addWidget(m_connectBtn);

    mainLayout->addLayout(btnLayout);

    connect(m_connectBtn, &QPushButton::clicked, this, &AddPeerDialog::onConnectAndAdd);
    connect(m_addOfflineBtn, &QPushButton::clicked, this, &AddPeerDialog::onAddOfflineAnyway);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_ipEdit, &QLineEdit::returnPressed, this, &AddPeerDialog::onConnectAndAdd);

    m_ipEdit->setFocus();
}

bool AddPeerDialog::parseAddress(const QString &text, QString &outIp, quint16 &outPort) {
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return false;

    QString ipPart = trimmed;
    quint16 portPart = 42424;

    if (trimmed.contains(':')) {
        QStringList parts = trimmed.split(':');
        ipPart = parts[0].trimmed();
        bool ok = false;
        int p = parts[1].trimmed().toInt(&ok);
        if (ok && p > 0 && p <= 65535) {
            portPart = static_cast<quint16>(p);
        } else {
            return false;
        }
    }

    QHostAddress addr(ipPart);
    if (addr.protocol() != QAbstractSocket::IPv4Protocol) {
        return false;
    }

    outIp = ipPart;
    outPort = portPart;
    return true;
}

void AddPeerDialog::onConnectAndAdd() {
    QString ip;
    quint16 port = 42424;

    if (!parseAddress(m_ipEdit->text(), ip, port)) {
        m_statusLabel->setStyleSheet("color: #ef4444; font-size: 12px;");
        m_statusLabel->setText("Please enter a valid IPv4 address (e.g. 192.168.1.100 or 192.168.1.100:42424).");
        return;
    }

    m_lastIp = ip;
    m_lastPort = port;

    m_connectBtn->setEnabled(false);
    m_ipEdit->setEnabled(false);
    m_spinner->setVisible(true);
    m_addOfflineBtn->setVisible(false);
    m_statusLabel->setStyleSheet("color: #00a884; font-size: 12px;");
    m_statusLabel->setText(QString("Connecting to peer at %1:%2 and fetching alias...").arg(ip).arg(port));

    NetworkManager::instance().addPeerManual(ip, port, [this, ip, port](bool success, QString alias, QString errorMsg) {
        m_spinner->setVisible(false);
        m_connectBtn->setEnabled(true);
        m_ipEdit->setEnabled(true);

        if (success) {
            m_statusLabel->setStyleSheet("color: #00a884; font-size: 13px; font-weight: bold;");
            m_statusLabel->setText(QString("Connected! Peer added with alias: \"%1\"").arg(alias));

            // Select this peer
            Peer peer;
            if (DatabaseManager::instance().getPeerByAddress(ip, port, peer)) {
                m_addedPeerId = peer.id;
            }

            QTimer::singleShot(800, this, &QDialog::accept);
        } else {
            m_statusLabel->setStyleSheet("color: #f59e0b; font-size: 12px;");
            m_statusLabel->setText(
                QString("Peer unreachable at %1:%2 (%3).\nAdd anyway? Alias will be updated once peer comes online.")
                .arg(ip).arg(port).arg(errorMsg)
            );
            m_addOfflineBtn->setVisible(true);
        }
    });
}

void AddPeerDialog::onAddOfflineAnyway() {
    if (m_lastIp.isEmpty()) return;

    // Add with IP as temporary alias and status OFFLINE
    int peerId = DatabaseManager::instance().addOrUpdatePeer(m_lastIp, m_lastPort, m_lastIp, false);
    m_addedPeerId = peerId;
    accept();
}

#include "SettingsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include "../db/DatabaseManager.h"
#include "../network/NetworkManager.h"

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent) {
    setWindowTitle("Settings");
    setModal(true);
    setFixedSize(480, 320);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 20, 24, 20);
    mainLayout->setSpacing(16);

    QLabel *titleLabel = new QLabel("Application Settings", this);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #00a884;");
    mainLayout->addWidget(titleLabel);

    QFormLayout *form = new QFormLayout();
    form->setSpacing(12);

    m_aliasEdit = new QLineEdit(this);
    m_aliasEdit->setText(DatabaseManager::instance().getUserAlias());
    m_aliasEdit->setPlaceholderText("Your Alias (supports spaces)...");
    form->addRow("Your Alias:", m_aliasEdit);

    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1024, 65535);
    m_portSpin->setValue(DatabaseManager::instance().getListenPort());
    form->addRow("Listen Port:", m_portSpin);

    m_pollingSpin = new QSpinBox(this);
    m_pollingSpin->setRange(2, 300);
    m_pollingSpin->setSuffix(" seconds");
    m_pollingSpin->setValue(DatabaseManager::instance().getPollingInterval());
    form->addRow("Polling Interval:", m_pollingSpin);

    QHBoxLayout *dirLayout = new QHBoxLayout();
    m_downloadDirEdit = new QLineEdit(this);
    m_downloadDirEdit->setText(DatabaseManager::instance().getDownloadDirectory());
    dirLayout->addWidget(m_downloadDirEdit);

    QPushButton *browseBtn = new QPushButton("Browse...", this);
    connect(browseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseDownloadDir);
    dirLayout->addWidget(browseBtn);

    form->addRow("Downloads Folder:", dirLayout);

    mainLayout->addLayout(form);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #ef4444; font-size: 12px;");
    m_statusLabel->setVisible(false);
    mainLayout->addWidget(m_statusLabel);

    mainLayout->addStretch();

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    QPushButton *cancelBtn = new QPushButton("Cancel", this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    QPushButton *saveBtn = new QPushButton("Save Changes", this);
    saveBtn->setObjectName("primaryBtn");
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::onSave);
    btnLayout->addWidget(saveBtn);

    mainLayout->addLayout(btnLayout);
}

void SettingsDialog::onBrowseDownloadDir() {
    QString dir = QFileDialog::getExistingDirectory(
        this,
        "Select Downloads Directory",
        m_downloadDirEdit->text()
    );
    if (!dir.isEmpty()) {
        m_downloadDirEdit->setText(dir);
    }
}

void SettingsDialog::onSave() {
    QString alias = m_aliasEdit->text().trimmed();
    if (alias.isEmpty()) {
        m_statusLabel->setText("Alias cannot be blank.");
        m_statusLabel->setVisible(true);
        return;
    }

    DatabaseManager::instance().setUserAlias(alias);
    DatabaseManager::instance().setDownloadDirectory(m_downloadDirEdit->text());

    int newPolling = m_pollingSpin->value();
    NetworkManager::instance().setPollingInterval(newPolling);

    quint16 newPort = static_cast<quint16>(m_portSpin->value());
    if (newPort != NetworkManager::instance().serverPort()) {
        NetworkManager::instance().startServer(newPort);
    }

    accept();
}

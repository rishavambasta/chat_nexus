#include "FirstLaunchDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>

FirstLaunchDialog::FirstLaunchDialog(QWidget *parent)
    : QDialog(parent) {
    setWindowTitle("Welcome to P2P Chat");
    setModal(true);
    setFixedSize(420, 240);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(28, 24, 28, 24);
    mainLayout->setSpacing(16);

    QLabel *titleLabel = new QLabel("Welcome to P2P Chat", this);
    titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #00a884;");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    QLabel *descLabel = new QLabel("Please choose your display alias. Spaces and special characters are supported.", this);
    descLabel->setStyleSheet("color: #8696a0; font-size: 13px;");
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(descLabel);

    m_aliasEdit = new QLineEdit(this);
    m_aliasEdit->setPlaceholderText("Enter your alias (e.g., Alice Wonderland)...");
    m_aliasEdit->setStyleSheet("font-size: 14px; padding: 10px; border-radius: 8px;");
    mainLayout->addWidget(m_aliasEdit);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setStyleSheet("color: #ef4444; font-size: 12px;");
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setVisible(false);
    mainLayout->addWidget(m_errorLabel);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_confirmBtn = new QPushButton("Get Started", this);
    m_confirmBtn->setObjectName("primaryBtn");
    m_confirmBtn->setMinimumSize(120, 36);
    m_confirmBtn->setCursor(Qt::PointingHandCursor);
    btnLayout->addWidget(m_confirmBtn);

    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    connect(m_confirmBtn, &QPushButton::clicked, this, &FirstLaunchDialog::onConfirm);
    connect(m_aliasEdit, &QLineEdit::returnPressed, this, &FirstLaunchDialog::onConfirm);

    m_aliasEdit->setFocus();
}

QString FirstLaunchDialog::enteredAlias() const {
    return m_aliasEdit->text().trimmed();
}

void FirstLaunchDialog::onConfirm() {
    QString alias = enteredAlias();
    if (alias.isEmpty()) {
        m_errorLabel->setText("Alias cannot be blank.");
        m_errorLabel->setVisible(true);
        m_aliasEdit->setFocus();
        return;
    }

    accept();
}

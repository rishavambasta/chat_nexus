#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void onBrowseDownloadDir();
    void onSave();

private:
    QLineEdit *m_aliasEdit = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QSpinBox *m_pollingSpin = nullptr;
    QLineEdit *m_downloadDirEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
};

#endif // SETTINGSDIALOG_H

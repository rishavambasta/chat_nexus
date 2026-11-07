#ifndef FIRSTLAUNCHDIALOG_H
#define FIRSTLAUNCHDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

class FirstLaunchDialog : public QDialog {
    Q_OBJECT

public:
    explicit FirstLaunchDialog(QWidget *parent = nullptr);

    QString enteredAlias() const;

private slots:
    void onConfirm();

private:
    QLineEdit *m_aliasEdit = nullptr;
    QLabel *m_errorLabel = nullptr;
    QPushButton *m_confirmBtn = nullptr;
};

#endif // FIRSTLAUNCHDIALOG_H

#pragma once

#include <QDialog>
#include <QStringList>

class QListWidget;

class ManageConnectionsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ManageConnectionsDialog(const QStringList &connections, QWidget *parent = nullptr);
    QStringList connections() const;

private:
    QListWidget *listWidget;
    QStringList connList;
};

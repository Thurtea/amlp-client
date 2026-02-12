#include "amlp_manage_connections.h"

#include <QListWidget>
#include <QPushButton>
#include <QBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QLabel>
#include <QIntValidator>

// Simple editor dialog for single connection
class ConnectionEditor : public QDialog {
    Q_OBJECT
public:
    ConnectionEditor(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("Connection");
        auto *lay = new QVBoxLayout(this);
        nameEdit = new QLineEdit(this);
        ipEdit = new QLineEdit(this);
        portEdit = new QLineEdit(this);
        portEdit->setValidator(new QIntValidator(1, 65535, this));

        lay->addWidget(new QLabel("Display name:", this));
        lay->addWidget(nameEdit);
        lay->addWidget(new QLabel("IP / Hostname:", this));
        lay->addWidget(ipEdit);
        lay->addWidget(new QLabel("Port:", this));
        lay->addWidget(portEdit);

        auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
        lay->addWidget(box);
    }

    void setValues(const QString &name, const QString &ip, int port) {
        nameEdit->setText(name);
        ipEdit->setText(ip);
        portEdit->setText(QString::number(port));
    }

    QString name() const { return nameEdit->text().trimmed(); }
    QString ip() const { return ipEdit->text().trimmed(); }
    int port() const { return portEdit->text().toInt(); }

private:
    QLineEdit *nameEdit;
    QLineEdit *ipEdit;
    QLineEdit *portEdit;
};

static QStringList parseEntry(const QString &entry) {
    return entry.split('|');
}

ManageConnectionsDialog::ManageConnectionsDialog(const QStringList &connections, QWidget *parent)
    : QDialog(parent), connList(connections) {
    setWindowTitle("Manage Connections");
    resize(480, 360);

    auto *mainLay = new QVBoxLayout(this);
    listWidget = new QListWidget(this);
    for (const QString &e : connList) {
        auto parts = parseEntry(e);
        QString name = parts.value(0, e);
        QListWidgetItem *it = new QListWidgetItem(name, listWidget);
        it->setData(Qt::UserRole, e);
    }
    mainLay->addWidget(listWidget);

    auto *btnLay = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton("Add", this);
    QPushButton *editBtn = new QPushButton("Edit", this);
    QPushButton *removeBtn = new QPushButton("Remove", this);
    QPushButton *upBtn = new QPushButton("Move Up", this);
    QPushButton *downBtn = new QPushButton("Move Down", this);
    QPushButton *importBtn = new QPushButton("Import", this);
    QPushButton *exportBtn = new QPushButton("Export", this);

    btnLay->addWidget(addBtn);
    btnLay->addWidget(editBtn);
    btnLay->addWidget(removeBtn);
    btnLay->addWidget(upBtn);
    btnLay->addWidget(downBtn);
    btnLay->addStretch();
    btnLay->addWidget(importBtn);
    btnLay->addWidget(exportBtn);
    mainLay->addLayout(btnLay);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLay->addWidget(box);

    connect(addBtn, &QPushButton::clicked, this, [this]() {
        ConnectionEditor ed(this);
        if (ed.exec() == QDialog::Accepted) {
            if (ed.name().isEmpty() || ed.ip().isEmpty() || ed.port() <= 0) {
                QMessageBox::warning(this, "Invalid", "Please provide a name, hostname and valid port.");
                return;
            }
            QString entry = ed.name() + "|" + ed.ip() + "|" + QString::number(ed.port());
            QListWidgetItem *it = new QListWidgetItem(ed.name(), listWidget);
            it->setData(Qt::UserRole, entry);
            connList.append(entry);
        }
    });

    connect(editBtn, &QPushButton::clicked, this, [this]() {
        QListWidgetItem *it = listWidget->currentItem();
        if (!it) return;
        QString e = it->data(Qt::UserRole).toString();
        auto parts = parseEntry(e);
        ConnectionEditor ed(this);
        ed.setValues(parts.value(0), parts.value(1), parts.value(2).toInt());
        if (ed.exec() == QDialog::Accepted) {
            if (ed.name().isEmpty() || ed.ip().isEmpty() || ed.port() <= 0) {
                QMessageBox::warning(this, "Invalid", "Please provide a name, hostname and valid port.");
                return;
            }
            QString entry = ed.name() + "|" + ed.ip() + "|" + QString::number(ed.port());
            it->setText(ed.name());
            it->setData(Qt::UserRole, entry);
            int idx = listWidget->row(it);
            if (idx >= 0 && idx < connList.size()) connList[idx] = entry;
        }
    });

    connect(removeBtn, &QPushButton::clicked, this, [this]() {
        QListWidgetItem *it = listWidget->currentItem();
        if (!it) return;
        int row = listWidget->row(it);
        if (QMessageBox::question(this, "Confirm", "Delete selected connection?") == QMessageBox::Yes) {
            connList.removeAt(row);
            delete listWidget->takeItem(row);
        }
    });

    connect(upBtn, &QPushButton::clicked, this, [this]() {
        int row = listWidget->currentRow();
        if (row <= 0) return;
        QListWidgetItem *it = listWidget->takeItem(row);
        listWidget->insertItem(row - 1, it);
        listWidget->setCurrentRow(row - 1);
        connList.move(row, row - 1);
    });

    connect(downBtn, &QPushButton::clicked, this, [this]() {
        int row = listWidget->currentRow();
        if (row < 0 || row >= listWidget->count() - 1) return;
        QListWidgetItem *it = listWidget->takeItem(row);
        listWidget->insertItem(row + 1, it);
        listWidget->setCurrentRow(row + 1);
        connList.move(row, row + 1);
    });

    connect(importBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Import connections", QString(), "JSON Files (*.json);;All Files (*)");
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QFile::ReadOnly)) { QMessageBox::warning(this, "Error", "Unable to open file."); return; }
        QByteArray data = f.readAll();
        f.close();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isArray()) { QMessageBox::warning(this, "Error", "Invalid JSON format - expected an array."); return; }
        QJsonArray arr = doc.array();
        for (const QJsonValue &v : arr) {
            if (!v.isObject()) continue;
            QJsonObject o = v.toObject();
            QString name = o.value("name").toString();
            QString ip = o.value("ip").toString();
            int port = o.value("port").toInt();
            if (name.isEmpty() || ip.isEmpty() || port <= 0) continue;
            QString entry = name + "|" + ip + "|" + QString::number(port);
            QListWidgetItem *it = new QListWidgetItem(name, listWidget);
            it->setData(Qt::UserRole, entry);
            connList.append(entry);
        }
    });

    connect(exportBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, "Export connections", QString(), "JSON Files (*.json);;All Files (*)");
        if (path.isEmpty()) return;
        QJsonArray arr;
        for (int i = 0; i < listWidget->count(); ++i) {
            QListWidgetItem *it = listWidget->item(i);
            QString e = it->data(Qt::UserRole).toString();
            auto parts = parseEntry(e);
            QJsonObject o;
            o.insert("name", parts.value(0));
            o.insert("ip", parts.value(1));
            o.insert("port", parts.value(2).toInt());
            arr.append(o);
        }
        QJsonDocument doc(arr);
        QFile f(path);
        if (!f.open(QFile::WriteOnly)) { QMessageBox::warning(this, "Error", "Unable to write file."); return; }
        f.write(doc.toJson(QJsonDocument::Indented));
        f.close();
        QMessageBox::information(this, "Exported", "Connections exported.");
    });

    connect(this, &QDialog::accepted, [this]() {
        connList.clear();
        for (int i = 0; i < listWidget->count(); ++i) connList.append(listWidget->item(i)->data(Qt::UserRole).toString());
    });

    connect(box, &QDialogButtonBox::rejected, this, &QDialog::accept);
}

QStringList ManageConnectionsDialog::connections() const {
    return connList;
}

#include "amlp_manage_connections.moc"

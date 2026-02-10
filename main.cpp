#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QTcpSocket>
#include <QMessageBox>
#include <QDialog>
#include <QLabel>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QTimer>

// Simple ANSI to QColor mapping (16 colors)
static QColor ansiColor(int code, bool bright=false) {
    switch(code) {
        case 30: return QColor("#000000"); // black
        case 31: return QColor("#b21818"); // red
        case 32: return QColor("#1fb839"); // green
        case 33: return QColor("#d0a800"); // yellow
        case 34: return QColor("#1f4fb8"); // blue
        case 35: return QColor("#b030b0"); // magenta
        case 36: return QColor("#18b0b8"); // cyan
        case 37: return QColor("#e0e0e0"); // white
        // bright variants fallback to lighter colors
        case 90: return QColor("#555555");
        case 91: return QColor("#ff6e6e");
        case 92: return QColor("#6efc6e");
        case 93: return QColor("#ffd86e");
        case 94: return QColor("#6ea6ff");
        case 95: return QColor("#ff6eff");
        case 96: return QColor("#6ef6ff");
        case 97: return QColor("#ffffff");
        default: return QColor();
    }
}

class ConnectionDialog : public QDialog {
    Q_OBJECT
public:
    ConnectionDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("Connect to Server");
        auto *layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel("IP Address:"));
        ipEdit = new QLineEdit("127.0.0.1", this);
        layout->addWidget(ipEdit);
        layout->addWidget(new QLabel("Port:"));
        portEdit = new QLineEdit("3000", this);
        layout->addWidget(portEdit);

        auto *btnLayout = new QHBoxLayout();
        auto *connectBtn = new QPushButton("Connect", this);
        auto *cancelBtn = new QPushButton("Cancel", this);
        btnLayout->addWidget(connectBtn);
        btnLayout->addWidget(cancelBtn);
        layout->addLayout(btnLayout);

        connect(connectBtn, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    }
    QString getIP() const { return ipEdit->text(); }
    int getPort() const { return portEdit->text().toInt(); }
private:
    QLineEdit *ipEdit;
    QLineEdit *portEdit;
};

class MudClient : public QWidget {
    Q_OBJECT
public:
    MudClient(QWidget *parent = nullptr) : QWidget(parent), passwordMode(false) {
        // UI setup
        auto *layout = new QVBoxLayout(this);
        output = new QPlainTextEdit(this);
        output->setReadOnly(true);
        input = new QLineEdit(this);
        auto *connectBtn = new QPushButton("Connect", this);

        layout->addWidget(output);
        layout->addWidget(input);
        layout->addWidget(connectBtn);

        // Socket setup
        socket = new QTcpSocket(this);

        // Connections
        connect(connectBtn, &QPushButton::clicked, this, &MudClient::connectToServer);
        connect(input, &QLineEdit::returnPressed, this, &MudClient::sendCommand);
        connect(socket, &QTcpSocket::readyRead, this, &MudClient::receiveData);
        connect(socket, &QTcpSocket::connected, [this]() {
            appendPlainTextColored("Connected!\n", QColor("#e0e0e0"));
        });
        connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
                [this](QAbstractSocket::SocketError) {
            QMessageBox::critical(this, "Error", socket->errorString());
        });

        setWindowTitle("AMLP-Client");
        resize(900, 650);

        // Apply dark stylesheet
        QString style = R"(
QWidget {
    background-color: #1a1a2e;
    color: #e0e0e0;
    font-family: 'Consolas', 'Monaco', monospace;
    font-size: 11pt;
}
QPlainTextEdit {
    background-color: #000000;
    color: #ffffff;
    border: 1px solid #3282b8;
    padding: 5px;
}
QLineEdit {
    background-color: #0a0a0a;
    color: #ffffff;
    border: 1px solid #3282b8;
    padding: 5px;
}
QPushButton {
    background-color: #0f4c75;
    color: #ffffff;
    border: none;
    padding: 8px 15px;
    border-radius: 4px;
}
QPushButton:hover {
    background-color: #3282b8;
}
QDialog {
    background-color: #0a0a0a;
}
QLabel {
    color: #ffffff;
}
QWidget {
    background-color: #0a0a0a;
    color: #ffffff;
    font-family: 'Consolas', 'Monaco', monospace;
    font-size: 11pt;
}
)";
        qApp->setStyleSheet(style);
    }

private slots:
    void connectToServer() {
        ConnectionDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            QString ip = dialog.getIP();
            int port = dialog.getPort();
            appendPlainTextColored(QString("Connecting to %1:%2...\n").arg(ip).arg(port), QColor("#e0e0e0"));
            socket->connectToHost(ip, port);
        }
    }

    void sendCommand() {
        QString cmd = input->text() + "\r\n";
        socket->write(cmd.toUtf8());
        // If we just sent password, reset echo mode
        if (passwordMode) {
            passwordMode = false;
            QTimer::singleShot(100, this, [this]() {
                input->setEchoMode(QLineEdit::Normal);
            });
        }
        input->clear();
    }

    // Basic ANSI parsing: split on ESC sequences and apply color formats
    void receiveData() {
        QByteArray data = socket->readAll();
        QString text = QString::fromUtf8(data);

        // Detect password prompts
        QString low = text.toLower();
        if (low.contains("password:") || low.contains("pass:")) {
            passwordMode = true;
            input->setEchoMode(QLineEdit::PasswordEchoOnEdit);
        }

        // Regex to find ANSI CSI sequences like \x1b[31m
        QRegularExpression re("(\\x1b\\[([0-9;]+)m)");
        int lastPos = 0;
        QRegularExpressionMatchIterator i = re.globalMatch(text);
        QTextCursor cur = output->textCursor();
        cur.movePosition(QTextCursor::End);
        QTextCharFormat fmt;
        fmt.setForeground(QBrush(QColor("#ffffff")));
        fmt.setFontWeight(QFont::Normal);
        while (i.hasNext()) {
            QRegularExpressionMatch m = i.next();
            int idx = m.capturedStart(1);
            QString chunk = text.mid(lastPos, idx - lastPos);
            if (!chunk.isEmpty()) {
                cur.insertText(chunk, fmt);
            }
            QString codeStr = m.captured(2);
            QStringList parts = codeStr.split(';');
            for (const QString &part : parts) {
                int code = part.toInt();
                if (code == 0) { // reset
                    fmt.setForeground(QBrush(QColor("#ffffff")));
                    fmt.setFontWeight(QFont::Normal);
                } else if (code == 1) { // bold
                    fmt.setFontWeight(QFont::Bold);
                } else if ((code >= 30 && code <= 37) || (code >= 90 && code <= 97)) {
                    QColor c = ansiColor(code);
                    if (c.isValid()) fmt.setForeground(QBrush(c));
                }
            }
            lastPos = m.capturedEnd(1);
        }
        QString tail = text.mid(lastPos);
        if (!tail.isEmpty()) cur.insertText(tail, fmt);
        output->setTextCursor(cur);
        output->ensureCursorVisible();
    }

    void appendPlainTextColored(const QString &text, const QColor &color) {
        QTextCursor cur = output->textCursor();
        cur.movePosition(QTextCursor::End);
        QTextCharFormat fmt;
        fmt.setForeground(QBrush(color));
        cur.insertText(text, fmt);
        output->setTextCursor(cur);
        output->ensureCursorVisible();
    }

private:
    QPlainTextEdit *output;
    QLineEdit *input;
    QTcpSocket *socket;
    bool passwordMode;
};

#include "main.moc"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MudClient client;
    client.show();
    return app.exec();
}

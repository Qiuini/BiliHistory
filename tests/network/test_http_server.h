#pragma once

#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <QList>
#include <QPointer>

// 用于 HttpClient 单元测试的本地 mock HTTP 服务器。
// 支持：排队响应、延迟、状态码、请求计数、记录最近一次 method/path/body。
class TestHttpServer : public QTcpServer {
    Q_OBJECT
public:
    struct Response {
        int statusCode = 200;
        QByteArray body;
        int delayMs = 0;
    };

    explicit TestHttpServer(QObject* parent = nullptr)
        : QTcpServer(parent)
    {
    }

    bool start() {
        return listen(QHostAddress::LocalHost, 0);
    }

    quint16 serverPort() const {
        return QTcpServer::serverPort();
    }

    void enqueueResponse(int statusCode, const QByteArray& body, int delayMs = 0) {
        m_responses.append({statusCode, body, delayMs});
    }

    void enqueueResponse(const Response& response) {
        m_responses.append(response);
    }

    int requestCount() const {
        return m_requestCount;
    }

    QByteArray lastPath() const {
        return m_lastPath;
    }

    QByteArray lastMethod() const {
        return m_lastMethod;
    }

    QByteArray lastBody() const {
        return m_lastBody;
    }

protected:
    void incomingConnection(qintptr handle) override {
        auto socket = new QTcpSocket(this);
        socket->setSocketDescriptor(handle);

        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            // 读取请求行 + 头部
            QByteArray firstLine;
            int contentLength = 0;
            while (socket->canReadLine()) {
                const QByteArray line = socket->readLine();
                if (firstLine.isEmpty()) {
                    firstLine = line;
                }
                // 解析 Content-Length 以便读取 body
                if (line.toLower().startsWith("content-length:")) {
                    const QByteArray value = line.mid(15).trimmed();
                    contentLength = value.toInt();
                }
                if (line == "\r\n" || line == "\n") {
                    break;
                }
            }

            // 读取 body（如果存在）
            QByteArray body;
            if (contentLength > 0) {
                while (socket->bytesAvailable() < contentLength) {
                    socket->waitForReadyRead(50);
                }
                body = socket->read(contentLength);
            }

            ++m_requestCount;
            const QList<QByteArray> parts = firstLine.split(' ');
            if (parts.size() >= 2) {
                m_lastMethod = parts[0].trimmed();
                m_lastPath = parts[1];
            }
            m_lastBody = body;

            Response resp;
            if (!m_responses.isEmpty()) {
                resp = m_responses.takeFirst();
            } else {
                resp.statusCode = 200;
                resp.body = QByteArrayLiteral("{}");
            }

            const QByteArray statusText = (resp.statusCode == 200) ? "OK"
                                      : (resp.statusCode == 401) ? "Unauthorized"
                                      : (resp.statusCode == 403) ? "Forbidden"
                                      : (resp.statusCode == 500) ? "Internal Server Error"
                                      : "Unknown";
            const QByteArray header = QByteArrayLiteral("HTTP/1.1 ")
                + QByteArray::number(resp.statusCode) + " " + statusText
                + QByteArrayLiteral("\r\nContent-Type: application/json\r\nContent-Length: ")
                + QByteArray::number(resp.body.size())
                + QByteArrayLiteral("\r\nConnection: close\r\n\r\n");

            QPointer<QTcpSocket> guard(socket);
            auto writeResponse = [guard, header, resp]() {
                if (!guard || guard->state() != QAbstractSocket::ConnectedState) {
                    return;
                }
                guard->write(header + resp.body);
                guard->flush();
                guard->disconnectFromHost();
            };

            if (resp.delayMs > 0) {
                QTimer::singleShot(resp.delayMs, this, writeResponse);
            } else {
                writeResponse();
            }
        });

        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }

private:
    QList<Response> m_responses;
    int m_requestCount = 0;
    QByteArray m_lastPath;
    QByteArray m_lastMethod;
    QByteArray m_lastBody;
};

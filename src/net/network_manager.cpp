#include "net/network_manager.h"

#include <utility>
#include <variant>

namespace {

constexpr qint64 kMaxLineBytes = 1 << 20;  // 一行訊息的上限；超過就視為對方壞掉

}  // namespace

NetworkManager::NetworkManager(NetConfig config, QObject* parent) : QObject(parent), config(config) {
    heartbeatTimer.setInterval(config.heartbeatInterval);
    timeoutTimer.setInterval(config.timeoutCheckInterval);
    connect(&heartbeatTimer, &QTimer::timeout, this, [this] { write(PingMessage{}); });  // N6
    connect(&timeoutTimer, &QTimer::timeout, this, &NetworkManager::onTimeoutCheck);
    connect(&server, &QTcpServer::newConnection, this, &NetworkManager::onNewConnection);
}

NetworkManager::~NetworkManager() {
    close();
}

bool NetworkManager::host(const MatchConfig& match, quint16 port) {
    isHost = true;
    matchConfig = match;
    return server.listen(QHostAddress::Any, port);
}

quint16 NetworkManager::serverPort() const {
    return server.serverPort();
}

void NetworkManager::join(const QHostAddress& address, quint16 port) {
    isHost = false;
    auto* s = new QTcpSocket(this);
    adopt(s);
    connect(s, &QTcpSocket::connected, this, [this] { write(HelloMessage{}); });  // N5
    s->connectToHost(address, port);
}

void NetworkManager::send(const Message& message) {
    if (handshaken) {
        write(message);
    }
}

void NetworkManager::sendLeave() {
    if (!handshaken) {
        close();
        return;
    }
    write(LeaveMessage{});
    QTcpSocket* s = std::exchange(socket, nullptr);
    handshaken = false;
    heartbeatTimer.stop();
    timeoutTimer.stop();
    // 不馬上關閉：等對方讀到 leave 後關閉連線（N8），逾時才強制中斷
    s->disconnect(this);
    s->setParent(nullptr);
    s->flush();
    connect(s, &QAbstractSocket::disconnected, s, &QObject::deleteLater);
    QTimer::singleShot(config.leaveGracePeriod, s, [s] {
        s->abort();
        s->deleteLater();
    });
}

void NetworkManager::close() {
    server.close();
    releaseSocket();
}

void NetworkManager::simulateConnectionLoss() {
    if (socket) {
        socket->abort();
        onSocketLost();
    }
}

void NetworkManager::stopHeartbeat() {
    heartbeatTimer.stop();
}

void NetworkManager::adopt(QTcpSocket* s) {
    socket = s;
    lastReceived.start();
    connect(s, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);
    connect(s, &QTcpSocket::disconnected, this, &NetworkManager::onSocketLost);
    connect(s, &QTcpSocket::errorOccurred, this, &NetworkManager::onSocketLost);
    heartbeatTimer.start();
    timeoutTimer.start();
}

// N1：一個房間只接受一位對手，其他連線直接關閉
void NetworkManager::onNewConnection() {
    while (server.hasPendingConnections()) {
        QTcpSocket* s = server.nextPendingConnection();
        if (socket) {
            s->abort();
            s->deleteLater();
            continue;
        }
        adopt(s);
    }
}

void NetworkManager::onReadyRead() {
    while (socket && socket->canReadLine()) {
        lastReceived.restart();
        if (const auto message = decodeMessage(socket->readLine())) {
            handle(*message);  // 壞掉的訊息直接略過
        }
    }
    if (socket && socket->bytesAvailable() > kMaxLineBytes) {
        socket->abort();
        onSocketLost();
    }
}

void NetworkManager::handle(const Message& message) {
    if (const auto* hello = std::get_if<HelloMessage>(&message)) {
        if (!isHost || handshaken) {
            return;
        }
        write(WelcomeMessage{kProtocolVersion, matchConfig, PlayerId::White});  // N7：主機為黑方
        if (hello->version != kProtocolVersion) {
            releaseSocket();  // N5：加入方看到 welcome 的版本號後顯示提示；主機繼續等下一位
            return;
        }
        handshaken = true;
        server.close();
        emit connected(matchConfig, PlayerId::Black);
        return;
    }
    if (const auto* welcome = std::get_if<WelcomeMessage>(&message)) {
        if (isHost || handshaken) {
            return;
        }
        if (welcome->version != kProtocolVersion) {
            releaseSocket();
            emit versionMismatch(welcome->version);  // N5
            return;
        }
        handshaken = true;
        emit connected(welcome->config, welcome->yourColor);
        return;
    }
    if (!handshaken || std::holds_alternative<PingMessage>(message)) {
        return;
    }
    if (std::holds_alternative<LeaveMessage>(message)) {
        releaseSocket();
        emit opponentLeft();  // N8：不是 disconnected
        return;
    }
    emit messageReceived(message);
}

void NetworkManager::write(const Message& message) {
    if (socket && socket->state() == QAbstractSocket::ConnectedState) {
        socket->write(encodeMessage(message));
    }
}

void NetworkManager::onTimeoutCheck() {
    if (socket && lastReceived.elapsed() > config.timeout) {
        socket->abort();  // N6
        onSocketLost();
    }
}

void NetworkManager::onSocketLost() {
    if (!socket) {
        return;
    }
    // 主機在握手前斷掉的連線不必通知，繼續等下一位；加入方連不上主機時要通知
    const bool notify = handshaken || !isHost;
    QTcpSocket* s = std::exchange(socket, nullptr);
    s->disconnect(this);
    s->abort();
    s->deleteLater();
    handshaken = false;
    heartbeatTimer.stop();
    timeoutTimer.stop();
    if (notify) {
        emit disconnected();
    }
}

void NetworkManager::releaseSocket() {
    handshaken = false;
    heartbeatTimer.stop();
    timeoutTimer.stop();
    if (!socket) {
        return;
    }
    QTcpSocket* s = std::exchange(socket, nullptr);
    s->disconnect(this);
    s->setParent(nullptr);
    s->readAll();  // 丟掉沒讀的資料，關閉時才不會送出 RST 讓對方漏掉最後一則訊息
    connect(s, &QAbstractSocket::disconnected, s, &QObject::deleteLater);
    QTimer::singleShot(config.leaveGracePeriod, s, [s] {
        s->abort();
        s->deleteLater();
    });
    s->disconnectFromHost();
}

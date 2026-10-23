#pragma once

#include <QElapsedTimer>
#include <QHostAddress>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include "core/config.h"
#include "core/types.h"
#include "net/protocol.h"

inline constexpr quint16 kDefaultPort = 50505;  // spec N1

// 區網連線的時間設定（spec N4、N6）；測試可以縮短
struct NetConfig {
    int heartbeatInterval = 1000;   // N6：每 1000 ms 送一次 ping
    int timeout = 5000;             // N6：超過 5000 ms 沒收到任何訊息就視為斷線
    int timeoutCheckInterval = 100; // 多久檢查一次是否逾時
    int snapshotInterval = 200;     // N4：對局中主機定時送快照的間隔
    int leaveGracePeriod = 1000;    // N8：送出 leave 後等對方關閉連線的最長時間
};

// 一條 TCP 連線（design §4.9）：主機只接受一位對手（N1）、握手時比對協定版本（N5）、
// 心跳與逾時（N6）、leave 後斷線（N8）。訊息內容由 protocol 編解碼，這裡不解讀遊戲內容。
class NetworkManager : public QObject {
    Q_OBJECT
public:
    explicit NetworkManager(NetConfig = {}, QObject* parent = nullptr);
    ~NetworkManager() override;

    // 主機：開始監聽；config 在握手時以 welcome 傳給加入方（N7）。port 為 0 時由系統挑選（測試用）
    bool host(const MatchConfig&, quint16 port);
    quint16 serverPort() const;
    // 加入方：連線後送出 hello
    void join(const QHostAddress&, quint16 port);

    const NetConfig& netConfig() const { return config; }
    bool isConnected() const { return handshaken; }  // 握手完成且尚未斷線
    void send(const Message&);                       // 握手完成後才會送出
    void sendLeave();                                // N8：送出 leave 後斷線，不再發出任何 signal
    void close();                                    // 不通知對方、不發 signal，直接結束連線
    void simulateConnectionLoss();                   // 測試用：像網路線被拔掉一樣中斷，並發出 disconnected
    void stopHeartbeat();                            // 測試用：像程式卡住一樣不再送 ping（N6）

signals:
    void connected(MatchConfig, PlayerId yourColor);  // 握手完成：主機收到相同版本的 hello、加入方收到 welcome
    void versionMismatch(int remoteVersion);          // N5：只有加入方會發出，之後連線已關閉
    void messageReceived(const Message&);              // 握手、ping、leave 以外的訊息
    void opponentLeft();                               // N8：收到 leave
    void disconnected();                               // N6：心跳逾時或 socket 斷開（加入方連不上主機時也會發出）

private:
    void adopt(QTcpSocket*);
    void onNewConnection();
    void onReadyRead();
    void onSocketLost();
    void onTimeoutCheck();
    void handle(const Message&);
    void write(const Message&);
    void releaseSocket();  // 讓 socket 自己等對方關閉後刪除，不影響本物件

    NetConfig config;
    QTcpServer server;
    QTcpSocket* socket = nullptr;
    bool isHost = false;
    bool handshaken = false;
    MatchConfig matchConfig;  // 主機才有意義
    QTimer heartbeatTimer;
    QTimer timeoutTimer;
    QElapsedTimer lastReceived;
};

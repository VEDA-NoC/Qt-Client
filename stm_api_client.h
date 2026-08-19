#pragma once

#include "stm_types.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector>

class PlaybackApiClient;
class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

// STM 장치 조회·제어 API 클라이언트. 타이머도 UI 상태도 갖지 않는다 — 요청
// 1건 -> 시그널 1건만 한다. 5초 폴링(장치 목록)과 명령 상태 재조회는 주기·
// 수명이 서로 다른 소비자라, 클라이언트가 타이머를 소유하면 서로의 주기를
// 덮어쓴다.
//
// 세션(토큰·TLS)은 자체 보유하지 않고 PlaybackApiClient를 매 요청 시 읽는다
// (소유하지 않는다) — 토큰 복사본을 두지 않으므로 재로그인 후에도 stale이
// 되지 않는다.
class StmApiClient : public QObject {
    Q_OBJECT

public:
    explicit StmApiClient(PlaybackApiClient *session, QObject *parent = nullptr);

    void fetchDevices();
    void submitCommand(int slave_address, int opcode);
    void fetchCommandStatus(int slave_address, quint32 command_id);
    void unregisterDevice(const QString &device_uid);

signals:
    void devicesReceived(const QVector<StmDevice> &devices);
    void commandSubmitted(int slave_address, const StmCommandSubmitOutcome &outcome);
    void commandStatusReceived(int slave_address, const StmCommandStatus &status);
    void deviceUnregistered(const QString &device_uid, int channel_id, const QString &space_id);
    void requestFailed(const QString &operation, const QString &message, int http_status);

private:
    QUrl endpointUrl(const QString &path) const;
    QNetworkRequest createRequest(const QUrl &url) const;
    void monitorReply(QNetworkReply *reply, const QString &operation, int timeout_ms);
    bool readReply(QNetworkReply *reply,
                   const QString &operation,
                   const QList<int> &expected_statuses,
                   QByteArray *body,
                   int *status_out);

    PlaybackApiClient *session_ = nullptr;
    QNetworkAccessManager *network_ = nullptr;
    bool devices_request_in_progress_ = false;
    quint64 next_request_id_ = 1;
};

#include "stm_api_client.h"

#include "playback_api_client.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslCertificate>
#include <QSslError>
#include <QStringList>
#include <QTimer>
#include <QUrlQuery>
#include <QVariant>

namespace {

constexpr int kCommandTimeoutMs = 10000;
constexpr int kDevicesTimeoutMs = 4000;  // 5초 폴링 주기보다 짧게 둔다.

QString networkErrorText(QNetworkReply *reply) {
    switch (reply->error()) {
    case QNetworkReply::ConnectionRefusedError:
        return "Control 서버에 연결할 수 없습니다. 서버 실행 상태와 주소를 확인하세요.";
    case QNetworkReply::HostNotFoundError:
        return "Control 서버 주소를 찾을 수 없습니다.";
    case QNetworkReply::TimeoutError:
        return "Control 서버 연결 시간이 초과되었습니다.";
    case QNetworkReply::RemoteHostClosedError:
        return "Control 서버가 연결을 종료했습니다.";
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::TemporaryNetworkFailureError:
        return "네트워크 연결을 사용할 수 없습니다.";
    default:
        return QString("Control 서버 연결 오류: %1").arg(reply->errorString());
    }
}

QString responseError(const QByteArray &body, const QString &fallback) {
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (document.isObject()) {
        const QString error = document.object().value("error").toString();
        if (!error.isEmpty()) {
            return error;
        }
    }
    return fallback;
}

bool readNullableInteger(const QJsonObject &object, const QString &key, bool *available, int *result) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        *available = false;
        *result = 0;
        return true;
    }
    if (!value.isDouble()) {
        return false;
    }
    *available = true;
    *result = value.toInt();
    return true;
}

StmDeviceRegistrationInfo parseRegistration(const QJsonValue &value) {
    StmDeviceRegistrationInfo registration;
    if (!value.isObject()) {
        return registration;
    }
    const QJsonObject object = value.toObject();
    registration.registered = true;
    registration.channel_id = object.value("channel_id").toInt();
    registration.space_id = object.value("space_id").toString();
    registration.space_label = object.value("space_label").toString();
    registration.space_type = object.value("space_type").toString();
    return registration;
}

bool parseState(const QJsonValue &value, StmDeviceState *state) {
    if (!value.isObject()) {
        *state = StmDeviceState();
        return true;
    }
    const QJsonObject object = value.toObject();
    state->available = true;
    state->temperature_c = object.value("temperature_c").toDouble();
    state->temperature_valid = object.value("temperature_valid").toBool();
    state->temperature_stale = object.value("temperature_stale").toBool();
    if (!readNullableInteger(object, "temperature_age_ms", &state->temperature_age_available,
                             &state->temperature_age_ms)) {
        return false;
    }
    state->occupied = object.value("occupied").toBool();
    state->fire_active = object.value("fire_active").toBool();
    state->sensor_fault = object.value("sensor_fault").toBool();
    state->actuator_fault = object.value("actuator_fault").toBool();
    state->rearm_required = object.value("rearm_required").toBool();
    state->stage1_status = object.value("stage1_status").toInt();
    state->stage1_status_name = object.value("stage1_status_name").toString();
    state->stage2_status = object.value("stage2_status").toInt();
    state->stage2_status_name = object.value("stage2_status_name").toString();
    return true;
}

bool parseDevice(const QJsonObject &object, StmDevice *device) {
    device->device_uid = object.value("device_uid").toString();
    device->slave_address = object.value("slave_address").toInt();
    device->link = object.value("link").toString();
    device->assigned_boot_session_id =
        static_cast<quint32>(object.value("assigned_boot_session_id").toVariant().toULongLong());
    device->registration = parseRegistration(object.value("registration"));
    return parseState(object.value("state"), &device->state);
}

bool parseDevices(const QByteArray &body, QVector<StmDevice> *devices) {
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isObject()) {
        return false;
    }
    const QJsonObject root = document.object();
    if (root.value("schema_version").toInt() != 1 || !root.value("devices").isArray()) {
        return false;
    }
    for (const QJsonValue &value : root.value("devices").toArray()) {
        if (!value.isObject()) {
            return false;
        }
        StmDevice device;
        if (!parseDevice(value.toObject(), &device)) {
            return false;
        }
        devices->push_back(device);
    }
    return true;
}

bool parseCommandSubmitOutcome(const QByteArray &body, int http_status, StmCommandSubmitOutcome *outcome) {
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isObject()) {
        return false;
    }
    const QJsonObject object = document.object();
    outcome->result = object.value("result").toString();
    outcome->command_id = static_cast<quint32>(object.value("command_id").toVariant().toULongLong());
    outcome->http_status = http_status;
    return !outcome->result.isEmpty();
}

bool parseCommandStatus(const QByteArray &body, StmCommandStatus *status) {
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isObject()) {
        return false;
    }
    const QJsonObject object = document.object();
    status->command_id = static_cast<quint32>(object.value("command_id").toVariant().toULongLong());
    status->opcode = object.value("opcode").toInt();
    status->origin = object.value("origin").toInt();
    status->status = object.value("status").toInt();
    status->reason = object.value("reason").toInt();
    status->completed = object.value("completed").toBool();
    return true;
}

}  // namespace

StmApiClient::StmApiClient(PlaybackApiClient *session, QObject *parent)
    : QObject(parent), session_(session), network_(new QNetworkAccessManager(this)) {
    qRegisterMetaType<QVector<StmDevice>>("QVector<StmDevice>");
    qRegisterMetaType<StmCommandSubmitOutcome>("StmCommandSubmitOutcome");
    qRegisterMetaType<StmCommandStatus>("StmCommandStatus");
}

QUrl StmApiClient::endpointUrl(const QString &path) const {
    QUrl url = session_->controlBaseUrl();
    QString base_path = url.path();
    while (base_path.endsWith('/')) {
        base_path.chop(1);
    }
    url.setPath(base_path + path);
    url.setQuery(QString());
    url.setFragment(QString());
    return url;
}

QNetworkRequest StmApiClient::createRequest(const QUrl &url) const {
    QNetworkRequest request(url);
    request.setSslConfiguration(session_->sslConfiguration());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization", "Bearer " + session_->accessToken().toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    return request;
}

void StmApiClient::monitorReply(QNetworkReply *reply, const QString &operation, int timeout_ms) {
    const quint64 request_id = next_request_id_++;
    reply->setProperty("stmOperation", operation);
    reply->setProperty("stmRequestId", QVariant::fromValue(request_id));
    reply->setProperty("stmStartedAtMs", QDateTime::currentMSecsSinceEpoch());
    reply->setProperty("stmTimeoutMs", timeout_ms);
    qInfo().noquote() << QString("[stm-api] request_id=%1 state=started operation=%2 timeout_ms=%3 url=%4")
                              .arg(request_id)
                              .arg(operation)
                              .arg(timeout_ms)
                              .arg(reply->url().toString(QUrl::FullyEncoded));
    connect(reply, &QNetworkReply::sslErrors, this, [reply](const QList<QSslError> &errors) {
        QStringList messages;
        messages.reserve(errors.size());
        for (const QSslError &error : errors) {
            messages.push_back(error.errorString());
        }
        reply->setProperty("stmTlsFailure", QString("TLS 검증 실패: %1").arg(messages.join("; ")));
        reply->abort();
    });
    connect(reply, &QNetworkReply::encrypted, this, [this, reply]() {
        const QSslCertificate peer = reply->sslConfiguration().peerCertificate();
        const QByteArray peer_digest = peer.digest(QCryptographicHash::Sha256);
        if (peer.isNull() || peer_digest != session_->pinnedCertificateSha256()) {
            reply->setProperty("stmTlsFailure", "TLS 인증서 pin이 설정된 server.crt와 일치하지 않습니다.");
            reply->abort();
        }
    });
    QTimer::singleShot(timeout_ms, reply, [reply]() {
        if (reply->isRunning()) {
            reply->setProperty("stmTimedOut", true);
            reply->abort();
        }
    });
}

bool StmApiClient::readReply(QNetworkReply *reply,
                             const QString &operation,
                             const QList<int> &expected_statuses,
                             QByteArray *body,
                             int *status_out) {
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status_out) {
        *status_out = status;
    }
    const quint64 request_id = reply->property("stmRequestId").toULongLong();
    const qint64 elapsed_ms =
        qMax<qint64>(0, QDateTime::currentMSecsSinceEpoch() - reply->property("stmStartedAtMs").toLongLong());
    qInfo().noquote() << QString("[stm-api] request_id=%1 state=finished operation=%2 http=%3 network_error=%4 elapsed_ms=%5")
                              .arg(request_id)
                              .arg(operation)
                              .arg(status)
                              .arg(static_cast<int>(reply->error()))
                              .arg(elapsed_ms);
    const QString tls_failure = reply->property("stmTlsFailure").toString();
    if (!tls_failure.isEmpty()) {
        emit requestFailed(operation, tls_failure, status);
        return false;
    }
    if (reply->property("stmTimedOut").toBool()) {
        const int timeout_ms = reply->property("stmTimeoutMs").toInt();
        emit requestFailed(operation, QString("요청 시간이 %1초를 초과했습니다.").arg(timeout_ms / 1000), status);
        return false;
    }
    if (status == 0 && reply->error() != QNetworkReply::NoError) {
        emit requestFailed(operation, networkErrorText(reply), status);
        return false;
    }
    *body = reply->readAll();
    const QSslCertificate peer = reply->sslConfiguration().peerCertificate();
    if (!peer.isNull() && peer.digest(QCryptographicHash::Sha256) != session_->pinnedCertificateSha256()) {
        emit requestFailed(operation, "TLS 인증서 pin이 설정된 server.crt와 일치하지 않습니다.", status);
        return false;
    }
    if (status == 401) {
        // 서버가 토큰을 회수한 경우 PlaybackApiClient::readReply와 동일하게
        // 세션을 정리한다 — 안 그러면 만료 시각만 보고 계속 유효로 판단해
        // 다른 소비자(장치 폴링 등)의 요청도 계속 401을 받는다.
        session_->clearSession();
    }
    if (!expected_statuses.contains(status)) {
        emit requestFailed(operation, responseError(*body, reply->errorString()), status);
        return false;
    }
    return true;
}

void StmApiClient::fetchDevices() {
    const QString operation = "stm.devices";
    if (!session_->hasValidAccessToken()) {
        emit requestFailed(operation, "로그인이 필요합니다.", 401);
        return;
    }
    if (devices_request_in_progress_) {
        return;
    }
    devices_request_in_progress_ = true;
    QNetworkRequest request = createRequest(endpointUrl("/api/v1/stm-devices"));
    QNetworkReply *reply = network_->get(request);
    monitorReply(reply, operation, kDevicesTimeoutMs);
    connect(reply, &QNetworkReply::finished, this, [this, reply, operation]() {
        devices_request_in_progress_ = false;
        QByteArray body;
        if (!readReply(reply, operation, {200}, &body, nullptr)) {
            reply->deleteLater();
            return;
        }
        QVector<StmDevice> devices;
        if (!parseDevices(body, &devices)) {
            emit requestFailed(operation, "STM 장치 응답 형식이 올바르지 않습니다.", 200);
        } else {
            emit devicesReceived(devices);
        }
        reply->deleteLater();
    });
}

void StmApiClient::submitCommand(int slave_address, int opcode) {
    const QString operation = QString("stm.command.submit.slave%1").arg(slave_address);
    if (!session_->hasValidAccessToken()) {
        emit requestFailed(operation, "로그인이 필요합니다.", 401);
        return;
    }
    QJsonObject payload;
    payload.insert("slave_address", slave_address);
    payload.insert("opcode", opcode);
    const QByteArray req_body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkRequest request = createRequest(endpointUrl("/api/v1/stm-commands"));
    QNetworkReply *reply = network_->post(request, req_body);
    monitorReply(reply, operation, kCommandTimeoutMs);
    connect(reply, &QNetworkReply::finished, this, [this, reply, operation, slave_address]() {
        QByteArray body;
        int status = 0;
        // 409는 오류가 아니다 — "이미 진행 중인 명령이 있습니다" 판정에
        // command_id가 필요하므로 정상 성공 경로로 취급한다.
        if (!readReply(reply, operation, {200, 201, 409}, &body, &status)) {
            reply->deleteLater();
            return;
        }
        StmCommandSubmitOutcome outcome;
        if (!parseCommandSubmitOutcome(body, status, &outcome)) {
            emit requestFailed(operation, "STM 명령 응답 형식이 올바르지 않습니다.", status);
        } else {
            emit commandSubmitted(slave_address, outcome);
        }
        reply->deleteLater();
    });
}

void StmApiClient::fetchCommandStatus(int slave_address, quint32 command_id) {
    const QString operation = QString("stm.command.status.slave%1").arg(slave_address);
    if (!session_->hasValidAccessToken()) {
        emit requestFailed(operation, "로그인이 필요합니다.", 401);
        return;
    }
    QUrl url = endpointUrl("/api/v1/stm-commands");
    QUrlQuery query;
    query.addQueryItem("slave_address", QString::number(slave_address));
    query.addQueryItem("command_id", QString::number(command_id));
    url.setQuery(query);
    QNetworkRequest request = createRequest(url);
    QNetworkReply *reply = network_->get(request);
    monitorReply(reply, operation, kCommandTimeoutMs);
    connect(reply, &QNetworkReply::finished, this, [this, reply, operation, slave_address]() {
        QByteArray body;
        if (!readReply(reply, operation, {200}, &body, nullptr)) {
            reply->deleteLater();
            return;
        }
        StmCommandStatus status;
        if (!parseCommandStatus(body, &status)) {
            emit requestFailed(operation, "STM 명령 상태 응답 형식이 올바르지 않습니다.", 200);
        } else {
            emit commandStatusReceived(slave_address, status);
        }
        reply->deleteLater();
    });
}

void StmApiClient::unregisterDevice(const QString &device_uid) {
    const QString operation = "stm.registration.delete";
    if (!session_->hasValidAccessToken()) {
        emit requestFailed(operation, "로그인이 필요합니다.", 401);
        return;
    }
    QNetworkRequest request = createRequest(endpointUrl("/api/v1/stm-registrations/" + device_uid));
    QNetworkReply *reply = network_->deleteResource(request);
    monitorReply(reply, operation, kCommandTimeoutMs);
    connect(reply, &QNetworkReply::finished, this, [this, reply, operation, device_uid]() {
        QByteArray body;
        if (!readReply(reply, operation, {200}, &body, nullptr)) {
            reply->deleteLater();
            return;
        }
        const QJsonDocument document = QJsonDocument::fromJson(body);
        if (!document.isObject()) {
            emit requestFailed(operation, "STM 등록 해제 응답 형식이 올바르지 않습니다.", 200);
            reply->deleteLater();
            return;
        }
        const QJsonObject object = document.object();
        emit deviceUnregistered(device_uid, object.value("channel_id").toInt(), object.value("space_id").toString());
        reply->deleteLater();
    });
}

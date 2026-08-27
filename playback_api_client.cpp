#include "playback_api_client.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslCertificate>
#include <QSslError>
#include <QSslSocket>
#include <QStringList>
#include <QTimer>
#include <QUrlQuery>
#include <QVariant>

#include <limits>

namespace {

constexpr int kControlRequestTimeoutMs = 10000;
constexpr int kMediaLookupTimeoutMs = 30000;
constexpr qsizetype kMaxResponseBytes = 8 * 1024 * 1024;

qint64 jsonInteger(const QJsonObject &object, const QString &key) {
    return object.value(key).toVariant().toLongLong();
}

bool isHexCapability(const QString &value) {
    if (value.size() != 64) {
        return false;
    }
    for (const QChar ch : value) {
        const ushort value = ch.unicode();
        const bool decimal = value >= '0' && value <= '9';
        const bool lower_hex = value >= 'a' && value <= 'f';
        if (!decimal && !lower_hex) {
            return false;
        }
    }
    return true;
}

bool readNullableDouble(const QJsonObject &object,
                        const QString &key,
                        bool *available,
                        double *result) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        *available = false;
        *result = 0.0;
        return true;
    }
    if (!value.isDouble()) {
        return false;
    }
    *available = true;
    *result = value.toDouble();
    return true;
}

bool readNullableInteger(const QJsonObject &object,
                         const QString &key,
                         bool *available,
                         qint64 *result) {
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
    *result = value.toVariant().toLongLong();
    return true;
}

QVector<ParkingValidationError> parseParkingValidationErrors(
    const QJsonArray &array) {
    QVector<ParkingValidationError> errors;
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        ParkingValidationError error;
        error.code = object.value("code").toString();
        error.message = object.value("message").toString();
        error.space_id = object.value("space_id").toString();
        if (!error.code.isEmpty()) {
            errors.push_back(error);
        }
    }
    return errors;
}

bool parseParkingConfiguration(const QByteArray &body,
                               ParkingConfiguration *result) {
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isObject()) {
        return false;
    }
    const QJsonObject object = document.object();
    if (object.value("schema_version").toInt() != 1) {
        return false;
    }
    result->draft_id = object.value("draft_id").toString();
    result->channel_id = object.value("channel_id").toInt();
    result->base_version = object.value("base_version").toInt();
    result->active_version = object.value("active_version").toInt();
    result->draft_version = object.value("draft_version").toInt();
    result->geometry_id = object.value("geometry_id").toString();
    result->validation_errors = parseParkingValidationErrors(
        object.value("validation_errors").toArray());
    for (const QJsonValue &value : object.value("spaces").toArray()) {
        const QJsonObject item = value.toObject();
        ParkingSpace space;
        space.space_id = item.value("space_id").toString();
        space.label = item.value("label").toString();
        space.space_type = item.value("space_type").toString();
        space.enabled = item.value("enabled").toBool(true);
        const QJsonObject mapping = item.value("stm_mapping").toObject();
        space.stm_mapping.device_uid = mapping.value("device_uid").toString();
        space.stm_mapping.sensor_zone_id =
            mapping.value("sensor_zone_id").toString();
        for (const QJsonValue &point_value : item.value("polygon").toArray()) {
            const QJsonObject point = point_value.toObject();
            const double x = point.value("x").toDouble(-1.0);
            const double y = point.value("y").toDouble(-1.0);
            if (x < 0.0 || x > 1.0 || y < 0.0 || y > 1.0) {
                return false;
            }
            space.polygon.push_back(QPointF(x, y));
        }
        if (space.space_id.isEmpty() || space.polygon.size() != 4) {
            return false;
        }
        result->spaces.push_back(space);
    }
    return result->channel_id >= 1 && result->channel_id <= 4 &&
           result->active_version >= 0 && result->draft_version >= 0;
}

QByteArray parkingConfigurationBody(const ParkingConfiguration &configuration) {
    QJsonObject root;
    root.insert("schema_version", 1);
    root.insert("channel_id", configuration.channel_id);
    root.insert("base_version", configuration.base_version);
    root.insert("geometry_id", configuration.geometry_id);
    QJsonArray spaces;
    for (const ParkingSpace &space : configuration.spaces) {
        QJsonObject item;
        item.insert("space_id", space.space_id);
        item.insert("label", space.label);
        item.insert("space_type", space.space_type);
        item.insert("enabled", space.enabled);
        QJsonArray polygon;
        for (const QPointF &point : space.polygon) {
            polygon.push_back(QJsonObject{{"x", point.x()}, {"y", point.y()}});
        }
        item.insert("polygon", polygon);
        item.insert("stm_mapping",
                    QJsonObject{{"device_uid", space.stm_mapping.device_uid},
                                {"sensor_zone_id", space.stm_mapping.sensor_zone_id}});
        spaces.push_back(item);
    }
    root.insert("spaces", spaces);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

bool parseParkingApplyJob(const QByteArray &body, ParkingApplyJob *job) {
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isObject()) {
        return false;
    }
    const QJsonObject object = document.object();
    if (object.value("schema_version").toInt() != 1) {
        return false;
    }
    job->job_id = object.value("job_id").toString();
    job->draft_id = object.value("draft_id").toString();
    job->state = object.value("state").toString();
    job->error_code = object.value("error_code").toString();
    job->active_version = object.value("active_version").toInt();
    job->draft_version = object.value("draft_version").toInt();
    static const QStringList states = {
        "applying", "succeeded", "failed", "unavailable"};
    return !job->job_id.isEmpty() && states.contains(job->state);
}

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

}  // namespace

PlaybackApiClient::PlaybackApiClient(QObject *parent)
    : QObject(parent), network_(new QNetworkAccessManager(this)) {
    qRegisterMetaType<PlaybackTimeline>("PlaybackTimeline");
    qRegisterMetaType<PlaybackSession>("PlaybackSession");
    qRegisterMetaType<StorageStatus>("StorageStatus");
    qRegisterMetaType<DeviceStatusSnapshot>("DeviceStatusSnapshot");
    qRegisterMetaType<ParkingConfiguration>("ParkingConfiguration");
    qRegisterMetaType<ParkingValidationResult>("ParkingValidationResult");
    qRegisterMetaType<ParkingApplyJob>("ParkingApplyJob");
}

PlaybackApiClient::~PlaybackApiClient() {
    clearSession();
    password_.fill(QChar('\0'));
    password_.clear();
}

bool PlaybackApiClient::configure(const QUrl &control_base_url,
                                  const QString &username,
                                  const QString &password,
                                  const QString &certificate_path,
                                  QString *error) {
    clearSession();
    password_.fill(QChar('\0'));
    password_.clear();
    configured_ = false;

    if (!control_base_url.isValid() ||
        control_base_url.scheme().compare("https", Qt::CaseInsensitive) != 0 ||
        control_base_url.host().isEmpty()) {
        *error = "Control URL은 https://호스트[:포트] 형식이어야 합니다.";
        return false;
    }
    if (username.isEmpty()) {
        *error = "Control 사용자 이름이 설정되지 않았습니다.";
        return false;
    }
    if (password.isEmpty()) {
        *error = "Control 비밀번호를 입력하세요.";
        return false;
    }
    if (!QFileInfo::exists(certificate_path)) {
        *error = QString("서버 인증서를 찾을 수 없습니다: %1").arg(certificate_path);
        return false;
    }

    const QList<QSslCertificate> certificates =
        QSslCertificate::fromPath(certificate_path, QSsl::Pem);
    if (certificates.isEmpty() || certificates.first().isNull()) {
        *error = "PEM 서버 인증서를 읽을 수 없습니다.";
        return false;
    }

    QSslConfiguration configuration = QSslConfiguration::defaultConfiguration();
    QList<QSslCertificate> trusted_certificates = configuration.caCertificates();
    trusted_certificates.append(certificates);
    configuration.setCaCertificates(trusted_certificates);
    configuration.setPeerVerifyMode(QSslSocket::VerifyPeer);
    configuration.setProtocol(QSsl::TlsV1_2OrLater);

    control_base_url_ = control_base_url;
    QString base_path = control_base_url_.path();
    while (base_path.endsWith('/')) {
        base_path.chop(1);
    }
    control_base_url_.setPath(base_path);
    control_base_url_.setQuery(QString());
    control_base_url_.setFragment(QString());
    username_ = username;
    password_ = password;
    certificate_path_ = certificate_path;
    ssl_configuration_ = configuration;
    pinned_certificate_sha256_ =
        certificates.first().digest(QCryptographicHash::Sha256);
    configured_ = true;
    return true;
}

bool PlaybackApiClient::isConfigured() const {
    return configured_;
}

bool PlaybackApiClient::hasValidAccessToken() const {
    return configured_ && !access_token_.isEmpty() &&
           QDateTime::currentDateTimeUtc() < token_expires_at_utc_;
}

void PlaybackApiClient::clearSession() {
    access_token_.fill('\0');
    access_token_.clear();
    token_expires_at_utc_ = {};
}

void PlaybackApiClient::deconfigure() {
    clearSession();
    configured_ = false;
    control_base_url_ = QUrl();
    username_.clear();
    password_.fill(QChar('\0'));
    password_.clear();
    certificate_path_.clear();
    ssl_configuration_ = QSslConfiguration();
    pinned_certificate_sha256_.clear();
}

QUrl PlaybackApiClient::endpointUrl(const QString &path) const {
    QUrl url = control_base_url_;
    QString base_path = url.path();
    while (base_path.endsWith('/')) {
        base_path.chop(1);
    }
    url.setPath(base_path + path);
    url.setQuery(QString());
    url.setFragment(QString());
    return url;
}

QNetworkRequest PlaybackApiClient::createRequest(const QUrl &url) const {
    QNetworkRequest request(url);
    request.setSslConfiguration(ssl_configuration_);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    return request;
}

void PlaybackApiClient::monitorReply(QNetworkReply *reply, const QString &operation) {
    const int timeout_ms =
        operation.startsWith("timeline.ch") || operation == "thumbnail"
            ? kMediaLookupTimeoutMs
            : kControlRequestTimeoutMs;
    const quint64 request_id = next_request_id_++;
    reply->setProperty("vmsOperation", operation);
    reply->setProperty("vmsRequestId", QVariant::fromValue(request_id));
    reply->setProperty("vmsStartedAtMs", QDateTime::currentMSecsSinceEpoch());
    reply->setProperty("vmsTimeoutMs", timeout_ms);
    qInfo().noquote()
        << QString("[playback-api] request_id=%1 state=started operation=%2 timeout_ms=%3 url=%4")
               .arg(request_id)
               .arg(operation)
               .arg(timeout_ms)
               .arg(reply->url().toString(QUrl::FullyEncoded));
    connect(reply,
            &QNetworkReply::sslErrors,
            this,
            [reply](const QList<QSslError> &errors) {
                QStringList messages;
                messages.reserve(errors.size());
                for (const QSslError &error : errors) {
                    messages.push_back(error.errorString());
                }
                reply->setProperty("vmsTlsFailure",
                                   QString("TLS 검증 실패: %1").arg(messages.join("; ")));
                reply->abort();
            });
    connect(reply, &QNetworkReply::encrypted, this, [this, reply]() {
        const QSslCertificate peer = reply->sslConfiguration().peerCertificate();
        const QByteArray peer_digest = peer.digest(QCryptographicHash::Sha256);
        if (peer.isNull() || peer_digest != pinned_certificate_sha256_) {
            reply->setProperty("vmsTlsFailure",
                               "TLS 인증서 pin이 설정된 server.crt와 일치하지 않습니다.");
            reply->abort();
            return;
        }
    });
    QTimer::singleShot(timeout_ms, reply, [reply]() {
        if (reply->isRunning()) {
            reply->setProperty("vmsTimedOut", true);
            reply->abort();
        }
    });
}

QString PlaybackApiClient::responseError(const QByteArray &body,
                                         const QString &fallback) const {
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (document.isObject()) {
        const QJsonObject object = document.object();
        const QString error = object.value("error").toString();
        if (!error.isEmpty()) {
            return error;
        }
    }
    return fallback;
}

bool PlaybackApiClient::readReply(QNetworkReply *reply,
                                  const QString &operation,
                                  const QList<int> &expected_statuses,
                                  QByteArray *body) {
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const quint64 request_id =
        reply->property("vmsRequestId").toULongLong();
    const qint64 elapsed_ms =
        qMax<qint64>(0,
                     QDateTime::currentMSecsSinceEpoch() -
                         reply->property("vmsStartedAtMs").toLongLong());
    qInfo().noquote()
        << QString("[playback-api] request_id=%1 state=finished operation=%2 http=%3 network_error=%4 elapsed_ms=%5")
               .arg(request_id)
               .arg(operation)
               .arg(status)
               .arg(static_cast<int>(reply->error()))
               .arg(elapsed_ms);
    const QString tls_failure = reply->property("vmsTlsFailure").toString();
    if (!tls_failure.isEmpty()) {
        emit requestFailed(operation, tls_failure, status);
        return false;
    }
    if (reply->property("vmsTimedOut").toBool()) {
        const int timeout_ms = reply->property("vmsTimeoutMs").toInt();
        emit requestFailed(operation,
                           QString("요청 시간이 %1초를 초과했습니다.")
                               .arg(timeout_ms / 1000),
                           status);
        return false;
    }
    // abort()된 timeout/TLS reply는 이미 닫혀 있으므로 readAll()하지 않는다.
    // 네트워크 오류 응답은 HTTP body를 신뢰할 수 없으므로 먼저 처리한다.
    if (status == 0 && reply->error() != QNetworkReply::NoError) {
        emit requestFailed(operation, networkErrorText(reply), status);
        return false;
    }
    *body = reply->readAll();
    if (body->size() > kMaxResponseBytes) {
        emit requestFailed(operation, "서버 응답 크기 제한을 초과했습니다.", status);
        return false;
    }
    // TCP 연결 자체가 실패하면 peer 인증서가 없으므로, 인증서 오류로
    // 오인하지 않고 네트워크 오류를 먼저 보고한다.
    // Qt가 재사용한 TLS 연결의 완료된 reply에는 peerCertificate()가
    // 비어 있을 수 있다. sslErrors와 encrypted 시점에서 이미 CA/SAN과
    // pin을 검증하므로, 여기서 인증서 부재를 TLS 실패로 재분류하지 않는다.
    // 인증서가 제공된 경우에는 방어적으로 pin 불일치만 다시 거부한다.
    const QSslCertificate peer = reply->sslConfiguration().peerCertificate();
    if (!peer.isNull() &&
        peer.digest(QCryptographicHash::Sha256) !=
            pinned_certificate_sha256_) {
        emit requestFailed(
            operation,
            "TLS 인증서 pin이 설정된 server.crt와 일치하지 않습니다.",
            status);
        return false;
    }
    if (status == 401) {
        clearSession();
    }
    if (!expected_statuses.contains(status)) {
        emit requestFailed(operation,
                           responseError(*body, reply->errorString()),
                           status);
        return false;
    }
    return true;
}

void PlaybackApiClient::login() {
    if (!configured_) {
        emit requestFailed("login", "Control API 설정이 적용되지 않았습니다.", 0);
        return;
    }

    QNetworkRequest request = createRequest(endpointUrl("/api/v1/login"));
    QByteArray raw_credentials =
        QString("%1:%2").arg(username_, password_).toUtf8();
    QByteArray encoded_credentials = raw_credentials.toBase64();
    request.setRawHeader("Authorization", "Basic " + encoded_credentials);
    QNetworkReply *reply = network_->post(request, QByteArray());
    raw_credentials.fill('\0');
    encoded_credentials.fill('\0');
    monitorReply(reply, "login");
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QByteArray body;
        if (!readReply(reply, "login", {200}, &body)) {
            reply->deleteLater();
            return;
        }
        const QJsonDocument document = QJsonDocument::fromJson(body);
        const QJsonObject object = document.object();
        const QByteArray token = object.value("access_token").toString().toUtf8();
        const qint64 expires_in = jsonInteger(object, "expires_in_seconds");
        if (!document.isObject() || token.isEmpty() || expires_in <= 0) {
            emit requestFailed("login", "로그인 응답 형식이 올바르지 않습니다.", 200);
            reply->deleteLater();
            return;
        }
        access_token_ = token;
        token_expires_at_utc_ =
            QDateTime::currentDateTimeUtc().addSecs(qMax<qint64>(1, expires_in - 30));
        emit loginSucceeded(expires_in);
        reply->deleteLater();
    });
}

void PlaybackApiClient::requestStatus() {
    if (status_request_in_progress_) {
        return;
    }
    if (!hasValidAccessToken()) {
        emit requestFailed("status", "로그인이 필요합니다.", 401);
        return;
    }

    QNetworkRequest request =
        createRequest(endpointUrl("/api/v1/status"));
    request.setRawHeader("Authorization", "Bearer " + access_token_);
    QNetworkReply *reply = network_->get(request);
    status_request_in_progress_ = true;
    monitorReply(reply, "status");
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        status_request_in_progress_ = false;
        QByteArray body;
        if (!readReply(reply, "status", {200}, &body)) {
            reply->deleteLater();
            return;
        }

        const QJsonDocument document = QJsonDocument::fromJson(body);
        const QJsonObject object = document.object();
        DeviceStatusSnapshot snapshot;
        bool valid = document.isObject() &&
                     object.value("schema_version").toInt() == 1 &&
                     object.value("service").toString() == "rpi-vms" &&
                     object.value("status").toString() == "ok";

        const QJsonValue server_time_value = object.value("server_time");
        if (valid && server_time_value.isObject()) {
            const QJsonObject server_time = server_time_value.toObject();
            snapshot.server_time.utc_ms = jsonInteger(server_time, "utc_ms");
            snapshot.server_time.monotonic_ms =
                jsonInteger(server_time, "monotonic_ms");
            snapshot.server_time.sync_state =
                server_time.value("sync_state").toString();
            snapshot.server_time.boot_id =
                server_time.value("boot_id").toString();
            static const QStringList allowed_sync_states = {
                "synchronized", "unsynchronized", "unknown"};
            valid = snapshot.server_time.utc_ms > 0 &&
                    snapshot.server_time.monotonic_ms >= 0 &&
                    allowed_sync_states.contains(
                        snapshot.server_time.sync_state);
        } else if (valid && !server_time_value.isUndefined()) {
            valid = false;
        }

        const QJsonValue system_value = object.value("system");
        if (valid && system_value.isObject()) {
            snapshot.system_available = true;
            const QJsonObject system = system_value.toObject();
            SystemStatus &status = snapshot.system;
            status.state = system.value("state").toString();
            status.sampled_at_utc_ms =
                jsonInteger(system, "sampled_at_utc_ms");
            status.sample_age_ms = jsonInteger(system, "sample_age_ms");
            static const QStringList allowed_system_states = {
                "normal", "warning", "critical", "unknown"};
            valid = allowed_system_states.contains(status.state) &&
                    status.sampled_at_utc_ms >= 0 &&
                    status.sample_age_ms >= 0 &&
                    readNullableDouble(system,
                                       "cpu_usage_percent",
                                       &status.cpu_usage_available,
                                       &status.cpu_usage_percent) &&
                    readNullableDouble(system,
                                       "load_average_1m",
                                       &status.load_average_available,
                                       &status.load_average_1m) &&
                    readNullableDouble(system,
                                       "temperature_celsius",
                                       &status.temperature_available,
                                       &status.temperature_celsius) &&
                    readNullableInteger(system,
                                        "uptime_seconds",
                                        &status.uptime_available,
                                        &status.uptime_seconds);
            valid = valid &&
                    (!status.cpu_usage_available ||
                     (status.cpu_usage_percent >= 0.0 &&
                      status.cpu_usage_percent <= 100.0)) &&
                    (!status.load_average_available ||
                     status.load_average_1m >= 0.0) &&
                    (!status.temperature_available ||
                     (status.temperature_celsius >= -100.0 &&
                      status.temperature_celsius <= 200.0)) &&
                    (!status.uptime_available ||
                     status.uptime_seconds >= 0);

            const QJsonValue memory_value = system.value("memory");
            if (valid && memory_value.isObject()) {
                const QJsonObject memory = memory_value.toObject();
                valid = memory.value("available").isBool();
                status.memory.available =
                    memory.value("available").toBool(false);
                if (valid && status.memory.available) {
                    status.memory.total_bytes =
                        jsonInteger(memory, "total_bytes");
                    status.memory.used_bytes =
                        jsonInteger(memory, "used_bytes");
                    status.memory.available_bytes =
                        jsonInteger(memory, "available_bytes");
                    status.memory.used_percent =
                        memory.value("used_percent").toDouble(-1.0);
                    valid = status.memory.total_bytes >= 0 &&
                            status.memory.used_bytes >= 0 &&
                            status.memory.available_bytes >= 0 &&
                            status.memory.used_bytes <=
                                status.memory.total_bytes &&
                            status.memory.available_bytes <=
                                status.memory.total_bytes &&
                            status.memory.used_percent >= 0.0 &&
                            status.memory.used_percent <= 100.0;
                }
            } else if (valid && !memory_value.isUndefined() &&
                       !memory_value.isNull()) {
                valid = false;
            }

            const QJsonValue throttling_value =
                system.value("throttling");
            if (valid && throttling_value.isObject()) {
                const QJsonObject throttling =
                    throttling_value.toObject();
                valid = throttling.value("available").isBool();
                status.throttling.available =
                    throttling.value("available").toBool(false);
                const quint64 raw_flags =
                    throttling.value("raw_flags")
                        .toVariant()
                        .toULongLong();
                if (status.throttling.available) {
                    valid = valid &&
                            throttling.value("raw_flags").isDouble() &&
                            raw_flags <=
                                std::numeric_limits<quint32>::max();
                }
                status.throttling.raw_flags =
                    static_cast<quint32>(raw_flags);
                status.throttling.under_voltage_now =
                    throttling.value("under_voltage_now").toBool(false);
                status.throttling.frequency_capped_now =
                    throttling.value("frequency_capped_now").toBool(false);
                status.throttling.throttled_now =
                    throttling.value("throttled_now").toBool(false);
                status.throttling.soft_temperature_limit_now =
                    throttling.value("soft_temperature_limit_now")
                        .toBool(false);
                status.throttling.under_voltage_occurred =
                    throttling.value("under_voltage_occurred")
                        .toBool(false);
                status.throttling.frequency_capped_occurred =
                    throttling.value("frequency_capped_occurred")
                        .toBool(false);
                status.throttling.throttled_occurred =
                    throttling.value("throttled_occurred").toBool(false);
                status.throttling.soft_temperature_limit_occurred =
                    throttling.value("soft_temperature_limit_occurred")
                        .toBool(false);
            } else if (valid && !throttling_value.isUndefined() &&
                       !throttling_value.isNull()) {
                valid = false;
            }
        } else if (valid && !system_value.isUndefined() &&
                   !system_value.isNull()) {
            valid = false;
        }

        const QJsonValue storage_value = object.value("storage");
        if (valid && storage_value.isObject()) {
            snapshot.storage_available = true;
            const QJsonObject storage = storage_value.toObject();
            StorageStatus &status = snapshot.storage;
            status.state = storage.value("state").toString();
            status.total_bytes = jsonInteger(storage, "total_bytes");
            status.used_bytes = jsonInteger(storage, "used_bytes");
            status.free_bytes = jsonInteger(storage, "free_bytes");
            status.available_bytes =
                jsonInteger(storage, "available_bytes");
            status.used_percent =
                storage.value("used_percent").toDouble(-1.0);
            status.recording_suspended =
                storage.value("recording_suspended").toBool(false);
            static const QStringList allowed_storage_states = {
                "normal", "warning", "critical", "read_only"};
            valid = allowed_storage_states.contains(status.state) &&
                    status.total_bytes >= 0 && status.used_bytes >= 0 &&
                    status.free_bytes >= 0 &&
                    status.available_bytes >= 0 &&
                    status.used_bytes <= status.total_bytes &&
                    status.free_bytes <= status.total_bytes &&
                    status.available_bytes <= status.free_bytes &&
                    status.used_percent >= 0.0 &&
                    status.used_percent <= 100.0 &&
                    storage.value("recording_suspended").isBool();
        } else if (valid && !storage_value.isUndefined() &&
                   !storage_value.isNull()) {
            valid = false;
        }

        if (!valid) {
            emit requestFailed(
                "status",
                "장치 상태 응답 형식이 올바르지 않습니다.",
                200);
            reply->deleteLater();
            return;
        }

        emit statusReceived(snapshot);
        reply->deleteLater();
    });
}

void PlaybackApiClient::requestTimeline(int channel_id,
                                        qint64 start_utc_ms,
                                        qint64 end_utc_ms) {
    if (!hasValidAccessToken()) {
        emit requestFailed(QString("timeline.ch%1").arg(channel_id),
                           "로그인이 필요합니다.",
                           401);
        return;
    }

    QUrl url = endpointUrl(
        QString("/api/v1/channels/%1/timeline").arg(channel_id));
    QUrlQuery query;
    query.addQueryItem("start_utc_ms", QString::number(start_utc_ms));
    query.addQueryItem("end_utc_ms", QString::number(end_utc_ms));
    url.setQuery(query);
    QNetworkRequest request = createRequest(url);
    request.setRawHeader("Authorization", "Bearer " + access_token_);
    QNetworkReply *reply = network_->get(request);
    const QString operation =
        QString("timeline.ch%1").arg(channel_id);
    monitorReply(reply, operation);
    connect(reply, &QNetworkReply::finished, this, [this, reply, channel_id,
                                                    operation]() {
        QByteArray body;
        if (!readReply(reply, operation, {200}, &body)) {
            reply->deleteLater();
            return;
        }

        const QJsonDocument document = QJsonDocument::fromJson(body);
        const QJsonObject object = document.object();
        const QJsonObject range = object.value("range").toObject();
        PlaybackTimeline timeline;
        timeline.channel_id = object.value("channel_id").toInt();
        timeline.start_utc_ms = jsonInteger(range, "start_utc_ms");
        timeline.end_utc_ms = jsonInteger(range, "end_utc_ms");
        if (!document.isObject() || object.value("schema_version").toInt() != 1 ||
            timeline.channel_id != channel_id ||
            timeline.end_utc_ms <= timeline.start_utc_ms) {
            emit requestFailed(operation,
                               "Timeline 응답 형식이 올바르지 않습니다.",
                               200);
            reply->deleteLater();
            return;
        }

        for (const QJsonValue &value : object.value("spans").toArray()) {
            const QJsonObject item = value.toObject();
            PlaybackTimelineSpan span;
            span.kind = item.value("kind").toString();
            span.start_utc_ms = jsonInteger(item, "start_utc_ms");
            span.end_utc_ms = jsonInteger(item, "end_utc_ms");
            for (const QJsonValue &segment_id :
                 item.value("segment_ids").toArray()) {
                span.segment_ids.push_back(segment_id.toVariant().toLongLong());
            }
            if ((span.kind != "recording" && span.kind != "gap") ||
                span.start_utc_ms < timeline.start_utc_ms ||
                span.end_utc_ms > timeline.end_utc_ms ||
                span.end_utc_ms <= span.start_utc_ms) {
                emit requestFailed(operation,
                                   "Timeline span이 올바르지 않습니다.",
                                   200);
                reply->deleteLater();
                return;
            }
            timeline.spans.push_back(span);
        }

        for (const QJsonValue &value : object.value("events").toArray()) {
            const QJsonObject item = value.toObject();
            PlaybackTimelineEvent event;
            event.request_id = jsonInteger(item, "request_id");
            event.event_type = item.value("event_type").toString();
            event.severity = item.value("severity").toString();
            event.correlation_id = item.value("correlation_id").toString();
            event.start_utc_ms = jsonInteger(item, "start_utc_ms");
            event.end_utc_ms = jsonInteger(item, "end_utc_ms");
            if (event.end_utc_ms > event.start_utc_ms) {
                timeline.events.push_back(event);
            }
        }

        emit timelineReceived(timeline);
        reply->deleteLater();
    });
}

void PlaybackApiClient::requestThumbnail(int channel_id,
                                         qint64 utc_ms,
                                         int width) {
    if (!hasValidAccessToken()) {
        emit requestFailed("thumbnail", "로그인이 필요합니다.", 401);
        return;
    }
    if (channel_id < 1 || utc_ms <= 0 || width < 160 || width > 1920) {
        emit requestFailed("thumbnail",
                           "Thumbnail 요청 값이 올바르지 않습니다.",
                           0);
        return;
    }

    QUrl url = endpointUrl(
        QString("/api/v1/channels/%1/thumbnail").arg(channel_id));
    QUrlQuery query;
    query.addQueryItem("utc_ms", QString::number(utc_ms));
    query.addQueryItem("width", QString::number(width));
    url.setQuery(query);
    QNetworkRequest request = createRequest(url);
    request.setRawHeader("Authorization", "Bearer " + access_token_);
    QNetworkReply *reply = network_->get(request);
    monitorReply(reply, "thumbnail");
    connect(reply,
            &QNetworkReply::finished,
            this,
            [this, reply, channel_id, utc_ms]() {
                QByteArray body;
                if (!readReply(reply, "thumbnail", {200}, &body)) {
                    reply->deleteLater();
                    return;
                }
                const QString content_type =
                    reply->header(QNetworkRequest::ContentTypeHeader)
                        .toString()
                        .toLower();
                const QImage image = QImage::fromData(body, "JPEG");
                if ((!content_type.isEmpty() &&
                     !content_type.startsWith("image/jpeg")) ||
                    image.isNull()) {
                    emit requestFailed(
                        "thumbnail",
                        "Thumbnail 응답이 올바른 JPEG가 아닙니다.",
                        200);
                    reply->deleteLater();
                    return;
                }
                emit thumbnailReceived(channel_id, utc_ms, image);
                reply->deleteLater();
            });
}

void PlaybackApiClient::createPlaybackSession(int channel_id,
                                              qint64 start_utc_ms,
                                              qint64 end_utc_ms) {
    if (!hasValidAccessToken()) {
        emit requestFailed("playback", "로그인이 필요합니다.", 401);
        return;
    }

    QUrl url = endpointUrl("/api/v1/playback-sessions");
    QUrlQuery query;
    query.addQueryItem("channel_id", QString::number(channel_id));
    query.addQueryItem("start_utc_ms", QString::number(start_utc_ms));
    query.addQueryItem("end_utc_ms", QString::number(end_utc_ms));
    url.setQuery(query);
    QNetworkRequest request = createRequest(url);
    request.setRawHeader("Authorization", "Bearer " + access_token_);
    QNetworkReply *reply = network_->post(request, QByteArray());
    monitorReply(reply, "playback");
    connect(reply, &QNetworkReply::finished, this, [this, reply, channel_id]() {
        QByteArray body;
        if (!readReply(reply, "playback", {201}, &body)) {
            reply->deleteLater();
            return;
        }

        const QJsonDocument document = QJsonDocument::fromJson(body);
        const QJsonObject object = document.object();
        PlaybackSession session;
        session.playback_session_id =
            object.value("playback_session_id").toString();
        session.rtsps_path = object.value("rtsps_path").toString();
        session.channel_id = object.value("channel_id").toInt();
        session.start_utc_ms = jsonInteger(object, "start_utc_ms");
        session.end_utc_ms = jsonInteger(object, "end_utc_ms");
        session.duration_ms = jsonInteger(object, "duration_ms");
        session.segment_count = object.value("segment_count").toInt();
        session.one_shot = object.value("one_shot").toBool();
        if (!document.isObject() || object.value("schema_version").toInt() != 1 ||
            !isHexCapability(session.playback_session_id) ||
            session.rtsps_path !=
                QString("/playback/%1").arg(session.playback_session_id) ||
            session.channel_id != channel_id ||
            session.end_utc_ms <= session.start_utc_ms ||
            session.duration_ms <= 0 || session.segment_count <= 0 ||
            !session.one_shot) {
            emit requestFailed("playback",
                               "Playback session 응답 형식이 올바르지 않습니다.",
                               201);
            reply->deleteLater();
            return;
        }
        emit playbackSessionCreated(session);
        reply->deleteLater();
    });
}

void PlaybackApiClient::deletePlaybackSession(const QString &session_id) {
    if (!hasValidAccessToken() || !isHexCapability(session_id)) {
        return;
    }
    QNetworkRequest request =
        createRequest(endpointUrl(QString("/api/v1/playback-sessions/%1")
                                      .arg(session_id)));
    request.setRawHeader("Authorization", "Bearer " + access_token_);
    QNetworkReply *reply = network_->deleteResource(request);
    monitorReply(reply, "playback.delete");
    connect(reply, &QNetworkReply::finished, this, [this, reply, session_id]() {
        QByteArray body;
        if (readReply(reply, "playback.delete", {200, 404}, &body)) {
            emit playbackSessionDeleted(session_id);
        }
        reply->deleteLater();
    });
}

void PlaybackApiClient::requestParkingSpaces(int channel_id) {
    const QString operation = QString("parking.spaces.ch%1").arg(channel_id);
    if (!hasValidAccessToken()) {
        emit requestFailed(operation, "로그인이 필요합니다.", 401);
        return;
    }
    if (channel_id < 1 || channel_id > 4) {
        emit requestFailed(operation, "채널 번호가 올바르지 않습니다.", 0);
        return;
    }
    QUrl url = endpointUrl("/api/v1/parking-spaces");
    QUrlQuery query;
    query.addQueryItem("channel_id", QString::number(channel_id));
    url.setQuery(query);
    QNetworkRequest request = createRequest(url);
    request.setRawHeader("Authorization", "Bearer " + access_token_);
    QNetworkReply *reply = network_->get(request);
    monitorReply(reply, operation);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, operation, channel_id]() {
        QByteArray body;
        if (!readReply(reply, operation, {200}, &body)) {
            reply->deleteLater();
            return;
        }
        ParkingConfiguration configuration;
        if (!parseParkingConfiguration(body, &configuration) ||
            configuration.channel_id != channel_id) {
            emit requestFailed(operation,
                               "주차 구역 응답 형식이 올바르지 않습니다.", 200);
        } else {
            emit parkingSpacesReceived(configuration);
        }
        reply->deleteLater();
    });
}

void PlaybackApiClient::createParkingDraft(
    const ParkingConfiguration &configuration) {
    const QString operation = "parking.draft.create";
    if (!hasValidAccessToken()) {
        emit requestFailed(operation, "로그인이 필요합니다.", 401);
        return;
    }
    const QByteArray req_body = parkingConfigurationBody(configuration);
    qInfo().noquote()
        << QString("[parking-api] operation=%1 req_body=%2")
               .arg(operation)
               .arg(QString::fromUtf8(req_body));
    QNetworkRequest request =
        createRequest(endpointUrl("/api/v1/parking-space-drafts"));
    request.setRawHeader("Authorization", "Bearer " + access_token_);
    QNetworkReply *reply = network_->post(request, req_body);
    monitorReply(reply, operation);
    connect(reply, &QNetworkReply::finished, this, [this, reply, operation]() {
        QByteArray body;
        const bool ok = readReply(reply, operation, {201}, &body);
        qInfo().noquote()
            << QString("[parking-api] operation=%1 resp_body=%2")
                   .arg(operation)
                   .arg(QString::fromUtf8(body.left(2048)));
        if (!ok) {
            reply->deleteLater();
            return;
        }
        ParkingConfiguration configuration;
        if (!parseParkingConfiguration(body, &configuration) ||
            configuration.draft_id.isEmpty()) {
            emit requestFailed(operation,
                               "주차 구역 draft 응답 형식이 올바르지 않습니다.", 201);
        } else {
            emit parkingDraftReceived(configuration);
        }
        reply->deleteLater();
    });
}

void PlaybackApiClient::updateParkingDraft(
    const QString &draft_id,
    const ParkingConfiguration &configuration) {
    const QString operation = "parking.draft.update";
    if (!hasValidAccessToken()) {
        emit requestFailed(operation, "로그인이 필요합니다.", 401);
        return;
    }
    const QByteArray req_body = parkingConfigurationBody(configuration);
    qInfo().noquote()
        << QString("[parking-api] operation=%1 draft_id=%2 req_body=%3")
               .arg(operation)
               .arg(draft_id)
               .arg(QString::fromUtf8(req_body));
    QNetworkRequest request = createRequest(
        endpointUrl(QString("/api/v1/parking-space-drafts/%1").arg(draft_id)));
    request.setRawHeader("Authorization", "Bearer " + access_token_);
    QNetworkReply *reply = network_->put(request, req_body);
    monitorReply(reply, operation);
    connect(reply, &QNetworkReply::finished, this, [this, reply, operation]() {
        QByteArray body;
        const bool ok = readReply(reply, operation, {200}, &body);
        qInfo().noquote()
            << QString("[parking-api] operation=%1 resp_body=%2")
                   .arg(operation)
                   .arg(QString::fromUtf8(body.left(2048)));
        if (!ok) {
            reply->deleteLater();
            return;
        }
        ParkingConfiguration configuration;
        if (!parseParkingConfiguration(body, &configuration) ||
            configuration.draft_id.isEmpty()) {
            emit requestFailed(operation,
                               "주차 구역 draft 응답 형식이 올바르지 않습니다.", 200);
        } else {
            emit parkingDraftReceived(configuration);
        }
        reply->deleteLater();
    });
}

void PlaybackApiClient::validateParkingDraft(const QString &draft_id) {
    const QString operation = "parking.validate";
    if (!hasValidAccessToken()) {
        emit requestFailed(operation, "로그인이 필요합니다.", 401);
        return;
    }
    QNetworkRequest request = createRequest(endpointUrl(
        QString("/api/v1/parking-space-drafts/%1/validate").arg(draft_id)));
    request.setRawHeader("Authorization", "Bearer " + access_token_);
    QNetworkReply *reply = network_->post(request, QByteArray("{}"));
    monitorReply(reply, operation);
    connect(reply, &QNetworkReply::finished, this, [this, reply, operation]() {
        QByteArray body;
        if (!readReply(reply, operation, {200}, &body)) {
            reply->deleteLater();
            return;
        }
        const QJsonDocument document = QJsonDocument::fromJson(body);
        const QJsonObject object = document.object();
        ParkingValidationResult result;
        result.draft_id = object.value("draft_id").toString();
        result.active_version = object.value("active_version").toInt();
        result.draft_version = object.value("draft_version").toInt();
        result.valid = object.value("valid").toBool();
        result.validation_errors = parseParkingValidationErrors(
            object.value("validation_errors").toArray());
        if (!document.isObject() ||
            object.value("schema_version").toInt() != 1 ||
            result.draft_id.isEmpty()) {
            emit requestFailed(operation,
                               "주차 구역 검증 응답 형식이 올바르지 않습니다.", 200);
        } else {
            emit parkingValidationReceived(result);
        }
        reply->deleteLater();
    });
}

void PlaybackApiClient::applyParkingDraft(const QString &draft_id,
                                          const QString &idempotency_key) {
    const QString operation = "parking.apply";
    if (!hasValidAccessToken()) {
        emit requestFailed(operation, "로그인이 필요합니다.", 401);
        return;
    }
    QNetworkRequest request = createRequest(endpointUrl(
        QString("/api/v1/parking-space-drafts/%1/apply").arg(draft_id)));
    request.setRawHeader("Authorization", "Bearer " + access_token_);
    request.setRawHeader("Idempotency-Key", idempotency_key.toUtf8());
    QNetworkReply *reply = network_->post(request, QByteArray("{}"));
    monitorReply(reply, operation);
    connect(reply, &QNetworkReply::finished, this, [this, reply, operation]() {
        QByteArray body;
        if (!readReply(reply, operation, {202}, &body)) {
            reply->deleteLater();
            return;
        }
        ParkingApplyJob job;
        if (!parseParkingApplyJob(body, &job)) {
            emit requestFailed(operation,
                               "주차 구역 적용 응답 형식이 올바르지 않습니다.", 202);
        } else {
            emit parkingApplyJobReceived(job);
        }
        reply->deleteLater();
    });
}

void PlaybackApiClient::requestParkingApplyJob(const QString &job_id) {
    const QString operation = "parking.job";
    if (!hasValidAccessToken()) {
        emit requestFailed(operation, "로그인이 필요합니다.", 401);
        return;
    }
    QNetworkRequest request = createRequest(endpointUrl(
        QString("/api/v1/parking-space-apply-jobs/%1").arg(job_id)));
    request.setRawHeader("Authorization", "Bearer " + access_token_);
    QNetworkReply *reply = network_->get(request);
    monitorReply(reply, operation);
    connect(reply, &QNetworkReply::finished, this, [this, reply, operation]() {
        QByteArray body;
        if (!readReply(reply, operation, {200}, &body)) {
            reply->deleteLater();
            return;
        }
        ParkingApplyJob job;
        if (!parseParkingApplyJob(body, &job)) {
            emit requestFailed(operation,
                               "주차 구역 적용 작업 응답 형식이 올바르지 않습니다.", 200);
        } else {
            emit parkingApplyJobReceived(job);
        }
        reply->deleteLater();
    });
}

#include "parking_event_api_client.h"
#include <QUrl>
#include <QDebug>

ParkingEventApiClient::ParkingEventApiClient(QObject *parent)
    : QObject(parent), nam_(new QNetworkAccessManager(this)) {
    retry_timer_.setSingleShot(true);
    connect(&retry_timer_, &QTimer::timeout, this, &ParkingEventApiClient::sendNextLongPollRequest);
}

ParkingEventApiClient::~ParkingEventApiClient() {
    stopPolling();
}

void ParkingEventApiClient::setSslConfiguration(const QSslConfiguration &sslConfig) {
    ssl_config_ = sslConfig;
}

void ParkingEventApiClient::startPolling(const QString &baseUrl, const QString &bearerToken) {
    base_url_ = baseUrl;
    bearer_token_ = bearerToken;
    polling_active_ = true;
    next_after_id_ = 0;
    retry_timer_.stop();

    qInfo().noquote() << QString("[event-api] startPolling base_url=%1 after_id=%2")
                            .arg(base_url_)
                            .arg(next_after_id_);

    if (current_reply_) {
        current_reply_->abort();
        current_reply_->deleteLater();
        current_reply_ = nullptr;
    }

    sendNextLongPollRequest();
}

void ParkingEventApiClient::stopPolling() {
    polling_active_ = false;
    retry_timer_.stop();
    if (current_reply_) {
        current_reply_->abort();
        current_reply_->deleteLater();
        current_reply_ = nullptr;
    }
}

void ParkingEventApiClient::sendNextLongPollRequest() {
    if (!polling_active_ || base_url_.isEmpty()) {
        return;
    }

    QUrl url(base_url_ + QStringLiteral("/api/v1/events"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("after_id"), QString::number(next_after_id_));
    query.addQueryItem(QStringLiteral("wait_ms"), QStringLiteral("25000"));
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("50"));
    url.setQuery(query);

    QNetworkRequest request(url);
    if (!bearer_token_.isEmpty()) {
        request.setRawHeader("Authorization", QString(QStringLiteral("Bearer ") + bearer_token_).toUtf8());
    }
    request.setSslConfiguration(ssl_config_);
    request.setTransferTimeout(30000); // 25s wait_ms + 5s buffer

    qInfo().noquote() << QString("[event-api] sending long poll request after_id=%1 url=%2")
                            .arg(next_after_id_)
                            .arg(url.toString());

    current_reply_ = nam_->get(request);
    connect(current_reply_, &QNetworkReply::finished, this, &ParkingEventApiClient::handlePollReplyFinished);
}

void ParkingEventApiClient::handlePollReplyFinished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }

    if (reply == current_reply_) {
        current_reply_ = nullptr;
    }

    reply->deleteLater();

    if (!polling_active_) {
        return;
    }

    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        QString errStr = reply->errorString();
        qWarning().noquote() << QString("[event-api] request failed http=%1 error=%2")
                                    .arg(httpStatus)
                                    .arg(errStr);
        emit pollingErrorOccurred(errStr);

        // 1.5초 후 백오프 재시도
        retry_timer_.start(1500);
        return;
    }

    QByteArray data = reply->readAll();
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning().noquote() << QString("[event-api] json parse failed http=%1 body_len=%2")
                                    .arg(httpStatus)
                                    .arg(data.size());
        emit pollingErrorOccurred(QStringLiteral("Invalid JSON response from event API"));
        retry_timer_.start(1500);
        return;
    }

    QJsonObject rootObj = doc.object();
    ParkingEventBatch batch;
    batch.schema_version = rootObj.value(QStringLiteral("schema_version")).toInt(1);
    // Pi는 이 필드를 "server_utc_ms"로 내보낸다 (control_server.cpp:626).
    // 이전 키 이름("server_time_utc_ms")으로는 항상 0이 읽혔다.
    batch.server_time_utc_ms = static_cast<quint64>(rootObj.value(QStringLiteral("server_utc_ms")).toVariant().toULongLong());
    batch.next_after_id = static_cast<quint64>(rootObj.value(QStringLiteral("next_after_id")).toVariant().toULongLong());

    if (rootObj.contains(QStringLiteral("events")) && rootObj.value(QStringLiteral("events")).isArray()) {
        QJsonArray arr = rootObj.value(QStringLiteral("events")).toArray();
        for (const QJsonValue &val : arr) {
            if (val.isObject()) {
                batch.events.append(ParkingEventItem::fromJson(val.toObject()));
            }
        }
    }

    qInfo().noquote() << QString("[event-api] poll finished http=%1 events_count=%2 next_after_id=%3")
                            .arg(httpStatus)
                            .arg(batch.events.size())
                            .arg(batch.next_after_id);

    if (batch.next_after_id > 0) {
        next_after_id_ = batch.next_after_id;
    }

    emit eventsReceived(batch);

    // 곧바로 다음 Long Polling 전송
    QTimer::singleShot(0, this, &ParkingEventApiClient::sendNextLongPollRequest);
}

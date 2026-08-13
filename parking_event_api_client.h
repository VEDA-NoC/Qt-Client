#ifndef PARKING_EVENT_API_CLIENT_H
#define PARKING_EVENT_API_CLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QTimer>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "parking_event_types.h"

class ParkingEventApiClient : public QObject {
    Q_OBJECT
public:
    explicit ParkingEventApiClient(QObject *parent = nullptr);
    ~ParkingEventApiClient() override;

    void setSslConfiguration(const QSslConfiguration &sslConfig);
    void startPolling(const QString &baseUrl, const QString &bearerToken);
    void stopPolling();

    bool isPolling() const { return polling_active_; }
    quint64 currentAfterId() const { return next_after_id_; }

signals:
    void eventsReceived(const ParkingEventBatch &batch);
    void pollingErrorOccurred(const QString &errorMsg);

private slots:
    void sendNextLongPollRequest();
    void handlePollReplyFinished();

private:
    QNetworkAccessManager *nam_{nullptr};
    QSslConfiguration ssl_config_;
    QString base_url_;
    QString bearer_token_;
    quint64 next_after_id_{0};
    bool polling_active_{false};
    QNetworkReply *current_reply_{nullptr};
    QTimer retry_timer_;
};

#endif // PARKING_EVENT_API_CLIENT_H

#pragma once

#include "playback_types.h"
#include "parking_types.h"

#include <QByteArray>
#include <QDateTime>
#include <QImage>
#include <QList>
#include <QObject>
#include <QSslConfiguration>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

class PlaybackApiClient : public QObject {
    Q_OBJECT

public:
    explicit PlaybackApiClient(QObject *parent = nullptr);
    ~PlaybackApiClient() override;

    bool configure(const QUrl &control_base_url,
                   const QString &username,
                   const QString &password,
                   const QString &certificate_path,
                   QString *error);
    bool isConfigured() const;
    bool hasValidAccessToken() const;
    void clearSession();
    // clearSession()보다 넓다 — 토큰뿐 아니라 configure()로 기억해둔
    // URL·사용자명·비밀번호·인증서까지 전부 잊는다. 로그아웃 시 사용:
    // 이걸 안 부르면 configured_가 true로 남아 "재생 다시 시도" 같은
    // 버튼들이 예전 비밀번호로 조용히 재인증해버린다.
    void deconfigure();

    QUrl controlBaseUrl() const { return control_base_url_; }
    QString accessToken() const { return QString::fromUtf8(access_token_); }
    QSslConfiguration sslConfiguration() const { return ssl_configuration_; }
    QByteArray pinnedCertificateSha256() const { return pinned_certificate_sha256_; }

    void login();
    void requestStatus();
    void requestTimeline(int channel_id, qint64 start_utc_ms, qint64 end_utc_ms);
    void requestThumbnail(int channel_id, qint64 utc_ms, int width);
    void createPlaybackSession(int channel_id, qint64 start_utc_ms, qint64 end_utc_ms);
    void deletePlaybackSession(const QString &session_id);
    void requestParkingSpaces(int channel_id);
    void createParkingDraft(const ParkingConfiguration &configuration);
    void updateParkingDraft(const QString &draft_id,
                            const ParkingConfiguration &configuration);
    void validateParkingDraft(const QString &draft_id);
    void applyParkingDraft(const QString &draft_id,
                           const QString &idempotency_key);
    void requestParkingApplyJob(const QString &job_id);

signals:
    void loginSucceeded(qint64 expires_in_seconds);
    void statusReceived(const DeviceStatusSnapshot &status);
    void timelineReceived(const PlaybackTimeline &timeline);
    void thumbnailReceived(int channel_id,
                           qint64 utc_ms,
                           const QImage &image);
    void playbackSessionCreated(const PlaybackSession &session);
    void playbackSessionDeleted(const QString &session_id);
    void parkingSpacesReceived(const ParkingConfiguration &configuration);
    void parkingDraftReceived(const ParkingConfiguration &configuration);
    void parkingValidationReceived(const ParkingValidationResult &result);
    void parkingApplyJobReceived(const ParkingApplyJob &job);
    void requestFailed(const QString &operation, const QString &message, int http_status);

private:
    QUrl endpointUrl(const QString &path) const;
    QNetworkRequest createRequest(const QUrl &url) const;
    void monitorReply(QNetworkReply *reply, const QString &operation);
    bool readReply(QNetworkReply *reply,
                   const QString &operation,
                   const QList<int> &expected_statuses,
                   QByteArray *body);
    QString responseError(const QByteArray &body, const QString &fallback) const;

    QNetworkAccessManager *network_ = nullptr;
    QUrl control_base_url_;
    QString username_;
    QString password_;
    QString certificate_path_;
    QSslConfiguration ssl_configuration_;
    QByteArray pinned_certificate_sha256_;
    QByteArray access_token_;
    QDateTime token_expires_at_utc_;
    bool configured_ = false;
    bool status_request_in_progress_ = false;
    quint64 next_request_id_ = 1;
};

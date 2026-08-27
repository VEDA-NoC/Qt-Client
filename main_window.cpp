#include "main_window.h"

#include "legal_notice_widget.h"
#include "login_page.h"
#include "signup_page.h"
#include "playback_api_client.h"
#include "parking_zone_editor.h"
#include "parking_event_api_client.h"
#include "parking_event_store.h"
#include "parking_event_card_widget.h"
#include "stm_api_client.h"
#include "stm_device_list_widget.h"
#include "stm_fire_response_dialog.h"
#include "stream_worker.h"
#include "timeline_widget.h"
#include "video_panel.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QButtonGroup>
#include <QCalendarWidget>
#include <QComboBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShortcut>
#include <QSize>
#include <QSizePolicy>
#include <QSlider>
#include <QStackedWidget>
#include <QStackedLayout>
#include <QStringList>
#include <QStyle>
#include <QTableWidget>
#include <QTimer>
#include <QTimeEdit>
#include <QTimeZone>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace {

QIcon navigationIcon(const QString &name) {
    const QString white_path = QString(":/icons/sidebar/%1.png").arg(name);
    const QString muted_path = QString(":/icons/sidebar/%1-muted.png").arg(name);

    QIcon icon;
    icon.addFile(muted_path, QSize(), QIcon::Normal, QIcon::Off);
    icon.addFile(white_path, QSize(), QIcon::Active, QIcon::Off);
    icon.addFile(white_path, QSize(), QIcon::Normal, QIcon::On);
    icon.addFile(white_path, QSize(), QIcon::Active, QIcon::On);
    icon.addFile(white_path, QSize(), QIcon::Selected, QIcon::On);
    return icon;
}

const QStringList kPageTitles = {
    "실시간 모니터링",
    "녹화 재생",
    "이벤트",
    "장치 및 제어",
    "설정",
};

const QStringList kPageSubtitles = {
    "4채널 실시간 영상과 최근 이벤트를 확인합니다.",
    "이벤트 녹화 구간을 검색하고 재생합니다.",
    "발생·확인·해결 상태를 분리하여 관리합니다.",
    "서버 저장소, 구역 제어기, 주차 구역·번호판·전기차 판정을 관리합니다.",
    "연결, 녹화 정책, 제품 정보를 관리합니다.",
};

constexpr int kPlaybackApiActionNone = 0;
constexpr int kPlaybackApiActionTimeline = 1;
constexpr int kPlaybackApiActionCreateSession = 2;
constexpr int kPlaybackApiActionThumbnail = 3;

QFrame *makeCard(QWidget *parent) {
    auto *card = new QFrame(parent);
    card->setProperty("card", true);
    return card;
}

QLabel *makeSectionTitle(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setProperty("sectionTitle", true);
    return label;
}

QLabel *makeMutedLabel(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setProperty("muted", true);
    label->setWordWrap(true);
    return label;
}

void refreshStyle(QWidget *widget) {
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

bool normalizeRtspsBaseUrl(const QString &input, QString *normalized, QString *error) {
    const QString trimmed = input.trimmed();
    const QUrl url(trimmed, QUrl::StrictMode);
    if (trimmed.isEmpty()) {
        *error = "Pi RTSPS Base URL을 입력하세요.";
        return false;
    }
    if (!url.isValid() || url.scheme().compare("rtsps", Qt::CaseInsensitive) != 0 || url.host().isEmpty()) {
        *error = "rtsps://호스트[:포트] 형식으로 입력하세요.";
        return false;
    }
    if (!url.userName().isEmpty() || !url.password().isEmpty()) {
        *error = "카메라 계정정보를 URL에 넣지 마세요. Qt는 Pi RTSPS 주소만 사용합니다.";
        return false;
    }
    if (url.hasQuery() || url.hasFragment()) {
        *error = "Base URL에는 query 또는 fragment를 사용할 수 없습니다.";
        return false;
    }

    QUrl clean = url;
    QString path = clean.path();
    while (path.endsWith('/') && path.size() > 1) {
        path.chop(1);
    }
    if (path == "/") {
        path.clear();
    }
    clean.setPath(path);
    *normalized = clean.toString(QUrl::FullyEncoded);
    return true;
}

bool normalizeHttpsBaseUrl(const QString &input, QUrl *normalized, QString *error) {
    const QString trimmed = input.trimmed();
    const QUrl url(trimmed, QUrl::StrictMode);
    if (trimmed.isEmpty()) {
        *error = "Pi Control HTTPS Base URL을 입력하세요.";
        return false;
    }
    if (!url.isValid() ||
        url.scheme().compare("https", Qt::CaseInsensitive) != 0 ||
        url.host().isEmpty()) {
        *error = "https://호스트[:포트] 형식으로 입력하세요.";
        return false;
    }
    if (!url.userName().isEmpty() || !url.password().isEmpty() ||
        url.hasQuery() || url.hasFragment()) {
        *error = "Control URL에는 계정정보, query 또는 fragment를 사용할 수 없습니다.";
        return false;
    }

    QUrl clean = url;
    QString path = clean.path();
    while (path.endsWith('/') && path.size() > 1) {
        path.chop(1);
    }
    if (path == "/") {
        path.clear();
    }
    clean.setPath(path);
    *normalized = clean;
    return true;
}

QUrl defaultControlBaseUrl(const QString &rtsps_base_url) {
    QUrl url(rtsps_base_url);
    url.setScheme("https");
    url.setPort(9443);
    url.setPath(QString());
    url.setQuery(QString());
    url.setFragment(QString());
    return url;
}

QString localRangeText(qint64 start_utc_ms, qint64 end_utc_ms) {
    const QString start =
        QDateTime::fromMSecsSinceEpoch(start_utc_ms, QTimeZone::UTC)
            .toTimeZone(QTimeZone("Asia/Seoul"))
            .toString("yyyy-MM-dd HH:mm:ss");
    const QString end =
        QDateTime::fromMSecsSinceEpoch(end_utc_ms, QTimeZone::UTC)
            .toTimeZone(QTimeZone("Asia/Seoul"))
            .toString("yyyy-MM-dd HH:mm:ss");
    return QString("%1 ~ %2").arg(start, end);
}

QString formatStorageBytes(qint64 bytes) {
    constexpr double kBytesPerGb = 1000.0 * 1000.0 * 1000.0;
    if (bytes >= static_cast<qint64>(kBytesPerGb)) {
        return QString("%1 GB")
            .arg(bytes / kBytesPerGb, 0, 'f', 1);
    }
    constexpr double kBytesPerMb = 1000.0 * 1000.0;
    return QString("%1 MB")
        .arg(bytes / kBytesPerMb, 0, 'f', 0);
}

QString formatStorageCapacityPair(qint64 available_bytes,
                                  qint64 total_bytes) {
    constexpr double kBytesPerGb = 1000.0 * 1000.0 * 1000.0;
    if (total_bytes >= static_cast<qint64>(kBytesPerGb)) {
        return QString("%1/%2 GB")
            .arg(available_bytes / kBytesPerGb, 0, 'f', 1)
            .arg(total_bytes / kBytesPerGb, 0, 'f', 1);
    }
    constexpr double kBytesPerMb = 1000.0 * 1000.0;
    return QString("%1/%2 MB")
        .arg(available_bytes / kBytesPerMb, 0, 'f', 0)
        .arg(total_bytes / kBytesPerMb, 0, 'f', 0);
}

QString playbackApiErrorText(const QString &operation,
                             const QString &message,
                             int http_status) {
    if (operation == "login" &&
        (http_status == 401 || message == "invalid_credentials")) {
        return "Control 비밀번호가 올바르지 않습니다.";
    }
    if (message == "authentication_required") {
        return "인증 세션이 만료되었습니다. 다시 로그인하세요.";
    }
    if (message == "playable_media_not_found") {
        return "선택한 시각부터 재생할 녹화 영상이 없습니다.";
    }
    if (message == "playback_unavailable") {
        return "현재 Playback 서비스를 사용할 수 없습니다.";
    }
    return message;
}

}  // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    channel_statuses_.fill("Stopped", 4);
    workers_.fill(nullptr, 4);
    channel_retry_attempts_.fill(0, 4);
    for (int channel = 0; channel < 4; ++channel) {
        auto *retry_timer = new QTimer(this);
        retry_timer->setSingleShot(true);
        connect(retry_timer, &QTimer::timeout, this, [this, channel]() {
            if (!live_desired_running_ || stopping_streams_ ||
                shutting_down_ || workers_[channel]) {
                return;
            }
            qInfo().noquote()
                << QString("[live-retry] channel=%1 state=starting attempt=%2")
                       .arg(channel + 1)
                       .arg(channel_retry_attempts_[channel]);
            startChannelStream(channel);
        });
        channel_retry_timers_.push_back(retry_timer);

        auto *stable_timer = new QTimer(this);
        stable_timer->setSingleShot(true);
        stable_timer->setInterval(30000);
        connect(stable_timer, &QTimer::timeout, this, [this, channel]() {
            if (!workers_[channel] ||
                !channel_statuses_[channel].startsWith("Playing")) {
                return;
            }
            channel_retry_attempts_[channel] = 0;
            qInfo().noquote()
                << QString("[live-retry] channel=%1 state=stable attempts_reset=true")
                       .arg(channel + 1);
        });
        channel_stable_timers_.push_back(stable_timer);
    }
    playback_api_ = new PlaybackApiClient(this);
    stm_api_ = new StmApiClient(playback_api_, this);
    connect(stm_api_, &StmApiClient::requestFailed, this,
            [](const QString &operation, const QString &message, int http_status) {
                qWarning().noquote() << QString("[stm-api] operation=%1 http=%2 error=%3")
                                            .arg(operation)
                                            .arg(http_status)
                                            .arg(message);
            });
    event_store_ = new ParkingEventStore(this);
    event_api_client_ = new ParkingEventApiClient(this);

    // 네트워크 수신 -> 스토어 저장 (이 연결이 batchAdded를 간접 발생시킴)
    connect(event_api_client_, &ParkingEventApiClient::eventsReceived,
            this, [this](const ParkingEventBatch &batch) {
                // 폴링 시작 후 처음 도착하는 배치가 백로그다. 그 배치가 갖고
                // 있는 가장 큰 event_id를 기준선으로 잡아, 그보다 작거나
                // 같은(=예전) 이벤트는 이후 상단 고정 판정에서 제외한다.
                if (!pin_baseline_established_) {
                    pin_baseline_event_id_ = batch.next_after_id;
                    pin_baseline_established_ = true;
                }
                if (event_store_) event_store_->addEvents(batch.events);
            });
    // 배치 수신 완료 시 1회만 UI 갱신 (이벤트별 rebuild 방지)
    connect(event_store_, &ParkingEventStore::batchAdded,
            this, [this](int) {
                updateLiveEventsPanel();
                refreshEventsTable();
            });
    // 단건 수신(addEvent 직접 호출) 및 ACK 갱신
    connect(event_store_, &ParkingEventStore::eventAdded,
            this, [this](const ParkingEventItem &) {
                updateLiveEventsPanel();
                refreshEventsTable();
            });
    connect(event_store_, &ParkingEventStore::eventUpdated,
            this, &MainWindow::handleEventUpdatedInStore);

    createUi();
    playback_seek_timer_ = new QTimer(this);
    playback_seek_timer_->setSingleShot(true);
    playback_seek_timer_->setInterval(250);
    connect(playback_seek_timer_, &QTimer::timeout, this, [this]() {
        const qint64 target = pending_seek_utc_ms_;
        pending_seek_utc_ms_ = 0;
        seekPlaybackTo(target);
    });
    playback_video_click_timer_ = new QTimer(this);
    playback_video_click_timer_->setSingleShot(true);
    playback_video_click_timer_->setInterval(
        QApplication::doubleClickInterval());
    connect(playback_video_click_timer_,
            &QTimer::timeout,
            this,
            [this]() {
                if (playbackShortcutAllowed()) {
                    togglePlaybackPause();
                }
            });
    timeline_prefetch_timer_ = new QTimer(this);
    timeline_prefetch_timer_->setSingleShot(true);
    timeline_prefetch_timer_->setInterval(250);
    connect(timeline_prefetch_timer_,
            &QTimer::timeout,
            this,
            &MainWindow::scheduleTimelinePrefetch);
    device_status_timer_ = new QTimer(this);
    device_status_timer_->setInterval(5000);
    connect(device_status_timer_,
            &QTimer::timeout,
            this,
            &MainWindow::pollDeviceStatus);
    device_status_timer_->start();

    connect(playback_api_,
            &PlaybackApiClient::loginSucceeded,
            this,
            [this](qint64 expires_in_seconds) {
                playback_login_in_progress_ = false;
                playback_login_ever_succeeded_ = true;
                // login_page_의 비밀번호는 일부러 지우지 않는다 — 실제
                // 인증은 설정 탭의 "Control API 적용"에서만 일어나고, 그
                // 버튼을 다시 눌러 재연결할 때도 같은 값을 읽어간다.
                refreshPlaybackAuthUi(true);
                if (control_feedback_label_) {
                    control_feedback_label_->setText(
                        QString("인증 완료 · token 유효시간 %1초")
                            .arg(expires_in_seconds));
                    control_feedback_label_->setProperty("severity", "ok");
                    refreshStyle(control_feedback_label_);
                }
                qInfo().noquote()
                    << QString("[playback-api] login succeeded expires_in=%1s")
                           .arg(expires_in_seconds);
                if (event_api_client_ && playback_api_) {
                    event_api_client_->setSslConfiguration(playback_api_->sslConfiguration());
                    pin_baseline_established_ = false;
                    pin_baseline_event_id_ = 0;
                    event_api_client_->startPolling(playback_api_->controlBaseUrl().toString(), playback_api_->accessToken());
                }
                const int action = pending_playback_api_action_;
                pending_playback_api_action_ = 0;
                performPlaybackApiAction(action);
                if (device_status_retry_after_login_) {
                    device_status_retry_after_login_ = false;
                    playback_api_->requestStatus();
                } else {
                    pollDeviceStatus();
                }
            });
    connect(playback_api_,
            &PlaybackApiClient::statusReceived,
            this,
            [this](const DeviceStatusSnapshot &status) {
                applyDeviceStatus(status);
                qInfo().noquote()
                    << QString("[device-status] system=%1 storage=%2 sync=%3 stale_ms=%4")
                           .arg(status.system_available
                                    ? status.system.state
                                    : "unavailable")
                           .arg(status.storage_available
                                    ? status.storage.state
                                    : "unavailable")
                           .arg(status.server_time.sync_state.isEmpty()
                                    ? "unavailable"
                                    : status.server_time.sync_state)
                           .arg(status.system_available
                                    ? status.system.sample_age_ms
                                    : -1);
            });
    connect(playback_api_,
            &PlaybackApiClient::timelineReceived,
            this,
            [this](const PlaybackTimeline &timeline) {
                if (!timeline_request_in_progress_ ||
                    timeline.start_utc_ms !=
                        timeline_pending_start_utc_ms_ ||
                    timeline.end_utc_ms !=
                        timeline_pending_end_utc_ms_) {
                    qWarning().noquote()
                        << QString("[playback-api] stale timeline ignored channel=%1 range=%2..%3")
                               .arg(timeline.channel_id)
                               .arg(timeline.start_utc_ms)
                               .arg(timeline.end_utc_ms);
                    return;
                }
                playback_timeline_->setTimeline(timeline);
                ++timeline_requests_completed_;
                ++timeline_requests_succeeded_;
                playback_timelines_received_ =
                    timeline_requests_succeeded_;
                if (timeline_requests_completed_ >= 4) {
                    finalizeTimelineWindowRequest();
                } else if (timeline_request_resets_cache_) {
                    playback_selection_label_->setText(
                        QString("Timeline 조회 중 · %1/4채널")
                            .arg(timeline_requests_completed_));
                }
                qInfo().noquote()
                    << QString("[playback-api] timeline channel=%1 spans=%2 events=%3")
                           .arg(timeline.channel_id)
                           .arg(timeline.spans.size())
                           .arg(timeline.events.size());
            });
    connect(playback_api_,
            &PlaybackApiClient::playbackSessionCreated,
            this,
            [this](const PlaybackSession &session) {
                qInfo().noquote()
                    << QString("[playback-api] session created channel=%1 duration_ms=%2 segments=%3")
                           .arg(session.channel_id)
                           .arg(session.duration_ms)
                           .arg(session.segment_count);
                startPlaybackWorker(session);
            });
    connect(playback_api_,
            &PlaybackApiClient::thumbnailReceived,
            this,
            [this](int channel_id,
                   qint64 requested_utc_ms,
                   const QImage &image) {
                Q_UNUSED(channel_id);
                Q_UNUSED(requested_utc_ms);
                if (!playback_worker_) {
                    playback_panel_->setPreviewImage(image);
                    playback_fullscreen_panel_->setPreviewImage(image);
                    setPlaybackUiState(PlaybackUiState::Preview);
                    setPlaybackFeedback("선택 시각 대표 썸네일 수신 완료",
                                        "ok");
                }
            });
    connect(
        playback_api_,
        &PlaybackApiClient::requestFailed,
        this,
        [this](const QString &operation,
               const QString &message,
               int http_status) {
            const QString display_message =
                playbackApiErrorText(operation, message, http_status);
            const bool certificate_failure =
                message.contains("TLS", Qt::CaseInsensitive) ||
                message.contains("인증서", Qt::CaseInsensitive) ||
                message.contains("pin", Qt::CaseInsensitive);
            if (operation == "status" && http_status == 401) {
                device_status_retry_after_login_ = true;
                if (pi_status_badge_) {
                    pi_status_badge_->setText("Pi 인증 갱신 중");
                    pi_status_badge_->setProperty("severity", "warning");
                    refreshStyle(pi_status_badge_);
                }
                if (storage_status_badge_) {
                    storage_status_badge_->setText(
                        "저장소 인증 갱신 중");
                    storage_status_badge_->setProperty(
                        "severity", "warning");
                    refreshStyle(storage_status_badge_);
                }
                if (!playback_login_in_progress_) {
                    playback_login_in_progress_ = true;
                    playback_api_->login();
                }
                qWarning().noquote()
                    << "[device-status] access token expired; login requested";
                return;
            }
            if (operation == "status" && !certificate_failure) {
                setDeviceControlError(display_message);
                qWarning().noquote()
                    << QString("[device-status] control request failed http=%1 error=%2")
                           .arg(http_status)
                           .arg(message);
                return;
            }
            const bool credential_failure =
                operation == "login" && http_status == 401;
            const bool session_expired =
                operation != "login" && http_status == 401;
            if (certificate_failure || credential_failure) {
                playback_login_ever_succeeded_ = false;
                clearProtectedPlaybackState();
                setDeviceControlError(display_message);
                setControlUiState(
                    ControlUiState::SettingsRequired,
                    certificate_failure
                        ? QString("서버 인증서를 확인해야 합니다.\n%1")
                              .arg(display_message)
                        : QString("Control 비밀번호가 올바르지 않습니다."));
            } else if (session_expired) {
                playback_login_ever_succeeded_ = false;
                clearProtectedPlaybackState();
                setControlUiState(
                    ControlUiState::RetryableFailure,
                    "인증 세션이 만료되었습니다.\n"
                    "저장된 설정으로 다시 연결하세요.");
            } else if (operation == "login") {
                playback_login_ever_succeeded_ = false;
                setDeviceControlError(display_message);
                setControlUiState(
                    ControlUiState::RetryableFailure,
                    QString("Control API에 연결할 수 없습니다.\n%1")
                        .arg(display_message));
            }
            if (certificate_failure || credential_failure ||
                session_expired || operation == "login") {
                playback_login_in_progress_ = false;
                if (control_feedback_label_) {
                    control_feedback_label_->setText(display_message);
                    control_feedback_label_->setProperty(
                        "severity", "critical");
                    refreshStyle(control_feedback_label_);
                }
                qWarning().noquote()
                    << QString("[playback-api] operation=%1 http=%2 error=%3")
                           .arg(operation)
                           .arg(http_status)
                           .arg(message);
                return;
            }
            if (operation.startsWith("timeline.ch")) {
                const int channel_id =
                    operation.mid(QString("timeline.ch").size())
                        .toInt();
                if (timeline_request_in_progress_) {
                    ++timeline_requests_completed_;
                    ++timeline_requests_failed_;
                    if (timeline_request_resets_cache_) {
                        playback_timeline_->setChannelError(
                            channel_id);
                    }
                    setPlaybackFeedback(
                        QString("CH %1 타임라인 조회 실패: %2")
                            .arg(channel_id)
                            .arg(display_message),
                        "critical");
                    if (timeline_requests_completed_ >= 4) {
                        finalizeTimelineWindowRequest();
                    }
                }
                qWarning().noquote()
                    << QString("[playback-api] operation=%1 http=%2 error=%3")
                           .arg(operation)
                           .arg(http_status)
                           .arg(message);
                return;
            }
            if (operation.startsWith("parking.")) {
                qWarning().noquote()
                    << QString("[parking-api] operation=%1 http=%2 error=%3")
                           .arg(operation)
                           .arg(http_status)
                           .arg(message);
                return;
            }
            if (operation != "playback.delete") {
                playback_login_in_progress_ = false;
                pending_playback_api_action_ = kPlaybackApiActionNone;
                if (playback_search_button_) {
                    playback_search_button_->setEnabled(
                        playback_login_ever_succeeded_ &&
                        operation != "login");
                }
                if (operation != "login") {
                    refreshPlaybackAuthUi(
                        playback_login_ever_succeeded_);
                }
                if (playback_play_button_) {
                    playback_play_button_->setEnabled(
                        playback_selection_end_utc_ms_ >
                        playback_selection_start_utc_ms_);
                }
                const QString http_text =
                    http_status > 0
                        ? QString(" (HTTP %1)").arg(http_status)
                        : QString();
                setPlaybackFeedback(
                    QString("%1 실패%2: %3")
                        .arg(operation)
                        .arg(http_text)
                        .arg(display_message),
                    operation == "thumbnail" ? "warning"
                                              : "critical");
                if (operation == "playback") {
                    setPlaybackUiState(PlaybackUiState::Error);
                } else if (operation == "thumbnail" &&
                           !playback_worker_) {
                    setPlaybackUiState(PlaybackUiState::Preview);
                }
                updatePlaybackControlState();
            }
            qWarning().noquote()
                << QString("[playback-api] operation=%1 http=%2 error=%3")
                       .arg(operation)
                       .arg(http_status)
                       .arg(message);
        });

    auto *space_shortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
    space_shortcut->setContext(Qt::ApplicationShortcut);
    connect(space_shortcut, &QShortcut::activated, this, [this]() {
        if (playbackShortcutAllowed()) {
            togglePlaybackPause();
        }
    });
    auto *left_shortcut = new QShortcut(QKeySequence(Qt::Key_Left), this);
    left_shortcut->setContext(Qt::ApplicationShortcut);
    connect(left_shortcut, &QShortcut::activated, this, [this]() {
        if (playbackShortcutAllowed()) {
            seekPlaybackBy(-1000);
        }
    });
    auto *right_shortcut = new QShortcut(QKeySequence(Qt::Key_Right), this);
    right_shortcut->setContext(Qt::ApplicationShortcut);
    connect(right_shortcut, &QShortcut::activated, this, [this]() {
        if (playbackShortcutAllowed()) {
            seekPlaybackBy(1000);
        }
    });
    auto *f11_shortcut = new QShortcut(QKeySequence(Qt::Key_F11), this);
    f11_shortcut->setContext(Qt::ApplicationShortcut);
    connect(f11_shortcut,
            &QShortcut::activated,
            this,
            &MainWindow::togglePlaybackFullscreen);
    auto *escape_shortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escape_shortcut->setContext(Qt::ApplicationShortcut);
    connect(escape_shortcut, &QShortcut::activated, this, [this]() {
        if (playback_fullscreen_window_ &&
            playback_fullscreen_window_->isVisible()) {
            togglePlaybackFullscreen();
        }
    });
}

MainWindow::~MainWindow() {
    shutting_down_ = true;
    live_desired_running_ = false;
    restart_after_stop_ = false;
    cancelAllChannelRetries(true);
    requestWorkerStop();
    const QVector<StreamWorker *> workers = workers_;
    for (auto *worker : workers) {
        if (worker) {
            worker->wait(6000);
        }
    }
    if (playback_worker_) {
        playback_worker_->stop();
        playback_worker_->wait(6000);
    }
}

void MainWindow::createUi() {
    auto *central = new QWidget(this);
    central->setObjectName("appRoot");
    auto *root_layout = new QVBoxLayout(central);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(0);

    auto *body = new QWidget(central);
    auto *body_layout = new QHBoxLayout(body);
    body_layout->setContentsMargins(0, 0, 0, 0);
    body_layout->setSpacing(0);

    app_sidebar_ = createSidebar(body);
    body_layout->addWidget(app_sidebar_);

    auto *content = new QWidget(body);
    auto *content_layout = new QVBoxLayout(content);
    content_layout->setContentsMargins(0, 0, 0, 0);
    content_layout->setSpacing(0);
    top_bar_ = createTopBar(content);
    content_layout->addWidget(top_bar_);

    page_stack_ = new QStackedWidget(content);
    page_stack_->setObjectName("pageStack");
    page_stack_->addWidget(createLivePage(page_stack_));
    page_stack_->addWidget(createPlaybackPage(page_stack_));
    page_stack_->addWidget(createEventsPage(page_stack_));
    page_stack_->addWidget(createDevicesPage(page_stack_));
    page_stack_->addWidget(createSettingsPage(page_stack_));
    parking_zone_editor_ = new ParkingZoneEditor(playback_api_, stm_api_, page_stack_);
    page_stack_->addWidget(parking_zone_editor_);
    content_layout->addWidget(page_stack_, 1);
    body_layout->addWidget(content, 1);
    root_layout->addWidget(body, 1);
    bottom_bar_ = createBottomBar(central);
    root_layout->addWidget(bottom_bar_);

    setWindowTitle("VEDA VMS Console");

    // 로그인 전에는 사이드바·상단바를 포함한 central 전체를 가려야 진짜
    // 로그인 화면이 된다 — page_stack_ 안이 아니라 그 바깥, central과
    // 같은 층에 둔다.
    login_page_ = new LoginPage(this);
    signup_page_ = new SignupPage(this);
    root_stack_ = new QStackedWidget(this);
    root_stack_->addWidget(login_page_);  // index 0
    root_stack_->addWidget(signup_page_); // index 1
    root_stack_->addWidget(central);      // index 2
    setCentralWidget(root_stack_);
    root_stack_->setCurrentWidget(login_page_);

    // 로그인 버튼은 실제 Control API 인증을 하지 않는다 — 그건 설정 탭의
    // "Control API 적용"(URL·인증서·비밀번호를 함께 검증)에서만 일어난다.
    // 여기서는 화면만 전환한다. 입력한 비밀번호는 login_page_ 안에 남아
    // 있다가 나중에 그 버튼을 눌렀을 때 재사용된다.
    connect(login_page_, &LoginPage::loginRequested, this,
            [this]() { root_stack_->setCurrentIndex(2); });
    connect(login_page_, &LoginPage::signupRequested, this,
            [this]() { root_stack_->setCurrentWidget(signup_page_); });
    connect(signup_page_, &SignupPage::backRequested, this,
            [this]() { root_stack_->setCurrentWidget(login_page_); });

    connect(navigation_group_, &QButtonGroup::idClicked, this, &MainWindow::setCurrentPage);
    connect(start_button_, &QPushButton::clicked, this, &MainWindow::startStreams);
    connect(stop_button_, &QPushButton::clicked, this, &MainWindow::stopStreams);
    connect(parking_zone_editor_, &ParkingZoneEditor::backRequested,
            this, &MainWindow::leaveParkingZoneEditor);

    if (auto *first_button = navigation_group_->button(0)) {
        first_button->setChecked(true);
    }
    setCurrentPage(0);

    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateTotalStats);
    timer->start(1000);
    updateTotalStats();
}

QWidget *MainWindow::createSidebar(QWidget *parent) {
    auto *sidebar = new QWidget(parent);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(226);

    auto *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(0, 28, 0, 20);
    layout->setSpacing(0);

    auto *brand = new QWidget(sidebar);
    auto *brand_layout = new QVBoxLayout(brand);
    brand_layout->setContentsMargins(24, 0, 20, 28);
    brand_layout->setSpacing(2);
    auto *brand_title = new QLabel("VEDA", brand);
    brand_title->setObjectName("brandTitle");
    auto *brand_subtitle = new QLabel("VMS CONSOLE", brand);
    brand_subtitle->setObjectName("brandSubtitle");
    brand_layout->addWidget(brand_title);
    brand_layout->addWidget(brand_subtitle);
    layout->addWidget(brand);

    auto *caption = new QLabel("  WORKSPACE", sidebar);
    caption->setObjectName("sidebarCaption");
    caption->setContentsMargins(20, 0, 0, 8);
    layout->addWidget(caption);

    navigation_group_ = new QButtonGroup(this);
    navigation_group_->setExclusive(true);
    const QStringList labels = {
        "실시간 모니터링",
        "녹화 재생",
        "이벤트",
        "장치 및 제어",
        "설정",
    };
    const QStringList icon_names = {
        "live",
        "playback",
        "events",
        "devices",
        "settings",
    };
    for (int index = 0; index < labels.size(); ++index) {
        auto *button = new QToolButton(sidebar);
        button->setText(QStringLiteral("\u2009") + labels[index]);
        button->setIcon(navigationIcon(icon_names[index]));
        button->setIconSize(QSize(20, 20));
        button->setCheckable(true);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setProperty("nav", true);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        navigation_group_->addButton(button, index);
        layout->addWidget(button);
    }

    layout->addStretch(1);

    logout_button_ = new QToolButton(sidebar);
    logout_button_->setText(QStringLiteral(" 로그아웃"));
    logout_button_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    logout_button_->setProperty("sidebarAction", true);
    logout_button_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(logout_button_);
    connect(logout_button_, &QToolButton::clicked, this, &MainWindow::logout);

    exit_button_ = new QToolButton(sidebar);
    exit_button_->setText("종료");
    exit_button_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    exit_button_->setProperty("sidebarAction", true);
    exit_button_->setProperty("critical", true);
    exit_button_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(exit_button_);
    layout->addSpacing(4);
    connect(exit_button_, &QToolButton::clicked, this, &QWidget::close);

    return sidebar;
}

QFrame *MainWindow::createTopBar(QWidget *parent) {
    auto *bar = new QFrame(parent);
    bar->setObjectName("topBar");
    bar->setFixedHeight(76);
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(24, 10, 22, 10);

    auto *titles = new QVBoxLayout();
    titles->setSpacing(2);
    page_title_label_ = new QLabel(bar);
    page_title_label_->setObjectName("pageTitle");
    page_subtitle_label_ = new QLabel(bar);
    page_subtitle_label_->setObjectName("pageSubtitle");
    titles->addWidget(page_title_label_);
    titles->addWidget(page_subtitle_label_);
    layout->addLayout(titles);
    layout->addStretch(1);

    stream_controls_ = new QWidget(bar);
    auto *controls_layout = new QHBoxLayout(stream_controls_);
    controls_layout->setContentsMargins(0, 0, 0, 0);
    controls_layout->setSpacing(8);

    connection_badge_ = new QLabel("영상 연결 대기", stream_controls_);
    connection_badge_->setProperty("badge", true);
    connection_badge_->setProperty("severity", "warning");
    channel_summary_label_ = new QLabel("영상 0/4", stream_controls_);
    channel_summary_label_->setProperty("muted", true);
    total_stats_label_ = new QLabel("수신 0.00 Mbps", stream_controls_);
    total_stats_label_->setProperty("muted", true);
    controls_layout->addWidget(channel_summary_label_);
    controls_layout->addWidget(total_stats_label_);
    controls_layout->addWidget(connection_badge_);

    start_button_ = new QPushButton("스트림 시작", stream_controls_);
    start_button_->setProperty("primary", true);
    stop_button_ = new QPushButton("중지", stream_controls_);
    stop_button_->setEnabled(false);
    controls_layout->addWidget(start_button_);
    controls_layout->addWidget(stop_button_);
    layout->addWidget(stream_controls_);
    return bar;
}

QFrame *MainWindow::createBottomBar(QWidget *parent) {
    auto *bar = new QFrame(parent);
    bar->setObjectName("bottomBar");
    bar->setFixedHeight(38);
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(0, 0, 20, 0);
    layout->setSpacing(12);

    auto *account = new QWidget(bar);
    account->setObjectName("footerAccount");
    account->setFixedWidth(226);
    auto *account_layout = new QHBoxLayout(account);
    account_layout->setContentsMargins(20, 0, 14, 0);
    account_layout->setSpacing(7);
    auto *user_icon = new QLabel(account);
    user_icon->setPixmap(QIcon(":/icons/user.svg").pixmap(14, 14));
    user_icon->setFixedSize(14, 14);
    auto *user_name = new QLabel("operator", account);
    user_name->setProperty("muted", true);
    footer_connection_label_ = new QLabel("● 설정 필요", account);
    footer_connection_label_->setProperty("footerConnection", true);
    footer_connection_label_->setProperty("severity", "warning");
    account_layout->addWidget(user_icon);
    account_layout->addWidget(user_name);
    account_layout->addStretch(1);
    account_layout->addWidget(footer_connection_label_);

    pi_status_badge_ = new QLabel("서버 · Control 설정 필요", bar);
    pi_status_badge_->setProperty("badge", true);
    pi_status_badge_->setProperty("severity", "warning");
    storage_status_badge_ =
        new QLabel("저장소 · Control 설정 필요", bar);
    storage_status_badge_->setProperty("badge", true);
    storage_status_badge_->setProperty("severity", "warning");

    layout->addWidget(account);
    layout->addWidget(pi_status_badge_);
    layout->addWidget(storage_status_badge_);
    layout->addStretch(1);
    server_time_label_ = new QLabel("서버 시각 · 연결 후 확인", bar);
    server_time_label_->setProperty("muted", true);
    layout->addWidget(server_time_label_);
    return bar;
}

QWidget *MainWindow::createLivePage(QWidget *parent) {
    auto *page = new QWidget(parent);
    live_page_ = page;
    live_page_layout_ = new QGridLayout(page);
    live_page_layout_->setContentsMargins(18, 18, 18, 18);
    live_page_layout_->setSpacing(14);

    auto *grid_host = new QWidget(page);
    grid_host->setObjectName("liveGridHost");
    grid_host->setAttribute(Qt::WA_StyledBackground, true);
    grid_host->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    live_grid_host_ = grid_host;
    live_grid_layout_ = new QGridLayout(grid_host);
    // 일반형과 초광폭형에서는 viewport가 grid geometry를 소유한다.
    // 세로형의 scroll content 높이는 updateLiveLayout()에서만 명시한다.
    live_grid_layout_->setSizeConstraint(QLayout::SetNoConstraint);
    live_grid_layout_->setContentsMargins(0, 0, 0, 0);
    live_grid_layout_->setHorizontalSpacing(10);
    live_grid_layout_->setVerticalSpacing(10);
    for (int channel = 0; channel < 4; ++channel) {
        auto *panel = new VideoPanel(channel, grid_host);
        panel->setAspectConstrained(false);
        panel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        panels_.push_back(panel);
        live_grid_layout_->addWidget(panel, channel / 2, channel % 2);
    }

    live_grid_scroll_ = new QScrollArea(page);
    live_grid_scroll_->setObjectName("liveGridScroll");
    live_grid_scroll_->setWidgetResizable(true);
    live_grid_scroll_->setFrameShape(QFrame::NoFrame);
    live_grid_scroll_->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff);
    live_grid_scroll_->viewport()->setObjectName(
        "liveGridViewport");
    live_grid_scroll_->viewport()->setAttribute(
        Qt::WA_StyledBackground, true);
    live_grid_scroll_->setWidget(grid_host);

    auto *events = makeCard(page);
    live_events_card_ = events;
    events->setMinimumWidth(286);
    events->setMaximumWidth(340);
    auto *events_layout = new QVBoxLayout(events);
    events_layout->setContentsMargins(14, 14, 14, 14);
    events_layout->setSpacing(10);
    events_layout->addWidget(makeSectionTitle("실시간 이벤트", events));

    // 고정 영역 — live_events_scroll_ 바깥에 둔다. 스크롤 안에 넣으면 내릴 때
    // 같이 사라져 "상단 고정"이 성립하지 않는다(Q3-1).
    live_events_pinned_container_ = new QWidget(events);
    live_events_pinned_layout_ = new QVBoxLayout(live_events_pinned_container_);
    live_events_pinned_layout_->setContentsMargins(0, 0, 0, 8);
    live_events_pinned_layout_->setSpacing(8);
    // 스크롤이 없는 고정 영역이라 트레일링 스트레치를 안 둔다 — syncEventCards()가
    // 마지막 아이템이 spacer인지 직접 확인해 있으면/없으면 둘 다 처리한다.
    live_events_pinned_container_->hide();
    events_layout->addWidget(live_events_pinned_container_);

    live_events_scroll_ = new QScrollArea(events);
    live_events_scroll_->setWidgetResizable(true);
    live_events_scroll_->setFrameShape(QFrame::NoFrame);
    live_events_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    live_events_scroll_->setStyleSheet("QScrollArea, QWidget { background: transparent; }");

    live_events_container_ = new QWidget(live_events_scroll_);
    live_events_card_layout_ = new QVBoxLayout(live_events_container_);
    live_events_card_layout_->setContentsMargins(0, 0, 0, 0);
    live_events_card_layout_->setSpacing(8);
    live_events_card_layout_->addStretch(1);
    live_events_container_->setLayout(live_events_card_layout_);
    live_events_scroll_->setWidget(live_events_container_);

    events_layout->addWidget(live_events_scroll_, 1);
    updateLiveEventsPanel();

    live_page_layout_->addWidget(live_grid_scroll_, 0, 0);
    live_page_layout_->addWidget(events, 0, 1);
    QTimer::singleShot(0, this, [this]() { updateLiveLayout(); });
    return page;
}

QWidget *MainWindow::createPlaybackPage(QWidget *parent) {
    auto *page = new QWidget(parent);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto *filters = makeCard(page);
    playback_filters_card_ = filters;
    auto *filter_layout = new QVBoxLayout(filters);
    filter_layout->setContentsMargins(16, 12, 16, 12);
    filter_layout->setSpacing(8);
    const QDateTime now =
        QDateTime::currentDateTimeUtc().toTimeZone(QTimeZone("Asia/Seoul"));
    playback_pivot_date_ = now.date();
    playback_query_controls_ = new QWidget(filters);
    auto *query_layout = new QGridLayout(playback_query_controls_);
    query_layout->setContentsMargins(0, 0, 0, 0);
    query_layout->setSpacing(8);
    playback_previous_button_ =
        new QPushButton("이전 구간", playback_query_controls_);
    playback_pivot_date_button_ =
        new QPushButton(playback_query_controls_);
    playback_pivot_date_button_->setMinimumWidth(112);
    setPlaybackPivotDate(now.date());
    playback_pivot_time_edit_ =
        new QTimeEdit(playback_query_controls_);
    playback_pivot_time_edit_->setDisplayFormat("HH:mm:ss");
    playback_pivot_time_edit_->setTime(now.time());
    playback_pivot_time_edit_->setToolTip(
        "기준 시각입니다. 날짜는 옆 버튼의 달력에서 선택합니다.");
    playback_range_combo_ = new QComboBox(playback_query_controls_);
    playback_range_combo_->addItem("30분", 30 * 60);
    playback_range_combo_->addItem("1시간", 60 * 60);
    playback_range_combo_->addItem("3시간", 3 * 60 * 60);
    playback_range_combo_->addItem("6시간", 6 * 60 * 60);
    playback_range_combo_->addItem("12시간", 12 * 60 * 60);
    playback_range_combo_->addItem("24시간", 24 * 60 * 60);
    playback_range_combo_->setCurrentIndex(1);
    playback_next_button_ =
        new QPushButton("다음 구간", playback_query_controls_);
    playback_now_button_ =
        new QPushButton("현재", playback_query_controls_);
    playback_search_button_ =
        new QPushButton("타임라인 불러오기", playback_query_controls_);
    playback_search_button_->setProperty("primary", true);
    query_layout->addWidget(playback_previous_button_, 0, 0);
    query_layout->addWidget(
        new QLabel("기준 시각", playback_query_controls_), 0, 1);
    query_layout->addWidget(playback_pivot_date_button_, 0, 2);
    query_layout->addWidget(playback_pivot_time_edit_, 0, 3);
    query_layout->addWidget(
        new QLabel("표시 범위", playback_query_controls_), 0, 4);
    query_layout->addWidget(playback_range_combo_, 0, 5);
    query_layout->addWidget(playback_next_button_, 0, 6);
    query_layout->addWidget(playback_now_button_, 0, 7);
    auto *timezone_label =
        makeMutedLabel("표시: 서버 기준 · KST", playback_query_controls_);
    timezone_label->setWordWrap(false);
    query_layout->addWidget(timezone_label, 1, 0, 1, 7);
    query_layout->addWidget(playback_search_button_, 1, 7);
    query_layout->setColumnStretch(6, 1);
    filter_layout->addWidget(playback_query_controls_);
    layout->addWidget(filters);

    auto *timeline_card = makeCard(page);
    playback_timeline_card_ = timeline_card;
    auto *timeline_layout = new QVBoxLayout(timeline_card);
    timeline_layout->setContentsMargins(16, 12, 16, 12);
    timeline_layout->setSpacing(8);
    auto *timeline_header = new QHBoxLayout();
    timeline_header->addWidget(makeSectionTitle("4채널 타임라인", timeline_card));
    auto *recording_legend = makeMutedLabel("● 녹화", timeline_card);
    recording_legend->setStyleSheet("color: #343958;");
    auto *event_legend = makeMutedLabel("│ 이벤트", timeline_card);
    event_legend->setStyleSheet("color: #F37321;");
    timeline_header->addWidget(recording_legend);
    timeline_header->addWidget(event_legend);
    auto *zoom_out_button = new QPushButton(timeline_card);
    zoom_out_button->setIcon(QIcon(":/icons/timeline/zoom-out.svg"));
    zoom_out_button->setToolTip("타임라인 축소 (Ctrl+마우스 휠 아래)");
    zoom_out_button->setFixedWidth(36);
    auto *zoom_in_button = new QPushButton(timeline_card);
    zoom_in_button->setIcon(QIcon(":/icons/timeline/zoom-in.svg"));
    zoom_in_button->setToolTip("타임라인 확대 (Ctrl+마우스 휠 위)");
    zoom_in_button->setFixedWidth(36);
    auto *zoom_reset_button = new QPushButton("캐시 전체", timeline_card);
    zoom_reset_button->setToolTip(
        "현재 PC에 받아 둔 타임라인 전체 범위를 표시합니다.");
    timeline_header->addWidget(zoom_out_button);
    timeline_header->addWidget(zoom_in_button);
    timeline_header->addWidget(zoom_reset_button);
    playback_selection_label_ =
        makeMutedLabel("조회 후 녹화 bar를 클릭하세요.", timeline_card);
    timeline_header->addStretch(1);
    timeline_header->addWidget(playback_selection_label_);
    timeline_layout->addLayout(timeline_header);
    auto *timeline_host = new QWidget(timeline_card);
    auto *timeline_stack = new QStackedLayout(timeline_host);
    timeline_stack->setContentsMargins(0, 0, 0, 0);
    timeline_stack->setStackingMode(QStackedLayout::StackAll);
    playback_timeline_ = new TimelineWidget(timeline_host);
    timeline_stack->addWidget(playback_timeline_);
    playback_timeline_auth_prompt_ = new QWidget(timeline_host);
    playback_timeline_auth_prompt_->setStyleSheet(
        "background: rgba(255, 255, 255, 245);");
    auto *auth_prompt_layout =
        new QVBoxLayout(playback_timeline_auth_prompt_);
    auth_prompt_layout->setContentsMargins(24, 24, 24, 24);
    auth_prompt_layout->setAlignment(Qt::AlignCenter);
    playback_auth_notice_label_ = makeMutedLabel(
        "Control API 연결이 필요합니다.\n"
        "설정에서 server.crt와 비밀번호를 적용하세요.",
        playback_timeline_auth_prompt_);
    playback_auth_notice_label_->setAlignment(Qt::AlignCenter);
    playback_auth_settings_button_ =
        new QPushButton("설정으로 이동",
                        playback_timeline_auth_prompt_);
    playback_auth_settings_button_->setFixedWidth(132);
    playback_auth_retry_button_ =
        new QPushButton("다시 연결",
                        playback_timeline_auth_prompt_);
    playback_auth_retry_button_->setFixedWidth(132);
    auth_prompt_layout->addWidget(playback_auth_notice_label_);
    auth_prompt_layout->addWidget(playback_auth_settings_button_,
                                  0,
                                  Qt::AlignHCenter);
    auth_prompt_layout->addWidget(playback_auth_retry_button_,
                                  0,
                                  Qt::AlignHCenter);
    timeline_stack->addWidget(playback_timeline_auth_prompt_);
    timeline_layout->addWidget(timeline_host);
    playback_timeline_navigator_ =
        new QSlider(Qt::Horizontal, timeline_card);
    playback_timeline_navigator_->setRange(0, 1000);
    playback_timeline_navigator_->setSingleStep(30);
    playback_timeline_navigator_->setPageStep(100);
    playback_timeline_navigator_->setEnabled(false);
    playback_timeline_navigator_->setToolTip(
        "캐시된 시간 범위 안에서 표시 구간을 이동합니다.");
    timeline_layout->addWidget(playback_timeline_navigator_);
    layout->addWidget(timeline_card);

    auto *viewer = makeCard(page);
    auto *viewer_layout = new QVBoxLayout(viewer);
    viewer_layout->setContentsMargins(16, 12, 16, 12);
    viewer_layout->setSpacing(8);
    auto *viewer_header = new QHBoxLayout();
    viewer_header->addWidget(makeSectionTitle("녹화 영상 플레이어", viewer));
    playback_feedback_label_ =
        makeMutedLabel("Control API 설정 후 Timeline을 조회하세요.", viewer);
    playback_feedback_label_->setProperty("settingsFeedback", true);
    viewer_header->addWidget(playback_feedback_label_, 1);
    viewer_layout->addLayout(viewer_header);
    playback_panel_ = new VideoPanel(0, viewer);
    playback_panel_->setAspectConstrained(true);
    playback_panel_->setTitle("PLAYBACK · 채널/시각 미선택");
    viewer_layout->addWidget(playback_panel_, 1);

    playback_fullscreen_window_ =
        new QWidget(this, Qt::Window | Qt::FramelessWindowHint);
    playback_fullscreen_window_->setObjectName("playbackFullscreen");
    playback_fullscreen_window_->setStyleSheet("background: #000000;");
    auto *fullscreen_layout =
        new QVBoxLayout(playback_fullscreen_window_);
    fullscreen_layout->setContentsMargins(0, 0, 0, 0);
    playback_fullscreen_panel_ =
        new VideoPanel(0, playback_fullscreen_window_);
    playback_fullscreen_panel_->setAspectConstrained(false);
    playback_fullscreen_panel_->setTitle("PLAYBACK");
    fullscreen_layout->addWidget(playback_fullscreen_panel_, 1);
    auto *fullscreen_controls = new QWidget(playback_fullscreen_window_);
    fullscreen_controls->setStyleSheet(
        "QWidget { background: #11131B; color: #FFFFFF; }");
    auto *fullscreen_controls_layout =
        new QHBoxLayout(fullscreen_controls);
    fullscreen_controls_layout->setContentsMargins(18, 10, 18, 14);
    fullscreen_controls_layout->setSpacing(10);
    playback_fullscreen_toggle_button_ =
        new QPushButton("▶ 재생 / Ⅱ 일시정지", fullscreen_controls);
    playback_fullscreen_toggle_button_->setEnabled(false);
    playback_fullscreen_stop_button_ =
        new QPushButton("■ 정지", fullscreen_controls);
    playback_fullscreen_stop_button_->setEnabled(false);
    playback_fullscreen_position_label_ =
        new QLabel("--:--:-- / --:--:--", fullscreen_controls);
    playback_fullscreen_position_slider_ =
        new QSlider(Qt::Horizontal, fullscreen_controls);
    playback_fullscreen_position_slider_->setRange(0, 1000);
    playback_fullscreen_position_slider_->setEnabled(false);
    playback_fullscreen_exit_button_ =
        new QPushButton("전체화면 종료", fullscreen_controls);
    fullscreen_controls_layout->addWidget(
        playback_fullscreen_toggle_button_);
    fullscreen_controls_layout->addWidget(
        playback_fullscreen_stop_button_);
    fullscreen_controls_layout->addWidget(
        playback_fullscreen_position_label_);
    fullscreen_controls_layout->addWidget(
        playback_fullscreen_position_slider_, 1);
    fullscreen_controls_layout->addWidget(
        playback_fullscreen_exit_button_);
    fullscreen_layout->addWidget(fullscreen_controls);
    playback_fullscreen_window_->hide();

    auto *player_controls = new QHBoxLayout();
    player_controls->setSpacing(8);
    playback_play_button_ = new QPushButton("▶ 재생", viewer);
    playback_play_button_->setProperty("primary", true);
    playback_play_button_->setEnabled(false);
    playback_play_button_->setToolTip("선택 시각부터 재생 (Space)");
    playback_pause_button_ = new QPushButton("Ⅱ 일시정지", viewer);
    playback_pause_button_->setEnabled(false);
    playback_pause_button_->setToolTip("현재 위치를 저장하고 session을 종료합니다 (Space)");
    playback_stop_button_ = new QPushButton("■ 정지", viewer);
    playback_stop_button_->setEnabled(false);
    playback_position_label_ = makeMutedLabel("--:--:-- / --:--:--", viewer);
    playback_position_slider_ = new QSlider(Qt::Horizontal, viewer);
    playback_position_slider_->setRange(0, 1000);
    playback_position_slider_->setEnabled(false);
    playback_speed_combo_ = new QComboBox(viewer);
    playback_speed_combo_->addItems({"0.5×", "1.0×", "2.0×", "4.0×"});
    playback_speed_combo_->setCurrentText("1.0×");
    playback_speed_combo_->setEnabled(false);
    playback_speed_combo_->setToolTip("Pi playback rate 규약 구현 후 활성화");
    playback_fullscreen_button_ = new QPushButton("⛶ 전체화면", viewer);
    playback_fullscreen_button_->setToolTip("전체화면 전환 (F11 또는 영상 더블클릭)");
    player_controls->addWidget(playback_play_button_);
    player_controls->addWidget(playback_pause_button_);
    player_controls->addWidget(playback_stop_button_);
    player_controls->addWidget(playback_position_label_);
    player_controls->addWidget(playback_position_slider_, 1);
    player_controls->addWidget(playback_speed_combo_);
    player_controls->addWidget(playback_fullscreen_button_);
    viewer_layout->addLayout(player_controls);

    auto *shortcut_hint = makeMutedLabel(
        "Space 재생/일시정지  ·  ←/→ 1초 이동  ·  F11 전체화면",
        viewer);
    shortcut_hint->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    viewer_layout->addWidget(shortcut_hint);
    layout->addWidget(viewer, 1);

    connect(playback_auth_settings_button_,
            &QPushButton::clicked,
            this,
            [this]() {
                if (auto *settings_button = navigation_group_->button(4)) {
                    settings_button->setChecked(true);
                }
                setCurrentPage(4);
            });
    connect(playback_auth_retry_button_,
            &QPushButton::clicked,
            this,
            [this]() {
                if (!playback_api_->isConfigured()) {
                    setControlUiState(
                        ControlUiState::SettingsRequired);
                    return;
                }
                setControlUiState(ControlUiState::Connecting);
                playback_login_in_progress_ = true;
                playback_api_->login();
            });
    connect(playback_pivot_date_button_,
            &QPushButton::clicked,
            this,
            [this]() {
                QDialog dialog(this);
                dialog.setWindowTitle("기준 날짜 선택");
                auto *dialog_layout = new QVBoxLayout(&dialog);
                auto *calendar = new QCalendarWidget(&dialog);
                calendar->setSelectedDate(playback_pivot_date_);
                calendar->setMaximumDate(QDate::currentDate());
                dialog_layout->addWidget(calendar);
                connect(calendar,
                        &QCalendarWidget::clicked,
                        &dialog,
                        [this, &dialog](const QDate &date) {
                            setPlaybackPivotDate(date);
                            dialog.accept();
                        });
                dialog.exec();
            });
    connect(playback_previous_button_,
            &QPushButton::clicked,
            this,
            [this]() { shiftPlaybackPivot(-1); });
    connect(playback_next_button_,
            &QPushButton::clicked,
            this,
            [this]() { shiftPlaybackPivot(1); });
    connect(playback_now_button_,
            &QPushButton::clicked,
            this,
            [this]() {
                const QDateTime now = currentServerDisplayTime();
                if (!now.isValid()) {
                    setPlaybackFeedback(
                        "서버 시각을 받은 뒤 현재 구간을 조회할 수 있습니다.",
                        "warning");
                    return;
                }
                setPlaybackPivotDate(now.date());
                playback_pivot_time_edit_->setTime(now.time());
                searchPlaybackTimeline();
            });
    connect(playback_search_button_,
            &QPushButton::clicked,
            this,
            &MainWindow::searchPlaybackTimeline);
    connect(zoom_out_button,
            &QPushButton::clicked,
            playback_timeline_,
            &TimelineWidget::zoomOut);
    connect(zoom_in_button,
            &QPushButton::clicked,
            playback_timeline_,
            &TimelineWidget::zoomIn);
    connect(zoom_reset_button,
            &QPushButton::clicked,
            playback_timeline_,
            &TimelineWidget::resetZoom);
    connect(playback_timeline_,
            &TimelineWidget::visibleRangeChanged,
            this,
            [this](qint64 start_utc_ms,
                   qint64 end_utc_ms,
                   qint64 pivot_utc_ms) {
                updateTimelineNavigator(start_utc_ms,
                                        end_utc_ms,
                                        pivot_utc_ms);
                timeline_prefetch_timer_->start();
            });
    connect(playback_timeline_navigator_,
            &QSlider::valueChanged,
            this,
            [this](int value) {
                if (timeline_navigator_updating_ ||
                    timeline_cache_end_utc_ms_ <=
                        timeline_cache_start_utc_ms_ ||
                    timeline_visible_duration_ms_ <= 0) {
                    return;
                }
                const qint64 travel =
                    qMax<qint64>(
                        0,
                        timeline_cache_end_utc_ms_ -
                            timeline_cache_start_utc_ms_ -
                            timeline_visible_duration_ms_);
                const qint64 start =
                    timeline_cache_start_utc_ms_ +
                    travel * value /
                        playback_timeline_navigator_->maximum();
                playback_timeline_->setVisibleRange(
                    start, start + timeline_visible_duration_ms_);
            });
    connect(playback_play_button_,
            &QPushButton::clicked,
            this,
            &MainWindow::playSelectedRange);
    connect(playback_pause_button_,
            &QPushButton::clicked,
            this,
            &MainWindow::togglePlaybackPause);
    connect(playback_stop_button_,
            &QPushButton::clicked,
            this,
            &MainWindow::stopPlayback);
    connect(playback_fullscreen_button_,
            &QPushButton::clicked,
            this,
            &MainWindow::togglePlaybackFullscreen);
    const auto queue_video_single_click = [this]() {
        playback_video_click_timer_->start();
    };
    connect(playback_panel_,
            &VideoPanel::singleClicked,
            this,
            queue_video_single_click);
    connect(playback_fullscreen_panel_,
            &VideoPanel::singleClicked,
            this,
            queue_video_single_click);
    const auto handle_video_double_click = [this]() {
        playback_video_click_timer_->stop();
        togglePlaybackFullscreen();
    };
    connect(playback_panel_,
            &VideoPanel::doubleClicked,
            this,
            handle_video_double_click);
    connect(playback_fullscreen_panel_,
            &VideoPanel::doubleClicked,
            this,
            handle_video_double_click);
    connect(playback_fullscreen_toggle_button_,
            &QPushButton::clicked,
            this,
            &MainWindow::togglePlaybackPause);
    connect(playback_fullscreen_stop_button_,
            &QPushButton::clicked,
            this,
            &MainWindow::stopPlayback);
    connect(playback_fullscreen_exit_button_,
            &QPushButton::clicked,
            this,
            &MainWindow::togglePlaybackFullscreen);
    connect(playback_fullscreen_position_slider_,
            &QSlider::sliderPressed,
            this,
            [this]() { playback_slider_dragging_ = true; });
    connect(playback_fullscreen_position_slider_,
            &QSlider::sliderReleased,
            this,
            [this]() {
                playback_slider_dragging_ = false;
                if (playback_range_end_utc_ms_ <=
                    playback_range_start_utc_ms_) {
                    return;
                }
                const qint64 target =
                    playback_range_start_utc_ms_ +
                    (playback_range_end_utc_ms_ -
                     playback_range_start_utc_ms_) *
                        playback_fullscreen_position_slider_->value() /
                        playback_fullscreen_position_slider_->maximum();
                seekPlaybackTo(target);
            });
    connect(playback_position_slider_,
            &QSlider::sliderPressed,
            this,
            [this]() { playback_slider_dragging_ = true; });
    connect(playback_position_slider_,
            &QSlider::sliderReleased,
            this,
            [this]() {
                playback_slider_dragging_ = false;
                if (playback_range_end_utc_ms_ <=
                    playback_range_start_utc_ms_) {
                    return;
                }
                const qint64 target =
                    playback_range_start_utc_ms_ +
                    (playback_range_end_utc_ms_ -
                     playback_range_start_utc_ms_) *
                        playback_position_slider_->value() /
                        playback_position_slider_->maximum();
                seekPlaybackTo(target);
            });
    connect(playback_timeline_,
            &TimelineWidget::timeSelected,
            this,
            [this](int channel_id,
                   qint64 start_utc_ms,
                   qint64 end_utc_ms,
                   bool user_initiated) {
                playback_channel_id_ = channel_id;
                playback_selection_start_utc_ms_ = start_utc_ms;
                playback_selection_end_utc_ms_ = end_utc_ms;
                if (user_initiated && !playback_seek_in_progress_) {
                    playback_range_start_utc_ms_ = start_utc_ms;
                    playback_range_end_utc_ms_ = end_utc_ms;
                } else {
                    playback_selection_end_utc_ms_ =
                        playback_range_end_utc_ms_;
                }
                playback_current_utc_ms_ = start_utc_ms;
                if (user_initiated) {
                    playback_paused_ = true;
                }
                playback_selection_label_->setText(
                    QString("선택: CH %1 · %2")
                        .arg(channel_id)
                        .arg(localRangeText(start_utc_ms, end_utc_ms)));
                if (!playback_worker_ && user_initiated &&
                    !playback_seek_in_progress_) {
                    active_playback_start_utc_ms_ = 0;
                    active_playback_end_utc_ms_ = 0;
                    playback_panel_->setTitle(
                        QString("PLAYBACK · CH %1 · %2")
                            .arg(channel_id)
                            .arg(QDateTime::fromMSecsSinceEpoch(
                                     start_utc_ms, QTimeZone::UTC)
                                     .toTimeZone(QTimeZone("Asia/Seoul"))
                                     .toString("yyyy-MM-dd HH:mm:ss")));
                    updatePlaybackPosition(start_utc_ms);
                }
                playback_play_button_->setEnabled(true);
                playback_fullscreen_toggle_button_->setEnabled(true);
                if (user_initiated && playback_worker_) {
                    pending_playback_api_action_ =
                        kPlaybackApiActionThumbnail;
                    setPlaybackUiState(PlaybackUiState::Pausing);
                    setPlaybackFeedback(
                        "재생을 멈추고 선택 시각 미리보기를 준비하는 중...",
                        "warning");
                    stopPlaybackWorker(false);
                } else if (user_initiated && !playback_worker_) {
                    setPlaybackUiState(PlaybackUiState::Preview);
                    setPlaybackFeedback("선택 시각 미리보기 요청 중...",
                                        "warning");
                    performPlaybackApiAction(kPlaybackApiActionThumbnail);
                }
                updatePlaybackControlState();
            });
    connect(playback_timeline_,
            &TimelineWidget::timeActivated,
            this,
            [this](int, qint64, qint64) {
                playSelectedRange();
            });
    refreshPlaybackAuthUi(false);
    updatePlaybackControlState();
    return page;
}

QWidget *MainWindow::createEventsPage(QWidget *parent) {
    auto *page = new QWidget(parent);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto *filterCard = makeCard(page);
    auto *filterLayout = new QHBoxLayout(filterCard);
    filterLayout->setContentsMargins(14, 10, 14, 10);
    filterLayout->setSpacing(12);

    auto *sevLabel = new QLabel("심각도:", filterCard);
    events_severity_filter_ = new QComboBox(filterCard);
    events_severity_filter_->addItems({"전체", "CRITICAL (긴급)", "WARNING (경고)", "INFO (정보)"});

    auto *chLabel = new QLabel("채널:", filterCard);
    events_channel_filter_ = new QComboBox(filterCard);
    events_channel_filter_->addItems({"전체", "CH 1", "CH 2", "CH 3", "CH 4"});

    filterLayout->addWidget(sevLabel);
    filterLayout->addWidget(events_severity_filter_);
    filterLayout->addSpacing(16);
    filterLayout->addWidget(chLabel);
    filterLayout->addWidget(events_channel_filter_);
    filterLayout->addStretch(1);

    layout->addWidget(filterCard);

    events_table_ = new QTableWidget(0, 6, page);
    events_table_->setHorizontalHeaderLabels({"발생 시각", "심각도", "구역/채널", "이벤트 내용", "차량 및 EV 정보", "확인 (ACK)"});
    events_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    events_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    events_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    events_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    events_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    events_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    events_table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    events_table_->verticalHeader()->setVisible(false);
    events_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    events_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    events_table_->setAlternatingRowColors(true);
    layout->addWidget(events_table_, 1);

    layout->addWidget(makeMutedLabel("Pi GET /api/v1/events Long Polling 연결 수신 중. 수신된 이벤트 세션 상태는 실시간으로 갱신됩니다.", page));

    connect(events_severity_filter_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { refreshEventsTable(); });
    connect(events_channel_filter_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { refreshEventsTable(); });

    refreshEventsTable();
    return page;
}

QWidget *MainWindow::createDevicesPage(QWidget *parent) {
    auto *page = new QWidget(parent);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto *summaries = new QHBoxLayout();
    summaries->setSpacing(12);
    summaries->addWidget(
        createSummaryCard("서버",
                          "상태 대기",
                          "Control API에서 5초 간격으로 확인",
                          page,
                          &system_device_value_label_,
                          &system_device_caption_label_));
    system_device_value_label_->setProperty("settingsFeedback", true);
    summaries->addWidget(
        createSummaryCard("녹화 저장소",
                          "상태 대기",
                          "Control API 연결 후 5초 간격으로 확인",
                          page,
                          &storage_device_value_label_,
                          &storage_device_caption_label_));
    storage_device_value_label_->setProperty(
        "settingsFeedback", true);
    summaries->addWidget(createSummaryCard("STM32 구역 제어기", "등록 대기", "구역당 1대 기본", page));
    layout->addLayout(summaries);
    storage_alert_label_ =
        makeMutedLabel(QString(), page);
    storage_alert_label_->setProperty("settingsFeedback", true);
    storage_alert_label_->setVisible(false);
    layout->addWidget(storage_alert_label_);

    auto *health = makeCard(page);
    auto *health_layout = new QFormLayout(health);
    health_layout->setContentsMargins(18, 16, 18, 16);
    health_layout->setHorizontalSpacing(24);
    health_layout->setVerticalSpacing(9);
    server_time_value_label_ = makeMutedLabel("Control API 연결 후 확인", health);
    server_time_value_label_->setProperty("settingsFeedback", true);
    system_metrics_value_label_ = makeMutedLabel("상태 대기", health);
    system_memory_value_label_ = makeMutedLabel("상태 대기", health);
    system_uptime_value_label_ = makeMutedLabel("상태 대기", health);
    system_throttling_value_label_ = makeMutedLabel("상태 대기", health);
    health_layout->addRow(makeSectionTitle("Pi 상태 상세", health));
    health_layout->addRow("서버 시각·NTP", server_time_value_label_);
    health_layout->addRow("CPU·온도", system_metrics_value_label_);
    health_layout->addRow("메모리", system_memory_value_label_);
    health_layout->addRow("Pi 가동 시간", system_uptime_value_label_);
    health_layout->addRow("전원·Throttling", system_throttling_value_label_);
    system_reboot_notice_label_ = makeMutedLabel(QString(), health);
    system_reboot_notice_label_->setProperty("settingsFeedback", true);
    system_reboot_notice_label_->setProperty("severity", "warning");
    system_reboot_notice_label_->setVisible(false);
    health_layout->addRow(system_reboot_notice_label_);
    layout->addWidget(health);

    auto *zones = makeCard(page);
    auto *zones_layout = new QVBoxLayout(zones);
    zones_layout->setContentsMargins(18, 16, 18, 16);
    zones_layout->addWidget(makeSectionTitle("주차 구역·번호판·전기차 판정", zones));
    zones_layout->addWidget(makeMutedLabel("채널 영상 위에 4점 polygon을 지정하고 STM 센서 구역과 연결합니다. Draft 저장·검증·카메라 readback 적용은 각각 분리됩니다.", zones));
    auto *parking_button = new QPushButton("주차 구역 관리 열기", zones);
    parking_button->setProperty("primary", true);
    parking_button->setMaximumWidth(180);
    zones_layout->addWidget(parking_button, 0, Qt::AlignLeft);
    connect(parking_button, &QPushButton::clicked,
            this, &MainWindow::showParkingZoneEditor);
    layout->addWidget(zones);

    auto *hierarchy = makeCard(page);
    auto *hierarchy_layout = new QVBoxLayout(hierarchy);
    hierarchy_layout->setContentsMargins(18, 16, 18, 16);
    hierarchy_layout->setSpacing(10);
    hierarchy_layout->addWidget(makeSectionTitle("충전 스테이션 관리", hierarchy));
    hierarchy_layout->addWidget(makeMutedLabel("사이트 → 충전 구역 → CCTV + STM32 구역 제어기 → 센서·방재판·스프링클러·충전 스테이션", hierarchy));
    stm_device_list_widget_ = new StmDeviceListWidget(stm_api_, hierarchy);
    hierarchy_layout->addWidget(stm_device_list_widget_, 1);
    layout->addWidget(hierarchy, 1);
    return page;
}

QWidget *MainWindow::createSettingsPage(QWidget *parent) {
    auto *scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *page = new QWidget(scroll);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    // Pi 실시간 RTSPS Base URL + Control HTTPS Base URL + 인증서를 한 카드로
    // 합친다. "채널 URL 규칙" · "적용 상태" · "보안 경계" 행은 없앤다 — base
    // URL 적용 결과 피드백은 아래 "연결 상태"(control_feedback_label_) 한
    // 곳으로 합쳐서 보여준다(setBaseUrlFeedback() 참조).
    auto *connection = makeCard(page);
    auto *connection_layout = new QFormLayout(connection);
    connection_layout->setContentsMargins(18, 16, 18, 16);
    connection_layout->setHorizontalSpacing(18);
    connection_layout->addRow(makeSectionTitle("URL 및 인증 관리", connection));

    base_url_edit_ = new QLineEdit(applied_base_url_, connection);
    base_url_edit_->setPlaceholderText("rtsps://server:8554");
    auto *base_url_row = new QWidget(connection);
    auto *base_url_layout = new QHBoxLayout(base_url_row);
    base_url_layout->setContentsMargins(0, 0, 0, 0);
    base_url_layout->setSpacing(8);
    base_url_apply_button_ = new QPushButton("적용 및 재연결", base_url_row);
    base_url_apply_button_->setEnabled(false);
    base_url_layout->addWidget(base_url_edit_, 1);
    base_url_layout->addWidget(base_url_apply_button_);
    connection_layout->addRow("RTSPS Base URL", base_url_row);
    connect(base_url_apply_button_, &QPushButton::clicked, this, &MainWindow::applyBaseUrl);
    connect(base_url_edit_, &QLineEdit::returnPressed, this, &MainWindow::applyBaseUrl);
    connect(base_url_edit_, &QLineEdit::textChanged, this, [this]() {
        base_url_apply_button_->setEnabled(base_url_edit_->text().trimmed() != applied_base_url_);
        if (base_url_apply_button_->isEnabled()) {
            setBaseUrlFeedback("변경사항이 아직 적용되지 않았습니다.", "warning");
        }
    });

    control_base_url_edit_ =
        new QLineEdit(defaultControlBaseUrl(applied_base_url_).toString(), connection);
    control_base_url_edit_->setPlaceholderText("https://server:9443");
    control_certificate_edit_ = new QLineEdit(connection);
    control_certificate_edit_->setPlaceholderText("Pi에서 받은 server.crt 경로");
    auto *certificate_row = new QWidget(connection);
    auto *certificate_layout = new QHBoxLayout(certificate_row);
    certificate_layout->setContentsMargins(0, 0, 0, 0);
    certificate_layout->setSpacing(8);
    auto *certificate_browse_button =
        new QPushButton("인증서 선택", certificate_row);
    certificate_layout->addWidget(control_certificate_edit_, 1);
    certificate_layout->addWidget(certificate_browse_button);
    control_apply_button_ = new QPushButton("Control API 적용", connection);
    control_apply_button_->setProperty("primary", true);
    control_feedback_label_ =
        makeMutedLabel("비밀번호는 메모리에만 보관하며 server.crt와 IP/hostname SAN을 검증합니다.",
                       connection);
    control_feedback_label_->setProperty("settingsFeedback", true);
    connection_layout->addRow("HTTPS Base URL", control_base_url_edit_);
    connection_layout->addRow("사용자", new QLabel("operator", connection));
    connection_layout->addRow("인증할 .crt의 경로", certificate_row);
    connection_layout->addRow("", control_apply_button_);
    connection_layout->addRow("연결 상태", control_feedback_label_);
    connect(control_apply_button_,
            &QPushButton::clicked,
            this,
            &MainWindow::applyControlSettings);
    connect(certificate_browse_button,
            &QPushButton::clicked,
            this,
            [this]() {
                const QString path = QFileDialog::getOpenFileName(
                    this,
                    "Pi server.crt 선택",
                    control_certificate_edit_->text(),
                    "PEM 인증서 (*.crt *.pem);;모든 파일 (*)");
                if (!path.isEmpty()) {
                    control_certificate_edit_->setText(path);
                }
            });
    layout->addWidget(connection);

    auto *recording = makeCard(page);
    auto *recording_layout = new QFormLayout(recording);
    recording_layout->setContentsMargins(18, 16, 18, 16);
    recording_layout->addRow("녹화 메인스트림", new QLabel("2592x1520 · 최대 30 fps", recording));
    recording_layout->addRow("실시간 서브스트림", new QLabel("1080p 목표", recording));
    recording_layout->addRow("움직임 녹화", new QLabel("사전 5초 · 사후 10초 · 세그먼트 최대 약 60초", recording));
    recording_layout->addRow("설정 소유권", new QLabel("Pi config.get/config.update · revision 충돌 검출", recording));
    layout->addWidget(recording);

    auto *legal = makeCard(page);
    auto *legal_layout = new QVBoxLayout(legal);
    legal_layout->setContentsMargins(18, 16, 18, 16);
    legal_notice_widget_ = new LegalNoticeWidget(legal);
    // Pi가 정책을 내려주는 API가 아직 없어 LegalNoticeWidget은 기본값이
    // 전부 빈 값(= "등록되지 않음")이다. 시연용 기본 정책을 채워 둔다 —
    // 13개 항목이 모두 채워져야 "고지 설정 완료" 배지로 바뀐다.
    // TODO: Pi 정책 API가 생기면 그 응답으로 교체한다.
    LegalNoticePolicy policy;
    policy.actual_retention_period =
        "위반 evidence 최대 30일 후 자동 파기 · 이의제기 건은 legal hold 해제 시까지";
    policy.camera_installation_purpose =
        "전기차 충전구역 주차 위반 판정 및 화재·과열 감지";
    policy.camera_location = "VEDA 충전 스테이션 주차구역 (CH1~CH4)";
    policy.camera_coverage = "충전구역 주차면 및 진출입 통로";
    policy.recording_schedule = "24시간 연속 녹화";
    policy.operator_name = "VEDA";
    policy.privacy_officer_contact =
        "VEDA 개인정보 보호책임자 · privacy@veda.example.com";
    policy.request_contact = "privacy@veda.example.com";
    policy.privacy_policy_url = "https://veda.example.com/privacy-policy";
    policy.video_policy_url = "https://veda.example.com/video-policy";
    policy.policy_version = "1.0.0";
    policy.effective_date = "2026-08-27";
    policy.last_updated_date = "2026-08-27";
    legal_notice_widget_->setPolicy(policy);
    legal_layout->addWidget(legal_notice_widget_);
    layout->addWidget(legal);
    layout->addStretch(1);
    scroll->setWidget(page);
    return scroll;
}

QFrame *MainWindow::createSummaryCard(const QString &title,
                                      const QString &value,
                                      const QString &caption,
                                      QWidget *parent,
                                      QLabel **value_label_out,
                                      QLabel **caption_label_out) const {
    auto *card = makeCard(parent);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(4);
    layout->addWidget(makeMutedLabel(title, card));
    auto *value_label = new QLabel(value, card);
    value_label->setProperty("value", true);
    layout->addWidget(value_label);
    auto *caption_label = makeMutedLabel(caption, card);
    layout->addWidget(caption_label);
    if (value_label_out) {
        *value_label_out = value_label;
    }
    if (caption_label_out) {
        *caption_label_out = caption_label;
    }
    return card;
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    QTimer::singleShot(0, this, [this]() { updateLiveLayout(); });
}

void MainWindow::updateLiveLayout() {
    if (!live_page_ || !live_page_layout_ || !live_grid_layout_ ||
        !live_grid_host_ || !live_grid_scroll_ || !live_events_card_ ||
        panels_.size() != 4) {
        return;
    }
    // live_page_와 그 자식의 최소 크기는 세로 4x1 모드에서 커진다.
    // 그 크기로 다시 모드를 판정하면 가로 화면으로 돌아와도 세로 모드가
    // 자기 자신을 유지하므로, 콘텐츠 크기와 무관한 최상위 표시 영역을 쓴다.
    const QSize available = contentsRect().size();
    if (!available.isValid()) {
        return;
    }
    const qreal aspect =
        static_cast<qreal>(available.width()) /
        qMax(1, available.height());
    const int mode = aspect < 0.95 ? 1 : (aspect > 2.6 ? 2 : 0);
    if (mode != live_layout_mode_) {
        // 세로 모드에서 설정한 큰 최소 높이가 새 배치의 size hint에
        // 남지 않도록 위젯을 재배치하기 전에 먼저 해제한다.
        for (VideoPanel *panel : panels_) {
            panel->setMinimumVideoHeight(135);
        }
        live_layout_mode_ = mode;
        live_grid_layout_->setContentsMargins(0, 0, 0, 0);

        live_page_layout_->removeWidget(live_grid_scroll_);
        live_page_layout_->removeWidget(live_events_card_);
        for (VideoPanel *panel : panels_) {
            live_grid_layout_->removeWidget(panel);
        }
        for (int index = 0; index < 4; ++index) {
            live_grid_layout_->setColumnStretch(index, 0);
            live_grid_layout_->setRowStretch(index, 0);
        }
        for (int index = 0; index < 3; ++index) {
            live_page_layout_->setColumnStretch(index, 0);
            live_page_layout_->setRowStretch(index, 0);
        }

        if (mode == 1) {
            live_grid_scroll_->setVerticalScrollBarPolicy(
                Qt::ScrollBarAsNeeded);
            for (int channel = 0; channel < 4; ++channel) {
                live_grid_layout_->addWidget(
                    panels_[channel], channel, 0);
            }
            live_grid_layout_->setColumnStretch(0, 1);
            live_events_card_->setMinimumWidth(286);
            live_events_card_->setMaximumWidth(340);
            live_events_card_->setMaximumHeight(QWIDGETSIZE_MAX);
            live_page_layout_->addWidget(live_grid_scroll_, 0, 0);
            live_page_layout_->addWidget(live_events_card_, 0, 1);
            live_page_layout_->setColumnStretch(0, 1);
        } else if (mode == 2) {
            live_grid_scroll_->setVerticalScrollBarPolicy(
                Qt::ScrollBarAlwaysOff);
            for (int channel = 0; channel < 4; ++channel) {
                live_grid_layout_->addWidget(
                    panels_[channel], 0, channel);
                live_grid_layout_->setColumnStretch(channel, 1);
            }
            live_grid_layout_->setRowStretch(0, 1);
            live_events_card_->setMinimumWidth(0);
            live_events_card_->setMaximumWidth(QWIDGETSIZE_MAX);
            live_events_card_->setMaximumHeight(210);
            live_page_layout_->addWidget(live_grid_scroll_, 0, 0);
            live_page_layout_->addWidget(live_events_card_, 1, 0);
            live_page_layout_->setRowStretch(0, 1);
        } else {
            live_grid_scroll_->setVerticalScrollBarPolicy(
                Qt::ScrollBarAlwaysOff);
            for (int channel = 0; channel < 4; ++channel) {
                live_grid_layout_->addWidget(
                    panels_[channel], channel / 2, channel % 2);
            }
            live_grid_layout_->setColumnStretch(0, 1);
            live_grid_layout_->setColumnStretch(1, 1);
            live_grid_layout_->setRowStretch(0, 1);
            live_grid_layout_->setRowStretch(1, 1);
            live_events_card_->setMinimumWidth(286);
            live_events_card_->setMaximumWidth(340);
            live_events_card_->setMaximumHeight(QWIDGETSIZE_MAX);
            live_page_layout_->addWidget(live_grid_scroll_, 0, 0);
            live_page_layout_->addWidget(live_events_card_, 0, 1);
            live_page_layout_->setColumnStretch(0, 1);
        }
    }

    if (mode == 1) {
        live_grid_layout_->setContentsMargins(0, 0, 0, 0);
        const int surface_width =
            qMax(240, live_grid_scroll_->viewport()->width() - 20);
        const int surface_height =
            qRound(surface_width * 1520.0 / 2592.0);
        for (VideoPanel *panel : panels_) {
            panel->setMinimumVideoHeight(surface_height);
        }
        live_grid_layout_->activate();
        live_grid_host_->setMinimumHeight(
            live_grid_layout_->minimumSize().height());
    } else {
        live_grid_host_->setMinimumHeight(0);
        for (VideoPanel *panel : panels_) {
            panel->setMinimumVideoHeight(135);
        }

        // 일반형/초광폭형은 viewport 전체를 억지로 채우지 않는다. 행 수를
        // 유지한 채 각 cell 안에 들어갈 수 있는 2592:1520 surface를 구하고,
        // 필요한 grid 크기만 중앙에 배치해 영상 내부 letterbox를 없앤다.
        live_grid_layout_->setContentsMargins(0, 0, 0, 0);
        live_grid_layout_->invalidate();
        live_grid_layout_->activate();

        const QSize viewport_size = live_grid_scroll_->viewport()->size();
        const int rows = mode == 2 ? 1 : 2;
        const int columns = mode == 2 ? 4 : 2;
        const int horizontal_spacing =
            qMax(0, live_grid_layout_->horizontalSpacing());
        const int vertical_spacing =
            qMax(0, live_grid_layout_->verticalSpacing());
        const int cell_width =
            qMax(1,
                 (viewport_size.width() -
                  (columns - 1) * horizontal_spacing) /
                     columns);
        const int cell_height =
            qMax(1,
                 (viewport_size.height() -
                  (rows - 1) * vertical_spacing) /
                     rows);

        int panel_chrome_height = 0;
        for (VideoPanel *panel : panels_) {
            panel_chrome_height =
                qMax(panel_chrome_height,
                     panel->chromeHeightHint());
        }
        constexpr int kPanelHorizontalChrome = 16;
        const QSize maximum_surface(
            qMax(1, cell_width - kPanelHorizontalChrome),
            qMax(1, cell_height - panel_chrome_height));
        const QSize fitted_surface =
            QSize(2592, 1520).scaled(maximum_surface,
                                     Qt::KeepAspectRatio);
        const int fitted_cell_width =
            fitted_surface.width() + kPanelHorizontalChrome;
        const int fitted_cell_height =
            fitted_surface.height() + panel_chrome_height;
        const int fitted_grid_width =
            columns * fitted_cell_width +
            (columns - 1) * horizontal_spacing;
        const int fitted_grid_height =
            rows * fitted_cell_height +
            (rows - 1) * vertical_spacing;
        const int horizontal_margin =
            qMax(0, (viewport_size.width() - fitted_grid_width) / 2);
        const int vertical_margin =
            qMax(0, (viewport_size.height() - fitted_grid_height) / 2);
        live_grid_layout_->setContentsMargins(horizontal_margin,
                                              vertical_margin,
                                              horizontal_margin,
                                              vertical_margin);
    }
}

void MainWindow::setCurrentPage(int page_index) {
    if (page_index < 0 || page_index >= kPageTitles.size()) {
        return;
    }
    const int previous_page = page_stack_->currentIndex();
    if (previous_page == 3 && page_index != 3 && stm_device_list_widget_) {
        stm_device_list_widget_->stopPolling();
    }
    if (page_index == 3 && previous_page != 3 && stm_device_list_widget_) {
        stm_device_list_widget_->startPolling();
    }
    if (parking_zone_editor_ &&
        page_stack_->currentWidget() == parking_zone_editor_ &&
        !parking_zone_editor_->confirmDiscard(this)) {
        if (auto *button = navigation_group_->button(3)) button->setChecked(true);
        return;
    }
    if (previous_page == 1 && page_index != 1 && playback_worker_) {
        togglePlaybackPause();
    }
    if (page_index == 1 && previous_page != 1) {
        live_suspended_for_playback_ =
            live_desired_running_ || activeLiveWorkerCount() > 0 ||
            hasPendingLiveRetries();
        if (live_suspended_for_playback_) {
            stopStreams();
        }
    } else if (page_index == 0 &&
               (live_suspended_for_playback_ ||
                !live_desired_running_ || stopping_streams_)) {
        live_suspended_for_playback_ = false;
        startStreams();
    }
    page_stack_->setCurrentIndex(page_index);
    page_title_label_->setText(kPageTitles[page_index]);
    page_subtitle_label_->setText(kPageSubtitles[page_index]);
    stream_controls_->setVisible(page_index == 0);
}

void MainWindow::showParkingZoneEditor() {
    if (!parking_zone_editor_) return;
    for (int channel = 0; channel < panels_.size(); ++channel) {
        parking_zone_editor_->setReferenceImage(channel + 1,
                                                panels_[channel]->currentImage());
    }
    if (stm_device_list_widget_) stm_device_list_widget_->stopPolling();
    parking_zone_editor_->openChannel(1,
        panels_.isEmpty() ? QImage() : panels_.first()->currentImage());
    page_stack_->setCurrentWidget(parking_zone_editor_);
    page_title_label_->setText("주차 구역 관리");
    page_subtitle_label_->setText("채널별 4점 구역과 STM 센서 매핑을 Draft로 검증하고 Pi에 적용합니다.");
    stream_controls_->setVisible(false);
    if (auto *button = navigation_group_->button(3)) button->setChecked(true);
}

void MainWindow::leaveParkingZoneEditor() {
    if (!parking_zone_editor_ || !parking_zone_editor_->confirmDiscard(this)) return;
    setCurrentPage(3);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (parking_zone_editor_ &&
        page_stack_->currentWidget() == parking_zone_editor_ &&
        !parking_zone_editor_->confirmDiscard(this)) {
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}

QString MainWindow::channelUrl(int channel) const {
    QString base = applied_base_url_;
    while (base.endsWith('/')) {
        base.chop(1);
    }
    return QString("%1/ch%2").arg(base).arg(channel + 1);
}

void MainWindow::applyBaseUrl() {
    QString normalized;
    QString error;
    if (!normalizeRtspsBaseUrl(base_url_edit_->text(), &normalized, &error)) {
        setBaseUrlFeedback(error, "critical");
        base_url_apply_button_->setEnabled(true);
        return;
    }
    if (normalized == applied_base_url_) {
        base_url_edit_->setText(normalized);
        base_url_apply_button_->setEnabled(false);
        setBaseUrlFeedback("현재 실행에 적용됨 · Pi 스트림 목록 API 구현 전 임시 설정", "ok");
        return;
    }

    applied_base_url_ = normalized;
    base_url_edit_->setText(normalized);
    if (control_base_url_edit_ && !playback_api_->isConfigured()) {
        control_base_url_edit_->setText(
            defaultControlBaseUrl(applied_base_url_).toString());
    }
    base_url_apply_button_->setEnabled(false);
    setBaseUrlFeedback("적용 완료 · 새 주소로 스트림을 연결합니다.", "ok");
    startStreams();
}

void MainWindow::setPlaybackPivotDate(const QDate &date) {
    playback_pivot_date_ = date;
    if (playback_pivot_date_button_) {
        playback_pivot_date_button_->setText(date.toString("yyyy-MM-dd"));
    }
}

QDateTime MainWindow::currentServerDisplayTime() const {
    if (!server_time_interpolation_active_ ||
        server_time_base_utc_ms_ <= 0 ||
        !server_time_elapsed_.isValid()) {
        return {};
    }
    const qint64 utc_ms =
        server_time_base_utc_ms_ + server_time_elapsed_.elapsed();
    return QDateTime::fromMSecsSinceEpoch(utc_ms, QTimeZone::UTC)
        .toTimeZone(QTimeZone("Asia/Seoul"));
}

void MainWindow::shiftPlaybackPivot(int direction) {
    if (!playback_range_combo_ || !playback_pivot_time_edit_ ||
        direction == 0) {
        return;
    }
    const qint64 duration_seconds =
        playback_range_combo_->currentData().toLongLong();
    QDateTime pivot(playback_pivot_date_,
                    playback_pivot_time_edit_->time(),
                    QTimeZone("Asia/Seoul"));
    pivot = pivot.addSecs(direction * duration_seconds);
    const QDateTime now = currentServerDisplayTime();
    if (!now.isValid()) {
        setPlaybackFeedback("서버 시각을 확인할 수 없습니다.", "warning");
        return;
    }
    if (pivot > now) {
        pivot = now;
    }
    setPlaybackPivotDate(pivot.date());
    playback_pivot_time_edit_->setTime(pivot.time());
    searchPlaybackTimeline();
}

void MainWindow::refreshPlaybackAuthUi(bool authenticated) {
    setControlUiState(
        authenticated
            ? ControlUiState::Ready
            : (playback_api_->isConfigured()
                   ? ControlUiState::RetryableFailure
                   : ControlUiState::SettingsRequired));
}

void MainWindow::setControlUiState(ControlUiState state,
                                   const QString &message) {
    const ControlUiState previous_state = control_ui_state_;
    control_ui_state_ = state;
    const bool ready = state == ControlUiState::Ready;
    const bool settings_required =
        state == ControlUiState::SettingsRequired;
    const bool retryable =
        state == ControlUiState::RetryableFailure;
    const bool show_prompt = !ready;

    if (footer_connection_label_) {
        QString connection_text;
        QString connection_severity;
        if (ready) {
            connection_text = "● 연결됨";
            connection_severity = "ok";
        } else if (state == ControlUiState::Connecting) {
            connection_text = "● 연결 중";
            connection_severity = "warning";
        } else if (settings_required) {
            connection_text = "● 설정 필요";
            connection_severity = "warning";
        } else {
            connection_text = "● 연결 끊김";
            connection_severity = "critical";
        }
        footer_connection_label_->setText(connection_text);
        footer_connection_label_->setProperty(
            "severity", connection_severity);
        refreshStyle(footer_connection_label_);
    }

    QString prompt = message;
    if (prompt.isEmpty()) {
        if (settings_required) {
            prompt =
                "Control API 설정이 필요합니다.\n"
                "server.crt와 비밀번호를 적용하세요.";
        } else if (state == ControlUiState::Connecting) {
            prompt = "Control API에 연결하는 중입니다…";
        } else if (retryable) {
            prompt =
                "Control API 연결이 끊어졌습니다.\n"
                "저장된 설정으로 다시 연결할 수 있습니다.";
        }
    }
    if (playback_query_controls_) {
        playback_query_controls_->setEnabled(ready);
    }
    if (playback_now_button_) {
        playback_now_button_->setEnabled(
            ready && server_time_interpolation_active_);
    }
    if (playback_next_button_) {
        playback_next_button_->setEnabled(
            ready && server_time_interpolation_active_);
    }
    if (storage_status_badge_ && !has_last_storage_status_ &&
        (!ready || previous_state != ControlUiState::Ready)) {
        QString storage_text;
        QString storage_severity = "warning";
        if (settings_required) {
            const bool configured = playback_api_->isConfigured();
            storage_text =
                configured
                    ? "저장소 · Control 설정 확인 필요"
                    : "저장소 · Control 설정 필요";
            storage_severity =
                configured ? "critical" : "warning";
        } else if (state == ControlUiState::Connecting) {
            storage_text = "저장소 · Control 연결 중";
        } else if (ready) {
            storage_text = "저장소 확인 중";
        } else {
            storage_text = "저장소 · Control 연결 오류";
            storage_severity = "critical";
        }
        storage_status_badge_->setText(storage_text);
        storage_status_badge_->setProperty(
            "severity", storage_severity);
        storage_status_badge_->setVisible(true);
        refreshStyle(storage_status_badge_);
    }
    if (pi_status_badge_ && !has_last_device_status_ &&
        (!ready || previous_state != ControlUiState::Ready)) {
        QString pi_text;
        QString pi_severity = "warning";
        if (settings_required) {
            pi_text = playback_api_->isConfigured()
                          ? "서버 · Control 설정 확인 필요"
                          : "서버 · Control 설정 필요";
            pi_severity = playback_api_->isConfigured() ? "critical"
                                                        : "warning";
        } else if (state == ControlUiState::Connecting) {
            pi_text = "서버 · Control 연결 중";
        } else if (ready) {
            pi_text = "서버 상태 확인 중";
        } else {
            pi_text = "서버 · Control 연결 오류";
            pi_severity = "critical";
        }
        pi_status_badge_->setText(pi_text);
        pi_status_badge_->setProperty("severity", pi_severity);
        pi_status_badge_->setVisible(true);
        refreshStyle(pi_status_badge_);
    }
    if (playback_auth_notice_label_) {
        playback_auth_notice_label_->setText(prompt);
        playback_auth_notice_label_->setVisible(show_prompt);
    }
    if (playback_auth_settings_button_) {
        playback_auth_settings_button_->setVisible(settings_required);
    }
    if (playback_auth_retry_button_) {
        playback_auth_retry_button_->setVisible(retryable);
    }
    if (playback_timeline_auth_prompt_) {
        playback_timeline_auth_prompt_->setVisible(show_prompt);
        if (show_prompt) {
            playback_timeline_auth_prompt_->raise();
        }
    }
    if (playback_timeline_) {
        playback_timeline_->setEmptyMessage(
            ready
                ? "타임라인을 불러오세요."
                : prompt);
        if (settings_required &&
            !playback_api_->isConfigured()) {
            playback_timeline_->clearTimeline();
        }
    }
    if (playback_selection_label_) {
        playback_selection_label_->setVisible(ready);
        if (ready && playback_selection_label_->text().isEmpty()) {
            playback_selection_label_->setText(
                "조회 후 녹화 bar를 클릭하세요.");
        }
    }
    if (show_prompt && playback_feedback_label_) {
        setPlaybackFeedback(prompt.replace('\n', ' '),
                            settings_required ? "warning"
                                              : "critical");
    }
}

void MainWindow::pollDeviceStatus() {
    if (!playback_api_->isConfigured() ||
        control_ui_state_ != ControlUiState::Ready) {
        return;
    }
    if (!playback_api_->hasValidAccessToken()) {
        device_status_retry_after_login_ = true;
        if (!playback_login_in_progress_) {
            playback_login_in_progress_ = true;
            playback_api_->login();
        }
        return;
    }
    playback_api_->requestStatus();
}

void MainWindow::updateServerTimeDisplay() {
    if (!server_time_interpolation_active_ ||
        server_time_base_utc_ms_ <= 0 ||
        !server_time_elapsed_.isValid()) {
        return;
    }

    const qint64 utc_ms =
        server_time_base_utc_ms_ + server_time_elapsed_.elapsed();
    const QString local_time =
        QDateTime::fromMSecsSinceEpoch(utc_ms, QTimeZone::UTC)
            .toTimeZone(QTimeZone("Asia/Seoul"))
            .toString("yyyy-MM-dd HH:mm:ss");
    const QString sync_state_lower = server_time_sync_state_.toLower().trimmed();
    QString sync_text;
    QString sync_severity = "warning";
    if (sync_state_lower.contains("sync") || sync_state_lower == "ok") {
        sync_text = QString("%1 · NTP 동기화").arg(local_time);
        sync_severity = "ok";
    } else if (sync_state_lower.contains("unsync") || sync_state_lower.contains("fail")) {
        sync_text = QString("%1 · NTP 미동기화").arg(local_time);
    } else {
        sync_text = QString("%1 · NTP 상태 미확인").arg(local_time);
    }

    if (server_time_label_) {
        server_time_label_->setText(QString("서버 %1").arg(sync_text));
    }
    if (server_time_value_label_) {
        server_time_value_label_->setText(sync_text);
        server_time_value_label_->setProperty("severity", sync_severity);
        refreshStyle(server_time_value_label_);
    }
}

void MainWindow::applyDeviceStatus(const DeviceStatusSnapshot &snapshot) {
    has_last_device_status_ = true;
    device_status_last_updated_at_ = QDateTime::currentDateTime();
    if (footer_connection_label_) {
        footer_connection_label_->setText("● 연결됨");
        footer_connection_label_->setProperty("severity", "ok");
        refreshStyle(footer_connection_label_);
    }

    if (snapshot.server_time.utc_ms > 0) {
        const bool first_server_time =
            !server_time_interpolation_active_;
        server_time_base_utc_ms_ = snapshot.server_time.utc_ms;
        server_time_sync_state_ = snapshot.server_time.sync_state;
        server_time_elapsed_.restart();
        server_time_interpolation_active_ = true;
        updateServerTimeDisplay();
        if (first_server_time) {
            const QDateTime server_now = currentServerDisplayTime();
            if (server_now.isValid()) {
                setPlaybackPivotDate(server_now.date());
                playback_pivot_time_edit_->setTime(server_now.time());
            }
        }
        if (playback_now_button_) {
            playback_now_button_->setEnabled(
                control_ui_state_ == ControlUiState::Ready);
        }
        if (playback_next_button_) {
            playback_next_button_->setEnabled(
                control_ui_state_ == ControlUiState::Ready);
        }
    } else {
        server_time_interpolation_active_ = false;
        server_time_base_utc_ms_ = 0;
        server_time_sync_state_.clear();
        if (playback_now_button_) {
            playback_now_button_->setEnabled(false);
        }
        if (playback_next_button_) {
            playback_next_button_->setEnabled(false);
        }
        if (server_time_label_) {
            server_time_label_->setText("서버 시각 미지원");
        }
        if (server_time_value_label_) {
            server_time_value_label_->setText("서버 시각 미지원");
            server_time_value_label_->setProperty("severity", "warning");
            refreshStyle(server_time_value_label_);
        }
    }

    const QString boot_id = snapshot.server_time.boot_id.trimmed();
    if (!boot_id.isEmpty()) {
        if (!last_server_boot_id_.isEmpty() &&
            last_server_boot_id_ != boot_id &&
            system_reboot_notice_label_) {
            system_reboot_notice_label_->setText(
                QString("Pi 재부팅 감지 · %1")
                    .arg(QDateTime::currentDateTime().toString(
                        "yyyy-MM-dd HH:mm:ss")));
            system_reboot_notice_label_->setVisible(true);
            refreshStyle(system_reboot_notice_label_);
        }
        last_server_boot_id_ = boot_id;
    }

    QString system_text = "측정 미지원";
    QString system_severity = "warning";
    QString system_caption = "서버 상태 측정값이 제공되지 않습니다.";
    if (snapshot.system_available) {
        const SystemStatus &system = snapshot.system;
        const bool stale = system.sample_age_ms > 10000;
        if (system.state == "normal") {
            system_text = stale ? "갱신 지연" : "정상";
            system_severity = stale ? "warning" : "ok";
        } else if (system.state == "warning") {
            system_text = "경고";
        } else if (system.state == "critical") {
            system_text = "위험";
            system_severity = "critical";
        } else {
            system_text = "상태 미확인";
        }
        const QString cpu =
            system.cpu_usage_available
                ? QString("CPU %1%").arg(system.cpu_usage_percent, 0, 'f', 1)
                : QString("CPU 미지원");
        const QString temperature =
            system.temperature_available
                ? QString("%1°C").arg(system.temperature_celsius, 0, 'f', 1)
                : QString("온도 미지원");
        system_caption = "Control API에서 5초 간격으로 상태 확인";
        if (stale) {
            system_caption +=
                QString(" · 표본 %1초 지연")
                    .arg(system.sample_age_ms / 1000.0, 0, 'f', 1);
        }

        if (system_metrics_value_label_) {
            system_metrics_value_label_->setText(
                QString("%1 · %2").arg(cpu, temperature));
        }
        if (system_memory_value_label_) {
            system_memory_value_label_->setText(
                system.memory.available
                    ? QString("전체 %1 · 사용 %2 · 사용 가능 %3 · %4%")
                          .arg(formatStorageBytes(system.memory.total_bytes),
                               formatStorageBytes(system.memory.used_bytes),
                               formatStorageBytes(
                                   system.memory.available_bytes))
                          .arg(system.memory.used_percent, 0, 'f', 1)
                    : QString("측정 미지원"));
        }
        if (system_uptime_value_label_) {
            if (system.uptime_available) {
                const qint64 days = system.uptime_seconds / 86400;
                const qint64 hours =
                    (system.uptime_seconds % 86400) / 3600;
                const qint64 minutes =
                    (system.uptime_seconds % 3600) / 60;
                system_uptime_value_label_->setText(
                    QString("%1일 %2시간 %3분").arg(days).arg(hours).arg(minutes));
            } else {
                system_uptime_value_label_->setText("측정 미지원");
            }
        }
        if (system_throttling_value_label_) {
            if (!system.throttling.available) {
                system_throttling_value_label_->setText("측정 미지원");
            } else {
                QStringList current;
                QStringList history;
                if (system.throttling.under_voltage_now) {
                    current << "저전압";
                }
                if (system.throttling.frequency_capped_now) {
                    current << "주파수 제한";
                }
                if (system.throttling.throttled_now) {
                    current << "throttling";
                }
                if (system.throttling.soft_temperature_limit_now) {
                    current << "온도 제한";
                }
                if (system.throttling.under_voltage_occurred) {
                    history << "저전압";
                }
                if (system.throttling.frequency_capped_occurred) {
                    history << "주파수 제한";
                }
                if (system.throttling.throttled_occurred) {
                    history << "throttling";
                }
                if (system.throttling.soft_temperature_limit_occurred) {
                    history << "온도 제한";
                }
                system_throttling_value_label_->setText(
                    QString("현재 %1 · 과거 이력 %2 · flags 0x%3")
                        .arg(current.isEmpty() ? "정상"
                                               : current.join(", "),
                             history.isEmpty() ? "없음"
                                               : history.join(", "))
                        .arg(system.throttling.raw_flags, 0, 16));
            }
        }
    } else {
        if (system_metrics_value_label_) {
            system_metrics_value_label_->setText("측정 미지원");
        }
        if (system_memory_value_label_) {
            system_memory_value_label_->setText("측정 미지원");
        }
        if (system_uptime_value_label_) {
            system_uptime_value_label_->setText("측정 미지원");
        }
        if (system_throttling_value_label_) {
            system_throttling_value_label_->setText("측정 미지원");
        }
    }

    if (system_device_value_label_) {
        system_device_value_label_->setText(system_text);
        system_device_value_label_->setProperty("severity", system_severity);
        refreshStyle(system_device_value_label_);
    }
    if (system_device_caption_label_) {
        system_device_caption_label_->setText(system_caption);
    }
    if (pi_status_badge_) {
        pi_status_badge_->setToolTip(QString());
        pi_status_badge_->setText(QString("서버 %1").arg(system_text));
        pi_status_badge_->setProperty("severity", system_severity);
        pi_status_badge_->setVisible(system_severity != "ok");
        refreshStyle(pi_status_badge_);
    }

    if (snapshot.storage_available) {
        applyStorageStatus(snapshot.storage);
    } else {
        has_last_storage_status_ = false;
        storage_last_updated_at_ = QDateTime();
        if (storage_status_badge_) {
            storage_status_badge_->setText("저장소 · 측정 미지원");
            storage_status_badge_->setProperty("severity", "warning");
            storage_status_badge_->setVisible(true);
            refreshStyle(storage_status_badge_);
        }
        if (storage_device_value_label_) {
            storage_device_value_label_->setText("측정 미지원");
            storage_device_value_label_->setProperty("severity", "warning");
            refreshStyle(storage_device_value_label_);
        }
        if (storage_device_caption_label_) {
            storage_device_caption_label_->setText(
                "저장소 측정값이 제공되지 않습니다. Control 연결은 유지됩니다.");
        }
        if (storage_alert_label_) {
            storage_alert_label_->clear();
            storage_alert_label_->setVisible(false);
        }
    }
}

void MainWindow::applyStorageStatus(const StorageStatus &status) {
    has_last_storage_status_ = true;
    storage_last_updated_at_ = QDateTime::currentDateTime();
    QString state_text;
    QString severity;
    QString alert;
    if (status.state == "normal") {
        state_text = "정상";
        severity = "ok";
    } else if (status.state == "warning") {
        state_text = "경고";
        severity = "warning";
        alert = "저장공간 부족 경고 · 현재 녹화는 계속됩니다.";
    } else if (status.state == "critical") {
        state_text = "위험";
        severity = "critical";
        alert =
            "저장공간 위험 · retention 수행 또는 공간 확보 실패 가능성이 있습니다.";
    } else {
        state_text = "읽기 전용";
        severity = "critical";
        alert = "저장장치 쓰기 불가 · 실시간 영상과 Control 연결 상태는 별개입니다.";
    }

    if (status.recording_suspended) {
        severity = "critical";
        alert = "용량 부족으로 신규 녹화 중단";
    }

    const QString available =
        formatStorageBytes(status.available_bytes);
    const QString capacity_pair = formatStorageCapacityPair(
        status.available_bytes, status.total_bytes);
    if (storage_status_badge_) {
        storage_status_badge_->setToolTip(QString());
        QString badge_text;
        if (status.recording_suspended) {
            badge_text = QString("신규 녹화 중단 · 저장소 여유 %1")
                             .arg(capacity_pair);
        } else if (status.state == "normal") {
            badge_text = QString("저장소 여유 %1").arg(capacity_pair);
        } else if (status.state == "warning") {
            badge_text = QString("저장소 경고 · 여유 %1")
                             .arg(capacity_pair);
        } else if (status.state == "critical") {
            badge_text = QString("저장소 위험 · 여유 %1")
                             .arg(capacity_pair);
        } else {
            badge_text = QString("저장소 쓰기 불가 · 여유 %1")
                             .arg(capacity_pair);
        }
        storage_status_badge_->setText(badge_text);
        storage_status_badge_->setProperty("severity", severity);
        storage_status_badge_->setVisible(severity != "ok");
        refreshStyle(storage_status_badge_);
    }
    if (storage_device_value_label_) {
        storage_device_value_label_->setText(
            QString("%1 · %2%")
                .arg(state_text)
                .arg(status.used_percent, 0, 'f', 1));
        storage_device_value_label_->setProperty(
            "severity", severity);
        refreshStyle(storage_device_value_label_);
    }
    if (storage_device_caption_label_) {
        storage_device_caption_label_->setText(
            QString("전체 %1 · 사용 %2 · 사용 가능 %3")
                .arg(formatStorageBytes(status.total_bytes),
                     formatStorageBytes(status.used_bytes),
                     available));
    }
    if (storage_alert_label_) {
        storage_alert_label_->setText(alert);
        storage_alert_label_->setProperty("severity", severity);
        storage_alert_label_->setVisible(!alert.isEmpty());
        refreshStyle(storage_alert_label_);
    }
}

void MainWindow::setDeviceControlError(const QString &message) {
    server_time_interpolation_active_ = false;
    if (footer_connection_label_) {
        footer_connection_label_->setText(
            has_last_device_status_ ? "● 연결 지연" : "● 연결 끊김");
        footer_connection_label_->setProperty(
            "severity", has_last_device_status_ ? "warning" : "critical");
        refreshStyle(footer_connection_label_);
    }
    if (pi_status_badge_) {
        pi_status_badge_->setText(
            has_last_device_status_ ? "서버 상태 지연"
                                    : "서버 · Control 연결 오류");
        pi_status_badge_->setToolTip(message);
        pi_status_badge_->setProperty(
            "severity", has_last_device_status_ ? "warning" : "critical");
        pi_status_badge_->setVisible(true);
        refreshStyle(pi_status_badge_);
    }
    if (system_device_value_label_) {
        system_device_value_label_->setText(
            has_last_device_status_ ? "갱신 지연" : "상태 확인 실패");
        system_device_value_label_->setProperty(
            "severity", has_last_device_status_ ? "warning" : "critical");
        refreshStyle(system_device_value_label_);
    }
    if (system_device_caption_label_) {
        const QString checked_at =
            device_status_last_updated_at_.isValid()
                ? device_status_last_updated_at_.time().toString("HH:mm:ss")
                : QString("-");
        system_device_caption_label_->setText(
            has_last_device_status_
                ? QString("Control 갱신 지연 · 마지막 확인 %1").arg(checked_at)
                : QString("Control API 오류 · %1").arg(message));
    }
    if (server_time_label_) {
        server_time_label_->setText(
            has_last_device_status_ ? "서버 시각 · 갱신 지연"
                                    : "서버 시각 · 연결 오류");
    }
    if (server_time_value_label_) {
        const QString checked_at =
            device_status_last_updated_at_.isValid()
                ? device_status_last_updated_at_.time().toString("HH:mm:ss")
                : QString("-");
        server_time_value_label_->setText(
            has_last_device_status_
                ? QString("갱신 지연 · 마지막 확인 %1").arg(checked_at)
                : QString("Control 연결 오류"));
        server_time_value_label_->setProperty(
            "severity", has_last_device_status_ ? "warning" : "critical");
        refreshStyle(server_time_value_label_);
    }

    if (has_last_storage_status_) {
        const QString checked_at =
            storage_last_updated_at_.isValid()
                ? storage_last_updated_at_.time().toString("HH:mm:ss")
                : QString("-");
        if (storage_status_badge_) {
            storage_status_badge_->setText(
                QString("저장소 · 갱신 지연 · 마지막 확인 %1")
                    .arg(checked_at));
            storage_status_badge_->setToolTip(message);
            storage_status_badge_->setProperty("severity", "warning");
            storage_status_badge_->setVisible(true);
            refreshStyle(storage_status_badge_);
        }
        // 마지막으로 검증된 용량과 저장소 자체의 경고 상태는 유지한다.
        // Control 통신 오류를 저장장치 장애로 오인하지 않도록 숫자와
        // 장치 경고 카드는 덮어쓰지 않는다.
        return;
    }

    if (storage_status_badge_) {
        storage_status_badge_->setText("저장소 · Control 연결 오류");
        storage_status_badge_->setToolTip(message);
        storage_status_badge_->setProperty("severity", "critical");
        storage_status_badge_->setVisible(true);
        refreshStyle(storage_status_badge_);
    }
    if (storage_device_value_label_) {
        storage_device_value_label_->setText("상태 확인 실패");
        storage_device_value_label_->setProperty(
            "severity", "critical");
        refreshStyle(storage_device_value_label_);
    }
    if (storage_device_caption_label_) {
        storage_device_caption_label_->setText(
            QString("Control API 오류 · %1").arg(message));
    }
    if (storage_alert_label_) {
        storage_alert_label_->setText(
            "저장소 상태를 조회하지 못했습니다. 저장장치 장애로 판정하지 않습니다.");
        storage_alert_label_->setProperty("severity", "critical");
        storage_alert_label_->setVisible(true);
        refreshStyle(storage_alert_label_);
    }
}

void MainWindow::updatePlaybackControlState() {
    const bool has_selection =
        playback_selection_end_utc_ms_ >
        playback_selection_start_utc_ms_;
    const bool playing =
        playback_ui_state_ == PlaybackUiState::Playing;
    const bool transition =
        playback_ui_state_ == PlaybackUiState::Starting ||
        playback_ui_state_ == PlaybackUiState::Pausing ||
        playback_ui_state_ == PlaybackUiState::Stopping;
    if (playback_play_button_) {
        playback_play_button_->setText("▶ 재생");
        playback_play_button_->setEnabled(
            !playing && !transition && has_selection);
    }
    if (playback_pause_button_) {
        playback_pause_button_->setText("Ⅱ 일시정지");
        playback_pause_button_->setEnabled(playing && !transition);
    }
    if (playback_fullscreen_toggle_button_) {
        QString text = playing ? "Ⅱ 일시정지" : "▶ 재생";
        if (playback_ui_state_ == PlaybackUiState::Starting) {
            text = "연결 중…";
        } else if (playback_ui_state_ == PlaybackUiState::Pausing) {
            text = "일시정지 중…";
        } else if (playback_ui_state_ == PlaybackUiState::Stopping) {
            text = "중지 중…";
        }
        playback_fullscreen_toggle_button_->setText(text);
        playback_fullscreen_toggle_button_->setEnabled(
            !transition && (playing || has_selection));
    }
}

void MainWindow::setPlaybackUiState(PlaybackUiState state) {
    playback_ui_state_ = state;
    updatePlaybackControlState();
}

void MainWindow::clearProtectedPlaybackState() {
    pending_playback_api_action_ = kPlaybackApiActionNone;
    if (playback_seek_timer_) {
        playback_seek_timer_->stop();
    }
    if (playback_video_click_timer_) {
        playback_video_click_timer_->stop();
    }
    playback_paused_ = false;
    playback_ui_state_ = PlaybackUiState::Preview;
    playback_selection_start_utc_ms_ = 0;
    playback_selection_end_utc_ms_ = 0;
    playback_range_start_utc_ms_ = 0;
    playback_range_end_utc_ms_ = 0;
    playback_current_utc_ms_ = 0;
    timeline_request_in_progress_ = false;
    timeline_cache_start_utc_ms_ = 0;
    timeline_cache_end_utc_ms_ = 0;
    timeline_pending_start_utc_ms_ = 0;
    timeline_pending_end_utc_ms_ = 0;
    if (playback_timeline_navigator_) {
        playback_timeline_navigator_->setEnabled(false);
        playback_timeline_navigator_->setValue(0);
    }
    if (playback_worker_) {
        disconnect(playback_worker_,
                   &StreamWorker::frameReady,
                   this,
                   nullptr);
        disconnect(playback_worker_,
                   &StreamWorker::statsReady,
                   this,
                   nullptr);
        disconnect(playback_worker_,
                   &StreamWorker::statusChanged,
                   this,
                   nullptr);
        disconnect(playback_worker_,
                   &StreamWorker::playbackPositionChanged,
                   this,
                   nullptr);
        stopPlaybackWorker(true);
    }
    playback_panel_->resetStats();
    playback_fullscreen_panel_->resetStats();
    playback_panel_->setStatus("Stopped");
    playback_fullscreen_panel_->setStatus("Stopped");
    playback_panel_->setTitle("PLAYBACK · 채널/시각 미선택");
    playback_fullscreen_panel_->setTitle("PLAYBACK");
    playback_position_label_->setText("--:--:-- / --:--:--");
    playback_fullscreen_position_label_->setText(
        "--:--:-- / --:--:--");
    playback_position_slider_->setEnabled(false);
    playback_fullscreen_position_slider_->setEnabled(false);
    playback_timeline_->clearTimeline();
    updatePlaybackControlState();
}

void MainWindow::resetControlSessionDisplay() {
    has_last_device_status_ = false;
    has_last_storage_status_ = false;
    device_status_last_updated_at_ = QDateTime();
    storage_last_updated_at_ = QDateTime();
    last_server_boot_id_.clear();
    server_time_interpolation_active_ = false;
    server_time_base_utc_ms_ = 0;
    server_time_sync_state_.clear();
    if (system_device_value_label_) {
        system_device_value_label_->setText("상태 대기");
        system_device_value_label_->setProperty("severity", "warning");
        refreshStyle(system_device_value_label_);
    }
    if (system_device_caption_label_) {
        system_device_caption_label_->setText(
            "Control API 연결 후 서버 상태를 표시합니다.");
    }
    if (server_time_value_label_) {
        server_time_value_label_->setText("Control API 연결 후 확인");
    }
    if (system_metrics_value_label_) {
        system_metrics_value_label_->setText("상태 대기");
    }
    if (system_memory_value_label_) {
        system_memory_value_label_->setText("상태 대기");
    }
    if (system_uptime_value_label_) {
        system_uptime_value_label_->setText("상태 대기");
    }
    if (system_throttling_value_label_) {
        system_throttling_value_label_->setText("상태 대기");
    }
    if (system_reboot_notice_label_) {
        system_reboot_notice_label_->clear();
        system_reboot_notice_label_->setVisible(false);
    }
    if (server_time_label_) {
        server_time_label_->setText("서버 시각 · 연결 후 확인");
    }
    if (storage_device_value_label_) {
        storage_device_value_label_->setText("상태 대기");
        storage_device_value_label_->setProperty("severity", "warning");
        refreshStyle(storage_device_value_label_);
    }
    if (storage_device_caption_label_) {
        storage_device_caption_label_->setText(
            "Control API 연결 후 실제 저장소 상태를 표시합니다.");
    }
    if (storage_alert_label_) {
        storage_alert_label_->clear();
        storage_alert_label_->setVisible(false);
    }
}

void MainWindow::logout() {
    // credential_failure/session_expired와 같은 "인증 안 된 상태"로
    // 되돌리는 경로를 그대로 재사용한다 — 재생·타임라인 등 보호된 화면들이
    // 이미 이 상태 조합(SettingsRequired)에서 "재연결 필요" 안내를 보여준다.
    //
    // clearSession()만으로는 토큰만 지워질 뿐 isConfigured()는 여전히
    // true라서, "다시 시도" 류 버튼들이 예전 비밀번호로 조용히 재인증해
    // 버린다. deconfigure()로 완전히 초기화해서 설정 탭에서 "Control API
    // 적용"을 다시 눌러야만 재인증되게 한다.
    playback_api_->deconfigure();
    playback_login_ever_succeeded_ = false;
    playback_login_in_progress_ = false;
    device_status_retry_after_login_ = false;
    clearProtectedPlaybackState();
    if (event_api_client_) {
        event_api_client_->stopPolling();
    }
    if (stm_device_list_widget_) {
        stm_device_list_widget_->stopPolling();
    }
    // 지난 세션에서 받아둔 화면 내용을 남기면 다시 로그인했을 때 "이미
    // 인증된 상태"처럼 보인다 — 실제로는 토큰이 없어 아무 요청도 못 하는데
    // Pi 상태 배지·이벤트 목록만 그대로 떠 있기 때문이다.
    resetControlSessionDisplay();
    if (event_store_) {
        event_store_->clear();
        ignored_critical_ids_.clear();
        pin_baseline_established_ = false;
        pin_baseline_event_id_ = 0;
        updateLiveEventsPanel();
        refreshEventsTable();
    }
    // setDeviceControlError()·setControlUiState()는 has_last_device_status_를
    // 내린 뒤에 불러야 한다 — 그 전에 부르면 "캐시가 있으니 배지 유지" 분기로
    // 빠져 옛 상태가 남는다.
    setDeviceControlError("로그아웃했습니다.");
    setControlUiState(ControlUiState::SettingsRequired,
                      "로그아웃했습니다. 설정에서 Control API를 다시 적용해야 연결됩니다.");
    if (control_feedback_label_) {
        control_feedback_label_->setText(
            "로그아웃했습니다. 비밀번호를 다시 입력하고 Control API 적용을 눌러 재인증하세요.");
        control_feedback_label_->setProperty("severity", "warning");
        refreshStyle(control_feedback_label_);
    }
    if (login_page_) {
        login_page_->clearPassword();
        login_page_->setFeedback(QString(), QString());
    }
    if (root_stack_) {
        root_stack_->setCurrentIndex(0);
    }
}

void MainWindow::applyControlSettings() {
    clearProtectedPlaybackState();
    // 이 함수는 설정 탭의 "Control API 적용"에서만 호출된다 — 로그인
    // 화면의 로그인 버튼은 화면 전환만 할 뿐 이 함수를 부르지 않는다.
    // 그래서 실패 피드백은 설정 탭의 control_feedback_label_ 하나로 충분하다.
    const auto reportFailure = [this](const QString &message) {
        control_feedback_label_->setText(message);
        control_feedback_label_->setProperty("severity", "critical");
        refreshStyle(control_feedback_label_);
    };
    QUrl normalized;
    QString error;
    if (!normalizeHttpsBaseUrl(control_base_url_edit_->text(),
                               &normalized,
                               &error)) {
        reportFailure(error);
        return;
    }

    const QString certificate_path =
        QDir::cleanPath(control_certificate_edit_->text().trimmed());
    if (!playback_api_->configure(normalized,
                                  "operator",
                                  login_page_ ? login_page_->password() : QString(),
                                  certificate_path,
                                  &error)) {
        reportFailure(error);
        return;
    }

    applied_control_base_url_ = normalized;
    control_certificate_path_ = certificate_path;
    resetControlSessionDisplay();
    if (server_time_label_) {
        server_time_label_->setText("서버 시각 · 연결 중");
    }
    control_base_url_edit_->setText(normalized.toString());
    control_certificate_edit_->setText(certificate_path);
    control_feedback_label_->setText("TLS 검증 후 로그인 중...");
    control_feedback_label_->setProperty("severity", "warning");
    refreshStyle(control_feedback_label_);
    setPlaybackFeedback("Control API 로그인 중...", "warning");
    playback_login_ever_succeeded_ = false;
    setControlUiState(ControlUiState::Connecting);
    playback_login_in_progress_ = true;
    pending_playback_api_action_ = kPlaybackApiActionNone;
    playback_api_->login();
}

void MainWindow::searchPlaybackTimeline() {
    if (!playback_api_->isConfigured() ||
        !playback_login_ever_succeeded_) {
        refreshPlaybackAuthUi(false);
        setPlaybackFeedback(
            "설정에서 Control HTTPS URL, 비밀번호, server.crt를 먼저 적용하세요.",
            "critical");
        return;
    }
    QDateTime pivot_display(playback_pivot_date_,
                            playback_pivot_time_edit_->time(),
                            QTimeZone("Asia/Seoul"));
    const QDateTime server_now = currentServerDisplayTime();
    if (!server_now.isValid()) {
        setPlaybackFeedback(
            "서버 시각을 받은 뒤 Timeline을 조회할 수 있습니다.",
            "warning");
        return;
    }
    if (pivot_display > server_now) {
        pivot_display = server_now;
        setPlaybackPivotDate(server_now.date());
        playback_pivot_time_edit_->setTime(server_now.time());
    }
    const qint64 pivot_utc_ms =
        pivot_display.toMSecsSinceEpoch();
    const qint64 duration_ms =
        playback_range_combo_->currentData().toLongLong() * 1000;
    playback_query_pivot_utc_ms_ = pivot_utc_ms;
    timeline_visible_duration_ms_ = duration_ms;
    timeline_fetch_span_ms_ =
        qMin<qint64>(duration_ms * 3, 24LL * 60 * 60 * 1000);
    const qint64 fetch_start =
        pivot_utc_ms - timeline_fetch_span_ms_ / 3;
    const qint64 fetch_end = fetch_start + timeline_fetch_span_ms_;
    playback_selection_start_utc_ms_ = 0;
    playback_selection_end_utc_ms_ = 0;
    playback_range_start_utc_ms_ = 0;
    playback_range_end_utc_ms_ = 0;
    playback_current_utc_ms_ = 0;
    playback_play_button_->setEnabled(false);
    playback_pause_button_->setEnabled(false);
    playback_position_slider_->setEnabled(false);
    playback_fullscreen_toggle_button_->setEnabled(false);
    playback_fullscreen_stop_button_->setEnabled(false);
    playback_fullscreen_position_slider_->setEnabled(false);
    requestTimelineWindow(fetch_start, fetch_end, true);
}

void MainWindow::requestTimelineWindow(qint64 start_utc_ms,
                                       qint64 end_utc_ms,
                                       bool reset_cache,
                                       int direction) {
    if (timeline_request_in_progress_ ||
        end_utc_ms <= start_utc_ms) {
        return;
    }
    timeline_request_in_progress_ = true;
    timeline_request_resets_cache_ = reset_cache;
    timeline_pending_direction_ = direction;
    timeline_pending_start_utc_ms_ = start_utc_ms;
    timeline_pending_end_utc_ms_ = end_utc_ms;
    timeline_requests_completed_ = 0;
    timeline_requests_succeeded_ = 0;
    timeline_requests_failed_ = 0;
    playback_timelines_received_ = 0;
    playback_query_start_utc_ms_ = start_utc_ms;
    playback_query_end_utc_ms_ = end_utc_ms;

    if (reset_cache) {
        timeline_cache_start_utc_ms_ = start_utc_ms;
        timeline_cache_end_utc_ms_ = end_utc_ms;
        playback_timeline_->clearTimeline();
        playback_timeline_->setQueryRange(start_utc_ms, end_utc_ms);
        const qint64 visible_start =
            playback_query_pivot_utc_ms_ -
            timeline_visible_duration_ms_ / 3;
        playback_timeline_->setVisibleRange(
            visible_start,
            visible_start + timeline_visible_duration_ms_);
        playback_timeline_->setPivotTime(
            playback_query_pivot_utc_ms_);
        playback_timeline_->beginChannelQuery(true);
        playback_query_controls_->setEnabled(false);
        playback_selection_label_->setText(
            "Timeline 조회 중 · 0/4채널");
    }
    performPlaybackApiAction(kPlaybackApiActionTimeline);
}

void MainWindow::finalizeTimelineWindowRequest() {
    if (!timeline_request_in_progress_) {
        return;
    }
    const bool reset_cache = timeline_request_resets_cache_;
    const int direction = timeline_pending_direction_;
    timeline_request_in_progress_ = false;

    if (!reset_cache && timeline_requests_succeeded_ > 0) {
        timeline_cache_start_utc_ms_ =
            qMin(timeline_cache_start_utc_ms_,
                 timeline_pending_start_utc_ms_);
        timeline_cache_end_utc_ms_ =
            qMax(timeline_cache_end_utc_ms_,
                 timeline_pending_end_utc_ms_);
        const qint64 maximum_cache =
            qMax(timeline_fetch_span_ms_,
                 timeline_fetch_span_ms_ * 3);
        if (timeline_cache_end_utc_ms_ -
                timeline_cache_start_utc_ms_ >
            maximum_cache) {
            if (direction > 0) {
                timeline_cache_start_utc_ms_ =
                    timeline_cache_end_utc_ms_ - maximum_cache;
            } else {
                timeline_cache_end_utc_ms_ =
                    timeline_cache_start_utc_ms_ + maximum_cache;
            }
            playback_timeline_->pruneToRange(
                timeline_cache_start_utc_ms_,
                timeline_cache_end_utc_ms_);
        }
        playback_timeline_->extendQueryRange(
            timeline_cache_start_utc_ms_,
            timeline_cache_end_utc_ms_);
    }

    refreshPlaybackAuthUi(true);
    playback_timeline_navigator_->setEnabled(
        timeline_cache_end_utc_ms_ -
                timeline_cache_start_utc_ms_ >
            timeline_visible_duration_ms_);
    updateTimelineNavigator(
        playback_timeline_->visibleStartUtcMs(),
        playback_timeline_->visibleEndUtcMs(),
        playback_timeline_->visibleStartUtcMs() +
            (playback_timeline_->visibleEndUtcMs() -
             playback_timeline_->visibleStartUtcMs()) /
                3);

    const QString result =
        timeline_requests_failed_ == 0
            ? QString("4채널 조회 완료")
            : QString("%1 성공 · %2 실패")
                  .arg(timeline_requests_succeeded_)
                  .arg(timeline_requests_failed_);
    if (reset_cache) {
        playback_selection_label_->setText(
            result +
            " · 녹화 bar를 클릭하면 미리보기, 더블클릭하면 재생합니다.");
    }
    setPlaybackFeedback(
        reset_cache ? QString("Timeline %1").arg(result)
                    : QString("인접 Timeline %1").arg(result),
        timeline_requests_failed_ == 0 ? "ok" : "warning");
}

void MainWindow::scheduleTimelinePrefetch() {
    if (timeline_request_in_progress_ ||
        !playback_login_ever_succeeded_ ||
        timeline_fetch_span_ms_ <= 0 ||
        timeline_cache_end_utc_ms_ <=
            timeline_cache_start_utc_ms_) {
        return;
    }
    const qint64 visible_start =
        playback_timeline_->visibleStartUtcMs();
    const qint64 visible_end =
        playback_timeline_->visibleEndUtcMs();
    const qint64 visible_duration =
        visible_end - visible_start;
    const qint64 threshold = qMax<qint64>(1000,
                                          visible_duration / 2);
    if (visible_start - timeline_cache_start_utc_ms_ <=
        threshold) {
        requestTimelineWindow(
            timeline_cache_start_utc_ms_ -
                timeline_fetch_span_ms_,
            timeline_cache_start_utc_ms_,
            false,
            -1);
    } else if (timeline_cache_end_utc_ms_ - visible_end <=
               threshold) {
        requestTimelineWindow(
            timeline_cache_end_utc_ms_,
            timeline_cache_end_utc_ms_ +
                timeline_fetch_span_ms_,
            false,
            1);
    }
}

void MainWindow::updateTimelineNavigator(qint64 visible_start_utc_ms,
                                         qint64 visible_end_utc_ms,
                                         qint64 pivot_utc_ms) {
    if (visible_end_utc_ms <= visible_start_utc_ms) {
        return;
    }
    timeline_visible_duration_ms_ =
        visible_end_utc_ms - visible_start_utc_ms;
    playback_query_pivot_utc_ms_ = pivot_utc_ms;
    const QDateTime pivot_local =
        QDateTime::fromMSecsSinceEpoch(pivot_utc_ms,
                                       QTimeZone::UTC)
            .toTimeZone(QTimeZone("Asia/Seoul"));
    setPlaybackPivotDate(pivot_local.date());
    playback_pivot_time_edit_->setTime(pivot_local.time());

    const qint64 travel =
        qMax<qint64>(0,
                     timeline_cache_end_utc_ms_ -
                         timeline_cache_start_utc_ms_ -
                         timeline_visible_duration_ms_);
    playback_timeline_navigator_->setEnabled(travel > 0);
    const int value =
        travel > 0
            ? static_cast<int>(
                  (visible_start_utc_ms -
                   timeline_cache_start_utc_ms_) *
                  playback_timeline_navigator_->maximum() / travel)
            : 0;
    timeline_navigator_updating_ = true;
    playback_timeline_navigator_->setValue(
        qBound(playback_timeline_navigator_->minimum(),
               value,
               playback_timeline_navigator_->maximum()));
    timeline_navigator_updating_ = false;
}

void MainWindow::playSelectedRange() {
    if (playback_selection_end_utc_ms_ <=
        playback_selection_start_utc_ms_) {
        setPlaybackFeedback("재생할 녹화 구간을 먼저 선택하세요.", "critical");
        return;
    }
    if (playback_ui_state_ == PlaybackUiState::Pausing &&
        playback_worker_) {
        playback_paused_ = false;
        pending_playback_api_action_ =
            kPlaybackApiActionCreateSession;
        setPlaybackUiState(PlaybackUiState::Starting);
        setPlaybackFeedback(
            "선택 시각부터 재생을 준비하는 중…", "warning");
        stopPlaybackWorker(true);
        return;
    }
    if (playback_ui_state_ == PlaybackUiState::Starting ||
        playback_ui_state_ == PlaybackUiState::Stopping) {
        setPlaybackFeedback("재생 상태를 전환하는 중입니다.", "warning");
        return;
    }
    playback_paused_ = false;
    setPlaybackUiState(PlaybackUiState::Starting);
    if (playback_worker_) {
        pending_playback_api_action_ = kPlaybackApiActionCreateSession;
        setPlaybackFeedback("기존 one-shot 재생을 종료하는 중...", "warning");
        stopPlaybackWorker(true);
        return;
    }
    performPlaybackApiAction(kPlaybackApiActionCreateSession);
}

void MainWindow::togglePlaybackPause() {
    if (playback_ui_state_ == PlaybackUiState::Starting ||
        playback_ui_state_ == PlaybackUiState::Pausing ||
        playback_ui_state_ == PlaybackUiState::Stopping) {
        setPlaybackFeedback("재생 상태를 전환하는 중입니다.", "warning");
        return;
    }
    if (playback_worker_ &&
        playback_ui_state_ == PlaybackUiState::Playing) {
        pending_playback_api_action_ = kPlaybackApiActionNone;
        qint64 pause_utc_ms =
            playback_current_utc_ms_ > 0
                ? playback_current_utc_ms_
                : playback_selection_start_utc_ms_;
        if (playback_range_end_utc_ms_ >
            playback_range_start_utc_ms_) {
            pause_utc_ms =
                qBound(playback_range_start_utc_ms_,
                       pause_utc_ms,
                       playback_range_end_utc_ms_ - 1);
        }
        playback_selection_start_utc_ms_ = pause_utc_ms;
        if (playback_range_end_utc_ms_ > pause_utc_ms) {
            playback_selection_end_utc_ms_ =
                playback_range_end_utc_ms_;
        }
        playback_current_utc_ms_ = pause_utc_ms;
        playback_paused_ = true;
        setPlaybackUiState(PlaybackUiState::Pausing);
        updatePlaybackPosition(pause_utc_ms);
        setPlaybackFeedback(
            "일시정지 · 재개 시 현재 시각부터 새 session을 연결합니다.",
            "warning");
        stopPlaybackWorker(false);
        return;
    }
    if (playback_ui_state_ == PlaybackUiState::Paused ||
        playback_ui_state_ == PlaybackUiState::Preview ||
        playback_ui_state_ == PlaybackUiState::Error ||
        playback_selection_end_utc_ms_ >
            playback_selection_start_utc_ms_) {
        playback_paused_ = false;
        playSelectedRange();
    }
}

void MainWindow::seekPlaybackBy(qint64 delta_ms) {
    if (playback_selection_end_utc_ms_ <=
        playback_selection_start_utc_ms_) {
        return;
    }
    const qint64 base =
        playback_seek_timer_->isActive() && pending_seek_utc_ms_ > 0
            ? pending_seek_utc_ms_
            : (playback_current_utc_ms_ > 0
                   ? playback_current_utc_ms_
                   : playback_selection_start_utc_ms_);
    pending_seek_utc_ms_ = base + delta_ms;
    updatePlaybackPosition(pending_seek_utc_ms_);
    setPlaybackFeedback("탐색 위치 선택 중...", "warning");
    playback_seek_timer_->start();
}

void MainWindow::seekPlaybackTo(qint64 utc_ms) {
    const bool resume_after_seek = playback_worker_ || playback_paused_;
    if (playback_range_end_utc_ms_ >
        playback_range_start_utc_ms_) {
        utc_ms = qBound(playback_range_start_utc_ms_,
                        utc_ms,
                        playback_range_end_utc_ms_ - 1);
    }
    playback_seek_in_progress_ = true;
    const bool selected =
        playback_timeline_->selectTime(playback_channel_id_, utc_ms);
    playback_seek_in_progress_ = false;
    if (!selected) {
        setPlaybackFeedback("해당 시각에는 녹화 영상이 없습니다.", "warning");
        return;
    }
    if (resume_after_seek) {
        playSelectedRange();
    }
}

void MainWindow::updatePlaybackPosition(qint64 utc_ms) {
    playback_current_utc_ms_ = utc_ms;
    const qint64 range_start = playback_range_start_utc_ms_;
    const qint64 range_end = playback_range_end_utc_ms_;

    const auto time_text = [](qint64 value) {
        if (value <= 0) {
            return QString("--:--:--");
        }
        return QDateTime::fromMSecsSinceEpoch(value, QTimeZone::UTC)
            .toTimeZone(QTimeZone("Asia/Seoul"))
            .toString("HH:mm:ss");
    };
    playback_position_label_->setText(
        QString("%1 / %2").arg(time_text(utc_ms), time_text(range_end)));

    if (!playback_slider_dragging_ && range_end > range_start) {
        const qint64 clamped = qBound(range_start, utc_ms, range_end);
        playback_position_slider_->setValue(
            static_cast<int>((clamped - range_start) *
                             playback_position_slider_->maximum() /
                             (range_end - range_start)));
        playback_fullscreen_position_slider_->setValue(
            playback_position_slider_->value());
    }
    playback_fullscreen_position_label_->setText(
        playback_position_label_->text());
}

bool MainWindow::playbackShortcutAllowed() const {
    if (!page_stack_ || page_stack_->currentIndex() != 1) {
        return false;
    }
    QWidget *focus = QApplication::focusWidget();
    if (!focus) {
        return true;
    }
    return !focus->inherits("QLineEdit") &&
           !focus->inherits("QComboBox") &&
           !focus->inherits("QAbstractSpinBox");
}

void MainWindow::togglePlaybackFullscreen() {
    if (!playback_fullscreen_window_) {
        return;
    }
    if (playback_fullscreen_window_->isVisible()) {
        const QImage current =
            playback_fullscreen_panel_->currentImage();
        if (!current.isNull()) {
            playback_panel_->setPreviewImage(current);
        }
        playback_fullscreen_window_->hide();
        if (playback_worker_) {
            playback_worker_->setOutputSize(
                playback_panel_->videoSurfaceSize());
        }
        playback_fullscreen_button_->setText("⛶ 전체화면");
        show();
        raise();
        activateWindow();
    } else {
        if (!page_stack_ || page_stack_->currentIndex() != 1) {
            return;
        }
        playback_fullscreen_panel_->setTitle(
            QString("PLAYBACK · CH %1").arg(playback_channel_id_));
        const QImage current = playback_panel_->currentImage();
        if (!current.isNull()) {
            playback_fullscreen_panel_->setPreviewImage(current);
        }
        playback_fullscreen_window_->showFullScreen();
        playback_fullscreen_window_->raise();
        playback_fullscreen_window_->activateWindow();
        QTimer::singleShot(0, this, [this]() {
            if (playback_worker_) {
                playback_worker_->setOutputSize(
                    playback_fullscreen_panel_->videoSurfaceSize());
            }
        });
        playback_fullscreen_button_->setText("전체화면 종료");
    }
}

void MainWindow::performPlaybackApiAction(int action) {
    if (action == kPlaybackApiActionNone) {
        return;
    }
    if (!playback_api_->isConfigured()) {
        playback_search_button_->setEnabled(true);
        playback_play_button_->setEnabled(
            playback_selection_end_utc_ms_ >
            playback_selection_start_utc_ms_);
        setPlaybackFeedback(
            "설정에서 Control HTTPS URL, 비밀번호, server.crt를 적용하세요.",
            "critical");
        setPlaybackUiState(PlaybackUiState::Error);
        return;
    }
    if (!playback_api_->hasValidAccessToken()) {
        pending_playback_api_action_ = action;
        if (!playback_login_in_progress_) {
            playback_login_in_progress_ = true;
            setPlaybackFeedback("Control API 로그인 중...", "warning");
            playback_api_->login();
        }
        return;
    }

    pending_playback_api_action_ = kPlaybackApiActionNone;
    if (action == kPlaybackApiActionTimeline) {
        if (activeLiveWorkerCount() > 0 || stopping_streams_) {
            pending_playback_api_action_ = action;
            setPlaybackFeedback(
                "라이브 연결 종료 후 Timeline을 조회합니다...", "warning");
            return;
        }
        setPlaybackFeedback("4채널 Timeline 병렬 조회 중...", "warning");
        for (int channel_id = 1; channel_id <= 4; ++channel_id) {
            playback_api_->requestTimeline(channel_id,
                                           playback_query_start_utc_ms_,
                                           playback_query_end_utc_ms_);
        }
    } else if (action == kPlaybackApiActionCreateSession) {
        setPlaybackFeedback("one-shot playback session 생성 중...", "warning");
        playback_api_->createPlaybackSession(
            playback_channel_id_,
            playback_selection_start_utc_ms_,
            playback_selection_end_utc_ms_);
    } else if (action == kPlaybackApiActionThumbnail) {
        setPlaybackFeedback("선택 시각 미리보기 요청 중...", "warning");
        playback_api_->requestThumbnail(playback_channel_id_,
                                        playback_selection_start_utc_ms_,
                                        960);
    }
}

void MainWindow::startPlaybackWorker(const PlaybackSession &session) {
    if (playback_worker_) {
        playback_api_->deletePlaybackSession(session.playback_session_id);
        setPlaybackFeedback("기존 playback worker가 아직 종료되지 않았습니다.",
                            "critical");
        setPlaybackUiState(PlaybackUiState::Playing);
        return;
    }

    QUrl playback_url(applied_base_url_);
    if (!playback_url.isValid() ||
        playback_url.scheme().compare("rtsps", Qt::CaseInsensitive) != 0 ||
        playback_url.host().isEmpty()) {
        playback_api_->deletePlaybackSession(session.playback_session_id);
        setPlaybackFeedback("적용된 RTSPS Base URL이 올바르지 않습니다.",
                            "critical");
        setPlaybackUiState(PlaybackUiState::Error);
        return;
    }
    playback_url.setPath(session.rtsps_path);
    playback_url.setQuery(QString());
    playback_url.setFragment(QString());

    active_playback_session_id_ = session.playback_session_id;
    active_playback_start_utc_ms_ = session.start_utc_ms;
    active_playback_end_utc_ms_ = session.end_utc_ms;
    playback_current_utc_ms_ = session.start_utc_ms;
    playback_paused_ = false;
    setPlaybackUiState(PlaybackUiState::Starting);
    playback_stop_clears_video_ = false;
    playback_panel_->resetStats();
    playback_panel_->setTitle(
        QString("PLAYBACK · CH %1 · %2")
            .arg(session.channel_id)
            .arg(QDateTime::fromMSecsSinceEpoch(
                     session.start_utc_ms, QTimeZone::UTC)
                     .toTimeZone(QTimeZone("Asia/Seoul"))
                     .toString("yyyy-MM-dd HH:mm:ss")));
    playback_fullscreen_panel_->setTitle(
        QString("PLAYBACK · CH %1").arg(session.channel_id));
    playback_panel_->setStatus("Connecting");
    playback_fullscreen_panel_->setStatus("Connecting");
    playback_stop_button_->setEnabled(true);
    playback_pause_button_->setEnabled(true);
    playback_position_slider_->setEnabled(true);
    playback_fullscreen_toggle_button_->setEnabled(true);
    playback_fullscreen_stop_button_->setEnabled(true);
    playback_fullscreen_position_slider_->setEnabled(true);
    updatePlaybackPosition(session.start_utc_ms);
    playback_search_button_->setEnabled(true);
    setPlaybackFeedback(
        QString("재생 연결 중 · %1 · %2개 segment")
            .arg(localRangeText(session.start_utc_ms, session.end_utc_ms))
            .arg(session.segment_count),
        "warning");

    auto *worker = new StreamWorker(session.channel_id - 1,
                                    playback_url.toString(QUrl::FullyEncoded),
                                    this,
                                    true);
    playback_worker_ = worker;
    updatePlaybackControlState();
    worker->setOutputSize(playback_panel_->videoSurfaceSize());
    connect(playback_panel_,
            &VideoPanel::renderSizeChanged,
            worker,
            [this, worker](const QSize &size) {
                if (!playback_fullscreen_window_->isVisible()) {
                    worker->setOutputSize(size);
                }
            });
    connect(playback_fullscreen_panel_,
            &VideoPanel::renderSizeChanged,
            worker,
            [this, worker](const QSize &size) {
                if (playback_fullscreen_window_->isVisible()) {
                    worker->setOutputSize(size);
                }
            });
    connect(worker,
            &StreamWorker::frameReady,
            this,
            [this, worker](const QImage &image, qint64 queued_at_ms) {
                VideoPanel *target =
                    playback_fullscreen_window_->isVisible()
                        ? playback_fullscreen_panel_
                        : playback_panel_;
                target->setFrame(image, queued_at_ms);
                worker->markFrameConsumed();
            },
            Qt::QueuedConnection);
    connect(worker,
            &StreamWorker::statsReady,
            this,
            [this](const StreamStats &stats) {
                VideoPanel *target =
                    playback_fullscreen_window_->isVisible()
                        ? playback_fullscreen_panel_
                        : playback_panel_;
                target->setStreamStats(stats);
            },
            Qt::QueuedConnection);
    connect(worker,
            &StreamWorker::playbackPositionChanged,
            this,
            [this](qint64 elapsed_ms) {
                if (playback_seek_timer_->isActive()) {
                    return;
                }
                updatePlaybackPosition(
                    qMin(active_playback_end_utc_ms_,
                         active_playback_start_utc_ms_ + elapsed_ms));
            },
            Qt::QueuedConnection);
    connect(worker,
            &StreamWorker::statusChanged,
            this,
            [this](const QString &status) {
                playback_panel_->setStatus(status);
                playback_fullscreen_panel_->setStatus(status);
                qInfo().noquote()
                    << QString("[playback] status=%1").arg(status);
                if (status.startsWith("Playing")) {
                    setPlaybackUiState(PlaybackUiState::Playing);
                    setPlaybackFeedback("녹화 영상 재생 중", "ok");
                } else if (status == "PlaybackEnded") {
                    playback_paused_ = true;
                    setPlaybackUiState(PlaybackUiState::Paused);
                    setPlaybackFeedback("선택 구간 재생 완료", "ok");
                } else if (status.contains("failed", Qt::CaseInsensitive) ||
                           status.contains("error", Qt::CaseInsensitive) ||
                           status.contains("timeout", Qt::CaseInsensitive) ||
                           status.startsWith("read ended")) {
                    setPlaybackUiState(PlaybackUiState::Error);
                    setPlaybackFeedback(
                        QString("VOD 재생 실패: %1").arg(status), "critical");
                }
            },
            Qt::QueuedConnection);
    connect(worker, &StreamWorker::finished, this, [this, worker]() {
        if (playback_worker_ != worker) {
            worker->deleteLater();
            return;
        }
        playback_worker_ = nullptr;
        active_playback_session_id_.clear();
        playback_stop_button_->setEnabled(false);
        playback_pause_button_->setEnabled(false);
        playback_fullscreen_stop_button_->setEnabled(false);
        playback_search_button_->setEnabled(true);
        playback_play_button_->setEnabled(
            playback_selection_end_utc_ms_ >
            playback_selection_start_utc_ms_);
        if (playback_stop_clears_video_) {
            playback_panel_->resetStats();
            playback_fullscreen_panel_->resetStats();
            playback_panel_->setStatus("Stopped");
            playback_fullscreen_panel_->setStatus("Stopped");
            playback_position_slider_->setEnabled(false);
            playback_fullscreen_position_slider_->setEnabled(false);
            setPlaybackUiState(PlaybackUiState::Preview);
        } else if (playback_paused_) {
            playback_panel_->clearStreamMetrics();
            playback_fullscreen_panel_->clearStreamMetrics();
            playback_panel_->setStatus("Paused");
            playback_fullscreen_panel_->setStatus("Paused");
            setPlaybackUiState(PlaybackUiState::Paused);
        } else {
            playback_panel_->clearStreamMetrics();
            playback_fullscreen_panel_->clearStreamMetrics();
            if (playback_ui_state_ != PlaybackUiState::Error) {
                setPlaybackUiState(PlaybackUiState::Preview);
            } else {
                updatePlaybackControlState();
            }
        }
        worker->deleteLater();

        const int action = pending_playback_api_action_;
        if (action != kPlaybackApiActionNone && !shutting_down_) {
            QTimer::singleShot(0, this, [this, action]() {
                if (pending_playback_api_action_ == action) {
                    performPlaybackApiAction(action);
                }
            });
        }
    });
    worker->start();
}

void MainWindow::stopPlaybackWorker(bool clear_video) {
    playback_stop_clears_video_ = clear_video;
    if (!active_playback_session_id_.isEmpty()) {
        playback_api_->deletePlaybackSession(active_playback_session_id_);
        active_playback_session_id_.clear();
    }
    if (playback_worker_) {
        playback_stop_button_->setEnabled(false);
        playback_worker_->stop();
    } else if (clear_video) {
        playback_panel_->resetStats();
        playback_fullscreen_panel_->resetStats();
        playback_panel_->setStatus("Stopped");
        playback_fullscreen_panel_->setStatus("Stopped");
    }
}

void MainWindow::stopPlayback() {
    pending_playback_api_action_ = kPlaybackApiActionNone;
    playback_seek_timer_->stop();
    pending_seek_utc_ms_ = 0;
    playback_paused_ = false;
    setPlaybackUiState(PlaybackUiState::Stopping);
    setPlaybackFeedback("재생을 중지하는 중...", "warning");
    stopPlaybackWorker(true);
    if (!playback_worker_) {
        setPlaybackUiState(PlaybackUiState::Preview);
    }
}

void MainWindow::setPlaybackFeedback(const QString &text,
                                     const QString &severity) {
    if (!playback_feedback_label_) {
        return;
    }
    playback_feedback_label_->setText(text);
    playback_feedback_label_->setProperty("severity", severity);
    refreshStyle(playback_feedback_label_);
}

void MainWindow::setBaseUrlFeedback(const QString &text, const QString &severity) {
    // "적용 상태" 행을 없애면서 base URL 적용 피드백은 설정 카드의
    // "연결 상태"(control_feedback_label_) 한 곳으로 합쳤다.
    if (!control_feedback_label_) {
        return;
    }
    control_feedback_label_->setText(text);
    control_feedback_label_->setProperty("severity", severity);
    refreshStyle(control_feedback_label_);
}

void MainWindow::startStreams() {
    live_desired_running_ = true;
    cancelAllChannelRetries(true);
    if (stopping_streams_ && activeLiveWorkerCount() == 0) {
        stopping_streams_ = false;
    }
    if (activeLiveWorkerCount() > 0) {
        restart_after_stop_ = true;
        stopping_streams_ = true;
        requestWorkerStop();
        start_button_->setText("다시 연결 중");
        start_button_->setEnabled(false);
        stop_button_->setEnabled(false);
        refreshConnectionSummary();
        return;
    }
    if (applied_base_url_.isEmpty()) {
        live_desired_running_ = false;
        connection_badge_->setText("URL 확인 필요");
        connection_badge_->setProperty("severity", "critical");
        refreshStyle(connection_badge_);
        return;
    }

    restart_after_stop_ = false;
    stopping_streams_ = false;
    start_button_->setText("연결 중");
    start_button_->setEnabled(false);
    stop_button_->setEnabled(true);
    for (int channel = 0; channel < 4; ++channel) {
        startChannelStream(channel);
    }
    start_button_->setText("다시 연결");
    start_button_->setEnabled(true);
    refreshConnectionSummary();
}

void MainWindow::stopStreams() {
    live_desired_running_ = false;
    restart_after_stop_ = false;
    cancelAllChannelRetries(true);
    if (activeLiveWorkerCount() == 0) {
        stopping_streams_ = false;
        finalizeStoppedState();
        return;
    }
    stopping_streams_ = true;
    requestWorkerStop();
    start_button_->setText("중지 중");
    start_button_->setEnabled(false);
    stop_button_->setEnabled(false);
    refreshConnectionSummary();
}

void MainWindow::requestWorkerStop() {
    for (auto *worker : workers_) {
        if (worker) {
            worker->stop();
        }
    }
}

void MainWindow::finalizeStoppedState() {
    cancelAllChannelRetries(true);
    for (int channel = 0; channel < panels_.size(); ++channel) {
        panels_[channel]->resetStats();
        handleChannelStatus(channel, "Stopped");
    }
    start_button_->setText("스트림 시작");
    start_button_->setEnabled(true);
    stop_button_->setEnabled(false);
}

int MainWindow::activeLiveWorkerCount() const {
    int count = 0;
    for (const auto *worker : workers_) {
        if (worker) {
            ++count;
        }
    }
    return count;
}

bool MainWindow::hasPendingLiveRetries() const {
    for (const auto *timer : channel_retry_timers_) {
        if (timer && timer->isActive()) {
            return true;
        }
    }
    return false;
}

void MainWindow::startChannelStream(int channel) {
    if (channel < 0 || channel >= workers_.size() ||
        channel >= panels_.size() || workers_[channel] ||
        !live_desired_running_ || stopping_streams_ || shutting_down_) {
        return;
    }

    channel_retry_timers_[channel]->stop();
    panels_[channel]->resetStats();
    handleChannelStatus(
        channel,
        channel_retry_attempts_[channel] > 0
            ? QString("RetryConnecting")
            : QString("Connecting"));

    auto *worker = new StreamWorker(channel, channelUrl(channel), this);
    workers_[channel] = worker;
    worker->setOutputSize(panels_[channel]->videoSurfaceSize());
    connect(panels_[channel],
            &VideoPanel::renderSizeChanged,
            worker,
            [worker](const QSize &size) {
                worker->setOutputSize(size);
            });
    connect(worker,
            &StreamWorker::frameReady,
            this,
            [this, channel, panel = panels_[channel], worker](
                const QImage &image,
                qint64 queued_at_ms) {
                if (workers_[channel] != worker) {
                    worker->markFrameConsumed();
                    return;
                }
                panel->setFrame(image, queued_at_ms);
                worker->markFrameConsumed();
            },
            Qt::QueuedConnection);
    connect(worker,
            &StreamWorker::statsReady,
            this,
            [this, channel, worker](const StreamStats &stats) {
                if (workers_[channel] == worker) {
                    panels_[channel]->setStreamStats(stats);
                }
            },
            Qt::QueuedConnection);
    connect(worker,
            &StreamWorker::statusChanged,
            this,
            [this, channel, worker](const QString &status) {
                if (workers_[channel] == worker) {
                    handleChannelStatus(channel, status);
                }
            },
            Qt::QueuedConnection);
    connect(worker,
            &StreamWorker::finished,
            this,
            [this, channel, worker]() {
                handleLiveWorkerFinished(channel, worker);
            });
    worker->start();
}

void MainWindow::handleLiveWorkerFinished(int channel,
                                          StreamWorker *worker) {
    if (channel < 0 || channel >= workers_.size()) {
        worker->deleteLater();
        return;
    }
    if (workers_[channel] != worker) {
        worker->deleteLater();
        return;
    }

    const QString terminal_status = channel_statuses_[channel];
    workers_[channel] = nullptr;
    channel_stable_timers_[channel]->stop();
    worker->deleteLater();

    if (stopping_streams_) {
        if (activeLiveWorkerCount() == 0) {
            const bool should_restart =
                restart_after_stop_ && live_desired_running_ &&
                !shutting_down_;
            finalizeStoppedState();
            stopping_streams_ = false;
            restart_after_stop_ = false;
            if (should_restart) {
                start_button_->setText("다시 연결 중");
                start_button_->setEnabled(false);
                QTimer::singleShot(0, this, &MainWindow::startStreams);
            }
            if (page_stack_->currentIndex() == 1 &&
                pending_playback_api_action_ ==
                    kPlaybackApiActionTimeline) {
                QTimer::singleShot(0, this, [this]() {
                    if (pending_playback_api_action_ ==
                        kPlaybackApiActionTimeline) {
                        performPlaybackApiAction(
                            kPlaybackApiActionTimeline);
                    }
                });
            }
        }
    } else if (live_desired_running_ && !shutting_down_ &&
               terminal_status != "Stopped") {
        scheduleChannelRetry(channel, terminal_status);
    }
    refreshConnectionSummary();
}

void MainWindow::scheduleChannelRetry(int channel,
                                      const QString &reason) {
    if (channel < 0 || channel >= channel_retry_timers_.size() ||
        !live_desired_running_ || stopping_streams_ || shutting_down_ ||
        workers_[channel]) {
        return;
    }

    channel_stable_timers_[channel]->stop();
    const int attempt = ++channel_retry_attempts_[channel];
    const int delay_ms = channelRetryDelayMs(attempt);
    const int display_seconds = qMax(1, qRound(delay_ms / 1000.0));
    handleChannelStatus(
        channel,
        QString("RetryWaiting:%1").arg(display_seconds));
    channel_retry_timers_[channel]->start(delay_ms);
    qInfo().noquote()
        << QString("[live-retry] channel=%1 state=scheduled attempt=%2 delay_ms=%3 reason=%4")
               .arg(channel + 1)
               .arg(attempt)
               .arg(delay_ms)
               .arg(reason);
}

void MainWindow::cancelChannelRetry(int channel, bool reset_attempt) {
    if (channel < 0 || channel >= channel_retry_timers_.size()) {
        return;
    }
    channel_retry_timers_[channel]->stop();
    channel_stable_timers_[channel]->stop();
    if (reset_attempt) {
        channel_retry_attempts_[channel] = 0;
    }
}

void MainWindow::cancelAllChannelRetries(bool reset_attempts) {
    for (int channel = 0; channel < channel_retry_timers_.size(); ++channel) {
        cancelChannelRetry(channel, reset_attempts);
    }
}

int MainWindow::channelRetryDelayMs(int attempt) const {
    static constexpr int kBackoffSeconds[] = {1, 2, 4, 8, 15, 30};
    static constexpr int kBackoffCount =
        sizeof(kBackoffSeconds) / sizeof(kBackoffSeconds[0]);
    const int index = qBound(
        0,
        attempt - 1,
        kBackoffCount - 1);
    const int base_ms = kBackoffSeconds[index] * 1000;
    const int jitter_range = qMax(1, base_ms / 5);
    const int jitter =
        static_cast<int>(QRandomGenerator::global()->bounded(
            static_cast<quint32>(jitter_range + 1))) -
        base_ms / 10;
    return qMax(250, base_ms + jitter);
}

void MainWindow::handleChannelStatus(int channel, const QString &status) {
    if (channel < 0 || channel >= panels_.size()) {
        return;
    }
    QString effective_status = status;
    if (status == "Opening" && channel_retry_attempts_[channel] > 0) {
        effective_status = "RetryConnecting";
    }
    if (channel_statuses_[channel] == effective_status) {
        return;
    }
    channel_statuses_[channel] = effective_status;
    panels_[channel]->setStatus(effective_status);
    if (effective_status.startsWith("Playing")) {
        channel_stable_timers_[channel]->start();
    } else {
        channel_stable_timers_[channel]->stop();
    }
    qInfo().noquote()
        << QString("[ch%1] status=%2")
               .arg(channel + 1)
               .arg(effective_status);
    refreshConnectionSummary();
}

void MainWindow::refreshConnectionSummary() {
    int playing = 0;
    int errors = 0;
    int retrying = 0;
    int retry_channel = -1;
    for (int channel = 0; channel < channel_statuses_.size(); ++channel) {
        const QString &status = channel_statuses_[channel];
        if (status.startsWith("Playing")) {
            ++playing;
        } else if (status.startsWith("Retry")) {
            ++retrying;
            retry_channel = channel;
        } else if (status.contains("failed", Qt::CaseInsensitive) ||
                   status.contains("error", Qt::CaseInsensitive) ||
                   status.contains("timeout", Qt::CaseInsensitive) ||
                   status.startsWith("read ended")) {
            ++errors;
        }
    }

    if (channel_summary_label_) {
        channel_summary_label_->setText(QString("영상 %1/4").arg(playing));
    }
    if (!connection_badge_) {
        return;
    }
    if (stopping_streams_) {
        connection_badge_->setText(restart_after_stop_ ? "재시작 준비 중" : "중지 중");
        connection_badge_->setProperty("severity", "warning");
    } else if (playing == 4) {
        connection_badge_->setText("4채널 연결됨");
        connection_badge_->setProperty("severity", "ok");
    } else if (retrying > 0) {
        connection_badge_->setText(
            retrying == 1 && retry_channel >= 0
                ? QString("CH%1 재연결 중 · %2/4")
                      .arg(retry_channel + 1)
                      .arg(playing)
                : QString("%1채널 재연결 중 · %2/4")
                      .arg(retrying)
                      .arg(playing));
        connection_badge_->setProperty("severity", "warning");
    } else if (errors > 0) {
        connection_badge_->setText(QString("채널 오류 · %1/4").arg(playing));
        connection_badge_->setProperty("severity", "critical");
    } else if (activeLiveWorkerCount() > 0) {
        connection_badge_->setText(QString("연결 중 · %1/4").arg(playing));
        connection_badge_->setProperty("severity", "warning");
    } else {
        connection_badge_->setText("영상 중지됨");
        connection_badge_->setProperty("severity", "warning");
    }
    refreshStyle(connection_badge_);
}

void MainWindow::updateTotalStats() {
    double total_mbps = 0.0;
    for (auto *panel : panels_) {
        total_mbps += panel->lastMbps();
    }
    total_stats_label_->setText(QString("수신 %1 Mbps").arg(total_mbps, 0, 'f', 2));
    updateServerTimeDisplay();
    refreshConnectionSummary();
}

void MainWindow::handleEventUpdatedInStore(const ParkingEventItem &item) {
    Q_UNUSED(item);
    // ACK 변경 등 업데이트 이벤트: 카드 패널과 테이블 전체를 가볍게 재동기화
    // (업데이트는 드물게 발생하므로 전체 재구성 허용)
    if (live_events_card_layout_) {
        for (int i = 0; i < live_events_card_layout_->count(); ++i) {
            auto *w = qobject_cast<ParkingEventCardWidget *>(
                live_events_card_layout_->itemAt(i)->widget());
            if (w && w->eventId() == item.event_id) {
                w->updateEvent(item);
                break;
            }
        }
    }
    // 테이블 ACK 버튼 상태 갱신: 해당 행의 버튼만 업데이트
    if (events_table_) {
        for (int r = 0; r < events_table_->rowCount(); ++r) {
            QTableWidgetItem *ti = events_table_->item(r, 0);
            if (!ti) continue;
            auto *btn = qobject_cast<QPushButton *>(events_table_->cellWidget(r, 5));
            if (btn && events_table_->item(r, 0)) {
                // event_id를 버튼 userData로 식별
                quint64 rowId = btn->property("vmsEventId").toULongLong();
                if (rowId == item.event_id) {
                    btn->setText(item.acked ? QStringLiteral("확인됨") : QStringLiteral("확인"));
                    btn->setEnabled(!item.acked);
                    break;
                }
            }
        }
    }
}

static QTableWidgetItem *makeTableItem(const QString &text, Qt::Alignment align = Qt::AlignCenter) {
    auto *ti = new QTableWidgetItem(text);
    ti->setTextAlignment(align);
    return ti;
}

void MainWindow::appendEventRow(const ParkingEventItem &item) {
    int row = events_table_->rowCount();
    events_table_->insertRow(row);

    quint64 ts_ms = item.occurred_at_utc_ms > 0 ? item.occurred_at_utc_ms : item.received_at_utc_ms;
    QDateTime dt = (ts_ms > 0) ? QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(ts_ms), QTimeZone("Asia/Seoul")) : QDateTime();
    events_table_->setItem(row, 0, makeTableItem(
        dt.isValid() ? dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QStringLiteral("-")));

    QString sevStr = item.severity == EventSeverity::Critical ? QStringLiteral("CRITICAL")
                   : item.severity == EventSeverity::Warning  ? QStringLiteral("WARNING")
                                                              : QStringLiteral("INFO");
    auto *sevItem = makeTableItem(sevStr);
    if (item.severity == EventSeverity::Critical)     sevItem->setForeground(QBrush(QColor(211, 47, 47)));
    else if (item.severity == EventSeverity::Warning) sevItem->setForeground(QBrush(QColor(230, 81, 0)));
    else                                               sevItem->setForeground(QBrush(QColor(46, 125, 50)));
    events_table_->setItem(row, 1, sevItem);

    QString chStr = QStringLiteral("CH %1").arg(item.channel_id);
    if (!item.payload.space_label.isEmpty()) chStr += QStringLiteral(" (%1)").arg(item.payload.space_label);
    events_table_->setItem(row, 2, makeTableItem(chStr));

    QString detail = item.payload.violation ? QStringLiteral("비전기차 충전구역 점유 위반") : item.payload.state;
    if (detail.isEmpty()) detail = item.event_type;
    events_table_->setItem(row, 3, makeTableItem(detail, Qt::AlignLeft | Qt::AlignVCenter));

    QString plateInfo = item.payload.plate;
    if (item.payload.ev == QStringLiteral("yes"))     plateInfo += QStringLiteral(" [EV 전기차]");
    else if (item.payload.ev == QStringLiteral("no")) plateInfo += QStringLiteral(" [내연기관]");
    else                                               plateInfo += QStringLiteral(" [미확인]");
    events_table_->setItem(row, 4, makeTableItem(plateInfo));

    auto *ackBtn = new QPushButton(item.acked ? QStringLiteral("확인됨") : QStringLiteral("확인"), events_table_);
    ackBtn->setEnabled(!item.acked);
    ackBtn->setCursor(Qt::PointingHandCursor);
    ackBtn->setProperty("vmsEventId", QVariant::fromValue(item.event_id));
    quint64 eventId = item.event_id;
    connect(ackBtn, &QPushButton::clicked, this, [this, eventId]() {
        if (event_store_) event_store_->setAcked(eventId, true);
    });
    events_table_->setCellWidget(row, 5, ackBtn);
}

void MainWindow::prependEventRow(const ParkingEventItem &item) {
    events_table_->insertRow(0);

    quint64 ts_ms = item.occurred_at_utc_ms > 0 ? item.occurred_at_utc_ms : item.received_at_utc_ms;
    QDateTime dt = (ts_ms > 0) ? QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(ts_ms), QTimeZone("Asia/Seoul")) : QDateTime();
    events_table_->setItem(0, 0, makeTableItem(
        dt.isValid() ? dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QStringLiteral("-")));

    QString sevStr = item.severity == EventSeverity::Critical ? QStringLiteral("CRITICAL")
                   : item.severity == EventSeverity::Warning  ? QStringLiteral("WARNING")
                                                              : QStringLiteral("INFO");
    auto *sevItem = makeTableItem(sevStr);
    if (item.severity == EventSeverity::Critical)     sevItem->setForeground(QBrush(QColor(211, 47, 47)));
    else if (item.severity == EventSeverity::Warning) sevItem->setForeground(QBrush(QColor(230, 81, 0)));
    else                                               sevItem->setForeground(QBrush(QColor(46, 125, 50)));
    events_table_->setItem(0, 1, sevItem);

    QString chStr = QStringLiteral("CH %1").arg(item.channel_id);
    if (!item.payload.space_label.isEmpty()) chStr += QStringLiteral(" (%1)").arg(item.payload.space_label);
    events_table_->setItem(0, 2, makeTableItem(chStr));

    QString detail = item.payload.violation ? QStringLiteral("비전기차 충전구역 점유 위반") : item.payload.state;
    if (detail.isEmpty()) detail = item.event_type;
    events_table_->setItem(0, 3, makeTableItem(detail, Qt::AlignLeft | Qt::AlignVCenter));

    QString plateInfo = item.payload.plate;
    if (item.payload.ev == QStringLiteral("yes"))     plateInfo += QStringLiteral(" [EV 전기차]");
    else if (item.payload.ev == QStringLiteral("no")) plateInfo += QStringLiteral(" [내연기관]");
    else                                               plateInfo += QStringLiteral(" [미확인]");
    events_table_->setItem(0, 4, makeTableItem(plateInfo));

    auto *ackBtn = new QPushButton(item.acked ? QStringLiteral("확인됨") : QStringLiteral("확인"), events_table_);
    ackBtn->setEnabled(!item.acked);
    ackBtn->setCursor(Qt::PointingHandCursor);
    ackBtn->setProperty("vmsEventId", QVariant::fromValue(item.event_id));
    quint64 eventId = item.event_id;
    connect(ackBtn, &QPushButton::clicked, this, [this, eventId]() {
        if (event_store_) event_store_->setAcked(eventId, true);
    });
    events_table_->setCellWidget(0, 5, ackBtn);
}


void MainWindow::updateLiveEventsPanel() {
    if (!event_store_) return;

    // 고정 판정: event_type을 반드시 봐야 한다 — stage1/2_state_changed도
    // 항상 CRITICAL이라(§2.9) severity만으로 거르면 진압 진행 상태 변화까지
    // 전부 상단에 고정된다. 결정 4에 따라 fire_cleared는 고정을 풀지 않는다
    // (ignored_critical_ids_에 넣는 것은 [무시] 클릭뿐). pin_baseline_event_id_
    // 이하는 로그인 시 다시 내려온 과거 이벤트이므로 고정 대상에서 제외한다.
    const QVector<ParkingEventItem> &all = event_store_->allEvents();
    QVector<ParkingEventItem> pinned;
    QSet<quint64> pinned_ids;
    for (int i = all.size() - 1; i >= 0; --i) {  // 최신이 위로
        const ParkingEventItem &item = all[i];
        if (item.source_type == QStringLiteral("stm") && item.event_type == QStringLiteral("stm.fire_started") &&
            item.severity == EventSeverity::Critical && !ignored_critical_ids_.contains(item.event_id) &&
            item.event_id > pin_baseline_event_id_) {
            pinned.append(item);
            pinned_ids.insert(item.event_id);
        }
    }

    // 일반 영역에서는 고정된 것을 제외한다 — 같은 카드가 화면에 두 번 보이지
    // 않게 한다(Q3-5 확정).
    QVector<ParkingEventItem> general;
    for (const ParkingEventItem &item : event_store_->recentEvents(15)) {
        if (!pinned_ids.contains(item.event_id)) {
            general.append(item);
        }
    }

    if (live_events_pinned_container_) {
        live_events_pinned_container_->setVisible(!pinned.isEmpty());
    }
    syncEventCards(live_events_pinned_layout_, live_events_pinned_container_, pinned);
    syncEventCards(live_events_card_layout_, live_events_container_, general);
}

void MainWindow::syncEventCards(QVBoxLayout *layout, QWidget *container, const QVector<ParkingEventItem> &items) {
    if (!layout || !container) return;

    container->setUpdatesEnabled(false);

    // 기존 카드 위젯 리스트 수집 (트레일링 spacer는 건드리지 않는다)
    QList<ParkingEventCardWidget*> existingCards;
    for (int i = 0; i < layout->count(); ++i) {
        auto *w = qobject_cast<ParkingEventCardWidget *>(layout->itemAt(i)->widget());
        if (w) {
            existingCards.append(w);
        } else {
            auto *item = layout->itemAt(i);
            if (item && item->widget()) {
                item->widget()->deleteLater();
            }
        }
    }

    // 마지막 아이템이 addStretch()로 만든 spacer면 그 앞에 삽입한다
    // (일반 목록 스크롤 영역용). 고정 영역처럼 spacer가 없으면 그냥 끝에 붙인다.
    auto insertPosition = [layout]() {
        if (layout->count() > 0 && layout->itemAt(layout->count() - 1)->spacerItem()) {
            return layout->count() - 1;
        }
        return layout->count();
    };

    // 1. 필요한 개수보다 모자라면 카드 추가
    while (existingCards.size() < items.size()) {
        auto *card = new ParkingEventCardWidget(items[existingCards.size()], container);
        connect(card, &ParkingEventCardWidget::actionTriggered, this,
                [this](const QString &action, const ParkingEventItem &ev) { handleEventCardAction(action, ev); });
        layout->insertWidget(insertPosition(), card);
        existingCards.append(card);
    }

    // 2. 필요한 개수보다 많으면 초과분 제거
    while (existingCards.size() > items.size()) {
        auto *card = existingCards.takeLast();
        card->deleteLater();
    }

    // 3. 위젯 삭제/생성 없이 1:1로 내용만 갱신 (무깜빡임)
    for (int i = 0; i < items.size(); ++i) {
        existingCards[i]->updateEvent(items[i]);
    }

    container->setUpdatesEnabled(true);
}

void MainWindow::handleEventCardAction(const QString &action, const ParkingEventItem &event) {
    if (action != QStringLiteral("open_response_screen")) return;

    if (event.source_type != QStringLiteral("stm") || event.event_type != QStringLiteral("stm.fire_started")) {
        // 카메라 쪽 기존 동작(§critical) — Q3 범위 밖이라 로그만 남긴다.
        qInfo().noquote() << QString("[event-action] open_response_screen event_id=%1").arg(event.event_id);
        return;
    }

    // event를 값으로 넘긴다 — exec() 동안 패널이 갱신되면 클릭된 카드
    // 위젯이 지워질 수 있어, 참조로 들고 있으면 dangling이 된다.
    StmFireResponseDialog dialog(event, stm_api_, event_store_, this);
    dialog.exec();
    if (dialog.ignoreRequested()) {
        ignored_critical_ids_.insert(event.event_id);
        updateLiveEventsPanel();
    }
}

void MainWindow::refreshEventsTable() {
    if (!events_table_ || !event_store_) return;

    events_table_->setUpdatesEnabled(false);

    int sevIdx = events_severity_filter_ ? events_severity_filter_->currentIndex() : 0;
    int chIdx  = events_channel_filter_  ? events_channel_filter_->currentIndex()  : 0;

    const auto &all = event_store_->allEvents();
    if (all.isEmpty()) {
        events_table_->setRowCount(0);
        events_table_->setUpdatesEnabled(true);
        return;
    }

    // 테이블이 비어있으면 전체 재구성 (초기 로딩)
    if (events_table_->rowCount() == 0) {
        for (int i = all.size() - 1; i >= 0; --i) {
            const auto &item = all[i];
            if (sevIdx == 1 && item.severity != EventSeverity::Critical) continue;
            if (sevIdx == 2 && item.severity != EventSeverity::Warning)  continue;
            if (sevIdx == 3 && item.severity != EventSeverity::Info)     continue;
            if (chIdx > 0  && item.channel_id != chIdx)                  continue;
            appendEventRow(item);
        }
        events_table_->setUpdatesEnabled(true);
        return;
    }

    // 이미 행이 있으면 최신 이벤트만 맨 위에 삽입
    const auto &item = all.last();
    if (sevIdx == 1 && item.severity != EventSeverity::Critical) { events_table_->setUpdatesEnabled(true); return; }
    if (sevIdx == 2 && item.severity != EventSeverity::Warning)  { events_table_->setUpdatesEnabled(true); return; }
    if (sevIdx == 3 && item.severity != EventSeverity::Info)     { events_table_->setUpdatesEnabled(true); return; }
    if (chIdx > 0  && item.channel_id != chIdx)                  { events_table_->setUpdatesEnabled(true); return; }
    prependEventRow(item);

    // 최대 500행 초과 시 오래된 행 제거
    constexpr int kMaxRows = 500;
    while (events_table_->rowCount() > kMaxRows) {
        events_table_->removeRow(events_table_->rowCount() - 1);
    }

    events_table_->setUpdatesEnabled(true);
}

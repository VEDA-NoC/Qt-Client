#pragma once

#include <QDate>
#include <QDateTime>
#include <QElapsedTimer>
#include <QMainWindow>
#include <QSet>
#include <QString>
#include <QUrl>
#include <QVector>

class QButtonGroup;
class QComboBox;
class QFrame;
class QGridLayout;
class QLabel;
class LegalNoticeWidget;
class QLineEdit;
class QPushButton;
class QSlider;
class QStackedLayout;
class QStackedWidget;
class QTimer;
class QToolButton;
class QTimeEdit;
class PlaybackApiClient;
struct DeviceStatusSnapshot;
struct PlaybackSession;
struct StorageStatus;
class StreamWorker;
class TimelineWidget;
class VideoPanel;
class QResizeEvent;
class QCloseEvent;
class QScrollArea;
class ParkingZoneEditor;
class ParkingEventApiClient;
class ParkingEventStore;
class ParkingEventCardWidget;
struct ParkingEventBatch;
struct ParkingEventItem;
class StmApiClient;
class StmDeviceListWidget;
class QTableWidget;
class QVBoxLayout;
class QHBoxLayout;
class LoginPage;
class SignupPage;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void applyBaseUrl();
    void applyControlSettings();
    void logout();
    void searchPlaybackTimeline();
    void playSelectedRange();
    void togglePlaybackPause();
    void stopPlayback();
    void togglePlaybackFullscreen();
    void startStreams();
    void stopStreams();
    void updateTotalStats();
    void setCurrentPage(int page_index);

private:
    enum class PlaybackUiState {
        Preview,
        Starting,
        Playing,
        Pausing,
        Paused,
        Stopping,
        Error,
    };
    enum class ControlUiState {
        SettingsRequired,
        Connecting,
        Ready,
        RetryableFailure,
    };

    QString channelUrl(int channel) const;
    void createUi();
    QWidget *createSidebar(QWidget *parent);
    QFrame *createTopBar(QWidget *parent);
    QFrame *createBottomBar(QWidget *parent);
    QWidget *createLivePage(QWidget *parent);
    QWidget *createPlaybackPage(QWidget *parent);
    QWidget *createEventsPage(QWidget *parent);
    QWidget *createDevicesPage(QWidget *parent);
    QWidget *createSettingsPage(QWidget *parent);
    // 인증 전 접근을 막는 자리(실시간 이벤트, 이벤트 목록, 장치 및 제어)에
    // 공통으로 쓰는 안내 위젯. 설정으로 이동 버튼까지 미리 배선해서 돌려준다.
    QWidget *createAuthGateOverlay(const QString &message, QWidget *parent);
    QFrame *createSummaryCard(const QString &title,
                              const QString &value,
                              const QString &caption,
                              QWidget *parent,
                              QLabel **value_label = nullptr,
                              QLabel **caption_label = nullptr) const;
    void handleChannelStatus(int channel, const QString &status);
    void refreshConnectionSummary();
    int activeLiveWorkerCount() const;
    bool hasPendingLiveRetries() const;
    void startChannelStream(int channel);
    void handleLiveWorkerFinished(int channel, StreamWorker *worker);
    void scheduleChannelRetry(int channel, const QString &reason);
    void cancelChannelRetry(int channel, bool reset_attempt);
    void cancelAllChannelRetries(bool reset_attempts);
    int channelRetryDelayMs(int attempt) const;
    void setBaseUrlFeedback(const QString &text, const QString &severity);
    void requestWorkerStop();
    void finalizeStoppedState();
    void performPlaybackApiAction(int action);
    void startPlaybackWorker(const PlaybackSession &session);
    void stopPlaybackWorker(bool clear_video);
    void setPlaybackFeedback(const QString &text, const QString &severity);
    void seekPlaybackBy(qint64 delta_ms);
    void seekPlaybackTo(qint64 utc_ms);
    void updatePlaybackPosition(qint64 utc_ms);
    bool playbackShortcutAllowed() const;
    void refreshPlaybackAuthUi(bool authenticated);
    void setControlUiState(ControlUiState state,
                           const QString &message = QString());
    // 마지막 인증 세션에서 받아둔 Pi/저장소/서버시각 표시를 "아직 모름"
    // 상태로 되돌린다. 캐시 플래그(has_last_*)를 같이 내려야 한다 —
    // setControlUiState()가 그 플래그를 보고 배지 갱신을 건너뛰기 때문에,
    // 안 내리면 로그아웃 후에도 옛 "정상" 배지가 그대로 남는다.
    void resetControlSessionDisplay();
    void setPlaybackPivotDate(const QDate &date);
    QDateTime currentServerDisplayTime() const;
    void shiftPlaybackPivot(int direction);
    void updateLiveLayout();
    void updatePlaybackControlState();
    void setPlaybackUiState(PlaybackUiState state);
    void clearProtectedPlaybackState();
    void requestTimelineWindow(qint64 start_utc_ms,
                               qint64 end_utc_ms,
                               bool reset_cache,
                               int direction = 0);
    void finalizeTimelineWindowRequest();
    void scheduleTimelinePrefetch();
    void updateTimelineNavigator(qint64 visible_start_utc_ms,
                                 qint64 visible_end_utc_ms,
                                 qint64 pivot_utc_ms);
    void pollDeviceStatus();
    void applyDeviceStatus(const DeviceStatusSnapshot &status);
    void applyStorageStatus(const StorageStatus &status);
    void setDeviceControlError(const QString &message);
    void updateServerTimeDisplay();
    void showParkingZoneEditor();
    void leaveParkingZoneEditor();
    void handleEventUpdatedInStore(const ParkingEventItem &item);
    void refreshEventsTable();
    void updateLiveEventsPanel();
    void syncEventCards(QVBoxLayout *layout, QWidget *container, const QVector<ParkingEventItem> &items);
    void handleEventCardAction(const QString &action, const ParkingEventItem &event);
    void fillEventRow(int row, const ParkingEventItem &item);
    void appendEventRow(const ParkingEventItem &item);
    bool eventPassesTableFilter(const ParkingEventItem &item) const;

    QButtonGroup *navigation_group_ = nullptr;
    QStackedWidget *page_stack_ = nullptr;
    QLabel *page_title_label_ = nullptr;
    QLabel *page_subtitle_label_ = nullptr;
    QLineEdit *base_url_edit_ = nullptr;
    QPushButton *base_url_apply_button_ = nullptr;
    QPushButton *start_button_ = nullptr;
    QPushButton *stop_button_ = nullptr;
    QLabel *connection_badge_ = nullptr;
    QLabel *channel_summary_label_ = nullptr;
    QLabel *total_stats_label_ = nullptr;
    QLabel *footer_connection_label_ = nullptr;
    QLabel *server_time_label_ = nullptr;
    QLabel *pi_status_badge_ = nullptr;
    QLabel *storage_status_badge_ = nullptr;
    QLabel *system_device_value_label_ = nullptr;
    QLabel *system_device_caption_label_ = nullptr;
    QLabel *server_time_value_label_ = nullptr;
    QLabel *system_metrics_value_label_ = nullptr;
    QLabel *system_memory_value_label_ = nullptr;
    QLabel *system_uptime_value_label_ = nullptr;
    QLabel *system_throttling_value_label_ = nullptr;
    QLabel *system_reboot_notice_label_ = nullptr;
    QLabel *storage_device_value_label_ = nullptr;
    QLabel *storage_device_caption_label_ = nullptr;
    QLabel *storage_alert_label_ = nullptr;
    QWidget *stream_controls_ = nullptr;
    QWidget *live_page_ = nullptr;
    QWidget *live_grid_host_ = nullptr;
    QScrollArea *live_grid_scroll_ = nullptr;
    QGridLayout *live_page_layout_ = nullptr;
    QGridLayout *live_grid_layout_ = nullptr;
    QFrame *live_events_card_ = nullptr;
    int live_layout_mode_ = -1;
    QWidget *app_sidebar_ = nullptr;
    QFrame *top_bar_ = nullptr;
    QFrame *bottom_bar_ = nullptr;
    QStackedWidget *root_stack_ = nullptr;
    LoginPage *login_page_ = nullptr;
    SignupPage *signup_page_ = nullptr;
    QLineEdit *control_base_url_edit_ = nullptr;
    QLineEdit *control_certificate_edit_ = nullptr;
    QPushButton *control_apply_button_ = nullptr;
    QToolButton *logout_button_ = nullptr;
    QToolButton *exit_button_ = nullptr;
    QLabel *control_feedback_label_ = nullptr;
    LegalNoticeWidget *legal_notice_widget_ = nullptr;
    ParkingZoneEditor *parking_zone_editor_ = nullptr;
    ParkingEventApiClient *event_api_client_ = nullptr;
    ParkingEventStore *event_store_ = nullptr;
    QWidget *live_events_pinned_container_ = nullptr;
    QVBoxLayout *live_events_pinned_layout_ = nullptr;
    QSet<quint64> ignored_critical_ids_;
    // 로그인 시 이벤트 백로그가 다시 내려와도 "상단 고정"은 새 이벤트에만
    // 걸리도록, 폴링 시작 후 첫 배치(백로그)의 최대 event_id를 기준선으로
    // 잡는다. 그 이후 도착한(=id가 이보다 큰) CRITICAL 이벤트만 고정 대상.
    quint64 pin_baseline_event_id_ = 0;
    bool pin_baseline_established_ = false;
    QVBoxLayout *live_events_card_layout_ = nullptr;
    QScrollArea *live_events_scroll_ = nullptr;
    QWidget *live_events_container_ = nullptr;
    // index 0=실제 목록, 1=인증 필요 안내. setControlUiState()가 토글한다.
    QStackedLayout *live_events_gate_stack_ = nullptr;
    QStackedLayout *events_gate_stack_ = nullptr;
    QStackedLayout *devices_page_stack_ = nullptr;
    QTableWidget *events_table_ = nullptr;
    QComboBox *events_severity_filter_ = nullptr;
    QComboBox *events_channel_filter_ = nullptr;

    QWidget *playback_query_controls_ = nullptr;
    QPushButton *playback_pivot_date_button_ = nullptr;
    QTimeEdit *playback_pivot_time_edit_ = nullptr;
    QComboBox *playback_range_combo_ = nullptr;
    QPushButton *playback_previous_button_ = nullptr;
    QPushButton *playback_next_button_ = nullptr;
    QPushButton *playback_now_button_ = nullptr;
    QLabel *playback_auth_notice_label_ = nullptr;
    QPushButton *playback_auth_settings_button_ = nullptr;
    QPushButton *playback_auth_retry_button_ = nullptr;
    QWidget *playback_timeline_auth_prompt_ = nullptr;
    QPushButton *playback_search_button_ = nullptr;
    QPushButton *playback_play_button_ = nullptr;
    QPushButton *playback_pause_button_ = nullptr;
    QPushButton *playback_stop_button_ = nullptr;
    QPushButton *playback_fullscreen_button_ = nullptr;
    QComboBox *playback_speed_combo_ = nullptr;
    QSlider *playback_position_slider_ = nullptr;
    QLabel *playback_position_label_ = nullptr;
    QLabel *playback_feedback_label_ = nullptr;
    QLabel *playback_selection_label_ = nullptr;
    QFrame *playback_filters_card_ = nullptr;
    QFrame *playback_timeline_card_ = nullptr;
    TimelineWidget *playback_timeline_ = nullptr;
    QSlider *playback_timeline_navigator_ = nullptr;
    VideoPanel *playback_panel_ = nullptr;
    QWidget *playback_fullscreen_window_ = nullptr;
    VideoPanel *playback_fullscreen_panel_ = nullptr;
    QPushButton *playback_fullscreen_toggle_button_ = nullptr;
    QPushButton *playback_fullscreen_stop_button_ = nullptr;
    QPushButton *playback_fullscreen_exit_button_ = nullptr;
    QSlider *playback_fullscreen_position_slider_ = nullptr;
    QLabel *playback_fullscreen_position_label_ = nullptr;
    PlaybackApiClient *playback_api_ = nullptr;
    StmApiClient *stm_api_ = nullptr;
    StmDeviceListWidget *stm_device_list_widget_ = nullptr;
    StreamWorker *playback_worker_ = nullptr;

    QVector<VideoPanel *> panels_;
    QVector<StreamWorker *> workers_;
    QVector<QString> channel_statuses_;
    QVector<QTimer *> channel_retry_timers_;
    QVector<QTimer *> channel_stable_timers_;
    QVector<int> channel_retry_attempts_;
    QString applied_base_url_ = "rtsps://100.93.115.22:8554";
    QUrl applied_control_base_url_;
    QString control_certificate_path_;
    QString active_playback_session_id_;
    int pending_playback_api_action_ = 0;
    int playback_channel_id_ = 1;
    QDate playback_pivot_date_;
    qint64 playback_query_pivot_utc_ms_ = 0;
    qint64 playback_query_start_utc_ms_ = 0;
    qint64 playback_query_end_utc_ms_ = 0;
    qint64 playback_selection_start_utc_ms_ = 0;
    qint64 playback_selection_end_utc_ms_ = 0;
    qint64 playback_range_start_utc_ms_ = 0;
    qint64 playback_range_end_utc_ms_ = 0;
    qint64 active_playback_start_utc_ms_ = 0;
    qint64 active_playback_end_utc_ms_ = 0;
    qint64 playback_current_utc_ms_ = 0;
    qint64 pending_seek_utc_ms_ = 0;
    int playback_timelines_received_ = 0;
    bool playback_login_in_progress_ = false;
    bool playback_login_ever_succeeded_ = false;
    bool playback_paused_ = false;
    bool playback_slider_dragging_ = false;
    bool playback_seek_in_progress_ = false;
    bool live_suspended_for_playback_ = false;
    QTimer *playback_seek_timer_ = nullptr;
    QTimer *playback_video_click_timer_ = nullptr;
    QTimer *timeline_prefetch_timer_ = nullptr;
    QTimer *device_status_timer_ = nullptr;
    bool playback_stop_clears_video_ = true;
    PlaybackUiState playback_ui_state_ = PlaybackUiState::Preview;
    ControlUiState control_ui_state_ =
        ControlUiState::SettingsRequired;
    qint64 timeline_cache_start_utc_ms_ = 0;
    qint64 timeline_cache_end_utc_ms_ = 0;
    qint64 timeline_fetch_span_ms_ = 0;
    qint64 timeline_visible_duration_ms_ = 0;
    qint64 timeline_pending_start_utc_ms_ = 0;
    qint64 timeline_pending_end_utc_ms_ = 0;
    int timeline_pending_direction_ = 0;
    int timeline_requests_completed_ = 0;
    int timeline_requests_succeeded_ = 0;
    int timeline_requests_failed_ = 0;
    bool timeline_request_in_progress_ = false;
    bool timeline_request_resets_cache_ = false;
    bool timeline_navigator_updating_ = false;
    bool device_status_retry_after_login_ = false;
    bool has_last_device_status_ = false;
    bool has_last_storage_status_ = false;
    QDateTime device_status_last_updated_at_;
    QDateTime storage_last_updated_at_;
    QString last_server_boot_id_;
    qint64 server_time_base_utc_ms_ = 0;
    QElapsedTimer server_time_elapsed_;
    QString server_time_sync_state_;
    bool server_time_interpolation_active_ = false;
    bool stopping_streams_ = false;
    bool restart_after_stop_ = false;
    bool live_desired_running_ = false;
    bool shutting_down_ = false;
};

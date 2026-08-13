#pragma once

#include <QMetaType>
#include <QString>
#include <QVector>

struct PlaybackTimelineSpan {
    QString kind;
    qint64 start_utc_ms = 0;
    qint64 end_utc_ms = 0;
    QVector<qint64> segment_ids;

    bool isRecording() const {
        return kind == "recording";
    }
};

struct PlaybackTimelineEvent {
    qint64 request_id = 0;
    QString event_type;
    QString severity;
    QString correlation_id;
    qint64 start_utc_ms = 0;
    qint64 end_utc_ms = 0;
};

struct PlaybackTimeline {
    int channel_id = 0;
    qint64 start_utc_ms = 0;
    qint64 end_utc_ms = 0;
    QVector<PlaybackTimelineSpan> spans;
    QVector<PlaybackTimelineEvent> events;
};

struct PlaybackSession {
    QString playback_session_id;
    QString rtsps_path;
    int channel_id = 0;
    qint64 start_utc_ms = 0;
    qint64 end_utc_ms = 0;
    qint64 duration_ms = 0;
    int segment_count = 0;
    bool one_shot = false;
};

struct StorageStatus {
    QString state;
    qint64 total_bytes = 0;
    qint64 used_bytes = 0;
    qint64 free_bytes = 0;
    qint64 available_bytes = 0;
    double used_percent = 0.0;
    bool recording_suspended = false;
};

struct ServerTimeStatus {
    qint64 utc_ms = 0;
    qint64 monotonic_ms = 0;
    QString sync_state;
    QString boot_id;
};

struct MemoryStatus {
    bool available = false;
    qint64 total_bytes = 0;
    qint64 used_bytes = 0;
    qint64 available_bytes = 0;
    double used_percent = 0.0;
};

struct ThrottlingStatus {
    bool available = false;
    quint32 raw_flags = 0;
    bool under_voltage_now = false;
    bool frequency_capped_now = false;
    bool throttled_now = false;
    bool soft_temperature_limit_now = false;
    bool under_voltage_occurred = false;
    bool frequency_capped_occurred = false;
    bool throttled_occurred = false;
    bool soft_temperature_limit_occurred = false;
};

struct SystemStatus {
    QString state;
    qint64 sampled_at_utc_ms = 0;
    qint64 sample_age_ms = 0;
    bool cpu_usage_available = false;
    double cpu_usage_percent = 0.0;
    bool load_average_available = false;
    double load_average_1m = 0.0;
    bool temperature_available = false;
    double temperature_celsius = 0.0;
    bool uptime_available = false;
    qint64 uptime_seconds = 0;
    MemoryStatus memory;
    ThrottlingStatus throttling;
};

struct DeviceStatusSnapshot {
    ServerTimeStatus server_time;
    bool system_available = false;
    SystemStatus system;
    bool storage_available = false;
    StorageStatus storage;
};

Q_DECLARE_METATYPE(PlaybackTimeline)
Q_DECLARE_METATYPE(PlaybackSession)
Q_DECLARE_METATYPE(StorageStatus)
Q_DECLARE_METATYPE(DeviceStatusSnapshot)

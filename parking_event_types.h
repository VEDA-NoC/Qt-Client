#ifndef PARKING_EVENT_TYPES_H
#define PARKING_EVENT_TYPES_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QVector>

enum class EventSeverity {
    Info,
    Warning,
    Critical
};

enum class EventPhase {
    Started,
    Updated,
    Ended,
    Occurred
};

struct ParkingEventPayload {
    QString parking_session_id;
    QString space_label;
    QString state;               // camera_detected, occupied_confirmed, departed, misparked 등
    bool violation{false};
    QString violation_reason;    // NON_EV_IN_EV_ZONE, NONE 등
    QString plate;               // "12가3456", "HOLD", ""
    QString ev;                  // "yes", "no", "unknown"
};

// source_type == "stm"인 이벤트의 payload. rtsps stm_event_bridge.cpp의
// stm_event_payload_json()이 채우는 필드(space_label 제외 — 그건 두 소스가
// 공유하는 ParkingEventPayload::space_label로 이미 파싱된다)와 1:1 대응한다.
struct StmEventPayload {
    double temperature_c{0.0};   // FIRE_STARTED/CLEARED에서만 유효
    bool occupied{false};        // OCCUPANCY_CHANGED에서만 유효
    int distance_mm{0};          // OCCUPANCY_CHANGED에서만 유효
    int actuator_status{0};      // STAGE1/2_STATE_CHANGED에서만 유효 (STM_ACTUATOR_*)
    int origin{0};               // STAGE1/2_STATE_CHANGED: 0=local, 1=remote
    int slave_address{0};
};

struct ParkingEventItem {
    quint64 event_id{0};
    QString event_type;
    EventPhase phase{EventPhase::Occurred};
    QString source_type;
    QString source_device_id;
    QString source_event_name;
    int camera_channel{0};
    int channel_id{1};
    quint64 occurred_at_utc_ms{0};
    quint64 received_at_utc_ms{0};
    EventSeverity severity{EventSeverity::Info};
    QString correlation_id;
    ParkingEventPayload payload;
    StmEventPayload stm_payload;  // source_type != "stm"이면 기본값 그대로
    bool acked{false};            // 운영자 확인 여부 (Qt 로컬 상태)

    static EventSeverity parseSeverity(const QString &str) {
        if (str.compare(QStringLiteral("CRITICAL"), Qt::CaseInsensitive) == 0) {
            return EventSeverity::Critical;
        } else if (str.compare(QStringLiteral("WARNING"), Qt::CaseInsensitive) == 0) {
            return EventSeverity::Warning;
        }
        return EventSeverity::Info;
    }

    static EventPhase parsePhase(const QString &str) {
        if (str.compare(QStringLiteral("started"), Qt::CaseInsensitive) == 0) {
            return EventPhase::Started;
        } else if (str.compare(QStringLiteral("updated"), Qt::CaseInsensitive) == 0) {
            return EventPhase::Updated;
        } else if (str.compare(QStringLiteral("ended"), Qt::CaseInsensitive) == 0) {
            return EventPhase::Ended;
        }
        return EventPhase::Occurred;
    }

    static ParkingEventItem fromJson(const QJsonObject &obj) {
        ParkingEventItem item;
        item.event_id = static_cast<quint64>(obj.value(QStringLiteral("event_id")).toVariant().toULongLong());
        item.event_type = obj.value(QStringLiteral("event_type")).toString();
        item.phase = parsePhase(obj.value(QStringLiteral("phase")).toString());
        item.source_type = obj.value(QStringLiteral("source_type")).toString();
        item.source_device_id = obj.value(QStringLiteral("source_device_id")).toString();
        item.source_event_name = obj.value(QStringLiteral("source_event_name")).toString();
        item.camera_channel = obj.value(QStringLiteral("camera_channel")).toInt(0);
        item.channel_id = obj.value(QStringLiteral("channel_id")).toInt(1);
        item.occurred_at_utc_ms = static_cast<quint64>(obj.value(QStringLiteral("occurred_at_utc_ms")).toVariant().toULongLong());
        item.received_at_utc_ms = static_cast<quint64>(obj.value(QStringLiteral("received_at_utc_ms")).toVariant().toULongLong());
        item.severity = parseSeverity(obj.value(QStringLiteral("severity")).toString());
        item.correlation_id = obj.value(QStringLiteral("correlation_id")).toString();

        if (obj.contains(QStringLiteral("payload")) && obj.value(QStringLiteral("payload")).isObject()) {
            QJsonObject pObj = obj.value(QStringLiteral("payload")).toObject();
            item.payload.parking_session_id = pObj.value(QStringLiteral("parking_session_id")).toString();
            item.payload.space_label = pObj.value(QStringLiteral("space_label")).toString();
            item.payload.state = pObj.value(QStringLiteral("state")).toString();
            item.payload.violation = pObj.value(QStringLiteral("violation")).toBool(false);
            item.payload.violation_reason = pObj.value(QStringLiteral("violation_reason")).toString();
            item.payload.plate = pObj.value(QStringLiteral("plate")).toString();
            item.payload.ev = pObj.value(QStringLiteral("ev")).toString();

            if (item.source_type == QStringLiteral("stm")) {
                item.stm_payload.temperature_c = pObj.value(QStringLiteral("temperature_c")).toDouble();
                item.stm_payload.occupied = pObj.value(QStringLiteral("occupied")).toBool();
                item.stm_payload.distance_mm = pObj.value(QStringLiteral("distance_mm")).toInt();
                item.stm_payload.actuator_status = pObj.value(QStringLiteral("actuator_status")).toInt();
                item.stm_payload.origin = pObj.value(QStringLiteral("origin")).toInt();
                item.stm_payload.slave_address = pObj.value(QStringLiteral("slave_address")).toInt();
            }
        }
        return item;
    }
};

struct ParkingEventBatch {
    int schema_version{1};
    quint64 server_time_utc_ms{0};
    quint64 next_after_id{0};
    QVector<ParkingEventItem> events;
};

#endif // PARKING_EVENT_TYPES_H

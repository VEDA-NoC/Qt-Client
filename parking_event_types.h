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
    QString state;               // "occupied" | "vacant" (카메라) 또는 소스가 준 raw 상태 문자열.
                                 // UI 문구로 번역하는 것은 표시하는 쪽 책임이다.
    bool violation{false};
    QString violation_reason;    // NON_EV_IN_EV_ZONE 등
    QString plate;               // 카메라 소스는 Pi가 "****"로 마스킹해서 보낸다 (원문은 절대 오지 않는다)
    QString ev;                  // "yes", "no", "unknown"
};

// source_type이 "stm"이 아닌 카메라 주차 관측 이벤트의 payload.
// rtsps parking_camera_observation.cpp의 parking_transition_to_event()가
// 채우는 필드와 1:1 대응한다 — 공용 필드(space_label/state/plate/ev/violation)는
// 아래 fromJson()에서 ParkingEventPayload 쪽으로도 옮겨 담는다.
//
// 주의: 서버가 실제로 보내는 키 이름은 masked_plate/ev_state/occupied/space_id이며,
// outputs/qt-event-long-polling-api-guide.md의 예시(plate/ev/state)와 다르다.
// 그 가이드가 아니라 서버 구현이 계약이다.
struct CameraEventPayload {
    QString space_id;            // Pi stable space id
    QString camera_space_id;     // 카메라 쪽 stable id (진단용)
    bool occupied{false};
    qint64 parked_ms_ago{-1};    // 점유 경과 시간. 미점유면 -1
    bool plate_present{false};   // 번호판이 읽혔는지 여부 (원문은 오지 않는다)
    QString ev_source;           // "registered" 등 EV 판정 출처
    QString evidence_path;
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
    CameraEventPayload camera_payload;  // source_type == "stm"이면 기본값 그대로
    bool acked{false};            // 운영자 확인 여부 (Qt 로컬 상태)

    // EV 전용 구역에 비전기차가 주차된 단속 대상 이벤트인지.
    // 위반 판정은 카메라 앱이 하고(ev=unknown은 위반으로 치지 않는다) Pi는
    // payload.violation 플래그로 중계만 한다 — 별도 event_type이 아니다.
    //
    // Ended(출차)를 제외하는 이유: Pi의 ended 전이는 직전 관측을 복사한 뒤
    // occupied/plate만 지우고 violation과 ev_state는 그대로 둔다
    // (parking_camera_observation.cpp:485-490). 그래서 위반 차량이 빠져나가면
    // "출차 완료"여야 할 이벤트가 violation=true를 달고 오고, 이 조건이 없으면
    // 차가 이미 없는데 위반 카드가 다시 뜬다.
    bool isParkingViolation() const {
        return source_type != QStringLiteral("stm") && payload.violation &&
               phase != EventPhase::Ended;
    }

    // 화면에 쓰는 심각도. 서버가 카메라 이벤트의 severity를 비워서 보내기
    // 때문에(rtsps include/rtsps/event.h:44 "camera sources currently leave
    // this empty") 위반 이벤트도 그대로 두면 INFO 초록 배지로 묻힌다.
    // 서버 원본 severity는 그대로 보존하고 표시할 때만 승격시킨다 —
    // 나중에 Pi가 WARNING을 채우기 시작해도 결과가 같다.
    EventSeverity effectiveSeverity() const {
        if (isParkingViolation() && severity == EventSeverity::Info) {
            return EventSeverity::Warning;
        }
        return severity;
    }

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
            } else {
                parseCameraPayload(pObj, item);
            }
        }
        return item;
    }

    // 카메라 주차 관측 payload를 읽어 전용 필드와 공용 필드를 함께 채운다.
    // 서버 키 이름(masked_plate/ev_state/occupied/space_id)이 공용 필드 이름과
    // 다르기 때문에 여기서 옮겨 담지 않으면 plate/ev/state/space_label이 전부
    // 빈 값으로 남고, 화면에는 "차량 진입"만 찍힌다.
    static void parseCameraPayload(const QJsonObject &pObj, ParkingEventItem &item) {
        item.camera_payload.space_id = pObj.value(QStringLiteral("space_id")).toString();
        item.camera_payload.camera_space_id = pObj.value(QStringLiteral("camera_space_id")).toString();
        item.camera_payload.occupied = pObj.value(QStringLiteral("occupied")).toBool(false);
        item.camera_payload.parked_ms_ago =
            static_cast<qint64>(pObj.value(QStringLiteral("parked_ms_ago")).toVariant().toLongLong());
        item.camera_payload.plate_present = pObj.value(QStringLiteral("plate_present")).toBool(false);
        item.camera_payload.ev_source = pObj.value(QStringLiteral("ev_source")).toString();
        item.camera_payload.evidence_path = pObj.value(QStringLiteral("evidence_path")).toString();

        // 공용 필드는 서버가 이미 채워 보낸 값이 있으면 그것을 우선한다
        // (스키마가 나중에 가이드 쪽으로 정렬되더라도 이 코드가 덮어쓰지 않게).
        if (item.payload.space_label.isEmpty()) {
            // 결정: 사람이 읽는 라벨을 따로 조회하지 않고 space_id를 그대로 쓴다.
            item.payload.space_label = !item.camera_payload.space_id.isEmpty()
                                           ? item.camera_payload.space_id
                                           : item.camera_payload.camera_space_id;
        }
        if (item.payload.ev.isEmpty()) {
            item.payload.ev = pObj.value(QStringLiteral("ev_state")).toString();
        }
        if (item.payload.plate.isEmpty()) {
            // Pi가 원문을 지우고 "****"만 보낸다 — 여기서 복원할 방법도, 복원할
            // 이유도 없다(개인정보 결정 08).
            item.payload.plate = pObj.value(QStringLiteral("masked_plate")).toString();
        }
        if (item.payload.state.isEmpty() && pObj.contains(QStringLiteral("occupied"))) {
            item.payload.state = item.camera_payload.occupied ? QStringLiteral("occupied")
                                                              : QStringLiteral("vacant");
        }
        if (item.payload.violation && item.payload.violation_reason.isEmpty()) {
            // 서버는 사유 코드를 따로 보내지 않는다. 카메라의 violation 정의가
            // "비EV 차량의 주차"뿐이므로(project-docs/PARKING_EVENTS.md) 그 한
            // 가지로 고정한다.
            item.payload.violation_reason = QStringLiteral("NON_EV_IN_EV_ZONE");
        }
    }
};

struct ParkingEventBatch {
    int schema_version{1};
    quint64 server_time_utc_ms{0};
    quint64 next_after_id{0};
    QVector<ParkingEventItem> events;
};

#endif // PARKING_EVENT_TYPES_H

#pragma once

#include <QMetaType>
#include <QString>
#include <QVector>

// rtsps-codex-hanwha-rtsp-raspberry-pi/include/rtsps/stm_protocol.h:372-375,
// :170-175 의 사본. 프로토콜이 바뀌면 여기도 같이 고쳐야 한다
// (P2.3에서는 프로토콜을 바꾸지 않기로 확정했다).
enum StmOpcode {
    StmStartStage1 = 0x0001,
    StmStartStage2 = 0x0002,
    StmStopActuator = 0x0003,
    StmRearm = 0x0004,
};

enum StmCommandStatusCode {
    StmStatusNone = 0,
    StmStatusAccepted = 1,
    StmStatusRunning = 2,
    StmStatusCompleted = 3,
    StmStatusFailed = 4,
    StmStatusRejected = 5,
};

// JSON "registration": null -> registered=false.
struct StmDeviceRegistrationInfo {
    bool registered = false;
    int channel_id = 0;
    QString space_id;
    QString space_label;
    QString space_type;
};

// JSON "state": null (node.has_base_state == false) -> available=false.
struct StmDeviceState {
    bool available = false;
    double temperature_c = 0.0;
    bool temperature_valid = false;
    bool temperature_stale = false;
    bool temperature_age_available = false;  // "temperature_age_ms": null 대응
    int temperature_age_ms = 0;
    bool occupied = false;
    bool fire_active = false;
    bool sensor_fault = false;
    bool actuator_fault = false;
    bool rearm_required = false;
    int stage1_status = 0;
    int stage2_status = 0;
    QString stage1_status_name;
    QString stage2_status_name;
};

struct StmDevice {
    QString device_uid;
    int slave_address = 0;
    QString link;  // "online" | "offline"
    quint32 assigned_boot_session_id = 0;
    StmDeviceRegistrationInfo registration;
    StmDeviceState state;
};

struct StmCommandSubmitOutcome {
    QString result;  // "submitted" | "reused" | "rejected"
    quint32 command_id = 0;
    int http_status = 0;
};

struct StmCommandStatus {
    quint32 command_id = 0;
    int opcode = 0;
    int origin = 0;
    int status = 0;
    int reason = 0;
    bool completed = false;
};

Q_DECLARE_METATYPE(StmDevice)
Q_DECLARE_METATYPE(QVector<StmDevice>)
Q_DECLARE_METATYPE(StmCommandSubmitOutcome)
Q_DECLARE_METATYPE(StmCommandStatus)

#pragma once

#include <QPointF>
#include <QMetaType>
#include <QString>
#include <QVector>

struct ParkingStmMapping {
    QString device_uid;
    QString sensor_zone_id;
};

struct ParkingSpace {
    QString space_id;
    QString label;
    QString space_type = "general";
    bool enabled = true;
    QVector<QPointF> polygon;
    ParkingStmMapping stm_mapping;
};

struct ParkingValidationError {
    QString code;
    QString message;
    QString space_id;
};

struct ParkingConfiguration {
    QString draft_id;
    int channel_id = 1;
    int base_version = 0;
    int active_version = 0;
    int draft_version = 0;
    QString geometry_id;
    QVector<ParkingSpace> spaces;
    QVector<ParkingValidationError> validation_errors;
};

struct ParkingValidationResult {
    QString draft_id;
    int active_version = 0;
    int draft_version = 0;
    bool valid = false;
    QVector<ParkingValidationError> validation_errors;
};

struct ParkingApplyJob {
    QString job_id;
    QString draft_id;
    QString state;
    QString error_code;
    int active_version = 0;
    int draft_version = 0;
};

Q_DECLARE_METATYPE(ParkingConfiguration)
Q_DECLARE_METATYPE(ParkingValidationResult)
Q_DECLARE_METATYPE(ParkingApplyJob)

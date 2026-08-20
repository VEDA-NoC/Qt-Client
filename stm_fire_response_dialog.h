#pragma once

#include "parking_event_types.h"
#include "stm_types.h"

#include <QDialog>

class ParkingEventStore;
class QLabel;
class QPushButton;
class StmApiClient;

// [화재 진압][닫기][무시] 대응 서브창. 카드가 삭제돼도 영향받지 않도록 이벤트를
// 값으로 들고 있는다 — 호출자가 카드 위젯의 event_ 참조를 넘기면, exec() 동안
// 패널이 갱신될 때 그 카드가 지워지면서 dangling이 된다.
class StmFireResponseDialog : public QDialog {
    Q_OBJECT

public:
    StmFireResponseDialog(ParkingEventItem event,
                          StmApiClient *api,
                          ParkingEventStore *store,
                          QWidget *parent = nullptr);

    // exec() 반환 후 호출자가 확인한다. true면 ignored_critical_ids_에 넣고
    // 패널을 다시 그려야 한다.
    bool ignoreRequested() const { return ignore_requested_; }

private:
    void setupUi();
    void handleSubmitClicked();
    void handleCommandSubmitted(int slave_address, const StmCommandSubmitOutcome &outcome);
    void handleRequestFailed(const QString &operation, const QString &message, int http_status);
    void handleStoreBatchAdded(int count);
    void setStatus(const QString &text, const QString &severity);

    ParkingEventItem event_;
    StmApiClient *api_ = nullptr;      // 소유하지 않는다
    ParkingEventStore *store_ = nullptr;  // 소유하지 않는다

    qint64 submitted_at_utc_ms_ = 0;
    bool command_submitted_ = false;
    bool rejection_detected_ = false;
    bool ignore_requested_ = false;

    QLabel *space_label_ = nullptr;
    QLabel *temperature_label_ = nullptr;
    QLabel *time_label_ = nullptr;
    QLabel *device_label_ = nullptr;
    QLabel *status_label_ = nullptr;
    QPushButton *suppress_button_ = nullptr;
    QPushButton *close_button_ = nullptr;
    QPushButton *ignore_button_ = nullptr;
};

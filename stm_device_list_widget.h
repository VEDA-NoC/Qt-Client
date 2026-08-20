#pragma once

#include "stm_types.h"

#include <QWidget>

class QLabel;
class QTimer;
class QVBoxLayout;
class StmApiClient;

// "장치 계층" 카드 안에 들어가는 등록된 STM 장치 목록. 요구 4가 정한 열은
// [등록 해제] · 이름 · 연결 상태 3개뿐이다(진압 상태는 이름 아래 보조 문구).
// 이 위젯이 그려지는 페이지에 있을 때만 폴링해야 하므로 시작·정지는
// MainWindow가 페이지 전환 시 호출한다.
class StmDeviceListWidget : public QWidget {
    Q_OBJECT

public:
    explicit StmDeviceListWidget(StmApiClient *api, QWidget *parent = nullptr);

public slots:
    void startPolling();
    void stopPolling();

private:
    void handleDevicesReceived(const QVector<StmDevice> &devices);
    void handleRequestFailed(const QString &operation, const QString &message, int http_status);
    void handleDeviceUnregistered(const QString &device_uid, int channel_id, const QString &space_id);
    void rebuildRows();
    void confirmUnregister(const QString &device_uid, const QString &display_name);

    StmApiClient *api_ = nullptr;  // 소유하지 않는다
    QTimer *poll_timer_ = nullptr;
    QVBoxLayout *rows_layout_ = nullptr;
    QLabel *error_label_ = nullptr;
    QVector<StmDevice> registered_devices_;
};

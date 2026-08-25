#pragma once

#include "stm_types.h"

#include <QWidget>

class QLabel;
class QTimer;
class QVBoxLayout;
class StmApiClient;

// "충전 스테이션 관리" 카드 안에 들어가는 STM 장치 목록. 등록된 장치뿐 아니라
// RS-485로 붙어 identity까지 읽혔지만 아직 어느 구역에도 매핑 안 된 장치도
// 보여준다 — 안 그러면 "장치가 아예 안 붙었다"와 "붙었는데 등록만 안 됐다"를
// 구분할 방법이 없다. 요구 4가 정한 열은 [등록 해제] · 이름 · 연결 상태
// 3개뿐이다(진압 상태는 이름 아래 보조 문구).
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
    // 등록·미등록 STM 장치를 모두 보관한다 — 붙어는 있는데 아직 구역에
    // 매핑되지 않은 장치도 여기서 보여야 "STM은 켜져 있는데 등록만 안 됐다"를
    // 구분할 수 있다(요구 3-a).
    QVector<StmDevice> all_devices_;
};

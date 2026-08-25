#include "stm_device_list_widget.h"

#include "stm_api_client.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {

// theme.cpp:93-95의 badge[severity=ok|critical] 색을 그대로 가져온다 —
// 전역 QSS 프로퍼티 대신 인라인으로 쓴다(카드 위젯 방금 생성 시점이라
// unpolish/polish 없이 바로 반영되게).
QString statusBadgeStyle(bool online) {
    return online ? QStringLiteral("background: #E7F5EE; color: #18794E; border-radius: 10px; "
                                    "font-weight: 600; font-size: 8.5pt; padding: 3px 9px;")
                  : QStringLiteral("background: #FDEBEC; color: #B42318; border-radius: 10px; "
                                    "font-weight: 600; font-size: 8.5pt; padding: 3px 9px;");
}

}  // namespace

StmDeviceListWidget::StmDeviceListWidget(StmApiClient *api, QWidget *parent) : QWidget(parent), api_(api) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);

    error_label_ = new QLabel(this);
    error_label_->setWordWrap(true);
    error_label_->setStyleSheet("color: #B42318;");
    error_label_->hide();
    root->addWidget(error_label_);

    rows_layout_ = new QVBoxLayout();
    rows_layout_->setSpacing(6);
    root->addLayout(rows_layout_, 1);

    poll_timer_ = new QTimer(this);
    poll_timer_->setInterval(5000);
    connect(poll_timer_, &QTimer::timeout, this, [this]() { api_->fetchDevices(); });

    connect(api_, &StmApiClient::devicesReceived, this, &StmDeviceListWidget::handleDevicesReceived);
    connect(api_, &StmApiClient::requestFailed, this, &StmDeviceListWidget::handleRequestFailed);
    connect(api_, &StmApiClient::deviceUnregistered, this, &StmDeviceListWidget::handleDeviceUnregistered);

    rebuildRows();
}

void StmDeviceListWidget::startPolling() {
    api_->fetchDevices();
    poll_timer_->start();
}

void StmDeviceListWidget::stopPolling() {
    poll_timer_->stop();
}

void StmDeviceListWidget::handleDevicesReceived(const QVector<StmDevice> &devices) {
    // 등록 여부로 거르지 않는다 — STM이 RS-485로 붙어 identity까지 읽혔지만
    // 아직 어느 구역에도 매핑되지 않은 장치도 그대로 보여야, "장치가 아예 안
    // 붙었다"와 "붙었는데 등록만 안 됐다"를 화면에서 구분할 수 있다.
    all_devices_ = devices;
    error_label_->hide();
    rebuildRows();
}

void StmDeviceListWidget::handleRequestFailed(const QString &operation, const QString &message, int http_status) {
    Q_UNUSED(http_status);
    if (operation != QStringLiteral("stm.devices") && operation != QStringLiteral("stm.registration.delete")) {
        return;
    }
    error_label_->setText(message);
    error_label_->show();
}

void StmDeviceListWidget::handleDeviceUnregistered(const QString &device_uid, int channel_id,
                                                    const QString &space_id) {
    Q_UNUSED(device_uid);
    Q_UNUSED(channel_id);
    Q_UNUSED(space_id);
    api_->fetchDevices();
}

void StmDeviceListWidget::rebuildRows() {
    QLayoutItem *item;
    while ((item = rows_layout_->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    if (all_devices_.isEmpty()) {
        auto *title = new QLabel(QStringLiteral("연결된 STM 장치가 없습니다"), this);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet("font-weight: 600; color: #1D1E37;");
        auto *description = new QLabel(
            QStringLiteral("STM 보드 전원과 RS-485 배선을 확인하세요"), this);
        description->setAlignment(Qt::AlignCenter);
        description->setProperty("muted", true);
        rows_layout_->addWidget(title);
        rows_layout_->addWidget(description);
        return;
    }

    for (const StmDevice &device : all_devices_) {
        auto *row = new QWidget(this);
        auto *row_layout = new QHBoxLayout(row);
        row_layout->setContentsMargins(0, 0, 0, 0);

        const QString device_uid = device.device_uid;
        const bool registered = device.registration.registered;
        const QString display_name = !registered ? QStringLiteral("미등록 STM 장치")
                                     : device.registration.space_label.isEmpty()
                                         ? device.registration.space_id
                                         : device.registration.space_label;

        auto *unregister_button = new QPushButton(QStringLiteral("등록 해제"), row);
        unregister_button->setEnabled(registered);
        if (!registered) {
            unregister_button->setToolTip(
                QStringLiteral("등록된 장치만 해제할 수 있습니다. 이 화면의 주차 구역 관리에서 구역에 연결하세요."));
        }
        connect(unregister_button, &QPushButton::clicked, this,
                [this, device_uid, display_name]() { confirmUnregister(device_uid, display_name); });
        row_layout->addWidget(unregister_button);

        auto *name_col = new QVBoxLayout();
        name_col->setSpacing(2);
        auto *name_label = new QLabel(display_name, row);
        name_label->setStyleSheet("font-weight: 600; color: #1D1E37;");

        // 진압 상태(방재포/펌프)는 별도 열이 아니라 보조 문구로만 덧붙인다
        // (요구 4의 열 구성을 바꾸지 않기 위함, Q4-b 확정).
        QString status_suffix;
        if (registered && device.state.available) {
            if (device.state.stage1_status_name == QStringLiteral("running")) {
                status_suffix = QStringLiteral(" · 방재포 전개됨");
            } else if (device.state.stage2_status_name == QStringLiteral("running")) {
                status_suffix = QStringLiteral(" · 살수 중");
            }
        }
        const QString subtitle_text = registered
            ? QString("CH%1 · slave %2 · %3%4")
                  .arg(device.registration.channel_id)
                  .arg(device.slave_address)
                  .arg(device_uid.left(12), status_suffix)
            : QString("slave %1 · %2 · 이 화면의 주차 구역 관리에서 연결 필요")
                  .arg(device.slave_address)
                  .arg(device_uid.left(12));
        auto *subtitle_label = new QLabel(subtitle_text, row);
        subtitle_label->setProperty("muted", true);
        name_col->addWidget(name_label);
        name_col->addWidget(subtitle_label);
        row_layout->addLayout(name_col, 1);

        auto *status_label = new QLabel(device.link == QStringLiteral("online") ? QStringLiteral("정상") : QStringLiteral("오류"), row);
        status_label->setStyleSheet(statusBadgeStyle(device.link == QStringLiteral("online")));
        row_layout->addWidget(status_label);

        rows_layout_->addWidget(row);
    }
}

void StmDeviceListWidget::confirmUnregister(const QString &device_uid, const QString &display_name) {
    QMessageBox box(QMessageBox::Question, QStringLiteral("등록 해제"),
                    QStringLiteral("%1을(를) 정말 등록 해제하시겠습니까?").arg(display_name), QMessageBox::NoButton,
                    this);
    auto *cancel_button = box.addButton(QStringLiteral("취소"), QMessageBox::RejectRole);
    auto *confirm_button = box.addButton(QStringLiteral("해제"), QMessageBox::DestructiveRole);
    box.setDefaultButton(qobject_cast<QPushButton *>(cancel_button));
    box.exec();
    if (box.clickedButton() == confirm_button) {
        api_->unregisterDevice(device_uid);
    }
}

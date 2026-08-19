#include "stm_fire_response_dialog.h"

#include "parking_event_store.h"
#include "stm_api_client.h"

#include <QDateTime>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTimeZone>
#include <QVBoxLayout>

#include <utility>

namespace {

void refreshWidgetStyle(QWidget *widget) {
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

QString formatEventTime(const ParkingEventItem &event) {
    // STM에는 RTC가 없어 occurred_at_utc_ms는 항상 비어 있다(S1-3) —
    // received_at_utc_ms가 항상 실질적인 발생 시각이다.
    const quint64 ts_ms = event.occurred_at_utc_ms > 0 ? event.occurred_at_utc_ms : event.received_at_utc_ms;
    if (ts_ms == 0) {
        return QStringLiteral("--:--:--");
    }
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(ts_ms), QTimeZone("Asia/Seoul"));
    return dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

}  // namespace

StmFireResponseDialog::StmFireResponseDialog(ParkingEventItem event,
                                              StmApiClient *api,
                                              ParkingEventStore *store,
                                              QWidget *parent)
    : QDialog(parent), event_(std::move(event)), api_(api), store_(store) {
    setupUi();

    if (api_) {
        connect(api_, &StmApiClient::commandSubmitted, this, &StmFireResponseDialog::handleCommandSubmitted);
        connect(api_, &StmApiClient::requestFailed, this, &StmFireResponseDialog::handleRequestFailed);
    }
    if (store_) {
        connect(store_, &ParkingEventStore::batchAdded, this, &StmFireResponseDialog::handleStoreBatchAdded);
    }
}

void StmFireResponseDialog::setupUi() {
    setWindowTitle(QStringLiteral("화재 대응"));
    setMinimumWidth(360);

    auto *layout = new QVBoxLayout(this);

    auto *form = new QFormLayout();
    const QString space =
        event_.payload.space_label.isEmpty()
            ? (event_.channel_id > 0 ? QStringLiteral("CH %1").arg(event_.channel_id) : QStringLiteral("미등록 장치"))
            : event_.payload.space_label;
    space_label_ = new QLabel(space, this);
    temperature_label_ =
        new QLabel(QStringLiteral("%1°C").arg(QString::number(event_.stm_payload.temperature_c, 'f', 1)), this);
    time_label_ = new QLabel(formatEventTime(event_), this);
    device_label_ = new QLabel(QStringLiteral("slave %1").arg(event_.stm_payload.slave_address), this);
    form->addRow(QStringLiteral("구역"), space_label_);
    form->addRow(QStringLiteral("온도"), temperature_label_);
    form->addRow(QStringLiteral("발생 시각"), time_label_);
    form->addRow(QStringLiteral("장치"), device_label_);
    layout->addLayout(form);

    status_label_ = new QLabel(QString(), this);
    status_label_->setWordWrap(true);
    status_label_->setProperty("settingsFeedback", true);
    layout->addWidget(status_label_);

    auto *button_row = new QHBoxLayout();
    suppress_button_ = new QPushButton(QStringLiteral("화재 진압"), this);
    suppress_button_->setProperty("primary", true);
    close_button_ = new QPushButton(QStringLiteral("닫기"), this);
    ignore_button_ = new QPushButton(QStringLiteral("무시"), this);
    button_row->addWidget(suppress_button_);
    button_row->addStretch(1);
    button_row->addWidget(close_button_);
    button_row->addWidget(ignore_button_);
    layout->addLayout(button_row);

    connect(suppress_button_, &QPushButton::clicked, this, &StmFireResponseDialog::handleSubmitClicked);
    connect(close_button_, &QPushButton::clicked, this, &QDialog::reject);
    connect(ignore_button_, &QPushButton::clicked, this, [this]() {
        ignore_requested_ = true;
        reject();
    });
}

void StmFireResponseDialog::handleSubmitClicked() {
    if (!api_) {
        return;
    }
    suppress_button_->setEnabled(false);
    submitted_at_utc_ms_ = QDateTime::currentMSecsSinceEpoch();
    rejection_detected_ = false;
    setStatus(QStringLiteral("진압 명령 전송 중…"), QStringLiteral("warning"));
    api_->submitCommand(event_.stm_payload.slave_address, StmStartStage1);
}

void StmFireResponseDialog::handleCommandSubmitted(int slave_address, const StmCommandSubmitOutcome &outcome) {
    if (slave_address != event_.stm_payload.slave_address) {
        return;
    }
    if (outcome.result == QStringLiteral("submitted") || outcome.result == QStringLiteral("reused")) {
        command_submitted_ = true;
        setStatus(QStringLiteral("진압 명령 전달됨"), QStringLiteral("ok"));
    } else if (outcome.result == QStringLiteral("rejected")) {
        setStatus(QStringLiteral("이미 진행 중인 명령이 있습니다"), QStringLiteral("warning"));
        suppress_button_->setEnabled(true);
    }
}

void StmFireResponseDialog::handleRequestFailed(const QString &operation, const QString &message, int http_status) {
    Q_UNUSED(http_status);
    if (operation != QStringLiteral("stm.command.submit.slave%1").arg(event_.stm_payload.slave_address)) {
        return;
    }
    setStatus(message, QStringLiteral("critical"));
    suppress_button_->setEnabled(true);
}

void StmFireResponseDialog::handleStoreBatchAdded(int count) {
    Q_UNUSED(count);
    // F1-b 이후 정상 경로에서는 rearm_required가 진압을 막지 않는다. stage1이
    // STOPPED/FAILED로 남은 예외에서만 STM이 거부하고 이 이벤트가 온다
    // (actuator_status==7 REARM_REQUIRED, §F1-b). 재무장 안내는 넣지 않는다 —
    // 정상 경로에서는 절대 뜨지 않을 문구라 오히려 혼란을 준다.
    if (!command_submitted_ || rejection_detected_ || !store_) {
        return;
    }
    for (const ParkingEventItem &item : store_->allEvents()) {
        if (item.source_type != QStringLiteral("stm")) continue;
        if (item.event_type != QStringLiteral("stm.stage1_state_changed")) continue;
        if (item.stm_payload.slave_address != event_.stm_payload.slave_address) continue;
        if (item.stm_payload.actuator_status != 7 /* STM_ACTUATOR_REARM_REQUIRED */) continue;
        if (item.received_at_utc_ms < static_cast<quint64>(submitted_at_utc_ms_)) continue;

        rejection_detected_ = true;
        setStatus(QStringLiteral("진압이 거부되었습니다 · 현장 확인 필요"), QStringLiteral("critical"));
        suppress_button_->setEnabled(true);
        break;
    }
}

void StmFireResponseDialog::setStatus(const QString &text, const QString &severity) {
    status_label_->setText(text);
    status_label_->setProperty("severity", severity);
    refreshWidgetStyle(status_label_);
}

#include "parking_event_card_widget.h"
#include <QDateTime>
#include <QMap>
#include <QTimeZone>
#include <QStyleOption>
#include <QPainter>

namespace {

const QString kBadgeDanger = QStringLiteral(
    "background-color: #FFEBEE; color: #D32F2F; border: 1px solid #FFCDD2; border-radius: 10px; font-weight: bold; font-size: 11px;");
const QString kBadgeCaution = QStringLiteral(
    "background-color: #FFF3E0; color: #E65100; border: 1px solid #FFE0B2; border-radius: 10px; font-weight: bold; font-size: 11px;");
const QString kBadgeInfo = QStringLiteral(
    "background-color: #E8F5E9; color: #2E7D32; border: 1px solid #C8E6C9; border-radius: 10px; font-weight: bold; font-size: 11px;");
// STM 진압 상태 변화 전용 4번째 등급. 위험(빨강)·주의(주황)·정보(초록)
// 어디에도 해당하지 않는 "동작 중" 알림이라 별도 톤(파랑)을 쓴다 — 항상
// severity=CRITICAL(§2.9)이라 위험 색으로 그리면 화재 경고와 구분이 안 된다.
const QString kBadgeAction = QStringLiteral(
    "background-color: #E3F2FD; color: #1565C0; border: 1px solid #BBDEFB; border-radius: 10px; font-weight: bold; font-size: 11px;");

// STM_ACTUATOR_* (stm_protocol.h) -> 한글 표시. stm_event_bridge.cpp의
// stm_actuator_status_name()과 같은 순서(0=not_present..7=rearm_required).
QString stmActuatorStatusLabel(int status) {
    switch (status) {
        case 0: return QStringLiteral("미장착");
        case 1: return QStringLiteral("대기");
        case 2: return QStringLiteral("동작 중");
        case 3: return QStringLiteral("완료");
        case 4: return QStringLiteral("실패");
        case 5: return QStringLiteral("정지");
        case 6: return QStringLiteral("인터록");
        case 7: return QStringLiteral("재무장 필요");
        default: return QStringLiteral("알 수 없음");
    }
}

bool isStmStageEvent(const QString &event_type) {
    return event_type == QStringLiteral("stm.stage1_state_changed") ||
           event_type == QStringLiteral("stm.stage2_state_changed");
}

}  // namespace

ParkingEventCardWidget::ParkingEventCardWidget(const ParkingEventItem &event, QWidget *parent)
    : QWidget(parent), event_(event) {
    setupUi();
    updateEvent(event);
}

void ParkingEventCardWidget::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(6);

    // Top Header: Icon + Title | Badge
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);

    icon_label_ = new QLabel(this);
    title_label_ = new QLabel(this);
    title_label_->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: bold; color: #1D1E37;"));

    badge_label_ = new QLabel(this);
    badge_label_->setAlignment(Qt::AlignCenter);
    badge_label_->setContentsMargins(8, 2, 8, 2);

    headerLayout->addWidget(icon_label_);
    headerLayout->addWidget(title_label_);
    headerLayout->addStretch();
    headerLayout->addWidget(badge_label_);

    // Time & Channel Space Info
    time_label_ = new QLabel(this);
    time_label_->setStyleSheet(QStringLiteral("font-size: 12px; color: #666666;"));

    channel_space_label_ = new QLabel(this);
    channel_space_label_->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: 500; color: #353968;"));

    // Detail & Vehicle/EV info
    detail_label_ = new QLabel(this);
    detail_label_->setStyleSheet(QStringLiteral("font-size: 12px; color: #333333;"));

    plate_ev_label_ = new QLabel(this);
    plate_ev_label_->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: bold; color: #F37321;"));

    // Action button (For CRITICAL emergency response)
    action_button_ = new QPushButton(tr("대응 화면 열기"), this);
    action_button_->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #D32F2F; color: white; font-weight: bold; padding: 6px 12px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #B71C1C; }"
    ));
    action_button_->setCursor(Qt::PointingHandCursor);
    action_button_->hide();

    connect(action_button_, &QPushButton::clicked, this, [this]() {
        emit actionTriggered(QStringLiteral("open_response_screen"), event_);
    });

    mainLayout->addLayout(headerLayout);
    mainLayout->addWidget(time_label_);
    mainLayout->addWidget(channel_space_label_);
    mainLayout->addWidget(detail_label_);
    mainLayout->addWidget(plate_ev_label_);
    mainLayout->addWidget(action_button_);

    applyTheme();
}

void ParkingEventCardWidget::updateEvent(const ParkingEventItem &event) {
    event_ = event;
    setCursor(isPinnableFireCard() ? Qt::PointingHandCursor : Qt::ArrowCursor);

    // Time formatting — occurred_at_utc_ms가 0이면 received_at_utc_ms를 fallback으로 사용
    quint64 ts_ms = event_.occurred_at_utc_ms > 0 ? event_.occurred_at_utc_ms : event_.received_at_utc_ms;
    if (ts_ms > 0) {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(ts_ms), QTimeZone("Asia/Seoul"));
        QDate today = QDateTime::currentDateTime().toTimeZone(QTimeZone("Asia/Seoul")).date();
        if (dt.date() == today) {
            time_label_->setText(dt.toString(QStringLiteral("HH:mm:ss")));
        } else if (dt.date().year() == today.year()) {
            time_label_->setText(dt.toString(QStringLiteral("MM/dd HH:mm:ss")));
        } else {
            time_label_->setText(dt.toString(QStringLiteral("yyyy/MM/dd HH:mm:ss")));
        }
    } else {
        time_label_->setText(QStringLiteral("--:--:--"));
    }

    // Channel & Space
    // channel_id<=0은 어느 주차 구역에도 연결되지 않은 STM 장치의 센티널이다
    // (stm_event_bridge.cpp:222) — "CH 0"으로 찍히지 않게 별도 문구를 쓴다.
    QString chStr = event_.channel_id > 0 ? QStringLiteral("CH %1").arg(event_.channel_id)
                                          : QStringLiteral("미등록 장치");
    if (!event_.payload.space_label.isEmpty()) {
        chStr += QStringLiteral(" · %1").arg(event_.payload.space_label);
    }
    channel_space_label_->setText(chStr);

    // Detail & Badge & Icon — source_type("stm") 우선, 그 다음 event_type,
    // 마지막에 severity 순으로 결정한다. STAGE1/2_STATE_CHANGED가 항상
    // CRITICAL이라(§2.9) severity만으로 분기하면 진압 진행 상태 변화까지
    // "화재 경고"로 오표시된다.
    if (event_.source_type == QStringLiteral("stm")) {
        applyStmEvent();
    } else if (event_.severity == EventSeverity::Critical) {
        icon_label_->setText(QStringLiteral("🔥"));
        title_label_->setText(QStringLiteral("화재 경고"));
        badge_label_->setText(QStringLiteral("긴급"));
        badge_label_->setStyleSheet(QStringLiteral("background-color: #FFEBEE; color: #D32F2F; border: 1px solid #FFCDD2; border-radius: 10px; font-weight: bold; font-size: 11px;"));
        detail_label_->setText(event_.payload.state.isEmpty() ? QStringLiteral("연기 감지 경보") : event_.payload.state);
        plate_ev_label_->clear();
        action_button_->show();
    } else if (event_.severity == EventSeverity::Warning) {
        icon_label_->setText(QStringLiteral("🅿️"));
        title_label_->setText(QStringLiteral("주차 칸 점유"));
        badge_label_->setText(QStringLiteral("경고"));
        badge_label_->setStyleSheet(QStringLiteral("background-color: #FFF3E0; color: #E65100; border: 1px solid #FFE0B2; border-radius: 10px; font-weight: bold; font-size: 11px;"));
        
        QString detail = event_.payload.violation ? QStringLiteral("비전기차 충전구역 점유 위반") : QStringLiteral("구역 점유 감지");
        detail_label_->setText(detail);

        QString plateInfo;
        if (!event_.payload.plate.isEmpty()) {
            plateInfo += QStringLiteral("차량: %1").arg(event_.payload.plate);
        }
        if (event_.payload.ev == QStringLiteral("no")) {
            plateInfo += QStringLiteral(" [내연기관]");
        } else if (event_.payload.ev == QStringLiteral("yes")) {
            plateInfo += QStringLiteral(" [EV 전기차]");
        }
        plate_ev_label_->setText(plateInfo);
        action_button_->hide();
    } else {
        icon_label_->setText(QStringLiteral("🚗"));
        title_label_->setText(event_.phase == EventPhase::Ended ? QStringLiteral("출차 완료") : QStringLiteral("차량 진입"));
        badge_label_->setText(QStringLiteral("정보"));
        badge_label_->setStyleSheet(QStringLiteral("background-color: #E8F5E9; color: #2E7D32; border: 1px solid #C8E6C9; border-radius: 10px; font-weight: bold; font-size: 11px;"));

        detail_label_->setText(event_.phase == EventPhase::Ended ? QStringLiteral("차량 진출 완료") : QStringLiteral("차량 진입 감지"));

        QString plateInfo;
        if (!event_.payload.plate.isEmpty()) {
            plateInfo += QStringLiteral("차량: %1").arg(event_.payload.plate);
        }
        if (event_.payload.ev == QStringLiteral("yes")) {
            plateInfo += QStringLiteral(" [EV 전기차]");
        }
        plate_ev_label_->setText(plateInfo);
        action_button_->hide();
    }

    applyTheme();
}

void ParkingEventCardWidget::applyStmEvent() {
    const QString &type = event_.event_type;
    const StmEventPayload &stm = event_.stm_payload;

    if (type == QStringLiteral("stm.fire_started")) {
        const QString temp = QString::number(stm.temperature_c, 'f', 1);
        if (event_.severity == EventSeverity::Critical) {
            icon_label_->setText(QStringLiteral("🔥"));
            title_label_->setText(QStringLiteral("온도 위험"));
            badge_label_->setText(QStringLiteral("위험"));
            badge_label_->setStyleSheet(kBadgeDanger);
            detail_label_->setText(QStringLiteral("%1°C 감지 · 현장 확인 필요").arg(temp));
            // 대응 서브창은 카드 전체 클릭으로 연다(Q3 확정) — 버튼은 중복이라 없앤다.
            action_button_->hide();
        } else {
            icon_label_->setText(QStringLiteral("🌡️"));
            title_label_->setText(QStringLiteral("온도 주의"));
            badge_label_->setText(QStringLiteral("주의"));
            badge_label_->setStyleSheet(kBadgeCaution);
            detail_label_->setText(QStringLiteral("%1°C 감지").arg(temp));
            action_button_->hide();
        }
        plate_ev_label_->clear();
    } else if (type == QStringLiteral("stm.fire_cleared")) {
        // fire_task.c:254-262(위험 해제), :271-277(주의 자동 해제) 둘 다 원래
        // fire_level을 그대로 실어 보낸다 — comm_task.c:227이 그 level로
        // severity를 정하므로 INFO는 나오지 않는다. 위험 해제(CRITICAL)는
        // 현장 물리 버튼을 눌러야만 오는 사건이라 정보성 초록으로 묻으면 안 된다.
        const QString temp = QString::number(stm.temperature_c, 'f', 1);
        icon_label_->setText(QStringLiteral("✅"));
        if (event_.severity == EventSeverity::Critical) {
            title_label_->setText(QStringLiteral("화재 위험 해제"));
            badge_label_->setText(QStringLiteral("위험 해제"));
            badge_label_->setStyleSheet(kBadgeDanger);
            detail_label_->setText(QStringLiteral("%1°C · 현장에서 해제됨").arg(temp));
        } else {
            title_label_->setText(QStringLiteral("온도 주의 해제"));
            badge_label_->setText(QStringLiteral("주의"));
            badge_label_->setStyleSheet(kBadgeCaution);
            detail_label_->setText(QStringLiteral("%1°C · 주의 해제됨").arg(temp));
        }
        plate_ev_label_->clear();
        action_button_->hide();
    } else if (type == QStringLiteral("stm.occupancy_changed")) {
        icon_label_->setText(QStringLiteral("📡"));
        title_label_->setText(QStringLiteral("물체 감지"));
        badge_label_->setText(QStringLiteral("정보"));
        badge_label_->setStyleSheet(kBadgeInfo);
        detail_label_->setText(stm.occupied ? QStringLiteral("물체 감지됨 · 거리 %1mm").arg(stm.distance_mm)
                                            : QStringLiteral("물체 감지 해제"));
        plate_ev_label_->clear();
        action_button_->hide();
    } else if (isStmStageEvent(type)) {
        const bool is_stage1 = type == QStringLiteral("stm.stage1_state_changed");
        icon_label_->setText(QStringLiteral("🧯"));
        title_label_->setText(is_stage1 ? QStringLiteral("진압 1단계 상태") : QStringLiteral("진압 2단계 상태"));
        badge_label_->setText(QStringLiteral("동작"));
        badge_label_->setStyleSheet(kBadgeAction);
        QString detail = QStringLiteral("%1 · %2").arg(is_stage1 ? QStringLiteral("방재포") : QStringLiteral("살수 펌프"),
                                                        stmActuatorStatusLabel(stm.actuator_status));
        if (stm.origin == 1) {
            detail += QStringLiteral(" (원격 명령)");
        }
        detail_label_->setText(detail);
        plate_ev_label_->clear();
        // 고정·대응 서브창 대상이 아니다 — 상단 고정은 fire_started+CRITICAL만
        // 해당한다(§2.9). 그 판정과 대응 버튼 노출은 여기서 일치시켜 둔다.
        action_button_->hide();
    } else {
        // stm.sensor_fault / actuator_fault / device_restarted / queue_overflow 등.
        // events 응답에 source_event_name이 없어(control_server.cpp:633-651)
        // event_type raw 문자열을 그대로 노출할 수 없다 — 고정 문구로 둔다.
        static const QMap<QString, QString> kFallbackTitles = {
            {QStringLiteral("stm.sensor_fault"), QStringLiteral("센서 이상")},
            {QStringLiteral("stm.actuator_fault"), QStringLiteral("액추에이터 이상")},
            {QStringLiteral("stm.device_restarted"), QStringLiteral("장치 재시작")},
            {QStringLiteral("stm.queue_overflow"), QStringLiteral("이벤트 유실 발생")},
        };
        static const QMap<QString, QString> kFallbackDetails = {
            {QStringLiteral("stm.sensor_fault"), QStringLiteral("온도 센서 읽기 실패")},
            {QStringLiteral("stm.actuator_fault"), QStringLiteral("방재포·펌프 구동 이상 감지")},
            {QStringLiteral("stm.device_restarted"), QStringLiteral("장치가 재시작됨 · 상태 확인 필요")},
            {QStringLiteral("stm.queue_overflow"), QStringLiteral("이벤트 처리 지연으로 일부 유실됨")},
        };
        icon_label_->setText(QStringLiteral("⚠️"));
        title_label_->setText(kFallbackTitles.value(type, QStringLiteral("STM 이벤트")));
        if (event_.severity == EventSeverity::Critical) {
            badge_label_->setText(QStringLiteral("위험"));
            badge_label_->setStyleSheet(kBadgeDanger);
        } else if (event_.severity == EventSeverity::Warning) {
            badge_label_->setText(QStringLiteral("주의"));
            badge_label_->setStyleSheet(kBadgeCaution);
        } else {
            badge_label_->setText(QStringLiteral("정보"));
            badge_label_->setStyleSheet(kBadgeInfo);
        }
        detail_label_->setText(kFallbackDetails.value(type, QStringLiteral("알 수 없는 STM 이벤트")));
        plate_ev_label_->clear();
        action_button_->hide();
    }
}

void ParkingEventCardWidget::applyTheme() {
    QString borderCol = QStringLiteral("#E0E0E0");
    QString bgCol = QStringLiteral("#FFFFFF");

    // stage1/2_state_changed는 severity가 항상 CRITICAL이라(§2.9) 그 색으로
    // 그리면 화재 위험 카드와 구분되지 않는다. badge와 같은 4번째 톤(파랑)을
    // 여기서도 먼저 검사한다.
    if (event_.source_type == QStringLiteral("stm") && isStmStageEvent(event_.event_type)) {
        borderCol = QStringLiteral("#BBDEFB");
        bgCol = QStringLiteral("#F5FAFF");
    } else if (event_.severity == EventSeverity::Critical) {
        borderCol = QStringLiteral("#FFCDD2");
        bgCol = QStringLiteral("#FFF5F5");
    } else if (event_.severity == EventSeverity::Warning) {
        borderCol = QStringLiteral("#FFE0B2");
        bgCol = QStringLiteral("#FAFAFA");
    }

    setStyleSheet(QString(QStringLiteral(
        "ParkingEventCardWidget { background-color: %1; border: 1px solid %2; border-radius: 8px; }"
    )).arg(bgCol, borderCol));
}

void ParkingEventCardWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

bool ParkingEventCardWidget::isPinnableFireCard() const {
    return event_.source_type == QStringLiteral("stm") && event_.event_type == QStringLiteral("stm.fire_started") &&
           event_.severity == EventSeverity::Critical;
}

void ParkingEventCardWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && isPinnableFireCard()) {
        emit actionTriggered(QStringLiteral("open_response_screen"), event_);
    }
    QWidget::mousePressEvent(event);
}

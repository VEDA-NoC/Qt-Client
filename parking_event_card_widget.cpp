#include "parking_event_card_widget.h"
#include <QDateTime>
#include <QTimeZone>
#include <QStyleOption>
#include <QPainter>

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
    QString chStr = QStringLiteral("CH %1").arg(event_.channel_id);
    if (!event_.payload.space_label.isEmpty()) {
        chStr += QStringLiteral(" · %1").arg(event_.payload.space_label);
    }
    channel_space_label_->setText(chStr);

    // Detail & Badge & Icon based on Severity
    if (event_.severity == EventSeverity::Critical) {
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

void ParkingEventCardWidget::applyTheme() {
    QString borderCol = QStringLiteral("#E0E0E0");
    QString bgCol = QStringLiteral("#FFFFFF");

    if (event_.severity == EventSeverity::Critical) {
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

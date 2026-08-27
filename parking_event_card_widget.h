#ifndef PARKING_EVENT_CARD_WIDGET_H
#define PARKING_EVENT_CARD_WIDGET_H

#include <QWidget>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPaintEvent>
#include "parking_event_types.h"

// 카드와 이벤트 탭 테이블이 같은 문구를 쓰도록 공유하는 표시 헬퍼.
// 두 화면이 각자 문자열을 만들면 같은 이벤트가 서로 다르게 설명된다.
namespace parking_event_text {

// "내연기관 · 번호판 ****" 형태의 차량/EV 한 줄 요약.
// 번호판 원문은 Pi가 마스킹해서 보내므로 절대 나오지 않는다.
QString vehicleSummary(const ParkingEventItem &event);

// 점유 경과 시간("32분", "1시간 5분"). 미점유이거나 값이 없으면 빈 문자열.
QString parkedDuration(const ParkingEventItem &event);

// 이벤트 내용 한 줄. 위반이면 위반 문구, 아니면 점유/출차 상태 문구.
QString detailLine(const ParkingEventItem &event);

}  // namespace parking_event_text

class ParkingEventCardWidget : public QWidget {
    Q_OBJECT
public:
    explicit ParkingEventCardWidget(const ParkingEventItem &event, QWidget *parent = nullptr);

    void updateEvent(const ParkingEventItem &event);
    quint64 eventId() const { return event_.event_id; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

signals:
    void actionTriggered(const QString &actionType, const ParkingEventItem &event);

private:
    void setupUi();
    void applyStmEvent();
    void applyTheme();
    // 고정 영역에만 나타나는 종류(stm.fire_started && CRITICAL)인지 — Q3에서
    // 이 종류만 카드 전체 클릭으로 대응 서브창을 연다.
    bool isPinnableFireCard() const;

    ParkingEventItem event_;
    QLabel *icon_label_{nullptr};
    QLabel *title_label_{nullptr};
    QLabel *badge_label_{nullptr};
    QLabel *time_label_{nullptr};
    QLabel *channel_space_label_{nullptr};
    QLabel *detail_label_{nullptr};
    QLabel *plate_ev_label_{nullptr};
    QPushButton *action_button_{nullptr};
};

#endif // PARKING_EVENT_CARD_WIDGET_H

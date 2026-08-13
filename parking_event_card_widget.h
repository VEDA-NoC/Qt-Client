#ifndef PARKING_EVENT_CARD_WIDGET_H
#define PARKING_EVENT_CARD_WIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPaintEvent>
#include "parking_event_types.h"

class ParkingEventCardWidget : public QWidget {
    Q_OBJECT
public:
    explicit ParkingEventCardWidget(const ParkingEventItem &event, QWidget *parent = nullptr);

    void updateEvent(const ParkingEventItem &event);
    quint64 eventId() const { return event_.event_id; }

protected:
    void paintEvent(QPaintEvent *event) override;

signals:
    void actionTriggered(const QString &actionType, const ParkingEventItem &event);

private:
    void setupUi();
    void applyTheme();

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

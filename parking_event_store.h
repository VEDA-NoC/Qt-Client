#ifndef PARKING_EVENT_STORE_H
#define PARKING_EVENT_STORE_H

#include <QObject>
#include <QVector>
#include <QMap>
#include "parking_event_types.h"

class ParkingEventStore : public QObject {
    Q_OBJECT
public:
    explicit ParkingEventStore(QObject *parent = nullptr);

    void addEvents(const QVector<ParkingEventItem> &events);
    void addEvent(const ParkingEventItem &item);
    void setAcked(quint64 event_id, bool acked);
    void clear();

    const QVector<ParkingEventItem>& allEvents() const { return events_; }
    QVector<ParkingEventItem> recentEvents(int count = 10) const;

signals:
    void eventAdded(const ParkingEventItem &item);
    void batchAdded(int count);      // addEvents 완료 후 1회 방출
    void eventUpdated(const ParkingEventItem &item);
    void storeCleared();

private:
    QVector<ParkingEventItem> events_;
    QMap<quint64, int> id_to_index_map_;
};

#endif // PARKING_EVENT_STORE_H

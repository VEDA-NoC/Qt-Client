#include "parking_event_store.h"
#include <algorithm>

ParkingEventStore::ParkingEventStore(QObject *parent)
    : QObject(parent) {}

void ParkingEventStore::addEvents(const QVector<ParkingEventItem> &events) {
    if (events.isEmpty()) return;
    int changed = 0;
    for (const auto &item : events) {
        if (id_to_index_map_.contains(item.event_id)) {
            int idx = id_to_index_map_[item.event_id];
            events_[idx] = item;
            ++changed;
        } else {
            events_.append(item);
            id_to_index_map_[item.event_id] = events_.size() - 1;
            ++changed;
        }
    }
    if (changed > 0) {
        // event_id 또는 occurred_at_utc_ms 기준 오름차순 정렬 (가장 최신 이벤트가 맨 뒤에 위치)
        std::sort(events_.begin(), events_.end(), [](const ParkingEventItem &a, const ParkingEventItem &b) {
            quint64 tsA = a.occurred_at_utc_ms > 0 ? a.occurred_at_utc_ms : a.received_at_utc_ms;
            quint64 tsB = b.occurred_at_utc_ms > 0 ? b.occurred_at_utc_ms : b.received_at_utc_ms;
            if (tsA != tsB) return tsA < tsB;
            return a.event_id < b.event_id;
        });
        id_to_index_map_.clear();
        for (int i = 0; i < events_.size(); ++i) {
            id_to_index_map_[events_[i].event_id] = i;
        }
        emit batchAdded(changed);
    }
}

void ParkingEventStore::addEvent(const ParkingEventItem &item) {
    if (id_to_index_map_.contains(item.event_id)) {
        int idx = id_to_index_map_[item.event_id];
        events_[idx] = item;
        emit eventUpdated(events_[idx]);
    } else {
        events_.append(item);
        id_to_index_map_[item.event_id] = events_.size() - 1;
        emit eventAdded(item);
    }
}

void ParkingEventStore::setAcked(quint64 event_id, bool acked) {
    if (id_to_index_map_.contains(event_id)) {
        int idx = id_to_index_map_[event_id];
        events_[idx].acked = acked;
        emit eventUpdated(events_[idx]);
    }
}

void ParkingEventStore::clear() {
    events_.clear();
    id_to_index_map_.clear();
    emit storeCleared();
}

QVector<ParkingEventItem> ParkingEventStore::recentEvents(int count) const {
    if (events_.isEmpty() || count <= 0) {
        return {};
    }
    int total = events_.size();
    int start = qMax(0, total - count);
    QVector<ParkingEventItem> res;
    for (int i = total - 1; i >= start; --i) {
        res.append(events_[i]);
    }
    return res;
}

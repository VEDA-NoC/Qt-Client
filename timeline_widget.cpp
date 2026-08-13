#include "timeline_widget.h"

#include <QDateTime>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QTimeZone>
#include <QWheelEvent>

#include <algorithm>

namespace {

constexpr int kChannelCount = 4;
constexpr qreal kLabelWidth = 58.0;
constexpr qreal kTrackTop = 64.0;
constexpr qreal kTrackHeight = 24.0;
constexpr qreal kRowPitch = 43.0;
constexpr qreal kTargetGridSpacing = 105.0;

QString localAxisLabel(qint64 utc_ms, bool include_date) {
    const QString format = include_date ? "MM-dd HH:mm" : "HH:mm";
    return QDateTime::fromMSecsSinceEpoch(utc_ms, QTimeZone::UTC)
        .toTimeZone(QTimeZone("Asia/Seoul"))
        .toString(format);
}

QColor eventColor(const PlaybackTimelineEvent &event) {
    if (event.severity.compare("critical", Qt::CaseInsensitive) == 0 ||
        event.event_type.contains("fire", Qt::CaseInsensitive)) {
        return QColor("#D92D20");
    }
    if (event.severity.compare("warning", Qt::CaseInsensitive) == 0 ||
        event.event_type.contains("parking", Qt::CaseInsensitive)) {
        return QColor("#F79009");
    }
    return QColor("#F37321");
}

qint64 chooseGridInterval(qint64 duration_ms, qreal width) {
    static const qint64 intervals[] = {
        5'000,       10'000,            15'000,
        30'000,      60'000,            2 * 60'000,
        5 * 60'000,
        10 * 60'000, 15 * 60'000,      30 * 60'000,
        60 * 60'000, 2 * 60 * 60'000,  3 * 60 * 60'000,
        6 * 60 * 60'000, 12 * 60 * 60'000, 24 * 60 * 60'000};
    const qreal target_count = qMax(2.0, width / kTargetGridSpacing);
    const qint64 desired =
        qMax<qint64>(1, static_cast<qint64>(duration_ms / target_count));
    for (const qint64 interval : intervals) {
        if (interval >= desired) {
            return interval;
        }
    }
    return intervals[std::size(intervals) - 1];
}

}  // namespace

TimelineWidget::TimelineWidget(QWidget *parent) : QWidget(parent) {
    setCursor(Qt::PointingHandCursor);
    setAccessibleName("4채널 녹화 타임라인");
    channel_states_.fill("조회 중", kChannelCount);
}

void TimelineWidget::setQueryRange(qint64 start_utc_ms, qint64 end_utc_ms) {
    query_start_utc_ms_ = start_utc_ms;
    query_end_utc_ms_ = end_utc_ms;
    visible_start_utc_ms_ = start_utc_ms;
    visible_end_utc_ms_ = end_utc_ms;
    clearSelection();
    update();
}

void TimelineWidget::extendQueryRange(qint64 start_utc_ms,
                                      qint64 end_utc_ms) {
    query_start_utc_ms_ = start_utc_ms;
    query_end_utc_ms_ = end_utc_ms;
    update();
}

void TimelineWidget::setVisibleRange(qint64 start_utc_ms,
                                     qint64 end_utc_ms) {
    if (query_end_utc_ms_ <= query_start_utc_ms_ ||
        end_utc_ms <= start_utc_ms) {
        return;
    }
    const qint64 duration =
        qMin(end_utc_ms - start_utc_ms,
             query_end_utc_ms_ - query_start_utc_ms_);
    qint64 start = start_utc_ms;
    qint64 end = start + duration;
    if (start < query_start_utc_ms_) {
        start = query_start_utc_ms_;
        end = start + duration;
    }
    if (end > query_end_utc_ms_) {
        end = query_end_utc_ms_;
        start = end - duration;
    }
    visible_start_utc_ms_ = start;
    visible_end_utc_ms_ = end;
    pivot_utc_ms_ = start + duration / 3;
    update();
    emit visibleRangeChanged(start, end, pivot_utc_ms_);
}

void TimelineWidget::shiftVisibleBy(qint64 delta_ms) {
    setVisibleRange(visible_start_utc_ms_ + delta_ms,
                    visible_end_utc_ms_ + delta_ms);
}

void TimelineWidget::setPivotTime(qint64 pivot_utc_ms) {
    pivot_utc_ms_ = pivot_utc_ms;
    update();
}

void TimelineWidget::setEmptyMessage(const QString &message) {
    empty_message_ = message;
    update();
}

void TimelineWidget::setTimeline(const PlaybackTimeline &timeline) {
    for (PlaybackTimeline &stored : timelines_) {
        if (stored.channel_id == timeline.channel_id) {
            stored.start_utc_ms =
                qMin(stored.start_utc_ms, timeline.start_utc_ms);
            stored.end_utc_ms =
                qMax(stored.end_utc_ms, timeline.end_utc_ms);
            for (const PlaybackTimelineSpan &span : timeline.spans) {
                const auto duplicate =
                    std::find_if(stored.spans.cbegin(),
                                 stored.spans.cend(),
                                 [&span](const PlaybackTimelineSpan &item) {
                                     return item.kind == span.kind &&
                                            item.start_utc_ms ==
                                                span.start_utc_ms &&
                                            item.end_utc_ms ==
                                                span.end_utc_ms;
                                 });
                if (duplicate == stored.spans.cend()) {
                    stored.spans.push_back(span);
                }
            }
            for (const PlaybackTimelineEvent &event : timeline.events) {
                const auto duplicate =
                    std::find_if(stored.events.cbegin(),
                                 stored.events.cend(),
                                 [&event](const PlaybackTimelineEvent &item) {
                                     return item.request_id ==
                                                event.request_id &&
                                            item.start_utc_ms ==
                                                event.start_utc_ms;
                                 });
                if (duplicate == stored.events.cend()) {
                    stored.events.push_back(event);
                }
            }
            channel_states_[timeline.channel_id - 1] = "완료";
            update();
            return;
        }
    }
    timelines_.push_back(timeline);
    if (timeline.channel_id >= 1 && timeline.channel_id <= kChannelCount) {
        channel_states_[timeline.channel_id - 1] = "완료";
    }
    std::sort(timelines_.begin(),
              timelines_.end(),
              [](const PlaybackTimeline &left, const PlaybackTimeline &right) {
                  return left.channel_id < right.channel_id;
              });
    update();
}

void TimelineWidget::beginChannelQuery(bool clear_existing) {
    if (clear_existing) {
        timelines_.clear();
    }
    channel_states_.fill("조회 중", kChannelCount);
    update();
}

void TimelineWidget::setChannelError(int channel_id) {
    if (channel_id >= 1 && channel_id <= kChannelCount) {
        channel_states_[channel_id - 1] = "조회 실패";
        update();
    }
}

void TimelineWidget::pruneToRange(qint64 start_utc_ms,
                                  qint64 end_utc_ms) {
    for (PlaybackTimeline &timeline : timelines_) {
        timeline.start_utc_ms = start_utc_ms;
        timeline.end_utc_ms = end_utc_ms;
        timeline.spans.erase(
            std::remove_if(timeline.spans.begin(),
                           timeline.spans.end(),
                           [start_utc_ms,
                            end_utc_ms](const PlaybackTimelineSpan &span) {
                               return span.end_utc_ms <= start_utc_ms ||
                                      span.start_utc_ms >= end_utc_ms;
                           }),
            timeline.spans.end());
        timeline.events.erase(
            std::remove_if(timeline.events.begin(),
                           timeline.events.end(),
                           [start_utc_ms,
                            end_utc_ms](const PlaybackTimelineEvent &event) {
                               return event.end_utc_ms <= start_utc_ms ||
                                      event.start_utc_ms >= end_utc_ms;
                           }),
            timeline.events.end());
    }
    update();
}

qint64 TimelineWidget::visibleStartUtcMs() const {
    return visible_start_utc_ms_;
}

qint64 TimelineWidget::visibleEndUtcMs() const {
    return visible_end_utc_ms_;
}

void TimelineWidget::clearTimeline() {
    timelines_.clear();
    query_start_utc_ms_ = 0;
    query_end_utc_ms_ = 0;
    visible_start_utc_ms_ = 0;
    visible_end_utc_ms_ = 0;
    pivot_utc_ms_ = 0;
    channel_states_.fill("조회 중", kChannelCount);
    clearSelection();
    update();
}

void TimelineWidget::clearSelection() {
    selection_channel_id_ = 0;
    selection_utc_ms_ = 0;
    update();
}

bool TimelineWidget::selectTime(int channel_id,
                                qint64 utc_ms,
                                bool user_initiated) {
    const PlaybackTimelineSpan *span = recordingSpanAt(channel_id, utc_ms);
    if (!span) {
        return false;
    }
    selection_channel_id_ = channel_id;
    selection_utc_ms_ = qBound(span->start_utc_ms,
                               utc_ms,
                               span->end_utc_ms - 1);
    update();
    emit timeSelected(channel_id,
                      selection_utc_ms_,
                      span->end_utc_ms,
                      user_initiated);
    return true;
}

void TimelineWidget::zoomIn() {
    zoomAt(0.5,
           pivot_utc_ms_ > 0
               ? pivot_utc_ms_
               : (visible_start_utc_ms_ + visible_end_utc_ms_) / 2);
}

void TimelineWidget::zoomOut() {
    zoomAt(2.0,
           pivot_utc_ms_ > 0
               ? pivot_utc_ms_
               : (visible_start_utc_ms_ + visible_end_utc_ms_) / 2);
}

void TimelineWidget::resetZoom() {
    visible_start_utc_ms_ = query_start_utc_ms_;
    visible_end_utc_ms_ = query_end_utc_ms_;
    pivot_utc_ms_ =
        visible_start_utc_ms_ +
        (visible_end_utc_ms_ - visible_start_utc_ms_) / 3;
    update();
    emit visibleRangeChanged(visible_start_utc_ms_,
                             visible_end_utc_ms_,
                             pivot_utc_ms_);
}

QSize TimelineWidget::minimumSizeHint() const {
    return {520, 260};
}

QSize TimelineWidget::sizeHint() const {
    return {1100, 270};
}

QRectF TimelineWidget::trackRect(int row) const {
    return QRectF(kLabelWidth,
                  kTrackTop + row * kRowPitch,
                  qMax(1.0, width() - kLabelWidth - 18.0),
                  kTrackHeight);
}

qint64 TimelineWidget::timeAtX(qreal x) const {
    const QRectF track = trackRect(0);
    if (visible_end_utc_ms_ <= visible_start_utc_ms_) {
        return 0;
    }
    const qreal ratio = qBound(0.0, (x - track.left()) / track.width(), 1.0);
    return visible_start_utc_ms_ +
           static_cast<qint64>(
               ratio * (visible_end_utc_ms_ - visible_start_utc_ms_));
}

qreal TimelineWidget::xAtTime(qint64 utc_ms) const {
    const QRectF track = trackRect(0);
    if (visible_end_utc_ms_ <= visible_start_utc_ms_) {
        return track.left();
    }
    const qreal ratio =
        static_cast<qreal>(utc_ms - visible_start_utc_ms_) /
        static_cast<qreal>(visible_end_utc_ms_ - visible_start_utc_ms_);
    return track.left() + qBound(0.0, ratio, 1.0) * track.width();
}

const PlaybackTimeline *TimelineWidget::timelineForChannel(int channel_id) const {
    for (const PlaybackTimeline &timeline : timelines_) {
        if (timeline.channel_id == channel_id) {
            return &timeline;
        }
    }
    return nullptr;
}

const PlaybackTimelineSpan *TimelineWidget::recordingSpanAt(
    int channel_id,
    qint64 utc_ms) const {
    const PlaybackTimeline *timeline = timelineForChannel(channel_id);
    if (!timeline) {
        return nullptr;
    }
    for (const PlaybackTimelineSpan &span : timeline->spans) {
        if (span.isRecording() && utc_ms >= span.start_utc_ms &&
            utc_ms < span.end_utc_ms) {
            return &span;
        }
    }
    return nullptr;
}

void TimelineWidget::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (visible_end_utc_ms_ <= visible_start_utc_ms_) {
        painter.setPen(QColor("#71758A"));
        painter.drawText(rect(),
                         Qt::AlignCenter | Qt::TextWordWrap,
                         empty_message_);
        return;
    }

    const QRectF first_track = trackRect(0);
    const QRectF last_track = trackRect(kChannelCount - 1);
    const qint64 visible_duration =
        visible_end_utc_ms_ - visible_start_utc_ms_;

    QDate boundary_date =
        QDateTime::fromMSecsSinceEpoch(visible_start_utc_ms_,
                                       QTimeZone::UTC)
            .toTimeZone(QTimeZone("Asia/Seoul"))
            .date()
            .addDays(1);
    const QDate last_visible_date =
        QDateTime::fromMSecsSinceEpoch(visible_end_utc_ms_,
                                       QTimeZone::UTC)
            .toTimeZone(QTimeZone("Asia/Seoul"))
            .date();
    while (boundary_date <= last_visible_date) {
        const qint64 boundary_utc_ms =
            QDateTime(boundary_date, QTime(0, 0))
                .toUTC()
                .toMSecsSinceEpoch();
        const qreal x = xAtTime(boundary_utc_ms);
        painter.setPen(QPen(QColor("#B8BDC8"), 1.5));
        painter.drawLine(QPointF(x, first_track.top() - 8.0),
                         QPointF(x, last_track.bottom() + 8.0));
        painter.setPen(QColor("#475467"));
        painter.drawText(QRectF(x - 55.0, 2.0, 110.0, 20.0),
                         Qt::AlignHCenter | Qt::AlignVCenter,
                         boundary_date.toString("MM-dd"));
        boundary_date = boundary_date.addDays(1);
    }

    const qint64 grid_interval =
        chooseGridInterval(visible_duration, first_track.width());
    const qint64 first_tick =
        ((visible_start_utc_ms_ + grid_interval - 1) / grid_interval) *
        grid_interval;
    qreal last_label_right = first_track.left() - 8.0;
    for (qint64 tick_ms = first_tick; tick_ms <= visible_end_utc_ms_;
         tick_ms += grid_interval) {
        const qreal x = xAtTime(tick_ms);
        const QDateTime tick_local =
            QDateTime::fromMSecsSinceEpoch(tick_ms, QTimeZone::UTC)
                .toTimeZone(QTimeZone("Asia/Seoul"));
        const QDateTime previous_local =
            QDateTime::fromMSecsSinceEpoch(tick_ms - grid_interval,
                                           QTimeZone::UTC)
                .toTimeZone(QTimeZone("Asia/Seoul"));
        const bool date_boundary =
            tick_local.date() != previous_local.date();
        painter.setPen(QPen(date_boundary ? QColor("#C7CBD4")
                                         : QColor("#E4E7EC"),
                            date_boundary ? 1.5 : 1.0));
        painter.drawLine(QPointF(x, first_track.top() - 8.0),
                         QPointF(x, last_track.bottom() + 8.0));
        painter.setPen(date_boundary ? QColor("#475467")
                                     : QColor("#667085"));
        const QString tick_text =
            tick_local.toString(grid_interval < 60'000
                                    ? "HH:mm:ss"
                                    : "HH:mm");
        const qreal label_width =
            painter.fontMetrics().horizontalAdvance(tick_text) + 14.0;
        const qreal label_left = x - label_width / 2.0;
        if (label_left >= last_label_right &&
            x + label_width / 2.0 <= first_track.right() + 1.0) {
            painter.drawText(
                QRectF(label_left, 25.0, label_width, 20.0),
                Qt::AlignHCenter | Qt::AlignVCenter,
                tick_text);
            last_label_right = x + label_width / 2.0 + 8.0;
        }
    }

    if (pivot_utc_ms_ >= visible_start_utc_ms_ &&
        pivot_utc_ms_ <= visible_end_utc_ms_) {
        const qreal pivot_x = xAtTime(pivot_utc_ms_);
        painter.setPen(QPen(QColor("#98A2B3"), 1.5, Qt::DashLine));
        painter.drawLine(QPointF(pivot_x, first_track.top() - 12.0),
                         QPointF(pivot_x, last_track.bottom() + 8.0));
        painter.setPen(QColor("#667085"));
        painter.drawText(QRectF(pivot_x - 40.0,
                                first_track.top() - 18.0,
                                80.0,
                                18.0),
                         Qt::AlignHCenter | Qt::AlignVCenter,
                         "기준");
    }

    for (int row = 0; row < kChannelCount; ++row) {
        const int channel_id = row + 1;
        const QRectF track = trackRect(row);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#ECEEF3"));
        painter.drawRoundedRect(track, 4.0, 4.0);

        painter.setPen(QColor("#25283D"));
        painter.drawText(QRectF(0.0, track.top(), kLabelWidth - 10.0,
                                track.height()),
                         Qt::AlignRight | Qt::AlignVCenter,
                         QString("CH %1").arg(channel_id));

        const PlaybackTimeline *timeline =
            timelineForChannel(channel_id);
        if (!timeline) {
            painter.setPen(QColor("#98A2B3"));
            painter.drawText(track,
                             Qt::AlignCenter,
                             channel_states_.value(row, "조회 중"));
            continue;
        }

        bool has_visible_recording = false;
        for (const PlaybackTimelineSpan &span : timeline->spans) {
            if (!span.isRecording()) {
                continue;
            }
            if (span.end_utc_ms <= visible_start_utc_ms_ ||
                span.start_utc_ms >= visible_end_utc_ms_) {
                continue;
            }
            has_visible_recording = true;
            const qreal left = xAtTime(span.start_utc_ms);
            const qreal right = xAtTime(span.end_utc_ms);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor("#343958"));
            painter.drawRoundedRect(
                QRectF(left, track.top(), qMax(1.0, right - left),
                       track.height()),
                3.0,
                3.0);
        }

        if (!has_visible_recording) {
            painter.setPen(QColor("#98A2B3"));
            painter.drawText(track,
                             Qt::AlignCenter,
                             channel_states_.value(row) == "조회 실패"
                                 ? "조회 실패"
                                 : "녹화 없음");
        }

        for (const PlaybackTimelineEvent &event_item : timeline->events) {
            if (event_item.end_utc_ms <= visible_start_utc_ms_ ||
                event_item.start_utc_ms >= visible_end_utc_ms_) {
                continue;
            }
            const qreal left = xAtTime(event_item.start_utc_ms);
            const qreal right = xAtTime(event_item.end_utc_ms);
            const QColor color = eventColor(event_item);
            painter.setPen(QPen(color, 2.0));
            painter.drawLine(QPointF(left, track.top() - 4.0),
                             QPointF(left, track.bottom() + 4.0));
            if (right - left >= 3.0) {
                painter.setPen(QPen(color, 3.0, Qt::SolidLine,
                                    Qt::RoundCap));
                painter.drawLine(QPointF(left, track.top() - 4.0),
                                 QPointF(right, track.top() - 4.0));
            }
        }
    }

    if (selection_channel_id_ > 0 &&
        selection_utc_ms_ >= visible_start_utc_ms_ &&
        selection_utc_ms_ <= visible_end_utc_ms_) {
        const qreal x = xAtTime(selection_utc_ms_);
        painter.setPen(QPen(QColor("#F37321"), 2.0));
        painter.drawLine(QPointF(x, first_track.top() - 8.0),
                         QPointF(x, last_track.bottom() + 8.0));
        painter.setBrush(QColor("#F37321"));
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(
            QPolygonF({QPointF(x - 6.0, first_track.top() - 8.0),
                       QPointF(x + 6.0, first_track.top() - 8.0),
                       QPointF(x, first_track.top() - 1.0)}));

        painter.setPen(QColor("#C84B08"));
        painter.drawText(
            QRectF(qBound(first_track.left(),
                   x - 95.0,
                          first_track.right() - 190.0),
                   last_track.bottom() + 5.0,
                   190.0,
                   20.0),
            Qt::AlignCenter,
            QString("CH %1 · %2")
                .arg(selection_channel_id_)
                .arg(localAxisLabel(selection_utc_ms_, true)));
    }
}

void TimelineWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    for (int row = 0; row < kChannelCount; ++row) {
        if (trackRect(row).contains(event->position())) {
            selectTime(row + 1,
                       timeAtX(event->position().x()),
                       true);
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void TimelineWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    for (int row = 0; row < kChannelCount; ++row) {
        if (!trackRect(row).contains(event->position())) {
            continue;
        }
        const qint64 utc_ms = timeAtX(event->position().x());
        if (selectTime(row + 1, utc_ms, true)) {
            const PlaybackTimelineSpan *span =
                recordingSpanAt(row + 1, utc_ms);
            if (span) {
                emit timeActivated(row + 1,
                                   selection_utc_ms_,
                                   span->end_utc_ms);
            }
        }
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void TimelineWidget::wheelEvent(QWheelEvent *event) {
    if (visible_end_utc_ms_ <= visible_start_utc_ms_) {
        QWidget::wheelEvent(event);
        return;
    }
    if (!(event->modifiers() & Qt::ControlModifier)) {
        const qint64 duration =
            visible_end_utc_ms_ - visible_start_utc_ms_;
        const int direction = event->angleDelta().y() > 0 ? -1 : 1;
        shiftVisibleBy(direction * qMax<qint64>(1000, duration / 10));
        event->accept();
        return;
    }
    const qint64 center = timeAtX(event->position().x());
    zoomAt(event->angleDelta().y() > 0 ? 0.8 : 1.25, center);
    event->accept();
}

void TimelineWidget::zoomAt(qreal factor, qint64 center_utc_ms) {
    if (query_end_utc_ms_ <= query_start_utc_ms_) {
        return;
    }
    constexpr qint64 kMinimumVisibleMs = 60000;
    const qint64 query_duration =
        query_end_utc_ms_ - query_start_utc_ms_;
    const qint64 current_duration =
        visible_end_utc_ms_ - visible_start_utc_ms_;
    const qint64 new_duration =
        qBound(kMinimumVisibleMs,
               static_cast<qint64>(current_duration * factor),
               query_duration);
    const qreal center_ratio =
        current_duration > 0
            ? qBound(0.0,
                     static_cast<qreal>(center_utc_ms -
                                        visible_start_utc_ms_) /
                         current_duration,
                     1.0)
            : 0.5;
    qint64 start =
        center_utc_ms - static_cast<qint64>(new_duration * center_ratio);
    qint64 end = start + new_duration;
    if (start < query_start_utc_ms_) {
        start = query_start_utc_ms_;
        end = start + new_duration;
    }
    if (end > query_end_utc_ms_) {
        end = query_end_utc_ms_;
        start = end - new_duration;
    }
    visible_start_utc_ms_ = start;
    visible_end_utc_ms_ = end;
    pivot_utc_ms_ = start + new_duration / 3;
    update();
    emit visibleRangeChanged(start, end, pivot_utc_ms_);
}

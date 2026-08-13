#pragma once

#include "playback_types.h"

#include <QRectF>
#include <QVector>
#include <QWidget>

class QWheelEvent;
class QMouseEvent;

class TimelineWidget : public QWidget {
    Q_OBJECT

public:
    explicit TimelineWidget(QWidget *parent = nullptr);

    void setQueryRange(qint64 start_utc_ms, qint64 end_utc_ms);
    void extendQueryRange(qint64 start_utc_ms, qint64 end_utc_ms);
    void setVisibleRange(qint64 start_utc_ms, qint64 end_utc_ms);
    void shiftVisibleBy(qint64 delta_ms);
    void setPivotTime(qint64 pivot_utc_ms);
    void setEmptyMessage(const QString &message);
    void setTimeline(const PlaybackTimeline &timeline);
    void clearTimeline();
    void clearSelection();
    void beginChannelQuery(bool clear_existing);
    void setChannelError(int channel_id);
    void pruneToRange(qint64 start_utc_ms, qint64 end_utc_ms);
    qint64 visibleStartUtcMs() const;
    qint64 visibleEndUtcMs() const;
    bool selectTime(int channel_id,
                    qint64 utc_ms,
                    bool user_initiated = false);
    void zoomIn();
    void zoomOut();
    void resetZoom();

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

signals:
    void timeSelected(int channel_id,
                      qint64 start_utc_ms,
                      qint64 recording_end_utc_ms,
                      bool user_initiated);
    void timeActivated(int channel_id,
                       qint64 start_utc_ms,
                       qint64 recording_end_utc_ms);
    void visibleRangeChanged(qint64 start_utc_ms,
                             qint64 end_utc_ms,
                             qint64 pivot_utc_ms);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    QRectF trackRect(int row) const;
    qint64 timeAtX(qreal x) const;
    qreal xAtTime(qint64 utc_ms) const;
    const PlaybackTimeline *timelineForChannel(int channel_id) const;
    const PlaybackTimelineSpan *recordingSpanAt(int channel_id,
                                                qint64 utc_ms) const;
    void zoomAt(qreal factor, qint64 center_utc_ms);

    QVector<PlaybackTimeline> timelines_;
    qint64 query_start_utc_ms_ = 0;
    qint64 query_end_utc_ms_ = 0;
    qint64 visible_start_utc_ms_ = 0;
    qint64 visible_end_utc_ms_ = 0;
    qint64 pivot_utc_ms_ = 0;
    int selection_channel_id_ = 0;
    qint64 selection_utc_ms_ = 0;
    QString empty_message_ = "타임라인을 불러오세요.";
    QVector<QString> channel_states_;
};

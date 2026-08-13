#pragma once

#include "stream_worker.h"

#include <QElapsedTimer>
#include <QSize>
#include <QWidget>

class QLabel;
class QEvent;
class QMouseEvent;
class QResizeEvent;

class VideoPanel : public QWidget {
    Q_OBJECT

public:
    explicit VideoPanel(int channel, QWidget *parent = nullptr);

    double lastMbps() const;
    QImage currentImage() const;
    QSize videoSurfaceSize() const;
    int chromeHeightHint() const;
    void resetStats();
    void clearStreamMetrics();
    void setTitle(const QString &title);
    void setPreviewImage(const QImage &image,
                         const QSize &expected_aspect = QSize());
    void setAspectConstrained(bool constrained,
                              const QSize &aspect = QSize(2592, 1520));
    void setMinimumVideoHeight(int height);

public slots:
    void setFrame(const QImage &image, qint64 queued_at_ms);
    void setStreamStats(const StreamStats &stats);
    void setStatus(const QString &status);

signals:
    void singleClicked();
    void doubleClicked();
    void renderSizeChanged(const QSize &size);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateStatsLabel();
    void updateStatsHeight();
    void updateVideoPixmap();
    void updateVideoSurfaceGeometry();
    void recordPaintedFrame();

    int channel_ = 0;
    QLabel *title_label_ = nullptr;
    QWidget *video_host_ = nullptr;
    QLabel *video_label_ = nullptr;
    QLabel *stats_label_ = nullptr;
    QLabel *status_label_ = nullptr;

    double last_mbps_ = 0.0;
    double decode_fps_ = 0.0;
    double ui_fps_ = 0.0;
    double paint_fps_ = 0.0;
    double packet_fps_ = 0.0;
    qint64 last_ui_queue_ms_ = 0;
    qint64 max_frame_gap_ms_ = 0;
    qint64 consumed_frames_ = 0;
    qint64 painted_frames_ = 0;
    quint64 pixmap_generation_ = 0;
    quint64 painted_generation_ = 0;
    QElapsedTimer render_timer_;
    QElapsedTimer paint_timer_;
    QElapsedTimer frame_gap_timer_;
    QImage last_image_;
    QSize source_size_;
    QSize preferred_aspect_{2592, 1520};
    bool aspect_constrained_ = false;
    int stats_detail_level_ = 0;
    bool suppress_next_release_ = false;
};

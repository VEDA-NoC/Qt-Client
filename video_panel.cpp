#include "video_panel.h"

#include <QDateTime>
#include <QDebug>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStyle>
#include <QVBoxLayout>

VideoPanel::VideoPanel(int channel, QWidget *parent) : QWidget(parent), channel_(channel) {
    setProperty("videoPanel", true);
    setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(7);

    auto *header = new QHBoxLayout();
    title_label_ = new QLabel(QString("CH %1  ·  CAMERA 1").arg(channel_ + 1), this);
    title_label_->setProperty("sectionTitle", true);
    status_label_ = new QLabel("Stopped", this);
    status_label_->setProperty("streamState", "offline");
    status_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    header->addWidget(title_label_);
    header->addStretch(1);
    header->addWidget(status_label_);

    video_host_ = new QWidget(this);
    video_host_->setMinimumSize(240, 135);
    video_host_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    video_host_->installEventFilter(this);

    video_label_ = new QLabel(video_host_);
    video_label_->setMinimumSize(1, 1);
    video_label_->setAlignment(Qt::AlignCenter);
    video_label_->setProperty("videoSurface", true);
    video_label_->setText("영상 대기 중");
    video_label_->setScaledContents(false);
    video_label_->installEventFilter(this);

    stats_label_ = new QLabel(this);
    stats_label_->setProperty("muted", true);
    stats_label_->setWordWrap(false);
    stats_label_->setMinimumWidth(0);
    stats_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

    layout->addLayout(header);
    layout->addWidget(video_host_, 1);
    layout->addWidget(stats_label_);

    render_timer_.start();
    paint_timer_.start();
    updateStatsHeight();
    updateStatsLabel();
}

double VideoPanel::lastMbps() const {
    return last_mbps_;
}

QImage VideoPanel::currentImage() const {
    return last_image_;
}

QSize VideoPanel::videoSurfaceSize() const {
    return video_label_->size();
}

int VideoPanel::chromeHeightHint() const {
    const int header_height =
        qMax(title_label_->sizeHint().height(), status_label_->sizeHint().height());
    return 16 + 14 + header_height + stats_label_->height();
}

void VideoPanel::setTitle(const QString &title) {
    title_label_->setText(title);
}

void VideoPanel::setPreviewImage(const QImage &image,
                                 const QSize &expected_aspect) {
    if (image.isNull()) {
        return;
    }
    last_image_ = image;
    source_size_ =
        expected_aspect.isValid() ? expected_aspect : image.size();
    if (expected_aspect.isValid() && expected_aspect.height() > 0) {
        const qreal actual_ratio =
            static_cast<qreal>(image.width()) / image.height();
        const qreal expected_ratio =
            static_cast<qreal>(expected_aspect.width()) /
            expected_aspect.height();
        if (qAbs(actual_ratio - expected_ratio) / expected_ratio > 0.12) {
            const int corrected_height =
                qMax(1, qRound(image.width() / expected_ratio));
            last_image_ = image.scaled(image.width(),
                                       corrected_height,
                                       Qt::IgnoreAspectRatio,
                                       Qt::SmoothTransformation);
            qWarning().noquote()
                << QString("[thumbnail] aspect corrected source=%1x%2 expected=%3x%4")
                       .arg(image.width())
                       .arg(image.height())
                       .arg(expected_aspect.width())
                       .arg(expected_aspect.height());
        }
    }
    updateVideoPixmap();
    updateStatsLabel();
}

void VideoPanel::setAspectConstrained(bool constrained,
                                      const QSize &aspect) {
    aspect_constrained_ = constrained;
    if (aspect.isValid()) {
        preferred_aspect_ = aspect;
    }
    updateVideoSurfaceGeometry();
    updateGeometry();
}

void VideoPanel::setMinimumVideoHeight(int height) {
    const int minimum_height = qMax(135, height);
    if (video_host_->minimumHeight() == minimum_height) {
        return;
    }
    video_host_->setMinimumHeight(minimum_height);
    updateGeometry();
}

void VideoPanel::clearStreamMetrics() {
    last_mbps_ = 0.0;
    decode_fps_ = 0.0;
    ui_fps_ = 0.0;
    paint_fps_ = 0.0;
    packet_fps_ = 0.0;
    last_ui_queue_ms_ = 0;
    max_frame_gap_ms_ = 0;
    consumed_frames_ = 0;
    painted_frames_ = 0;
    render_timer_.restart();
    paint_timer_.restart();
    frame_gap_timer_.invalidate();
    source_size_ = QSize();
    updateStatsLabel();
}

void VideoPanel::resetStats() {
    clearStreamMetrics();
    last_image_ = QImage();
    ++pixmap_generation_;
    video_label_->clear();
    video_label_->setText("영상 대기 중");
}

void VideoPanel::setFrame(const QImage &image, qint64 queued_at_ms) {
    ++consumed_frames_;
    const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
    last_ui_queue_ms_ = now_ms - queued_at_ms;
    if (frame_gap_timer_.isValid()) {
        max_frame_gap_ms_ = qMax(max_frame_gap_ms_, frame_gap_timer_.restart());
    } else {
        frame_gap_timer_.start();
    }

    const qint64 elapsed_ms = render_timer_.elapsed();
    if (elapsed_ms >= 1000) {
        ui_fps_ = consumed_frames_ * 1000.0 / elapsed_ms;
        qInfo().noquote()
            << QString("[ui-ch%1] ui_fps=%2 paint_fps=%3 max_gap_ms=%4 ui_queue_ms=%5 frame=%6x%7 panel=%8x%9")
                   .arg(channel_ + 1)
                   .arg(ui_fps_, 0, 'f', 1)
                   .arg(paint_fps_, 0, 'f', 1)
                   .arg(max_frame_gap_ms_)
                   .arg(last_ui_queue_ms_)
                   .arg(image.width())
                   .arg(image.height())
                   .arg(video_label_->width())
                   .arg(video_label_->height());
        consumed_frames_ = 0;
        max_frame_gap_ms_ = 0;
        render_timer_.restart();
        updateStatsLabel();
    }

    last_image_ = image;
    updateVideoPixmap();
}

void VideoPanel::setStreamStats(const StreamStats &stats) {
    last_mbps_ = stats.recv_mbps;
    decode_fps_ = stats.decode_fps;
    packet_fps_ = stats.packet_fps;
    if (stats.source_size.isValid()) {
        source_size_ = stats.source_size;
    }
    updateStatsLabel();
}

void VideoPanel::setStatus(const QString &status) {
    QString state = "offline";
    QString display = status;
    bool clear_video = false;
    if (status.startsWith("Playing")) {
        state = "online";
        display = "연결됨";
    } else if (status == "PlaybackEnded") {
        display = "재생 완료";
    } else if (status == "Paused") {
        state = "pending";
        display = "일시정지";
    } else if (status.startsWith("RetryWaiting:")) {
        state = "pending";
        display = QString("재연결 대기 %1초")
                      .arg(status.mid(QString("RetryWaiting:").size()));
    } else if (status == "RetryConnecting") {
        state = "pending";
        display = "재연결 중";
    } else if (status.startsWith("Opening") || status == "Connecting") {
        state = "pending";
        display = "연결 중";
    } else if (status.contains("failed", Qt::CaseInsensitive) ||
               status.contains("error", Qt::CaseInsensitive) ||
               status.contains("timeout", Qt::CaseInsensitive) ||
               status.startsWith("read ended") ||
               status == "no video stream" ||
               status == "decoder not found") {
        state = "error";
        display = "연결 오류";
        clear_video = true;
    } else if (status == "Stopped") {
        display = "중지됨";
    }
    if (clear_video) {
        resetStats();
    }
    status_label_->setText(display);
    status_label_->setProperty("streamState", state);
    status_label_->style()->unpolish(status_label_);
    status_label_->style()->polish(status_label_);
    status_label_->update();
}

void VideoPanel::updateStatsLabel() {
    const QString display =
        QString("%1×%2").arg(video_label_->width()).arg(video_label_->height());
    const QString source =
        source_size_.isValid()
            ? QString("%1×%2").arg(source_size_.width()).arg(source_size_.height())
            : QString("확인 대기");

    QString text;
    switch (stats_detail_level_) {
    case 0:
        text = QString("%1 Mbps  ·  %2 fps")
                   .arg(last_mbps_, 0, 'f', 2)
                   .arg(decode_fps_, 0, 'f', 1);
        break;
    case 1:
        text = QString("%1 Mbps  ·  %2 fps  ·  %3")
                   .arg(last_mbps_, 0, 'f', 2)
                   .arg(decode_fps_, 0, 'f', 1)
                   .arg(display);
        break;
    case 2:
        text = QString("수신 %1 Mbps  ·  디코드 %2 fps  ·  표시 %3")
                   .arg(last_mbps_, 0, 'f', 2)
                   .arg(decode_fps_, 0, 'f', 1)
                   .arg(display);
        break;
    case 3:
        text = QString("수신 %1 Mbps  ·  패킷 %2/s  ·  디코드 %3 fps  ·  원본 %4  ·  표시 %5")
                   .arg(last_mbps_, 0, 'f', 2)
                   .arg(packet_fps_, 0, 'f', 1)
                   .arg(decode_fps_, 0, 'f', 1)
                   .arg(source)
                   .arg(display);
        break;
    default:
        text = QString("수신 %1 Mbps  ·  패킷 %2/s  ·  디코드 %3 fps  ·  UI %4 fps  ·  화면 %5 fps  ·  큐 %6 ms  ·  원본 %7  ·  표시 %8")
                   .arg(last_mbps_, 0, 'f', 2)
                   .arg(packet_fps_, 0, 'f', 1)
                   .arg(decode_fps_, 0, 'f', 1)
                   .arg(ui_fps_, 0, 'f', 1)
                   .arg(paint_fps_, 0, 'f', 1)
                   .arg(last_ui_queue_ms_)
                   .arg(source)
                   .arg(display);
        break;
    }
    stats_label_->setText(text);
    stats_label_->setToolTip(QString());
}

void VideoPanel::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateStatsHeight();
    updateVideoSurfaceGeometry();
    updateStatsLabel();
}

void VideoPanel::updateStatsHeight() {
    // 통계 값 갱신은 geometry를 바꾸지 않는다. panel 폭이 바뀔 때만
    // 한 줄 안에 표시할 정보의 밀도를 선택한다.
    const int available_width = qMax(1, width() - 16);
    if (available_width < 300) {
        stats_detail_level_ = 0;
    } else if (available_width < 430) {
        stats_detail_level_ = 1;
    } else if (available_width < 720) {
        stats_detail_level_ = 2;
    } else if (available_width < 1120) {
        stats_detail_level_ = 3;
    } else {
        stats_detail_level_ = 4;
    }
    const QFontMetrics metrics(stats_label_->font());
    const int line_height = qMax(1, metrics.lineSpacing());
    stats_label_->setFixedHeight(line_height + 2);
}

void VideoPanel::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        emit doubleClicked();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

bool VideoPanel::eventFilter(QObject *watched, QEvent *event) {
    if (watched == video_host_ && event->type() == QEvent::Resize) {
        updateVideoSurfaceGeometry();
        return false;
    }
    if (watched == video_label_ &&
        event->type() == QEvent::MouseButtonRelease) {
        auto *mouse_event = static_cast<QMouseEvent *>(event);
        if (mouse_event->button() == Qt::LeftButton) {
            if (suppress_next_release_) {
                suppress_next_release_ = false;
                return true;
            }
            emit singleClicked();
            return true;
        }
    }
    if (watched == video_label_ &&
        event->type() == QEvent::MouseButtonDblClick) {
        auto *mouse_event = static_cast<QMouseEvent *>(event);
        if (mouse_event->button() == Qt::LeftButton) {
            suppress_next_release_ = true;
            emit doubleClicked();
            return true;
        }
    }
    if (watched == video_label_ && event->type() == QEvent::Paint &&
        painted_generation_ != pixmap_generation_) {
        painted_generation_ = pixmap_generation_;
        recordPaintedFrame();
    }
    return QWidget::eventFilter(watched, event);
}

void VideoPanel::recordPaintedFrame() {
    ++painted_frames_;
    const qint64 elapsed_ms = paint_timer_.elapsed();
    if (elapsed_ms >= 1000) {
        paint_fps_ = painted_frames_ * 1000.0 / elapsed_ms;
        painted_frames_ = 0;
        paint_timer_.restart();
        updateStatsLabel();
    }
}

void VideoPanel::updateVideoPixmap() {
    if (last_image_.isNull()) {
        return;
    }
    const QPixmap pixmap = QPixmap::fromImage(last_image_);
    const QSize fitted_size =
        pixmap.size().scaled(video_label_->size(), Qt::KeepAspectRatio);
    if (pixmap.size() == fitted_size) {
        video_label_->setPixmap(pixmap);
    } else {
        video_label_->setPixmap(
            pixmap.scaled(fitted_size,
                          Qt::KeepAspectRatio,
                          Qt::SmoothTransformation));
    }
    ++pixmap_generation_;
}

void VideoPanel::updateVideoSurfaceGeometry() {
    if (!video_host_ || !video_label_) {
        return;
    }
    const QSize available = video_host_->size();
    if (!available.isValid()) {
        return;
    }
    QSize surface = available;
    if (aspect_constrained_ && preferred_aspect_.isValid()) {
        surface = preferred_aspect_.scaled(available, Qt::KeepAspectRatio);
    }
    const QRect geometry((available.width() - surface.width()) / 2,
                         (available.height() - surface.height()) / 2,
                         surface.width(),
                         surface.height());
    if (video_label_->geometry() != geometry) {
        video_label_->setGeometry(geometry);
        emit renderSizeChanged(surface);
    }
    updateVideoPixmap();
    updateStatsLabel();
}

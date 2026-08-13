#pragma once

#include <QImage>
#include <QSize>
#include <QThread>
#include <atomic>

struct StreamStats {
    int channel = 0;
    double recv_mbps = 0.0;
    double decode_fps = 0.0;
    double packet_fps = 0.0;
    double queue_drop_fps = 0.0;
    qint64 decoded_frames = 0;
    qint64 packets = 0;
    qint64 queue_drops = 0;
    QSize source_size;
};

class StreamWorker : public QThread {
    Q_OBJECT

public:
    StreamWorker(int channel,
                 QString url,
                 QObject *parent = nullptr,
                 bool playback = false);
    ~StreamWorker() override;

    void stop();
    void markFrameConsumed();
    void setOutputSize(const QSize &size);

signals:
    void frameReady(const QImage &image, qint64 queued_at_ms);
    void statsReady(const StreamStats &stats);
    void statusChanged(const QString &status);
    void playbackPositionChanged(qint64 elapsed_ms);

protected:
    void run() override;

private:
    void runImpl();
    bool tryReserveFrameSlot();

    int channel_ = 0;
    QString url_;
    bool playback_ = false;
    std::atomic_bool stop_requested_{false};
    std::atomic_int queued_frames_{0};
    std::atomic_int output_width_{0};
    std::atomic_int output_height_{0};
};

Q_DECLARE_METATYPE(StreamStats)

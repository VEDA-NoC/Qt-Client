#include "stream_worker.h"

#include <QDateTime>
#include <QDebug>
#include <QUrl>

#include <chrono>
#include <exception>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

namespace {

constexpr int kMaxQueuedFrames = 3;
constexpr int kIoTimeoutMs = 5000;
constexpr int kFirstFrameTimeoutMs = 15000;
constexpr int kDecodedFrameStallTimeoutMs = 5000;
constexpr int kDecodeErrorLogIntervalMs = 1000;

bool isDeprecatedFullRangeFormat(AVPixelFormat format) {
    return format == AV_PIX_FMT_YUVJ420P ||
           format == AV_PIX_FMT_YUVJ422P ||
           format == AV_PIX_FMT_YUVJ444P ||
           format == AV_PIX_FMT_YUVJ440P ||
           format == AV_PIX_FMT_YUVJ411P;
}

AVPixelFormat normalizedPixelFormat(AVPixelFormat format) {
    switch (format) {
    case AV_PIX_FMT_YUVJ420P:
        return AV_PIX_FMT_YUV420P;
    case AV_PIX_FMT_YUVJ422P:
        return AV_PIX_FMT_YUV422P;
    case AV_PIX_FMT_YUVJ444P:
        return AV_PIX_FMT_YUV444P;
    case AV_PIX_FMT_YUVJ440P:
        return AV_PIX_FMT_YUV440P;
    case AV_PIX_FMT_YUVJ411P:
        return AV_PIX_FMT_YUV411P;
    default:
        return format;
    }
}

int swsColorSpace(AVColorSpace color_space, int width) {
    switch (color_space) {
    case AVCOL_SPC_BT709:
        return SWS_CS_ITU709;
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
        return SWS_CS_ITU601;
    case AVCOL_SPC_SMPTE240M:
        return SWS_CS_SMPTE240M;
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
        return SWS_CS_BT2020;
    default:
        return width >= 1280 ? SWS_CS_ITU709 : SWS_CS_ITU601;
    }
}

struct InterruptContext {
    std::atomic_bool *stop_requested = nullptr;
    std::chrono::steady_clock::time_point deadline;
};

int interruptIo(void *opaque) {
    auto *context = static_cast<InterruptContext *>(opaque);
    if (!context) {
        return 0;
    }
    if (context->stop_requested && context->stop_requested->load()) {
        return 1;
    }
    return std::chrono::steady_clock::now() >= context->deadline ? 1 : 0;
}

void resetIoDeadline(InterruptContext &context) {
    context.deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kIoTimeoutMs);
}

bool ioTimedOut(const InterruptContext &context) {
    return std::chrono::steady_clock::now() >= context.deadline;
}

struct AvPacketDeleter {
    void operator()(AVPacket *packet) const {
        av_packet_free(&packet);
    }
};

struct AvFrameDeleter {
    void operator()(AVFrame *frame) const {
        av_frame_free(&frame);
    }
};

QString ffmpegError(int error_code) {
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(error_code, buffer, sizeof(buffer));
    return QString::fromLocal8Bit(buffer);
}

}  // namespace

StreamWorker::StreamWorker(int channel,
                           QString url,
                           QObject *parent,
                           bool playback)
    : QThread(parent),
      channel_(channel),
      url_(std::move(url)),
      playback_(playback) {
    qRegisterMetaType<StreamStats>("StreamStats");
}

StreamWorker::~StreamWorker() {
    stop();
    wait(3000);
}

void StreamWorker::stop() {
    stop_requested_.store(true);
}

void StreamWorker::markFrameConsumed() {
    const int previous = queued_frames_.fetch_sub(1);
    if (previous <= 0) {
        queued_frames_.store(0);
    }
}

void StreamWorker::setOutputSize(const QSize &size) {
    output_width_.store(qMax(0, size.width()));
    output_height_.store(qMax(0, size.height()));
}

bool StreamWorker::tryReserveFrameSlot() {
    int queued = queued_frames_.load();
    while (queued < kMaxQueuedFrames) {
        if (queued_frames_.compare_exchange_weak(queued, queued + 1)) {
            return true;
        }
    }
    return false;
}

void StreamWorker::run() {
    try {
        runImpl();
    } catch (const std::exception &e) {
        emit statusChanged(QString("worker error: %1").arg(e.what()));
    } catch (...) {
        emit statusChanged("worker error: unknown exception");
    }
}

void StreamWorker::runImpl() {
    queued_frames_.store(0);
    emit statusChanged("Opening");
    const auto worker_started_at = std::chrono::steady_clock::now();
    const QString route = playback_ ? QString("playback")
                                    : QUrl(url_).path();

    avformat_network_init();

    AVFormatContext *format_context = avformat_alloc_context();
    if (!format_context) {
        emit statusChanged("format context alloc failed");
        return;
    }

    InterruptContext interrupt_context;
    interrupt_context.stop_requested = &stop_requested_;
    resetIoDeadline(interrupt_context);
    format_context->interrupt_callback.callback = interruptIo;
    format_context->interrupt_callback.opaque = &interrupt_context;

    AVDictionary *options = nullptr;
    av_dict_set(&options, "rtsp_transport", "tcp", 0);
    av_dict_set(&options, "stimeout", "5000000", 0);
    av_dict_set(&options, "rw_timeout", "5000000", 0);
    av_dict_set(&options, "fflags", "nobuffer", 0);
    av_dict_set(&options, "flags", "low_delay", 0);

    int ret = avformat_open_input(&format_context, url_.toUtf8().constData(), nullptr, &options);
    av_dict_free(&options);
    if (ret < 0) {
        if (stop_requested_.load()) {
            emit statusChanged("Stopped");
        } else if (ioTimedOut(interrupt_context)) {
            emit statusChanged(QString("open timeout after %1 ms").arg(kIoTimeoutMs));
        } else {
            emit statusChanged(QString("open failed: %1").arg(ffmpegError(ret)));
        }
        if (format_context) {
            avformat_free_context(format_context);
        }
        return;
    }
    qInfo().noquote()
        << QString("[stream-open] channel=%1 mode=%2 route=%3 state=open_success")
               .arg(channel_ + 1)
               .arg(playback_ ? "playback" : "live")
               .arg(route);

    resetIoDeadline(interrupt_context);
    ret = avformat_find_stream_info(format_context, nullptr);
    if (ret < 0) {
        if (stop_requested_.load()) {
            emit statusChanged("Stopped");
        } else if (ioTimedOut(interrupt_context)) {
            emit statusChanged(QString("stream info timeout after %1 ms").arg(kIoTimeoutMs));
        } else {
            emit statusChanged(QString("stream info failed: %1").arg(ffmpegError(ret)));
        }
        avformat_close_input(&format_context);
        return;
    }

    const int video_stream_index = av_find_best_stream(format_context, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_index < 0) {
        emit statusChanged("no video stream");
        avformat_close_input(&format_context);
        return;
    }

    AVStream *video_stream = format_context->streams[video_stream_index];
    const AVCodec *codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!codec) {
        emit statusChanged("decoder not found");
        avformat_close_input(&format_context);
        return;
    }
    qInfo().noquote()
        << QString("[stream-open] channel=%1 mode=%2 route=%3 state=stream_info codec=%4")
               .arg(channel_ + 1)
               .arg(playback_ ? "playback" : "live")
               .arg(route)
               .arg(QString::fromLatin1(codec->name));

    AVCodecContext *codec_context = avcodec_alloc_context3(codec);
    if (!codec_context) {
        emit statusChanged("decoder context alloc failed");
        avformat_close_input(&format_context);
        return;
    }

    ret = avcodec_parameters_to_context(codec_context, video_stream->codecpar);
    if (ret < 0) {
        emit statusChanged(QString("codec params failed: %1").arg(ffmpegError(ret)));
        avcodec_free_context(&codec_context);
        avformat_close_input(&format_context);
        return;
    }
    codec_context->flags |= AV_CODEC_FLAG_LOW_DELAY;
    ret = avcodec_open2(codec_context, codec, nullptr);
    if (ret < 0) {
        emit statusChanged(QString("decoder open failed: %1").arg(ffmpegError(ret)));
        avcodec_free_context(&codec_context);
        avformat_close_input(&format_context);
        return;
    }
    qInfo().noquote()
        << QString("[stream-open] channel=%1 mode=%2 route=%3 state=decoder_open codec=%4")
               .arg(channel_ + 1)
               .arg(playback_ ? "playback" : "live")
               .arg(route)
               .arg(QString::fromLatin1(codec->name));

    std::unique_ptr<AVPacket, AvPacketDeleter> packet(av_packet_alloc());
    std::unique_ptr<AVFrame, AvFrameDeleter> frame(av_frame_alloc());
    if (!packet || !frame) {
        emit statusChanged("packet/frame alloc failed");
        avcodec_free_context(&codec_context);
        avformat_close_input(&format_context);
        return;
    }

    SwsContext *sws_context = nullptr;
    int configured_width = 0;
    int configured_height = 0;
    AVPixelFormat configured_format = AV_PIX_FMT_NONE;
    int configured_source_range = -1;
    int configured_color_space = -1;
    int configured_output_width = 0;
    int configured_output_height = 0;
    bool playing_announced = false;
    bool fatal_frame_error = false;
    bool first_video_packet_seen = false;
    qint64 recoverable_decode_errors = 0;
    auto last_decode_error_log_at = worker_started_at;
    auto last_decoded_frame_at = worker_started_at;
    qint64 last_playback_pts_ms = -1;
    qint64 accumulated_playback_position_ms = 0;
    qint64 last_emitted_playback_position_ms = -1;
    bool has_last_playback_pts = false;
    const auto first_frame_deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(kFirstFrameTimeoutMs);

    qint64 interval_start_ms = QDateTime::currentMSecsSinceEpoch();
    qint64 interval_bytes = 0;
    qint64 interval_packets = 0;
    qint64 interval_decoded = 0;
    qint64 interval_queue_drops = 0;
    qint64 total_packets = 0;
    qint64 total_decoded = 0;
    qint64 total_queue_drops = 0;
    QSize last_source_size;

    const auto recordRecoverableDecodeError =
        [&](const char *stage, int error_code) {
            ++recoverable_decode_errors;
            const auto now = std::chrono::steady_clock::now();
            if (recoverable_decode_errors == 1 ||
                now - last_decode_error_log_at >=
                    std::chrono::milliseconds(kDecodeErrorLogIntervalMs)) {
                qWarning().noquote()
                    << QString("[stream-decode] channel=%1 mode=live route=%2 state=packet_dropped stage=%3 error=%4 total_errors=%5")
                           .arg(channel_ + 1)
                           .arg(route)
                           .arg(QString::fromLatin1(stage))
                           .arg(ffmpegError(error_code))
                           .arg(recoverable_decode_errors);
                last_decode_error_log_at = now;
            }
        };

    while (!stop_requested_.load()) {
        resetIoDeadline(interrupt_context);
        ret = av_read_frame(format_context, packet.get());
        if (ret < 0) {
            if (stop_requested_.load()) {
                break;
            }
            if (playback_ && ret == AVERROR_EOF) {
                emit statusChanged("PlaybackEnded");
                break;
            }
            if (ioTimedOut(interrupt_context)) {
                emit statusChanged(QString("read timeout after %1 ms").arg(kIoTimeoutMs));
            } else {
                emit statusChanged(QString("read ended: %1").arg(ffmpegError(ret)));
            }
            break;
        }

        interval_bytes += packet->size;
        ++interval_packets;
        ++total_packets;

        if (packet->stream_index == video_stream_index) {
            if (!first_video_packet_seen) {
                first_video_packet_seen = true;
                qInfo().noquote()
                    << QString("[stream-open] channel=%1 mode=%2 route=%3 state=first_video_packet bytes=%4")
                           .arg(channel_ + 1)
                           .arg(playback_ ? "playback" : "live")
                           .arg(route)
                           .arg(packet->size);
            }
            ret = avcodec_send_packet(codec_context, packet.get());
            if (ret < 0) {
                if (!playback_ && ret == AVERROR_INVALIDDATA) {
                    recordRecoverableDecodeError("send_packet", ret);
                } else {
                    emit statusChanged(QString("decode packet failed: %1").arg(ffmpegError(ret)));
                    fatal_frame_error = true;
                }
            } else {
                while (ret >= 0) {
                    ret = avcodec_receive_frame(codec_context, frame.get());
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                        break;
                    }
                    if (ret < 0) {
                        if (!playback_ && ret == AVERROR_INVALIDDATA) {
                            recordRecoverableDecodeError("receive_frame", ret);
                            av_frame_unref(frame.get());
                        } else {
                            emit statusChanged(QString("decode failed: %1").arg(ffmpegError(ret)));
                            fatal_frame_error = true;
                        }
                        break;
                    }

                    const int frame_width = frame->width;
                    const int frame_height = frame->height;
                    if (frame_width <= 0 ||
                        frame_height <= 0 ||
                        frame->format < 0 ||
                        !frame->data[0] ||
                        frame->linesize[0] == 0 ||
                        !av_pix_fmt_desc_get(static_cast<AVPixelFormat>(frame->format)) ||
                        av_image_check_size(static_cast<unsigned int>(frame_width),
                                            static_cast<unsigned int>(frame_height),
                                            0,
                                            nullptr) < 0) {
                        emit statusChanged(QString("decoded frame error: invalid %1x%2 format=%3")
                                               .arg(frame_width)
                                               .arg(frame_height)
                                               .arg(frame->format));
                        fatal_frame_error = true;
                        av_frame_unref(frame.get());
                        break;
                    }
                    last_source_size = QSize(frame_width, frame_height);

                    const auto source_format = static_cast<AVPixelFormat>(frame->format);
                    const AVPixelFormat conversion_format = normalizedPixelFormat(source_format);
                    const AVPixFmtDescriptor *conversion_descriptor =
                        av_pix_fmt_desc_get(conversion_format);
                    if (!conversion_descriptor) {
                        emit statusChanged(QString("decoded frame error: unsupported format=%1")
                                               .arg(frame->format));
                        fatal_frame_error = true;
                        av_frame_unref(frame.get());
                        break;
                    }

                    int output_width = output_width_.load();
                    int output_height = output_height_.load();
                    if (output_width <= 0 || output_height <= 0) {
                        output_width = frame_width;
                        output_height = frame_height;
                    } else {
                        const double width_ratio =
                            static_cast<double>(output_width) / frame_width;
                        const double height_ratio =
                            static_cast<double>(output_height) / frame_height;
                        const double scale =
                            qMin(1.0, qMin(width_ratio, height_ratio));
                        output_width =
                            qMax(2, static_cast<int>(frame_width * scale));
                        output_height =
                            qMax(2, static_cast<int>(frame_height * scale));
                        output_width &= ~1;
                        output_height &= ~1;
                    }

                    sws_context = sws_getCachedContext(
                        sws_context,
                        frame_width,
                        frame_height,
                        conversion_format,
                        output_width,
                        output_height,
                        AV_PIX_FMT_RGB24,
                        SWS_BICUBIC,
                        nullptr,
                        nullptr,
                        nullptr);
                    if (!sws_context) {
                        emit statusChanged(QString("sws context failed: %1x%2 format=%3")
                                               .arg(frame_width)
                                               .arg(frame_height)
                                               .arg(frame->format));
                        fatal_frame_error = true;
                        av_frame_unref(frame.get());
                        break;
                    }

                    const int source_range =
                        frame->color_range == AVCOL_RANGE_JPEG ||
                                isDeprecatedFullRangeFormat(source_format)
                            ? 1
                            : 0;
                    const int color_space =
                        swsColorSpace(static_cast<AVColorSpace>(frame->colorspace), frame_width);
                    const bool is_rgb = (conversion_descriptor->flags & AV_PIX_FMT_FLAG_RGB) != 0;
                    const bool color_details_changed =
                        configured_width != frame_width ||
                        configured_height != frame_height ||
                        configured_format != conversion_format ||
                        configured_source_range != source_range ||
                        configured_color_space != color_space ||
                        configured_output_width != output_width ||
                        configured_output_height != output_height;
                    if (!is_rgb && color_details_changed) {
                        const int *coefficients = sws_getCoefficients(color_space);
                        const int color_result =
                            sws_setColorspaceDetails(sws_context,
                                                     coefficients,
                                                     source_range,
                                                     coefficients,
                                                     1,
                                                     0,
                                                     1 << 16,
                                                     1 << 16);
                        if (color_result < 0) {
                            emit statusChanged(QString("sws colorspace failed: %1")
                                                   .arg(color_result));
                            fatal_frame_error = true;
                            av_frame_unref(frame.get());
                            break;
                        }
                        configured_width = frame_width;
                        configured_height = frame_height;
                        configured_format = conversion_format;
                        configured_source_range = source_range;
                        configured_color_space = color_space;
                        configured_output_width = output_width;
                        configured_output_height = output_height;
                    }

                    ++interval_decoded;
                    ++total_decoded;
                    if (playback_ &&
                        frame->best_effort_timestamp != AV_NOPTS_VALUE) {
                        const qint64 current_pts_ms = av_rescale_q(
                            frame->best_effort_timestamp,
                            video_stream->time_base,
                            AVRational{1, 1000});
                        if (has_last_playback_pts) {
                            const qint64 delta_ms =
                                current_pts_ms - last_playback_pts_ms;
                            // Segment 경계에서 PTS가 다시 시작될 수 있다.
                            // 역행은 누적 위치를 되돌리지 않고 다음 양수 delta부터
                            // 이어서 계산한다.
                            if (delta_ms >= 0 && delta_ms < 10000) {
                                accumulated_playback_position_ms += delta_ms;
                            }
                        }
                        last_playback_pts_ms = current_pts_ms;
                        has_last_playback_pts = true;
                        const qint64 elapsed_ms =
                            accumulated_playback_position_ms;
                        if (last_emitted_playback_position_ms < 0 ||
                            elapsed_ms -
                                    last_emitted_playback_position_ms >=
                                200) {
                            last_emitted_playback_position_ms = elapsed_ms;
                            emit playbackPositionChanged(elapsed_ms);
                        }
                    }
                    last_decoded_frame_at =
                        std::chrono::steady_clock::now();
                    if (!tryReserveFrameSlot()) {
                        ++interval_queue_drops;
                        ++total_queue_drops;
                        av_frame_unref(frame.get());
                        continue;
                    }

                    QImage output_image(output_width,
                                        output_height,
                                        QImage::Format_RGB888);
                    if (output_image.isNull()) {
                        markFrameConsumed();
                        emit statusChanged(QString("output image alloc failed: %1x%2")
                                               .arg(frame_width)
                                               .arg(frame_height));
                        fatal_frame_error = true;
                        av_frame_unref(frame.get());
                        break;
                    }

                    uint8_t *destination_data[4] = {
                        output_image.bits(),
                        nullptr,
                        nullptr,
                        nullptr,
                    };
                    const int destination_linesize[4] = {
                        static_cast<int>(output_image.bytesPerLine()),
                        0,
                        0,
                        0,
                    };
                    const int scaled_rows = sws_scale(sws_context,
                                                      frame->data,
                                                      frame->linesize,
                                                      0,
                                                      frame_height,
                                                      destination_data,
                                                      destination_linesize);
                    if (scaled_rows <= 0) {
                        markFrameConsumed();
                        emit statusChanged(QString("frame conversion failed: %1").arg(scaled_rows));
                        fatal_frame_error = true;
                        av_frame_unref(frame.get());
                        break;
                    }

                    if (!playing_announced) {
                        const char *source_format_name = av_get_pix_fmt_name(source_format);
                        const char *conversion_format_name =
                            av_get_pix_fmt_name(conversion_format);
                        const char *color_space_name =
                            av_color_space_name(static_cast<AVColorSpace>(frame->colorspace));
                        qInfo().noquote()
                            << QString("[ch%1] video_format source=%2 conversion=%3 range=%4 colorspace=%5")
                                   .arg(channel_ + 1)
                                   .arg(source_format_name ? source_format_name : "unknown")
                                   .arg(conversion_format_name ? conversion_format_name : "unknown")
                                   .arg(source_range ? "full" : "limited")
                                   .arg(color_space_name ? color_space_name : "unspecified");
                        emit statusChanged(QString("Playing %1x%2 %3")
                                               .arg(frame_width)
                                               .arg(frame_height)
                                               .arg(QString::fromLatin1(codec->name)));
                        const qint64 first_frame_ms =
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() -
                                worker_started_at)
                                .count();
                        qInfo().noquote()
                            << QString("[stream-open] channel=%1 mode=%2 route=%3 state=first_decoded_frame elapsed_ms=%4 recoverable_errors=%5")
                                   .arg(channel_ + 1)
                                   .arg(playback_ ? "playback" : "live")
                                   .arg(route)
                                   .arg(first_frame_ms)
                                   .arg(recoverable_decode_errors);
                        playing_announced = true;
                    }
                    emit frameReady(output_image, QDateTime::currentMSecsSinceEpoch());

                    av_frame_unref(frame.get());
                }
            }
        }

        av_packet_unref(packet.get());
        if (fatal_frame_error) {
            break;
        }
        const auto now_steady = std::chrono::steady_clock::now();
        if (!playing_announced && now_steady >= first_frame_deadline) {
            emit statusChanged(QString("first frame timeout after %1 ms").arg(kFirstFrameTimeoutMs));
            break;
        }
        if (!playback_ && playing_announced &&
            now_steady - last_decoded_frame_at >=
                std::chrono::milliseconds(kDecodedFrameStallTimeoutMs)) {
            emit statusChanged(
                QString("decode stall timeout after %1 ms")
                    .arg(kDecodedFrameStallTimeoutMs));
            break;
        }

        const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
        const qint64 elapsed_ms = now_ms - interval_start_ms;
        if (elapsed_ms >= 1000) {
            const double seconds = elapsed_ms / 1000.0;
            StreamStats stats;
            stats.channel = channel_;
            stats.recv_mbps = (interval_bytes * 8.0) / (seconds * 1000.0 * 1000.0);
            stats.packet_fps = interval_packets / seconds;
            stats.decode_fps = interval_decoded / seconds;
            stats.queue_drop_fps = interval_queue_drops / seconds;
            stats.packets = total_packets;
            stats.decoded_frames = total_decoded;
            stats.queue_drops = total_queue_drops;
            stats.source_size = last_source_size;
            emit statsReady(stats);

            qInfo().noquote() << QString("[ch%1] recv=%2 Mbps packets=%3/s decode_fps=%4 queue_drops=%5/s total_frames=%6")
                                     .arg(channel_ + 1)
                                     .arg(stats.recv_mbps, 0, 'f', 2)
                                     .arg(stats.packet_fps, 0, 'f', 1)
                                     .arg(stats.decode_fps, 0, 'f', 1)
                                     .arg(stats.queue_drop_fps, 0, 'f', 1)
                                     .arg(stats.decoded_frames);

            interval_start_ms = now_ms;
            interval_bytes = 0;
            interval_packets = 0;
            interval_decoded = 0;
            interval_queue_drops = 0;
        }
    }

    sws_freeContext(sws_context);
    avcodec_free_context(&codec_context);
    avformat_close_input(&format_context);
    if (stop_requested_.load()) {
        emit statusChanged("Stopped");
    }
}

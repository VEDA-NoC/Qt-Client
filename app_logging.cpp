#include "app_logging.h"

#include <QByteArray>
#include <QString>
#include <QtLogging>

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <mutex>

extern "C" {
#include <libavutil/log.h>
}

namespace {

std::mutex g_log_mutex;

void writeLogLine(QByteArray message) {
    while (message.endsWith('\n') || message.endsWith('\r')) {
        message.chop(1);
    }

    const std::lock_guard<std::mutex> lock(g_log_mutex);
    if (!message.isEmpty()) {
        std::fwrite(message.constData(), 1, static_cast<std::size_t>(message.size()), stderr);
    }
    std::fputc('\n', stderr);
    std::fflush(stderr);
}

void qtMessageHandler(QtMsgType type, const QMessageLogContext &, const QString &message) {
#ifdef Q_OS_WIN
    // Windows Qt Creator의 Application Output은 시스템 코드 페이지를
    // 사용하는 경우가 있어 UTF-8 바이트를 그대로 쓰면 한글이 깨진다.
    writeLogLine(message.toLocal8Bit());
#else
    writeLogLine(message.toUtf8());
#endif
    if (type == QtFatalMsg) {
        std::abort();
    }
}

void ffmpegLogHandler(void *context, int level, const char *format, va_list arguments) {
    if (level > av_log_get_level()) {
        return;
    }

    char buffer[4096] = {};
    int print_prefix = 1;
    av_log_format_line2(context,
                        level,
                        format,
                        arguments,
                        buffer,
                        static_cast<int>(sizeof(buffer)),
                        &print_prefix);
    writeLogLine(QByteArray(buffer));
}

}  // namespace

void AppLogging::install() {
    qInstallMessageHandler(qtMessageHandler);
    av_log_set_level(AV_LOG_WARNING);
    av_log_set_callback(ffmpegLogHandler);
}

#include "parking_zone_canvas.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QLineF>
#include <QPolygon>
#include <QSizePolicy>

ParkingZoneCanvas::ParkingZoneCanvas(QWidget *parent) : QWidget(parent) {
    setMinimumSize(480, 300);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

void ParkingZoneCanvas::setReferenceImage(const QImage &image) {
    image_ = image;
    update();
}

void ParkingZoneCanvas::setSpaces(const QVector<ParkingSpace> &spaces) {
    spaces_ = spaces;
    if (selected_index_ >= spaces_.size()) {
        selected_index_ = -1;
    }
    update();
}

void ParkingZoneCanvas::setSelectedIndex(int index) {
    selected_index_ = index;
    update();
}

void ParkingZoneCanvas::setEditingEnabled(bool enabled) {
    editing_enabled_ = enabled;
    if (!enabled) {
        dragged_handle_ = -1;
    }
}

QRectF ParkingZoneCanvas::imageRect() const {
    if (image_.isNull()) {
        return QRectF();
    }
    const QSizeF fitted = image_.size().scaled(size(), Qt::KeepAspectRatio);
    return QRectF((width() - fitted.width()) / 2.0,
                  (height() - fitted.height()) / 2.0,
                  fitted.width(), fitted.height());
}

QPointF ParkingZoneCanvas::toWidgetPoint(const QPointF &point) const {
    const QRectF rect = imageRect();
    return QPointF(rect.left() + point.x() * rect.width(),
                   rect.top() + point.y() * rect.height());
}

QPointF ParkingZoneCanvas::toNormalizedPoint(const QPointF &point) const {
    const QRectF rect = imageRect();
    return QPointF(qBound(0.0, (point.x() - rect.left()) / rect.width(), 1.0),
                   qBound(0.0, (point.y() - rect.top()) / rect.height(), 1.0));
}

int ParkingZoneCanvas::nearestHandle(const QPointF &position) const {
    if (selected_index_ < 0 || selected_index_ >= spaces_.size()) {
        return -1;
    }
    int result = -1;
    qreal distance = 14.0;
    const auto &polygon = spaces_[selected_index_].polygon;
    for (int index = 0; index < polygon.size(); ++index) {
        const qreal candidate = QLineF(position, toWidgetPoint(polygon[index])).length();
        if (candidate < distance) {
            distance = candidate;
            result = index;
        }
    }
    return result;
}

void ParkingZoneCanvas::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor("#11131c"));
    const QRectF target = imageRect();
    if (image_.isNull()) {
        painter.setPen(QColor("#9aa3b7"));
        painter.drawText(rect(), Qt::AlignCenter,
                         "실시간 기준 이미지를 아직 받지 못했습니다.");
        return;
    }
    painter.drawImage(target, image_);
    painter.save();
    painter.setClipRect(target);
    for (int space_index = 0; space_index < spaces_.size(); ++space_index) {
        const ParkingSpace &space = spaces_[space_index];
        const bool selected = space_index == selected_index_;
        QPolygonF polygon;
        for (const QPointF &point : space.polygon) {
            polygon.push_back(toWidgetPoint(point));
        }
        QColor color = selected ? QColor("#ff6b1a") : QColor("#24b47e");
        if (polygon.size() >= 3) {
            painter.setBrush(QColor(color.red(), color.green(), color.blue(), 48));
            painter.setPen(QPen(color, selected ? 3.0 : 2.0));
            painter.drawPolygon(polygon);
        } else if (polygon.size() >= 2) {
            painter.setPen(QPen(color, 2.0, Qt::DashLine));
            painter.drawPolyline(polygon);
        }
        for (int point_index = 0; point_index < polygon.size(); ++point_index) {
            painter.setBrush(Qt::white);
            painter.setPen(QPen(color, 2.0));
            painter.drawEllipse(polygon[point_index], 6.0, 6.0);
            if (selected) {
                painter.setPen(color);
                painter.drawText(polygon[point_index] + QPointF(8, -8),
                                 QString::number(point_index + 1));
            }
        }
    }
    painter.restore();
}

void ParkingZoneCanvas::mousePressEvent(QMouseEvent *event) {
    const QRectF target = imageRect();
    if (!editing_enabled_ || !target.contains(event->position()) ||
        selected_index_ < 0 || selected_index_ >= spaces_.size()) {
        return;
    }
    dragged_handle_ = nearestHandle(event->position());
    QVector<QPointF> polygon = spaces_[selected_index_].polygon;
    if (dragged_handle_ >= 0 || polygon.size() < 4) {
        emit editBegan();
    }
    if (dragged_handle_ < 0 && polygon.size() < 4) {
        polygon.push_back(toNormalizedPoint(event->position()));
        spaces_[selected_index_].polygon = polygon;
        emit polygonEdited(selected_index_, polygon);
        update();
    }
}

void ParkingZoneCanvas::mouseMoveEvent(QMouseEvent *event) {
    if (dragged_handle_ < 0 || selected_index_ < 0 ||
        selected_index_ >= spaces_.size()) {
        return;
    }
    QVector<QPointF> polygon = spaces_[selected_index_].polygon;
    polygon[dragged_handle_] = toNormalizedPoint(event->position());
    spaces_[selected_index_].polygon = polygon;
    emit polygonEdited(selected_index_, polygon);
    update();
}

void ParkingZoneCanvas::mouseReleaseEvent(QMouseEvent *) {
    dragged_handle_ = -1;
}

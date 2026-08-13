#pragma once

#include "parking_types.h"

#include <QImage>
#include <QWidget>

class ParkingZoneCanvas : public QWidget {
    Q_OBJECT

public:
    explicit ParkingZoneCanvas(QWidget *parent = nullptr);
    void setReferenceImage(const QImage &image);
    void setSpaces(const QVector<ParkingSpace> &spaces);
    void setSelectedIndex(int index);
    void setEditingEnabled(bool enabled);

signals:
    void editBegan();
    void polygonEdited(int index, const QVector<QPointF> &polygon);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QRectF imageRect() const;
    QPointF toWidgetPoint(const QPointF &normalized) const;
    QPointF toNormalizedPoint(const QPointF &widget_point) const;
    int nearestHandle(const QPointF &position) const;

    QImage image_;
    QVector<ParkingSpace> spaces_;
    int selected_index_ = -1;
    int dragged_handle_ = -1;
    bool editing_enabled_ = true;
};

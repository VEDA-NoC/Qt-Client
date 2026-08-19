#pragma once

#include "parking_types.h"
#include "stm_types.h"

#include <QImage>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTimer;
class ParkingZoneCanvas;
class PlaybackApiClient;
class StmApiClient;

class ParkingZoneEditor : public QWidget {
    Q_OBJECT

public:
    explicit ParkingZoneEditor(PlaybackApiClient *api, StmApiClient *stm_api, QWidget *parent = nullptr);
    void setReferenceImage(int channel_id, const QImage &reference_image);
    void openChannel(int channel_id, const QImage &reference_image);
    bool isDirty() const;
    bool confirmDiscard(QWidget *parent);

signals:
    void backRequested();
    void dirtyChanged(bool dirty);

private:
    void selectChannel(int channel_id);
    void loadSelection();
    void storeSelection();
    void refreshUi();
    void setDirty(bool dirty);
    void setFeedback(const QString &text, const QString &severity);
    void addSpace();
    void removeSpace();
    void saveDraft();
    void validateDraft();
    void applyDraft();
    bool locallyValid(QString *message) const;
    void handleApplyJob(const ParkingApplyJob &job);
    void pushHistory();
    void undo();
    void redo();
    void rebuildDeviceCombo();

    PlaybackApiClient *api_ = nullptr;
    StmApiClient *stm_api_ = nullptr;
    ParkingZoneCanvas *canvas_ = nullptr;
    QListWidget *space_list_ = nullptr;
    QLineEdit *label_edit_ = nullptr;
    QComboBox *type_combo_ = nullptr;
    QCheckBox *enabled_check_ = nullptr;
    QComboBox *device_combo_ = nullptr;
    QLineEdit *sensor_zone_edit_ = nullptr;
    QLineEdit *geometry_edit_ = nullptr;
    QLabel *geometry_label_ = nullptr;
    QLabel *feedback_label_ = nullptr;
    QPushButton *save_button_ = nullptr;
    QPushButton *validate_button_ = nullptr;
    QPushButton *apply_button_ = nullptr;
    QVector<QPushButton *> channel_buttons_;
    QVector<ParkingSpace> spaces_;
    QVector<QVector<ParkingSpace>> undo_history_;
    QVector<QVector<ParkingSpace>> redo_history_;
    QImage reference_images_[4];
    QString draft_id_;
    QString geometry_id_;
    QString active_job_id_;
    QVector<StmDevice> known_devices_;
    bool devices_available_ = false;
    int channel_id_ = 1;
    int active_version_ = 0;
    int draft_version_ = 0;
    int selected_index_ = -1;
    bool dirty_ = false;
    bool loading_controls_ = false;
    bool validated_ = false;
    bool job_request_in_flight_ = false;
    QTimer *job_poll_timer_ = nullptr;
};

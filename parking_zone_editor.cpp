#include "parking_zone_editor.h"

#include "parking_zone_canvas.h"
#include "playback_api_client.h"
#include "stm_api_client.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QStandardItemModel>
#include <QStyle>
#include <QStringList>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

namespace {
QFrame *card(QWidget *parent) {
    auto *result = new QFrame(parent);
    result->setProperty("card", true);
    result->setFrameShape(QFrame::StyledPanel);
    return result;
}
}

ParkingZoneEditor::ParkingZoneEditor(PlaybackApiClient *api, StmApiClient *stm_api, QWidget *parent)
    : QWidget(parent), api_(api), stm_api_(stm_api), job_poll_timer_(new QTimer(this)) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);

    auto *toolbar = new QHBoxLayout;
    auto *back = new QPushButton("← 설정", this);
    toolbar->addWidget(back);
    auto *title = new QLabel("주차 구역 관리", this);
    title->setObjectName("pageTitle");
    toolbar->addWidget(title);
    toolbar->addSpacing(18);
    for (int channel = 1; channel <= 4; ++channel) {
        auto *button = new QPushButton(QString("CH %1").arg(channel), this);
        button->setCheckable(true);
        channel_buttons_.push_back(button);
        toolbar->addWidget(button);
        connect(button, &QPushButton::clicked, this,
                [this, channel]() { selectChannel(channel); });
    }
    toolbar->addStretch(1);
    geometry_label_ = new QLabel(this);
    geometry_label_->setProperty("muted", true);
    toolbar->addWidget(geometry_label_);
    root->addLayout(toolbar);

    auto *content = new QHBoxLayout;
    content->setSpacing(12);
    auto *canvas_card = card(this);
    auto *canvas_layout = new QVBoxLayout(canvas_card);
    auto *guide = new QLabel(
        "새 구역은 영상 위를 시계 방향으로 4번 클릭하세요. 점을 끌어 위치를 조정할 수 있습니다.",
        canvas_card);
    guide->setProperty("muted", true);
    canvas_layout->addWidget(guide);
    canvas_ = new ParkingZoneCanvas(canvas_card);
    canvas_layout->addWidget(canvas_, 1);
    content->addWidget(canvas_card, 1);

    auto *inspector = card(this);
    inspector->setFixedWidth(330);
    auto *inspector_layout = new QVBoxLayout(inspector);
    auto *inspector_title = new QLabel("구역 및 센서 연결", inspector);
    inspector_title->setProperty("sectionTitle", true);
    inspector_layout->addWidget(inspector_title);
    space_list_ = new QListWidget(inspector);
    inspector_layout->addWidget(space_list_, 1);
    auto *list_actions = new QHBoxLayout;
    auto *add_button = new QPushButton("+ 구역 추가", inspector);
    auto *remove_button = new QPushButton("구역 삭제", inspector);
    auto *undo_button = new QPushButton("실행 취소", inspector);
    auto *redo_button = new QPushButton("다시 실행", inspector);
    list_actions->addWidget(add_button);
    list_actions->addWidget(remove_button);
    inspector_layout->addLayout(list_actions);
    auto *history_actions = new QHBoxLayout;
    history_actions->addWidget(undo_button);
    history_actions->addWidget(redo_button);
    inspector_layout->addLayout(history_actions);
    auto *form = new QFormLayout;
    label_edit_ = new QLineEdit(inspector);
    type_combo_ = new QComboBox(inspector);
    type_combo_->addItem("일반", "general");
    type_combo_->addItem("전기차 전용", "ev_only");
    enabled_check_ = new QCheckBox("활성", inspector);
    device_combo_ = new QComboBox(inspector);
    sensor_zone_edit_ = new QLineEdit(inspector);
    geometry_edit_ = new QLineEdit(inspector);
    sensor_zone_edit_->setPlaceholderText("sensor_zone_id");
    geometry_edit_->setPlaceholderText("카메라 기준 geometry_id");
    form->addRow("Geometry ID", geometry_edit_);
    form->addRow("이름", label_edit_);
    form->addRow("유형", type_combo_);
    form->addRow("상태", enabled_check_);
    form->addRow("STM 장치", device_combo_);
    form->addRow("센서 구역", sensor_zone_edit_);
    inspector_layout->addLayout(form);
    auto *mapping_note = new QLabel(
        "STM 장치는 목록에서 선택합니다. 다른 구역에 이미 연결된 장치는 선택할 수 "
        "없습니다. 전기차 전용 구역에 장치를 연결하지 않으면 온도·화재 감지가 되지 않습니다.",
        inspector);
    mapping_note->setWordWrap(true);
    mapping_note->setProperty("muted", true);
    inspector_layout->addWidget(mapping_note);
    content->addWidget(inspector);
    root->addLayout(content, 1);

    auto *footer = new QHBoxLayout;
    feedback_label_ = new QLabel(this);
    feedback_label_->setWordWrap(true);
    feedback_label_->setProperty("settingsFeedback", true);
    footer->addWidget(feedback_label_, 1);
    save_button_ = new QPushButton("Draft 저장", this);
    validate_button_ = new QPushButton("검증", this);
    apply_button_ = new QPushButton("Pi에 적용", this);
    apply_button_->setProperty("primary", true);
    footer->addWidget(save_button_);
    footer->addWidget(validate_button_);
    footer->addWidget(apply_button_);
    root->addLayout(footer);

    connect(back, &QPushButton::clicked, this, &ParkingZoneEditor::backRequested);
    connect(add_button, &QPushButton::clicked, this, &ParkingZoneEditor::addSpace);
    connect(remove_button, &QPushButton::clicked, this, &ParkingZoneEditor::removeSpace);
    connect(undo_button, &QPushButton::clicked, this, &ParkingZoneEditor::undo);
    connect(redo_button, &QPushButton::clicked, this, &ParkingZoneEditor::redo);
    connect(save_button_, &QPushButton::clicked, this, &ParkingZoneEditor::saveDraft);
    connect(validate_button_, &QPushButton::clicked, this, &ParkingZoneEditor::validateDraft);
    connect(apply_button_, &QPushButton::clicked, this, &ParkingZoneEditor::applyDraft);
    connect(space_list_, &QListWidget::currentRowChanged, this, [this](int row) {
        storeSelection();
        selected_index_ = row;
        loadSelection();
        canvas_->setSelectedIndex(row);
    });
    connect(canvas_, &ParkingZoneCanvas::polygonEdited, this,
            [this](int index, const QVector<QPointF> &polygon) {
        if (index >= 0 && index < spaces_.size()) {
            spaces_[index].polygon = polygon;
            setDirty(true);
            refreshUi();
        }
    });
    connect(canvas_, &ParkingZoneCanvas::editBegan,
            this, &ParkingZoneEditor::pushHistory);
    const auto edited = [this]() {
        if (!loading_controls_ && selected_index_ >= 0) {
            storeSelection();
            setDirty(true);
        }
    };
    connect(label_edit_, &QLineEdit::textEdited, this, edited);
    connect(device_combo_, &QComboBox::currentIndexChanged, this, edited);
    connect(sensor_zone_edit_, &QLineEdit::textEdited, this, edited);
    connect(geometry_edit_, &QLineEdit::textEdited, this, [this](const QString &text) {
        if (loading_controls_) return;
        geometry_id_ = text.trimmed();
        setDirty(true);
    });
    connect(type_combo_, &QComboBox::currentIndexChanged, this, edited);
    connect(enabled_check_, &QCheckBox::toggled, this, edited);

    connect(api_, &PlaybackApiClient::parkingSpacesReceived, this,
            [this](const ParkingConfiguration &configuration) {
        if (configuration.channel_id != channel_id_) return;
        active_version_ = configuration.active_version;
        draft_version_ = configuration.draft_version;
        geometry_id_ = configuration.geometry_id;
        draft_id_.clear();
        spaces_ = configuration.spaces;
        undo_history_.clear();
        redo_history_.clear();
        selected_index_ = spaces_.isEmpty() ? -1 : 0;
        setDirty(false);
        validated_ = false;
        refreshUi();
        setFeedback(geometry_id_.isEmpty()
                        ? "활성 geometry_id가 없습니다. 카메라 측에서 받은 Geometry ID를 입력해야 Draft를 저장할 수 있습니다."
                        : "Pi의 활성 구역을 불러왔습니다. 변경 후 Draft 저장과 검증을 진행하세요.",
                    geometry_id_.isEmpty() ? "warning" : "ok");
    });
    connect(api_, &PlaybackApiClient::parkingDraftReceived, this,
            [this](const ParkingConfiguration &configuration) {
        if (configuration.channel_id != channel_id_) return;
        draft_id_ = configuration.draft_id;
        draft_version_ = configuration.draft_version;
        spaces_ = configuration.spaces;
        setDirty(false);
        validated_ = false;
        refreshUi();
        setFeedback(QString("Draft v%1을 저장했습니다. 검증 전에는 활성 설정이 바뀌지 않습니다.")
                        .arg(draft_version_), "ok");
    });
    connect(api_, &PlaybackApiClient::parkingValidationReceived, this,
            [this](const ParkingValidationResult &result) {
        if (result.draft_id != draft_id_) return;
        validated_ = result.valid;
        if (result.valid) {
            setFeedback("검증을 통과했습니다. 적용 후 카메라 readback 성공까지 기다립니다.", "ok");
        } else {
            QStringList codes;
            for (const auto &error : result.validation_errors) codes << error.code;
            setFeedback("검증 실패: " + codes.join(", "), "critical");
        }
        refreshUi();
    });
    connect(api_, &PlaybackApiClient::parkingApplyJobReceived,
            this, &ParkingZoneEditor::handleApplyJob);
    connect(stm_api_, &StmApiClient::devicesReceived, this, [this](const QVector<StmDevice> &devices) {
        known_devices_ = devices;
        devices_available_ = true;
        refreshUi();
    });
    connect(stm_api_, &StmApiClient::requestFailed, this,
            [this](const QString &operation, const QString &message, int) {
        if (operation != QStringLiteral("stm.devices")) return;
        devices_available_ = false;
        setFeedback(QString("STM 장치 목록을 불러오지 못했습니다: %1").arg(message), "warning");
        refreshUi();
    });
    connect(api_, &PlaybackApiClient::requestFailed, this,
            [this](const QString &operation, const QString &message, int status) {
        if (!operation.startsWith("parking.")) return;
        if (operation == "parking.job") job_request_in_flight_ = false;
        if (status == 409) {
            setFeedback("버전 충돌(409)입니다. 활성 설정을 다시 불러온 뒤 변경을 재적용하세요.", "critical");
        } else {
            setFeedback(QString("%1 실패%2: %3")
                            .arg(operation,
                                 status > 0 ? QString(" (HTTP %1)").arg(status) : QString(),
                                 message), "critical");
        }
        refreshUi();
    });
    job_poll_timer_->setInterval(1000);
    connect(job_poll_timer_, &QTimer::timeout, this, [this]() {
        if (!active_job_id_.isEmpty() && !job_request_in_flight_) {
            job_request_in_flight_ = true;
            api_->requestParkingApplyJob(active_job_id_);
        }
    });
    refreshUi();
}

void ParkingZoneEditor::openChannel(int channel_id, const QImage &image) {
    if (channel_id >= 1 && channel_id <= 4 && !image.isNull()) {
        reference_images_[channel_id - 1] = image;
    }
    if (channel_id == channel_id_) {
        canvas_->setReferenceImage(reference_images_[channel_id - 1]);
        setFeedback("Pi에서 활성 구역을 불러오는 중입니다.", "info");
        api_->requestParkingSpaces(channel_id_);
        stm_api_->fetchDevices();
    } else {
        selectChannel(channel_id);
    }
}

void ParkingZoneEditor::setReferenceImage(int channel_id, const QImage &image) {
    if (channel_id < 1 || channel_id > 4 || image.isNull()) return;
    reference_images_[channel_id - 1] = image;
    if (channel_id == channel_id_) canvas_->setReferenceImage(image);
}

bool ParkingZoneEditor::isDirty() const { return dirty_; }

bool ParkingZoneEditor::confirmDiscard(QWidget *parent) {
    if (!dirty_) return true;
    QMessageBox box(QMessageBox::Warning, "저장되지 않은 주차 구역 변경",
                    "Draft로 저장하지 않은 변경이 있습니다. 변경을 버리고 이동하시겠습니까?",
                    QMessageBox::NoButton, parent);
    auto *continue_button = box.addButton("계속 편집", QMessageBox::RejectRole);
    auto *discard_button = box.addButton("변경 폐기 후 이동", QMessageBox::DestructiveRole);
    box.setDefaultButton(qobject_cast<QPushButton *>(continue_button));
    box.exec();
    if (box.clickedButton() != discard_button) return false;
    setDirty(false);
    return true;
}

void ParkingZoneEditor::selectChannel(int channel_id) {
    if (channel_id < 1 || channel_id > 4 || channel_id == channel_id_) {
        if (channel_id == channel_id_ && spaces_.isEmpty()) {
            canvas_->setReferenceImage(reference_images_[channel_id - 1]);
            api_->requestParkingSpaces(channel_id_);
        }
        return;
    }
    if (!confirmDiscard(this)) return;
    channel_id_ = channel_id;
    spaces_.clear();
    undo_history_.clear();
    redo_history_.clear();
    selected_index_ = -1;
    draft_id_.clear();
    geometry_id_.clear();
    active_version_ = 0;
    draft_version_ = 0;
    validated_ = false;
    canvas_->setReferenceImage(reference_images_[channel_id_ - 1]);
    refreshUi();
    setFeedback("Pi에서 활성 구역을 불러오는 중입니다.", "info");
    api_->requestParkingSpaces(channel_id_);
    stm_api_->fetchDevices();
}

void ParkingZoneEditor::loadSelection() {
    loading_controls_ = true;
    const bool available = selected_index_ >= 0 && selected_index_ < spaces_.size();
    label_edit_->setEnabled(available);
    type_combo_->setEnabled(available);
    enabled_check_->setEnabled(available);
    device_combo_->setEnabled(available && devices_available_);
    sensor_zone_edit_->setEnabled(available);
    if (available) {
        const ParkingSpace &space = spaces_[selected_index_];
        label_edit_->setText(space.label);
        type_combo_->setCurrentIndex(qMax(0, type_combo_->findData(space.space_type)));
        enabled_check_->setChecked(space.enabled);
        sensor_zone_edit_->setText(space.stm_mapping.sensor_zone_id);
    } else {
        label_edit_->clear();
        sensor_zone_edit_->clear();
    }
    rebuildDeviceCombo();
    geometry_edit_->setText(geometry_id_);
    loading_controls_ = false;
}

void ParkingZoneEditor::storeSelection() {
    if (loading_controls_ || selected_index_ < 0 || selected_index_ >= spaces_.size()) return;
    ParkingSpace &space = spaces_[selected_index_];
    space.label = label_edit_->text().trimmed();
    space.space_type = type_combo_->currentData().toString();
    space.enabled = enabled_check_->isChecked();
    space.stm_mapping.device_uid = device_combo_->currentData().toString();
    space.stm_mapping.sensor_zone_id = sensor_zone_edit_->text().trimmed();
    if (auto *item = space_list_->item(selected_index_)) item->setText(space.label);
}

void ParkingZoneEditor::rebuildDeviceCombo() {
    // 현재 선택된 구역의 매핑을 기준으로 다시 그린다 — 콤보 자체의 이전 선택을
    // 보존하는 게 아니라, 구역이 바뀔 때마다 그 구역의 실제 매핑을 반영해야 한다.
    const bool has_selection = selected_index_ >= 0 && selected_index_ < spaces_.size();
    const QString target_uid = has_selection ? spaces_[selected_index_].stm_mapping.device_uid : QString();

    device_combo_->blockSignals(true);
    device_combo_->clear();
    device_combo_->addItem("(연결 안 함)", QString());

    QSet<QString> known_uids;
    for (const StmDevice &device : known_devices_) {
        known_uids.insert(device.device_uid);
        QString text = QString("slave %1 · %2").arg(device.slave_address).arg(device.device_uid.left(12));
        bool disabled = false;
        if (device.registration.registered) {
            const bool is_current_space = has_selection && device.registration.channel_id == channel_id_ &&
                                          device.registration.space_id == spaces_[selected_index_].space_id;
            if (!is_current_space) {
                const QString where = device.registration.space_label.isEmpty() ? device.registration.space_id
                                                                                 : device.registration.space_label;
                text += QString(" (CH%1 · %2에 등록됨)").arg(device.registration.channel_id).arg(where);
                disabled = true;
            }
        }
        device_combo_->addItem(text, device.device_uid);
        if (disabled) {
            if (auto *model = qobject_cast<QStandardItemModel *>(device_combo_->model())) {
                if (auto *item = model->item(device_combo_->count() - 1)) item->setEnabled(false);
            }
        }
    }

    // 활성 config엔 있지만(목록에는 없는) device_uid는 그대로 살려 둔다 —
    // 안 그러면 콤보가 "(연결 안 함)"으로 튀면서 저장 시 매핑이 조용히 날아간다.
    if (!target_uid.isEmpty() && !known_uids.contains(target_uid)) {
        device_combo_->addItem(QString("%1 (오프라인)").arg(target_uid.left(12)), target_uid);
    }

    const int index = device_combo_->findData(target_uid);
    device_combo_->setCurrentIndex(index >= 0 ? index : 0);
    device_combo_->blockSignals(false);
}

void ParkingZoneEditor::refreshUi() {
    for (int i = 0; i < channel_buttons_.size(); ++i)
        channel_buttons_[i]->setChecked(i + 1 == channel_id_);
    geometry_label_->setText(geometry_id_.isEmpty()
        ? QString("CH %1 · geometry 미제공").arg(channel_id_)
        : QString("CH %1 · active v%2 · %3").arg(channel_id_).arg(active_version_).arg(geometry_id_));
    space_list_->blockSignals(true);
    space_list_->clear();
    for (const ParkingSpace &space : spaces_)
        space_list_->addItem(space.label.isEmpty() ? space.space_id : space.label);
    space_list_->setCurrentRow(selected_index_);
    space_list_->blockSignals(false);
    canvas_->setSpaces(spaces_);
    canvas_->setSelectedIndex(selected_index_);
    loadSelection();
    QString local_message;
    const bool local_valid = locallyValid(&local_message);
    save_button_->setEnabled(dirty_ && !geometry_id_.isEmpty() && local_valid);
    validate_button_->setEnabled(!dirty_ && !draft_id_.isEmpty());
    apply_button_->setEnabled(!dirty_ && !draft_id_.isEmpty() && validated_ && active_job_id_.isEmpty());
}

void ParkingZoneEditor::setDirty(bool dirty) {
    if (dirty_ == dirty) return;
    dirty_ = dirty;
    if (dirty) validated_ = false;
    emit dirtyChanged(dirty);
    refreshUi();
}

void ParkingZoneEditor::setFeedback(const QString &text, const QString &severity) {
    feedback_label_->setText(text);
    feedback_label_->setProperty("severity", severity);
    feedback_label_->style()->unpolish(feedback_label_);
    feedback_label_->style()->polish(feedback_label_);
}

void ParkingZoneEditor::addSpace() {
    storeSelection();
    pushHistory();
    ParkingSpace space;
    space.space_id = "space-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    space.label = QString("주차 구역 %1").arg(spaces_.size() + 1);
    spaces_.push_back(space);
    selected_index_ = spaces_.size() - 1;
    setDirty(true);
    refreshUi();
    setFeedback("영상 위에 4개 점을 순서대로 지정하세요.", "info");
}

void ParkingZoneEditor::removeSpace() {
    if (selected_index_ < 0 || selected_index_ >= spaces_.size()) return;
    pushHistory();
    spaces_.removeAt(selected_index_);
    selected_index_ = qMin(selected_index_, static_cast<int>(spaces_.size()) - 1);
    setDirty(true);
    refreshUi();
}

void ParkingZoneEditor::pushHistory() {
    undo_history_.push_back(spaces_);
    if (undo_history_.size() > 50) undo_history_.removeFirst();
    redo_history_.clear();
}

void ParkingZoneEditor::undo() {
    if (undo_history_.isEmpty()) return;
    redo_history_.push_back(spaces_);
    spaces_ = undo_history_.takeLast();
    selected_index_ = spaces_.isEmpty()
        ? -1 : qBound(0, selected_index_, static_cast<int>(spaces_.size()) - 1);
    setDirty(true);
    refreshUi();
}

void ParkingZoneEditor::redo() {
    if (redo_history_.isEmpty()) return;
    undo_history_.push_back(spaces_);
    spaces_ = redo_history_.takeLast();
    selected_index_ = spaces_.isEmpty()
        ? -1 : qBound(0, selected_index_, static_cast<int>(spaces_.size()) - 1);
    setDirty(true);
    refreshUi();
}

bool ParkingZoneEditor::locallyValid(QString *message) const {
    // 서버(parking_space.cpp:255)도 stm_device_uid를 필수로 요구하지 않는다
    // (allow_empty=true) — Qt가 서버보다 엄격할 이유가 없다(Q4 확정 결정).
    // 장치 없는 전기차 전용 구역은 온도·화재 감지가 안 된다는 주의만
    // mapping_note로 안내한다.
    if (spaces_.isEmpty()) { *message = "구역을 하나 이상 추가하세요."; return false; }
    for (const ParkingSpace &space : spaces_) {
        if (space.label.trimmed().isEmpty() || space.polygon.size() != 4) {
            *message = "모든 구역에 이름과 4개 점이 필요합니다.";
            return false;
        }
    }
    return true;
}

void ParkingZoneEditor::saveDraft() {
    storeSelection();
    QString message;
    if (!locallyValid(&message)) { setFeedback(message, "warning"); return; }
    if (geometry_id_.isEmpty()) {
        setFeedback("카메라 기준 Geometry ID를 입력해야 Draft를 만들 수 있습니다.", "warning");
        return;
    }
    ParkingConfiguration configuration;
    configuration.channel_id = channel_id_;
    configuration.base_version = active_version_;
    configuration.geometry_id = geometry_id_;
    configuration.spaces = spaces_;
    setFeedback("Draft를 저장하는 중입니다.", "info");
    if (draft_id_.isEmpty()) api_->createParkingDraft(configuration);
    else api_->updateParkingDraft(draft_id_, configuration);
}

void ParkingZoneEditor::validateDraft() {
    if (!draft_id_.isEmpty()) {
        setFeedback("Pi에서 구역과 STM 매핑을 검증하는 중입니다.", "info");
        api_->validateParkingDraft(draft_id_);
    }
}

void ParkingZoneEditor::applyDraft() {
    if (draft_id_.isEmpty() || !validated_) return;
    const QString key = QUuid::createUuid().toString(QUuid::WithoutBraces);
    setFeedback("Pi와 카메라에 적용하고 readback을 확인하는 중입니다.", "info");
    api_->applyParkingDraft(draft_id_, key);
}

void ParkingZoneEditor::handleApplyJob(const ParkingApplyJob &job) {
    job_request_in_flight_ = false;
    if (!draft_id_.isEmpty() && job.draft_id != draft_id_) return;
    if (job.state == "applying") {
        active_job_id_ = job.job_id;
        job_poll_timer_->start();
        setFeedback("카메라 적용 및 readback 확인 중입니다.", "info");
    } else {
        job_poll_timer_->stop();
        active_job_id_.clear();
        if (job.state == "succeeded") {
            active_version_ = job.active_version;
            draft_id_.clear();
            validated_ = false;
            setFeedback("카메라 readback까지 확인되어 활성 설정이 갱신되었습니다.", "ok");
            api_->requestParkingSpaces(channel_id_);
        } else if (job.state == "unavailable") {
            setFeedback("카메라 주차 설정 어댑터가 아직 준비되지 않아 적용하지 못했습니다. 활성 설정은 변경되지 않았습니다.", "warning");
        } else {
            setFeedback("적용 실패: " + job.error_code, "critical");
        }
    }
    refreshUi();
}

#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QFrame;

struct LegalNoticePolicy {
    QString actual_retention_period;
    QString camera_installation_purpose;
    QString camera_location;
    QString camera_coverage;
    QString recording_schedule;
    QString operator_name;
    QString privacy_officer_contact;
    QString request_contact;
    QString privacy_policy_url;
    QString video_policy_url;
    QString policy_version;
    QString effective_date;
    QString last_updated_date;
};

class LegalNoticeWidget : public QWidget {
    Q_OBJECT

public:
    explicit LegalNoticeWidget(QWidget *parent = nullptr);

    void setPolicy(const LegalNoticePolicy &policy);
    void clearPolicy();
    bool hasCompletePolicy() const;

private:
    void updateStatus();
    void setPlainValue(QLabel *label, const QString &value);
    void setPolicyLink(QLabel *label,
                       const QString &url,
                       const QString &link_text);

    LegalNoticePolicy policy_;
    bool policy_complete_ = false;
    QLabel *status_badge_ = nullptr;
    QFrame *incomplete_notice_ = nullptr;
    QLabel *incomplete_notice_body_ = nullptr;
    QLabel *retention_value_ = nullptr;
    QLabel *installation_purpose_value_ = nullptr;
    QLabel *location_value_ = nullptr;
    QLabel *coverage_value_ = nullptr;
    QLabel *schedule_value_ = nullptr;
    QLabel *operator_value_ = nullptr;
    QLabel *officer_value_ = nullptr;
    QLabel *request_contact_value_ = nullptr;
    QLabel *privacy_policy_link_ = nullptr;
    QLabel *video_policy_link_ = nullptr;
    QLabel *policy_version_value_ = nullptr;
    QLabel *effective_date_value_ = nullptr;
    QLabel *last_updated_value_ = nullptr;
};

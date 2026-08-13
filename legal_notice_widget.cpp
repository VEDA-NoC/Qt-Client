#include "legal_notice_widget.h"

#include <QCoreApplication>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QSizePolicy>
#include <QStyle>
#include <QStyleOptionToolButton>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {

class DisclosureButton final : public QToolButton {
public:
    explicit DisclosureButton(const QString &text, QWidget *parent)
        : QToolButton(parent), text_(text) {
        setCheckable(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setProperty("legalSectionToggle", true);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QStyleOptionToolButton option;
        initStyleOption(&option);
        option.text.clear();
        option.icon = QIcon();
        option.arrowType = Qt::NoArrow;

        QPainter painter(this);
        style()->drawComplexControl(
            QStyle::CC_ToolButton, &option, &painter, this);

        const QRect content = rect().adjusted(12, 0, -12, 0);
        painter.setFont(font());
        painter.setPen(palette().buttonText().color());
        painter.drawText(content.adjusted(0, 0, -24, 0),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         text_);

        QStyleOption arrow;
        arrow.initFrom(this);
        arrow.rect = QRect(content.right() - 16,
                           content.center().y() - 8,
                           16,
                           16);
        style()->drawPrimitive(isChecked()
                                   ? QStyle::PE_IndicatorArrowDown
                                   : QStyle::PE_IndicatorArrowRight,
                               &arrow,
                               &painter,
                               this);
    }

private:
    QString text_;
};

QLabel *makeTitle(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setProperty("sectionTitle", true);
    return label;
}

QLabel *makeValue(QWidget *parent) {
    auto *label = new QLabel(parent);
    label->setProperty("legalValue", true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);
    return label;
}

QLabel *makeFixedValue(const QString &text, QWidget *parent) {
    auto *label = makeValue(parent);
    label->setText(text);
    return label;
}

void addSectionSpacing(QVBoxLayout *layout) {
    layout->addSpacing(6);
}

QWidget *makeCollapsibleSection(const QString &title,
                                QWidget *content,
                                QWidget *parent) {
    auto *section = new QWidget(parent);
    auto *layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    auto *toggle = new DisclosureButton(title, section);
    toggle->setChecked(false);
    content->setParent(section);
    content->setVisible(false);
    layout->addWidget(toggle);
    layout->addWidget(content);
    QObject::connect(toggle,
                     &QToolButton::toggled,
                     content,
                     [toggle, content](bool expanded) {
                         content->setVisible(expanded);
                         toggle->update();
                     });
    return section;
}

}  // namespace

LegalNoticeWidget::LegalNoticeWidget(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto *title_row = new QHBoxLayout();
    title_row->setContentsMargins(0, 0, 0, 0);
    title_row->addWidget(makeTitle("정보 및 법적 고지", this));
    title_row->addStretch(1);
    status_badge_ = new QLabel(this);
    status_badge_->setProperty("badge", true);
    title_row->addWidget(status_badge_);
    layout->addLayout(title_row);

    incomplete_notice_ = new QFrame(this);
    incomplete_notice_->setProperty("notice", "warning");
    auto *notice_layout = new QVBoxLayout(incomplete_notice_);
    notice_layout->setContentsMargins(14, 10, 14, 10);
    notice_layout->setSpacing(3);
    auto *notice_title =
        new QLabel("법적 고지 설정 미완료", incomplete_notice_);
    notice_title->setProperty("noticeTitle", true);
    incomplete_notice_body_ = new QLabel(incomplete_notice_);
    incomplete_notice_body_->setWordWrap(true);
    notice_layout->addWidget(notice_title);
    notice_layout->addWidget(incomplete_notice_body_);
    layout->addWidget(incomplete_notice_);

    addSectionSpacing(layout);
    layout->addWidget(makeTitle("핵심 고지", this));
    auto *summary_layout = new QFormLayout();
    summary_layout->setContentsMargins(0, 0, 0, 0);
    summary_layout->setHorizontalSpacing(22);
    summary_layout->setVerticalSpacing(8);
    summary_layout->addRow(
        "처리 목적",
        makeFixedValue(
            "주차구역 점유·위반 판정, 시설 안전 및 이의제기 처리",
            this));
    summary_layout->addRow(
        "처리 항목",
        makeFixedValue(
            "차량번호, 위반 evidence, 입출차·주차 시각",
            this));
    retention_value_ = makeValue(this);
    operator_value_ = makeValue(this);
    request_contact_value_ = makeValue(this);
    policy_version_value_ = makeValue(this);
    summary_layout->addRow("현장 실제 보존기간", retention_value_);
    summary_layout->addRow("운영 주체", operator_value_);
    summary_layout->addRow("열람·삭제 문의", request_contact_value_);
    summary_layout->addRow("정책 버전", policy_version_value_);
    layout->addLayout(summary_layout);

    addSectionSpacing(layout);
    layout->addWidget(makeTitle("개인정보 처리 요약", this));
    auto *privacy_content = new QWidget(this);
    auto *privacy_layout = new QFormLayout(privacy_content);
    privacy_layout->setContentsMargins(18, 6, 0, 8);
    privacy_layout->setHorizontalSpacing(22);
    privacy_layout->setVerticalSpacing(8);
    privacy_layout->addRow(
        "번호판 표시",
        makeFixedValue(
            "이벤트 목록에서는 기본 마스킹하며, 전체 번호는 권한 있는 "
            "상세 화면에서만 일시 표시하고 조회 이력을 기록합니다.",
            privacy_content));
    privacy_layout->addRow(
        "보존·자동 파기",
        makeFixedValue(
            "정상 주차는 raw 번호판과 crop을 별도로 보존하지 않습니다. "
            "camera+STM으로 확정된 EV 전용면 위반 evidence만 최대 30일 "
            "보존한 뒤 자동 파기하며, 이의제기 건은 legal hold 해제 시점까지 "
            "별도 관리합니다.",
            privacy_content));
    layout->addWidget(privacy_content);

    auto *operation_content = new QWidget(this);
    auto *operation_layout = new QFormLayout(operation_content);
    operation_layout->setContentsMargins(18, 6, 0, 8);
    operation_layout->setHorizontalSpacing(22);
    operation_layout->setVerticalSpacing(8);
    installation_purpose_value_ = makeValue(operation_content);
    location_value_ = makeValue(operation_content);
    coverage_value_ = makeValue(operation_content);
    schedule_value_ = makeValue(operation_content);
    officer_value_ = makeValue(operation_content);
    operation_layout->addRow("설치 목적", installation_purpose_value_);
    operation_layout->addRow("설치 장소", location_value_);
    operation_layout->addRow("촬영 범위", coverage_value_);
    operation_layout->addRow("촬영 시간", schedule_value_);
    operation_layout->addRow("관리책임자 연락처", officer_value_);
    layout->addWidget(makeCollapsibleSection(
        "영상정보처리기기 운영 상세", operation_content, this));

    addSectionSpacing(layout);
    layout->addWidget(makeTitle("정책·공식 출처", this));
    auto *policy_content = new QWidget(this);
    auto *policy_layout = new QFormLayout(policy_content);
    policy_layout->setContentsMargins(18, 6, 0, 8);
    policy_layout->setHorizontalSpacing(22);
    policy_layout->setVerticalSpacing(8);
    privacy_policy_link_ = makeValue(policy_content);
    video_policy_link_ = makeValue(policy_content);
    effective_date_value_ = makeValue(policy_content);
    last_updated_value_ = makeValue(policy_content);
    policy_layout->addRow("개인정보 처리방침", privacy_policy_link_);
    policy_layout->addRow("영상정보 운영·관리 방침", video_policy_link_);
    policy_layout->addRow("시행일", effective_date_value_);
    policy_layout->addRow("마지막 갱신일", last_updated_value_);

    auto *official_sources = new QLabel(
        "<a style=\"color:#D85D10;\" href=\"https://www.law.go.kr/lsLinkCommonInfo.do?chrClsCd="
        "010202&amp;lsJoLnkSeq=1027191397\">개인정보 보호법 제25조</a>"
        " · <a style=\"color:#D85D10;\" href=\"https://www.pipc.go.kr/\">개인정보보호위원회</a>",
        policy_content);
    official_sources->setOpenExternalLinks(true);
    official_sources->setTextInteractionFlags(Qt::TextBrowserInteraction);
    official_sources->setProperty("legalLink", true);
    policy_layout->addRow("공식 출처", official_sources);
    layout->addWidget(policy_content);

    addSectionSpacing(layout);
    layout->addWidget(makeTitle("오픈소스·제3자 라이선스", this));
    auto *license_layout = new QFormLayout();
    license_layout->setContentsMargins(0, 0, 0, 0);
    license_layout->setHorizontalSpacing(22);
    license_layout->addRow(
        "번들 글꼴",
        makeFixedValue("Pretendard · SIL Open Font License 1.1", this));
    layout->addLayout(license_layout);

    addSectionSpacing(layout);
    auto *product_layout = new QFormLayout();
    product_layout->setContentsMargins(0, 0, 0, 0);
    product_layout->setHorizontalSpacing(22);
    const QString version = QCoreApplication::applicationVersion().isEmpty()
                                ? "미지정"
                                : QCoreApplication::applicationVersion();
    product_layout->addRow(
        "제품", makeFixedValue("VEDA VMS Console", this));
    product_layout->addRow(
        "버전", makeFixedValue(version, this));
    layout->addLayout(product_layout);

    clearPolicy();
}

void LegalNoticeWidget::setPolicy(const LegalNoticePolicy &policy) {
    policy_ = policy;
    setPlainValue(retention_value_, policy.actual_retention_period);
    setPlainValue(installation_purpose_value_,
                  policy.camera_installation_purpose);
    setPlainValue(location_value_, policy.camera_location);
    setPlainValue(coverage_value_, policy.camera_coverage);
    setPlainValue(schedule_value_, policy.recording_schedule);
    setPlainValue(operator_value_, policy.operator_name);
    setPlainValue(officer_value_, policy.privacy_officer_contact);
    setPlainValue(request_contact_value_, policy.request_contact);
    setPolicyLink(privacy_policy_link_,
                  policy.privacy_policy_url,
                  "개인정보 처리방침 열기");
    setPolicyLink(video_policy_link_,
                  policy.video_policy_url,
                  "영상정보 운영·관리 방침 열기");
    setPlainValue(policy_version_value_, policy.policy_version);
    setPlainValue(effective_date_value_, policy.effective_date);
    setPlainValue(last_updated_value_, policy.last_updated_date);
    updateStatus();
}

void LegalNoticeWidget::clearPolicy() {
    setPolicy({});
}

bool LegalNoticeWidget::hasCompletePolicy() const {
    return policy_complete_;
}

void LegalNoticeWidget::updateStatus() {
    const QUrl privacy_url(policy_.privacy_policy_url, QUrl::StrictMode);
    const QUrl video_url(policy_.video_policy_url, QUrl::StrictMode);
    const auto is_https = [](const QUrl &url) {
        return url.isValid() && !url.host().isEmpty() &&
               url.scheme().compare("https", Qt::CaseInsensitive) == 0;
    };
    int missing_count = 0;
    const auto require_text = [&missing_count](const QString &value) {
        if (value.trimmed().isEmpty()) {
            ++missing_count;
        }
    };
    require_text(policy_.actual_retention_period);
    require_text(policy_.camera_installation_purpose);
    require_text(policy_.camera_location);
    require_text(policy_.camera_coverage);
    require_text(policy_.recording_schedule);
    require_text(policy_.operator_name);
    require_text(policy_.privacy_officer_contact);
    require_text(policy_.request_contact);
    if (!is_https(privacy_url)) {
        ++missing_count;
    }
    if (!is_https(video_url)) {
        ++missing_count;
    }
    require_text(policy_.policy_version);
    require_text(policy_.effective_date);
    require_text(policy_.last_updated_date);
    policy_complete_ = missing_count == 0;

    status_badge_->setText(
        policy_complete_
            ? "고지 설정 완료"
            : QString("설정 미완료 · %1개").arg(missing_count));
    status_badge_->setProperty("severity",
                               policy_complete_ ? "ok" : "warning");
    status_badge_->style()->unpolish(status_badge_);
    status_badge_->style()->polish(status_badge_);
    status_badge_->update();
    incomplete_notice_body_->setText(
        policy_complete_
            ? QString()
            : QString("현장 운영 정보 %1개가 등록되지 않았습니다. "
                      "관리자가 Pi 정책 설정을 완료할 때까지 실제 운영 "
                      "정보로 사용할 수 없습니다.")
                  .arg(missing_count));
    incomplete_notice_->setVisible(!policy_complete_);
}

void LegalNoticeWidget::setPlainValue(QLabel *label, const QString &value) {
    const QString trimmed = value.trimmed();
    label->setText(trimmed.isEmpty() ? "등록되지 않음" : trimmed);
    label->setProperty("missing", trimmed.isEmpty());
    label->style()->unpolish(label);
    label->style()->polish(label);
    label->update();
}

void LegalNoticeWidget::setPolicyLink(QLabel *label,
                                      const QString &url,
                                      const QString &link_text) {
    const QUrl parsed(url, QUrl::StrictMode);
    const bool valid = parsed.isValid() && !parsed.host().isEmpty() &&
                       parsed.scheme().compare("https", Qt::CaseInsensitive) == 0;
    if (!valid) {
        label->setText("등록되지 않음");
        label->setOpenExternalLinks(false);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setProperty("missing", true);
    } else {
        label->setText(QString("<a style=\"color:#D85D10;\" href=\"%1\">%2</a>")
                           .arg(parsed.toString(QUrl::FullyEncoded).toHtmlEscaped(),
                                link_text.toHtmlEscaped()));
        label->setOpenExternalLinks(true);
        label->setTextInteractionFlags(Qt::TextBrowserInteraction);
        label->setProperty("missing", false);
    }
    label->style()->unpolish(label);
    label->style()->polish(label);
    label->update();
}

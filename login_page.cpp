#include "login_page.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

LoginPage::LoginPage(QWidget *parent) : QWidget(parent) {
    auto *outer = new QVBoxLayout(this);
    outer->addStretch(1);

    auto *center_row = new QHBoxLayout;
    center_row->addStretch(1);

    auto *card = new QFrame(this);
    card->setProperty("card", true);
    card->setFrameShape(QFrame::StyledPanel);
    card->setFixedWidth(360);
    auto *card_layout = new QVBoxLayout(card);
    card_layout->setContentsMargins(32, 32, 32, 28);
    card_layout->setSpacing(16);

    auto *title = new QLabel("VEDA", card);
    title->setObjectName("pageTitle");
    title->setAlignment(Qt::AlignHCenter);
    auto *subtitle = new QLabel("VMS CONSOLE 로그인", card);
    subtitle->setProperty("muted", true);
    subtitle->setAlignment(Qt::AlignHCenter);
    card_layout->addWidget(title);
    card_layout->addWidget(subtitle);
    card_layout->addSpacing(8);

    username_edit_ = new QLineEdit(card);
    username_edit_->setPlaceholderText("사용자 이름");
    username_edit_->setText("operator");
    card_layout->addWidget(username_edit_);

    password_edit_ = new QLineEdit(card);
    password_edit_->setPlaceholderText("비밀번호");
    password_edit_->setEchoMode(QLineEdit::Password);
    card_layout->addWidget(password_edit_);

    feedback_label_ = new QLabel(card);
    feedback_label_->setWordWrap(true);
    feedback_label_->setProperty("settingsFeedback", true);
    feedback_label_->setVisible(false);
    card_layout->addWidget(feedback_label_);

    login_button_ = new QPushButton("로그인", card);
    login_button_->setProperty("primary", true);
    card_layout->addWidget(login_button_);

    auto *signup_button = new QPushButton("회원가입", card);
    card_layout->addWidget(signup_button);

    center_row->addWidget(card);
    center_row->addStretch(1);
    outer->addLayout(center_row);
    outer->addStretch(1);

    const auto attemptLogin = [this]() {
        // 실제 Control API 인증이 아니라 화면 전환용 하드코딩 게이트다
        // (진짜 인증은 설정 탭 "Control API 적용"에서 이 값을 그대로
        // 재사용해 서버와 검증한다). 그래서 서버 왕복 없이 여기서
        // 바로 판정한다.
        if (username_edit_->text() != QStringLiteral("operator")) {
            setFeedback("올바르지 않은 ID입니다.", "critical");
            return;
        }
        if (password_edit_->text() != QStringLiteral("123qweasdzxc")) {
            setFeedback("올바르지 않은 비밀번호입니다.", "critical");
            return;
        }
        setFeedback(QString(), QString());
        emit loginRequested();
    };
    connect(login_button_, &QPushButton::clicked, this, attemptLogin);
    connect(password_edit_, &QLineEdit::returnPressed, this, attemptLogin);
    connect(signup_button, &QPushButton::clicked, this, &LoginPage::signupRequested);
}

QString LoginPage::password() const {
    return password_edit_->text();
}

void LoginPage::clearPassword() {
    password_edit_->clear();
}

void LoginPage::setFeedback(const QString &text, const QString &severity) {
    feedback_label_->setText(text);
    feedback_label_->setProperty("severity", severity);
    feedback_label_->setVisible(!text.isEmpty());
    feedback_label_->style()->unpolish(feedback_label_);
    feedback_label_->style()->polish(feedback_label_);
}

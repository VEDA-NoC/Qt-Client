#include "signup_page.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

SignupPage::SignupPage(QWidget *parent) : QWidget(parent) {
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

    auto *title = new QLabel("회원가입", card);
    title->setObjectName("pageTitle");
    title->setAlignment(Qt::AlignHCenter);
    card_layout->addWidget(title);
    card_layout->addSpacing(8);

    auto *username_row = new QHBoxLayout;
    username_row->setSpacing(8);
    auto *duplicate_check_button = new QPushButton("중복 검사", card);
    auto *username_edit = new QLineEdit(card);
    username_edit->setPlaceholderText("새 사용자 이름");
    username_row->addWidget(duplicate_check_button);
    username_row->addWidget(username_edit, 1);
    card_layout->addLayout(username_row);

    auto *password_edit = new QLineEdit(card);
    password_edit->setPlaceholderText("비밀번호 설정");
    password_edit->setEchoMode(QLineEdit::Password);
    card_layout->addWidget(password_edit);

    auto *password_confirm_edit = new QLineEdit(card);
    password_confirm_edit->setPlaceholderText("비밀번호 재확인");
    password_confirm_edit->setEchoMode(QLineEdit::Password);
    card_layout->addWidget(password_confirm_edit);

    notice_label_ = new QLabel(card);
    notice_label_->setWordWrap(true);
    notice_label_->setProperty("settingsFeedback", true);
    notice_label_->setProperty("severity", "warning");
    notice_label_->setVisible(false);
    card_layout->addWidget(notice_label_);

    auto *footer_row = new QHBoxLayout;
    footer_row->setSpacing(8);
    auto *save_button = new QPushButton("저장", card);
    save_button->setProperty("primary", true);
    auto *back_button = new QPushButton("뒤로가기", card);
    footer_row->addWidget(save_button);
    footer_row->addWidget(back_button);
    card_layout->addLayout(footer_row);

    center_row->addWidget(card);
    center_row->addStretch(1);
    outer->addLayout(center_row);
    outer->addStretch(1);

    const auto showDemoNotice = [this]() {
        notice_label_->setText("데모 화면입니다 — 실제 계정 생성은 지원하지 않습니다.");
        notice_label_->setVisible(true);
        notice_label_->style()->unpolish(notice_label_);
        notice_label_->style()->polish(notice_label_);
    };
    connect(duplicate_check_button, &QPushButton::clicked, this, showDemoNotice);
    connect(save_button, &QPushButton::clicked, this, showDemoNotice);
    connect(back_button, &QPushButton::clicked, this, &SignupPage::backRequested);
}

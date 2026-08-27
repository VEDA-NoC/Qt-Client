#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

// 로그인 화면. 서버와 통신하지 않는다 — loginRequested()는 사용자명/
// 비밀번호가 하드코딩된 값("operator" / "123qweasdzxc")과 일치할 때만
// 내부에서 판정 후 emit된다(로컬 게이트). 틀리면 화면 안에서
// "올바르지 않은 ID/비밀번호입니다" 피드백만 보여주고 emit하지 않는다.
//
// 실제 Control API 인증은 password()가 반환하는 값을 기존
// MainWindow::applyControlSettings() 흐름(설정 탭 "Control API 적용")에
// 그대로 태워서 처리한다 — 여기서 통과한 값이 곧 실제 인증에도 쓰인다.
class LoginPage : public QWidget {
    Q_OBJECT

public:
    explicit LoginPage(QWidget *parent = nullptr);

    QString password() const;
    void clearPassword();
    void setFeedback(const QString &text, const QString &severity);

signals:
    void loginRequested();
    void signupRequested();

private:
    QLineEdit *username_edit_ = nullptr;
    QLineEdit *password_edit_ = nullptr;
    QLabel *feedback_label_ = nullptr;
    QPushButton *login_button_ = nullptr;
};

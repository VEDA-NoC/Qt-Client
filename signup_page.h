#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;

// 회원가입 화면. 데모용 정적 화면이다 — 중복 검사·저장 버튼은 클릭에
// 반응(안내 문구 표시)만 하고 실제 계정 생성은 하지 않는다. 서버에
// 계정 생성 API가 없다(credential 파일이 단일 계정만 허용).
// 뒤로가기만 실제로 로그인 화면으로 돌아가는 기능을 한다.
class SignupPage : public QWidget {
    Q_OBJECT

public:
    explicit SignupPage(QWidget *parent = nullptr);

signals:
    void backRequested();

private:
    QLabel *notice_label_ = nullptr;
};

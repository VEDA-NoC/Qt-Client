# Qt 다음 작업 시작 프롬프트

- 상태: 구현 완료 / 사용자 빌드·실기 검증 대기
- 작성 기준일: 2026-08-03

## 2026-08-03 구현 결과

- live grid는 `SetNoConstraint`와 `Ignored` size policy를 사용해 일반형·초광폭형에서
  viewport가 geometry를 소유한다. 세로형만 4개 panel의 명시적 최소 높이로 scroll
  content를 구성한다.
- `VideoPanel`은 영상 전용 host 안에서 child 영상 label의 geometry를 계산한다. child의
  maximum size를 부모 resize 중 변경하지 않으므로 layout feedback loop가 생기지 않는다.
  영상은 `KeepAspectRatio`로 렌더링하며 crop과 왜곡을 적용하지 않는다.
- 영상 통계는 항상 한 줄이며 panel 폭이 바뀌는 `resizeEvent`에서만 표시 항목 수를
  선택한다. 좁은 panel에서는 오른쪽 상세 항목부터 생략하고 hover tooltip은 사용하지
  않는다. 통계 값·source 해상도 갱신은 grid 높이를 바꾸지 않는다.
- 일반형·초광폭형은 행 수를 유지하면서 `2592:1520` surface가 들어가는 grid 폭과
  높이만 사용해 중앙 정렬한다. 남는 공간은 영상 내부 letterbox가 아니라 앱 배경으로
  둔다.
- 정보 및 법적 고지는 개인정보 요약, 정책 링크, 공식 출처, 제품·버전을 항상
  표시하고 `영상정보처리기기 운영 상세`만 전체 너비 disclosure로 유지한다.
- `/api/v1/status` polling은 5초를 유지하고, 수신한 `server_time.utc_ms`에
  `QElapsedTimer` 경과 시간을 더해 기존 1초 UI timer에서 서버 시각을 진행한다.
- 하단 상태는 `서버 ...`, `저장소 여유 available/total` 계열로 정리했다. CPU·온도,
  메모리와 `Pi 가동 시간`은 장치 상세에만 표시하고 load average는 UI에서 제외한다.
- Windows 저DPI의 Pretendard 한글 가로획 소실을 피하기 위해 번들 글꼴을 static TTF로
  교체하고 강제 vertical hinting은 제거했다. Control 인증서 기본 경로는 사용자 고정
  경로를 사용한다.
- 녹화 Timeline은 라이브 worker 종료 뒤 네 채널을 병렬 조회한다. 조회 기준의 `현재`와
  미래 시각 제한은 PC clock이 아니라 `server_time.utc_ms + monotonic elapsed`를
  사용하며 화면에는 서버 기준 KST로 표시한다.

빌드·실행·GUI 검증은 프로젝트 규칙에 따라 수행하지 않았으며 사용자가 검증한다.

### 사용자 실기 검증 추가 항목

1. 문제가 발생했던 96 DPI/Windows 배율에서 `스`, `시스템`, `오픈소스`, `저장소`의
   `ㅡ` 가로획이 유지되는지 확인한다.
2. 설정의 영상정보 운영 상세 row에서 title과 우측 chevron이 겹치지 않고 전체 row
   클릭과 키보드 `Space`로 열고 닫히는지 확인한다.
3. Control 인증서 입력란의 최초 값이
   `C:/Users/shini/Documents/Codex/2026-07-10/server.crt`인지 확인한다.
4. 장치 상세에 load average가 없고 CPU·온도·메모리·Pi 가동 시간만 남는지 확인한다.
5. 일반형 2x2에서 각 영상 surface가 `2592:1520`에 근접하고 검은 letterbox 대신
   grid 바깥의 앱 배경 여백이 생기는지 확인한다.
6. 모든 panel의 통계가 한 줄인지, 폭이 줄면 오른쪽 상세 항목부터 생략되고 hover
   tooltip이 나타나지 않는지 확인한다.
7. 재생 화면 진입 직후 라이브 worker가 모두 종료된 다음 CH1~CH4 Timeline 요청이
   병렬로 시작되는지 로그의 `request_id`와 `elapsed_ms`로 확인한다.
8. `현재`와 `다음 구간`이 서버 시각을 넘지 않고 `표시: 서버 기준 · KST`와 실제
   Timeline 눈금이 일치하는지 확인한다.

아래 내용을 새 Codex 작업의 시작 프롬프트로 사용한다.

```text
VEDA VMS Qt 프로젝트의 후속 작업이다.

먼저 다음 파일과 그 문서가 연결하는 관련 결정을 읽고 현재 source와 대조해라.
- AGENTS.md
- .ref/docs/README.md
- .ref/docs/qt-next-task-prompt.md
- .ref/docs/decisions/07-storage-and-operations.md
- .ref/docs/decisions/08-ui-brand-compliance.md
- .ref/docs/decisions/11-control-tls-and-playback-ui.md
- .ref/docs/qt-storage-status-test-plan.md

현재 working tree의 기존 변경은 다른 작업 소유일 수 있으므로 임의로 되돌리거나
정리하지 마라. 사용자가 직접 빌드·실행·실기 테스트한다. Codex는 build command,
실행 파일, GUI, offscreen platform을 포함해 직접 빌드하거나 실행하지 않는다.

이번 작업의 목표는 다음 UI·UX 문제를 현재 source 기준으로 분석하고, 사용자 승인 후
수정하는 것이다.

1. 라이브 4채널을 순차로 로드할 때 개별 영상 영역과 레이아웃 크기가 커졌다 작아지는
   원인을 확인한다. 특히 live grid의 size constraint, VideoPanel resizeEvent에서
   child QLabel 제약을 변경하는 로직, stats text의 wrap/size hint, 부모 layout과
   child geometry 사이 feedback loop를 확인한다. 영상 출력 widget이 별도 외부 창인지
   Qt 내부 child widget인지 source 근거로 설명한다.
2. 채널 수신 시작이나 source 해상도 변경이 부모 창과 grid geometry를 바꾸지 않도록
   안정적인 layout 경계를 제안한다. 일반 가로 화면 2x2, 세로 화면 4x1, 초광폭 1x4와
   가로 화면 복귀를 유지하고 네 채널의 행·열 크기를 동일하게 한다.
3. KeepAspectRatio를 유지해 crop과 왜곡은 기본 적용하지 않는다. 원본과 surface 비율이
   다를 때의 정상 letterbox와 잘못된 panel geometry 때문에 커진 불필요한 letterbox를
   구분해 설명하고 후자만 줄이는 계획을 제시한다.
4. 설정의 정보 및 법적 고지 화면을 정리한다. 짧은 개인정보 처리 내용은 항상
   표시하고 영상정보처리기기 운영 상세만 disclosure로 둔다. 작은 action button형
   토글 대신 전체 너비 section row와 chevron을 검토한다. 제품·버전은 하단에 항상
   표시하고 번들 글꼴은 오픈소스·제3자 라이선스에 포함한다.
5. /api/v1/status의 server_time.utc_ms를 5초마다 기준점으로 받고 monotonic elapsed를
   더해 기존 1초 UI timer에서 서버 시각을 진행시키는 방식을 검토한다. polling 주기는
   5초로 유지하고 NTP 동기화 상태는 별도로 표시한다.
6. 하단 상태 문구는 `Pi 정상` 대신 `서버 정상` 계열을, 저장소는
   `저장소 여유 388/491 GB` 형식을 우선 검토한다. CPU·온도·메모리는 상세 화면에
   둔다.
7. `Load 1m`은 요약에서 제외하고 상세에서 `시스템 부하(1분)`으로 표시한다. CPU
   percentage로 변환하거나 코어 수를 Qt에 하드코딩하지 않는다. `Uptime`은
   `Pi 가동 시간`으로 바꾸고, Pi API가 제공하기 전에는 VMS service uptime을
   추정하지 않는다.

UI·UX 문제이므로 첫 답변에서는 코드를 수정하지 말고 다음을 제시한다.
- source에서 확인된 정확한 원인과 근거 파일·함수
- 서로 연관된 영향
- 권장 동작과 변경할 파일
- crop 여부처럼 실제 선택이 필요한 대안
- 정적 검토와 사용자가 수행할 실기 테스트 계획

사용자가 계획을 승인한 뒤에만 코드와 필요한 문서를 수정한다. 승인 범위 안에서 새
UX 결정이 필요하면 임의 선택하지 말고 다시 계획 단계로 돌아간다.
```

## 다음 작업에서 확인할 주요 가설

아래는 확정 원인이 아니라 source로 검증할 조사 항목이다.

- `QGridLayout::SetMinimumSize`와 child size hint가 scroll area geometry를 밀어내는지
- `VideoPanel::resizeEvent()`가 영상 label의 maximum height를 변경해 layout 재계산을
  반복시키는지
- 연결 전후로 길이가 달라지는 통계 label의 word-wrap이 각 행 높이를 바꾸는지
- `Qt::KeepAspectRatio`의 정상 letterbox와 과도한 panel 높이가 합쳐져 보이는지

조사 결과와 수정 계획을 사용자가 승인하기 전에는 이 가설을 확정 설계로 옮기지 않는다.

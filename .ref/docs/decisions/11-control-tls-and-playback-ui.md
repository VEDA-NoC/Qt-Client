# 11. Control TLS와 녹화 재생 UI

- 상태: pivot 기반 조회 UX 승인 / 구현 및 실기 검증 진행
- 기준일: 2026-07-30

## 전역 규칙

- 인증서의 개인 키는 Qt 프로그램, 설치 파일, 사용자 PC에 포함하지 않는다.
- 네트워크 오류, TLS 검증 오류, 인증 오류를 서로 다른 사용자 메시지로 표시한다.
- 구현되지 않은 서버 기능을 동작하는 것처럼 보이는 UI로 제공하지 않는다.

## 전역 스킬

- 코드 변경과 검증 인계에는 `development-workflow`를 적용한다.
- UI 시안은 실제 Qt Widgets에서 구현할 수 있는 레이아웃과 상태를 기준으로 작성한다.

## 프로젝트 규칙

### 인증서 배포

`server.crt`는 공개 인증서이므로 파일 자체를 프로그램에 포함한다고 비밀이 노출되지는
않는다. 문제는 단일 Pi의 leaf 인증서를 앱에 고정하면 인증서 갱신, Pi 교체, 키 유출
대응 때마다 Qt 프로그램을 다시 배포해야 한다는 운영 결합이다.

제품 배포 권장안은 다음과 같다.

1. VEDA 전용 CA의 공개 root 인증서를 앱 또는 설치 프로그램에 포함한다.
2. 각 Pi는 자신의 hostname/IP SAN을 가진 고유 서버 인증서와 개인 키를 보관한다.
3. Qt는 CA chain과 hostname/IP SAN을 검증한다.
4. 장치 등록 시 필요한 경우 leaf fingerprint를 추가 pin으로 저장한다.
5. 일반 사용자가 매 실행마다 인증서 파일을 직접 선택하지 않도록 한다.

현재 자체 서명 `server.crt`를 사용하는 M6 단계에서는 파일 선택 방식을 개발용 장치
등록 절차로 유지한다. 추후 인증서를 앱 설정 영역에 복사해 등록한 뒤 원본 경로에
의존하지 않도록 변경한다.

### 오류 분류

| 상황 | 표시 문구 원칙 |
|---|---|
| 서버 미실행·연결 거부 | Control 서버에 연결할 수 없음 |
| 요청 시간 초과 | Control 서버 연결/응답 시간 초과 |
| CA·SAN 검증 실패 | TLS 검증 실패와 상세 원인 |
| 연결된 서버의 leaf pin 불일치 | 등록된 서버 인증서와 불일치 |
| HTTP 401, `invalid_credentials` | Control 비밀번호가 올바르지 않음 |
| token 만료 | 인증 세션 만료, 재로그인 안내 |

## 프로젝트 내용

### 녹화 재생 화면 권장 흐름

1. 사용자는 시작·종료 범위를 직접 조합하지 않고 기준 시각(pivot)과 표시 범위를
   지정한다. 기본 표시 범위는 1시간이며 pivot은 조회 결과의 약 1/3 지점에 둔다.
2. 조회 결과는 CH 1~4를 공통 시간축에 동시에 표시한다.
3. 녹화 구간은 얇은 연속 bar, 이벤트는 그 위의 marker로 구분한다.
   이벤트 시작은 세로선으로 표시하고 지속시간이 있으면 얇은 가로선을 연결한다.
   유형·심각도는 색과 후속 legend/filter로 구분한다.
4. 눈금과 시각 label을 표시하고 확대 단계에 따라 간격을 조절한다.
5. 사용자는 녹화가 존재하는 시각을 한 번 클릭해 재생 시작 시각을 정한다.
6. 단일 클릭은 재생 중인 영상을 멈추고 선택 시각의 썸네일과 채널·시각을 player에
   반영한다. 더블클릭 또는 재생 버튼은 해당 시각부터 새 one-shot session을 연다.
7. 재생, 일시정지, 정지, 속도, 현재/전체 시각은 영상 아래 control bar에 둔다.
8. 구간 드래그는 재생이 아니라 내보내기 기능에서만 사용한다.
9. 날짜·시간 입력과 달력 popup은 OS palette에 맡기지 않고 light theme 토큰으로
   통일해 어두운 popup, 낮은 대비, 잘린 label을 방지한다.
10. `Space`는 재생/일시정지, 좌우 방향키는 1초 이동, `F11`과 영상 더블클릭은
    전체화면 전환, `Esc`는 전체화면 종료로 사용한다.
11. 좌우 방향키 연속 입력은 250 ms 동안 합쳐 마지막 시각에 대해서만 새
    playback session을 발급한다.
12. 전체화면은 main window 상태를 변경하지 않고 별도 fullscreen player window를
    띄워, 종료 시 기존 최대화 창을 즉시 다시 보이게 한다.
13. 전체화면 player에도 재생/일시정지, 정지, 현재 시각, seek bar, 종료 버튼을
    항상 표시한다.
14. 타임라인은 `Ctrl+마우스 휠`과 확대·축소 아이콘, `캐시 전체` 버튼으로 조회
    결과 안에서
    확대·축소한다. 확대는 새 API 요청을 만들지 않는다. 휠은 포인터 시각, 버튼은
    pivot의 화면상 위치를 유지한다.
15. 세로 눈금은 화면 폭과 표시 시간에 따라 5초~24시간 후보 중 간격을 자동으로
    고르고 선택한 모든 급간을 표시한다. 날짜 자정 경계는 더 진한 회색선으로 별도
    표시한다.
16. 날짜는 keyboard 편집이 없는 달력 전용 버튼, 시간은 `HH:mm:ss` widget으로
    제공한다.
17. 인증 전에는 조회 control 전체를 비활성화하고 타임라인 중앙에 Control API
    안내를 우선 표시한다. 인증서·비밀번호의 입력 또는 교체가 실제로 필요할 때만
    안내 바로 아래에 `설정으로 이동` 버튼을 표시한다. 서버 일시 중단이나 token
    만료처럼 기존 설정을 재사용할 수 있는 상태에는 `다시 연결`만 표시한다.

### 현재 M6 API와 추가 요구사항

| 기능 | 현재 M6 | Qt 처리 | Pi API 추가 요구 |
|---|---|---|---|
| 4채널 timeline | 채널별 GET 가능 | 4회 조회 후 공통 축 표시 가능 | 없음 |
| 클릭 시각부터 재생 | session 범위 지정 가능 | recording 끝을 end로 사용 가능 | 없음 |
| 즉시 썸네일 | JPEG API 제공 | 선택 채널·시각 일치 응답만 preview 반영 | 구현 |
| 정지·재시작 | one-shot session 가능 | 새 session 발급 | 없음 |
| 일시정지·1초 seek | one-shot 재발급 가능 | frame PTS로 UTC를 계산하고 session 재발급 | GOP에 따른 실제 시작 오차 검증 |
| 배속 | forward 1.0x만 지원 | 1.0x 고정 | 추가 rate와 timestamp 규약 |
| 역재생 | 없음 | 전체 구간 client cache 없이는 부적절 | reverse trick-play 또는 역방향 frame API |
| Export | 생성·다운로드·삭제 흐름 제공 | 응답 schema 확정 후 구현 | HTTP status, JSON 예시, query 이름 확인 |

배속과 역재생은 녹화 MP4를 디코딩하지 않는 Pi 원칙과 충돌할 수 있다. Pi에서
재인코딩하지 않고 지원 가능한 `GStreamer`/RTSP trick-play 범위를 먼저 검증한 뒤
지원 속도 목록을 확정한다.

### 화면별 영상 자원 정책

- N2부터 각 live 채널은 Pi의 준비 순서와 같은 `High → Standard → Mobile` 순서로
  연결한다. 현재 profile을 baseline으로 계속 표시한 채 다음 profile을 candidate로
  열고, candidate의 첫 decoded frame에서만 baseline을 교체한다.
- Mobile 승격 뒤에는 `/chN/mobile` baseline worker를 앱의 명시적 전체 중지 전까지
  유지한다. 녹화 재생 화면에 들어가도 네 연결과 채널별 retry를 중지하지 않는다.
- 전체화면에서는 Mobile baseline을 유지한 채 High candidate를 연결한다. High 첫
  frame에서만 foreground로 표시하고, 전체화면 종료나 High hard failure 시 최신
  Mobile frame으로 즉시 복귀한다.
- 녹화 재생 화면을 나가면 playback을 현재 PTS에서 일시정지한다.
- 실시간 모니터링 화면으로 돌아갈 때 각 baseline worker가 보관한 최신 decoded
  `QImage`를 즉시 표시한다. baseline 재연결을 기다리지 않는다.
- playback 화면에서는 숨겨진 live `VideoPanel`을 매 frame 갱신하지 않는다. 아직
  Mobile까지 준비되지 않은 채널은 준비 전환을 계속하고, Mobile worker가 최신
  decoded `QImage` 한 장만 교체한다. 안정 상태의 기본 동시 실행 범위는
  `4 Mobile baseline + 1 playback`이다.
- 재생 worker가 없는 상태에서는 이전 수신·패킷·decode·UI 지표를 0으로 초기화한다.
- `UI 처리 fps`와 실제 unique pixmap `화면 갱신 fps`를 구분해 표시한다.

Mobile baseline 네 개와 playback 하나의 동시 CPU/RSS 한계는 사용자 실기에서
확인한다. 자동 ABR, hardware decoder와 GPU renderer는 N2 범위에 포함하지 않는다.

### Qt 표시 성능 정책

- decoder는 원본 해상도를 유지하되 RGB 변환 결과는 현재 panel에 맞는 크기로
  worker thread에서 직접 생성한다.
- worker downscale는 `SWS_BICUBIC`을 사용한다.
- GUI thread는 panel 크기의 `QImage`를 `QPixmap`으로 전달하며 같은 크기로 다시
  scaling하지 않는다.
- live 4채널의 고해상도 RGB frame을 GUI thread에서 매 frame 축소하는 구조는
  사용하지 않는다.

Pi의 live profile route는 `High`, `Standard`, `Mobile`이며 Qt는 route 이름으로
codec을 가정하지 않는다. 채널별 수동 selector는 두지 않고 현재 표시 profile을
비조작 badge로만 알린다. readiness 전환과 전체화면 High 전환은 모두 첫 decoded
frame 기준 make-before-break이며 candidate 실패는 현재 표시 worker와 frame을
중지하거나 지우지 않는다. 자동 ABR은 아니며 서버 준비 순서를 따라 Mobile warm
baseline까지 한 방향으로 내려가는 정책이다.

live decoder는 일시적인 `AVERROR_INVALIDDATA` packet 한 건으로 worker를 종료하지
않는다. 첫 frame 전에는 기존 15초 deadline까지 유효 frame을 기다리고, Playing 뒤에는
마지막 정상 decoded frame부터 5초간 frame이 없을 때만 decode stall로 재연결한다.
녹화 손상을 숨기지 않도록 playback decoder의 오류 정책은 기존 fatal 처리를 유지한다.

### 반응형 영상과 재생 조작

- 라이브 4채널은 세로형 `4행×1열`, 일반형 `2행×2열`, 초광폭형
  `1행×4열`로 재배치한다.
- 세로형에서는 네 행을 viewport 높이에 압축하지 않는다. 왼쪽 카메라 목록만
  세로로 scroll하며, 각 영상 surface는 사용 가능한 가로폭에서 원본
  `2592:1520` 비율로 높이를 계산한다.
- 최근 이벤트 panel은 세로형과 일반형에서 영상 오른쪽에 고정한다. 영상 목록과
  함께 아래로 이동해 tooltip과 본문이 잘리는 배치는 사용하지 않는다.
- 일반 창의 playback surface는 녹화 원본 `2592:1520` 비율을 유지하고 fullscreen은
  화면 전체를 사용한다.
- 영상 통계에는 원본 해상도와 현재 표시 surface 해상도를 함께 표시한다.
- playback 영상 단일 클릭은 OS double-click 판정 시간 후 재생/일시정지한다.
  더블클릭이 확인되면 대기 중인 단일 클릭을 취소하고 fullscreen만 전환한다.
- 타임라인 단일 클릭은 thumbnail, 더블클릭은 playback session 생성을 수행한다.
- 일반 창, fullscreen control과 `Space`는 하나의 재생 상태를 공유한다.
- 재생 상태는 `Preview`, `Starting`, `Playing`, `Pausing`, `Paused`, `Stopping`,
  `Error`로 명시하며 전환 중 중복 요청을 막는다. 버튼 text와 enabled 상태는 이
  상태 하나에서 파생한다.

### Timeline rolling metadata cache

- 최초 요청 크기는 `min(표시 범위 × 3, 24시간)`이다.
- 표시 window가 cache 경계에서 표시 범위의 절반 이내로 접근하면 250 ms debounce 후
  인접 구간을 background로 요청한다.
- Qt는 span과 event를 병합하고 중복을 제거하며 최근 3개 요청 block을 유지한다.
- 일반 mouse wheel은 시간축 이동, `Ctrl+wheel`은 확대·축소, 하단 navigator는 cache
  안에서 표시 window를 이동한다.
- channel 상태는 `조회 중`, `녹화 없음`, `조회 실패`, 녹화 bar 표시로 구분한다.
- Control 인증서 재적용과 TLS·pin·인증 실패 시 이전 thumbnail, frame, timeline,
  통계와 playback session을 즉시 제거한다.

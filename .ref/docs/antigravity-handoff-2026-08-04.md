# Antigravity 작업 인계 — 채널 재연결·주차 구역 편집기·하단 상태 UI

- 작성일: 2026-08-04
- 대상 저장소: `C:/Users/shini/Documents/QtProjects/qt_4ch_viewer`
- 관련 RPi 작업 폴더:
  `C:/Users/shini/Documents/Codex/2026-07-10/rtsps-codex-hanwha-rtsp-raspberry-pi`
- 현재 단계: channel별 retry와 footer 재배치 정적 구현 완료·사용자 검증 대기,
  주차 편집기와 Pi API 구현 대기

## 1. 인계 목적과 작업 규칙

다음 작업자는 아래 순서로 규칙과 기준선을 읽고 현재 source를 다시 확인한다.

1. 루트 `AGENTS.md`
2. `.ref/docs/README.md`
3. 이 문서
4. `.ref/docs/decisions/06-network-resilience.md`
5. `.ref/docs/decisions/08-ui-brand-compliance.md`
6. `.ref/docs/decisions/10-parking-zone-lpr-ev.md`
7. `.ref/docs/decisions/11-control-tls-and-playback-ui.md`
8. `.ref/docs/qt-m6-playback-test-plan.md`

중요한 프로젝트 제약은 다음과 같다.

- 사용자가 Windows Qt 환경에서 직접 빌드·실행·실기 테스트한다.
- 사용자가 별도로 요청하지 않는 한 에이전트는 빌드, 실행, GUI 또는 offscreen
  테스트를 수행하지 않는다.
- 현재 폴더는 2026-08-04 정적 확인 기준 Git 저장소가 아니다. `git status`는
  `fatal: not a git repository`를 반환한다. 따라서 파일을 임의로 되돌리거나
  정리하지 말고 현재 파일 내용을 기준선으로 취급한다.
- Qt는 Pi HTTPS/RTSPS만 사용하고 카메라 또는 STM32를 직접 호출하지 않는다.
- 기존 Timeline 3배 선행·rolling cache는 의도된 기능이다. 성능 문제 해결을 이유로
  이를 제거하거나 최초 조회 범위를 다시 1배로 줄이지 않는다.

## 2. 현재 저장소에서 확인된 완료 변경

아래 항목은 source에 구현돼 있다. 다만 대부분 사용자의 최종 실기 확인은 아직
별도로 남아 있다.

### 2.1 앱 셸과 실시간 4채널

- `MainWindow::createUi()`가 실시간, 녹화 재생, 이벤트, 장치 및 제어, 설정의 다섯
  페이지를 구성한다.
- `MainWindow::createSidebar()`는 Navy sidebar와 Orange 선택 표시를 사용한다.
- `MainWindow::updateLiveLayout()`는 창 종횡비에 따라 다음 배치를 사용한다.
  - 일반형: 2행×2열
  - 세로형: 4행×1열, 영상 목록만 세로 scroll
  - 초광폭형: 1행×4열
- 일반형과 초광폭형에서는 `2592:1520` surface가 viewport 안에 들어가는 크기를
  계산하고 grid 자체를 중앙 배치한다.
- `VideoPanel`은 별도 OS 창이 아니라 Qt 내부 child widget이다.
- `VideoPanel::updateVideoSurfaceGeometry()`와
  `VideoPanel::updateVideoPixmap()`은 `Qt::KeepAspectRatio`를 사용한다.
- `VideoPanel::updateStatsHeight()`는 panel resize 때만 한 줄에 표시할 통계 밀도를
  결정하고, `updateStatsLabel()`은 tooltip을 비운다.
- `StreamWorker`는 현재 panel surface 크기에 맞춰 worker thread에서 RGB frame을
  downscale하고 GUI thread에 전달한다.

### 2.2 글꼴과 디자인 토큰

- `resources.qrc`에 Pretendard Regular, Medium, SemiBold, Bold 정적 TTF 네 개가
  포함돼 있다.
- `AppTheme::loadBundledFonts()`와 `AppTheme::apply()`가 번들 글꼴과 QSS를 적용한다.
- 이전의 저DPI 한글 `ㅡ` 가로획 소실은 사용자가 해결됐다고 확인했다.
- 사용자는 글자 두께가 일부 위치에서 불규칙해 보이는 현상은 남아 있다고 보고했다.
  이 문제는 아직 원인 확정이나 추가 수정이 이뤄지지 않았다.

### 2.3 Control TLS, 상태와 서버 시각

- `PlaybackApiClient`는 HTTPS, bearer token, `server.crt`, SAN 검증과 leaf pin을
  사용한다.
- 설정의 인증서 기본 경로는
  `C:/Users/shini/Documents/Codex/2026-07-10/server.crt`이다.
- `/api/v1/status`는 5초 간격으로 조회한다.
- `MainWindow::applyDeviceStatus()`가 `server_time.utc_ms`를 기준점으로 저장하고,
  `updateServerTimeDisplay()`가 monotonic elapsed를 더해 1초 단위로 서버 시각을
  진행시킨다.
- CPU·온도, 메모리, Pi 가동 시간, throttling은 `장치 및 제어` 상세에 표시한다.
- load average는 현재 UI에서 제외돼 있다.

### 2.4 Timeline과 녹화 재생

- `MainWindow::performPlaybackApiAction(kPlaybackApiActionTimeline)`은 live worker가
  모두 종료된 뒤 CH1~CH4 Timeline 요청을 한 번에 시작한다.
- `PlaybackApiClient::monitorReply()`는 request ID, operation, timeout, URL, 완료
  HTTP 상태와 elapsed time을 기록한다.
- 최초 Timeline fetch는 `min(표시 범위 × 3, 24시간)`이다.
- `MainWindow::scheduleTimelinePrefetch()`는 cache 경계 접근 시 인접 구간을 요청한다.
- `MainWindow::finalizeTimelineWindowRequest()`는 최근 세 요청 block 범위 안에서
  cache를 유지하고 `TimelineWidget::pruneToRange()`로 정리한다.
- 기준 시각, 현재와 미래 제한, 화면 표시는 PC clock이 아니라 보간된 서버 시각과
  KST를 사용한다.
- playback fullscreen 전환 시 `togglePlaybackFullscreen()`이 worker output size를
  fullscreen surface로 바꾸고, 복귀 시 일반 player surface로 되돌린다.
- 위 fullscreen 화질 전환은 source에는 구현돼 있지만 사용자가 실제 화질 개선을
  최종 확인한 기록은 없다.

### 2.5 주차 구역 UX 콘셉트

다음 콘셉트가 생성됐고 사용자는 전체 디자인 방향을 승인했다.

- `.ref/ui/parking-zone-concepts-v1/01-settings-entry.png`
- `.ref/ui/parking-zone-concepts-v1/02-four-point-editor.png`
- `.ref/ui/parking-zone-concepts-v1/03-apply-readback.png`
- `.ref/ui/parking-zone-concepts-v1/README.md`

이미지 생성 과정에서 세 번째 그림의 상단 문구가
`꼭짓점 4/4 · 다음 모서리를 클릭하세요`로 잘못 표현됐다. 실제 구현 문구는
`꼭짓점 4/4 · 구역 지정 완료`여야 한다.

### 2.6 2026-08-04 후속 구현

- live worker를 channel index가 고정된 네 slot으로 관리하도록 변경했다.
- channel별 retry timer, 실패 횟수와 30초 stable timer를 추가했다.
- 실패한 channel만 `1, 2, 4, 8, 15, 30초 상한 + jitter`로 재연결한다.
- 사용자 stop, playback 진입, URL 전체 재적용과 종료에서는 예약된 retry를
  취소한다.
- `[live-retry]`에 channel, attempt, delay, reason과 stable reset을 기록한다.
- footer가 sidebar 아래까지 전체 창 너비를 차지하도록 app body와 분리했다.
- footer 왼쪽은 user icon, `operator`, Control 연결 상태를 표시한다.
- 영상 수와 total Mbps는 live 상단으로 이동했다.
- 서버·저장소 정상 badge는 숨기고 warning/critical 또는 설정 필요 상태만 footer에
  표시한다.
- footer 오른쪽은 PC clock을 제거하고 서버 기준 KST를 유지한다.

위 변경은 프로젝트 규칙에 따라 빌드·실행하지 않았으며 사용자 검증 대기다.

## 3. 확인된 미완료 작업

### 3.1 일부 live 채널만 실패했을 때의 자동 재연결 — 구현 완료, 검증 대기

현재 `MainWindow`는 `QVector<StreamWorker *> workers_` 하나로 모든 채널 worker를
관리한다.

- `MainWindow::startStreams()`는 worker가 하나라도 남아 있으면 전체 worker를 먼저
  중지한 뒤 전체 재연결을 예약한다.
- 각 worker의 `finished` handler는 `workers_.removeOne(worker)`만 수행한다.
- 모든 worker가 사라진 경우에만 전체 restart 또는 stopped 처리가 실행된다.
- 따라서 CH2만 비정상 종료되고 CH1·CH3·CH4가 계속 재생되면 CH2 worker는 자동으로
  다시 만들어지지 않는다.

구현된 승인 동작은 다음과 같다.

- 실패한 채널만 재연결하고 정상 채널 worker는 유지한다.
- retry 간격은 `1, 2, 4, 8, 15, 30초 상한 + jitter`를 사용한다.
- 사용자 정지, playback 진입, 앱 종료, RTSPS URL 전체 재적용 때는 예약된 retry를
  취소한다.
- 한 채널에 worker가 둘 이상 존재하지 않게 한다.
- 안정적인 `Playing`이 약 30초 유지되면 해당 채널의 실패 횟수를 초기화한다.
- UI에는 `재연결 대기 4초`, `재연결 중`처럼 자동 복구 중임을 표시한다.
- 정상 채널의 영상과 통계를 초기화하지 않는다.

향후 정책 변경도 고려한다. 사용자는 30초 상한에 도달한 뒤 무조건 RTSP open을
반복하기보다 Pi/채널 상태를 먼저 확인하는 방식을 선호한다. 첫 구현은 승인대로
백오프 재시도로 하되 아래를 분리한다.

1. worker 종료 원인 판정
2. 다음 행동 결정 정책
3. 채널 worker 재생성

이 구조라면 후속 작업에서 30초 상한 이후의 행동만 `status/capability 확인 → 채널이
준비됐을 때 재연결`로 바꿀 수 있다.

### 3.2 하단 상태 영역 재배치 — 구현 완료, 검증 대기

현재 `MainWindow::createBottomBar()`는 왼쪽부터 다음을 표시한다.

- `영상 0/4`
- `수신 0.00 Mbps`
- 서버 상태 badge
- 저장소 상태 badge
- 서버 시각
- PC 시각

사용자가 승인한 방향은 콘셉트처럼 하단 왼쪽에 작은 사용자 아이콘, 사용자명과
초록 연결 상태를 두는 것이다. 현재 `LIGHT THEME · M3 UI SHELL` caption은 제거 대상이다.

현재 실제 Control 사용자인 `operator`를 표시한다. 영상 수와 총 수신량은 live 상단,
서버·저장소 상세는 `장치 및 제어`, 비정상 상태만 전역 footer, 오른쪽 시각은 서버
기준 KST로 배치했다. 실제 account/role API가 생기면 고정 문자열을 서버 사용자
정보로 교체한다.

### 3.3 주차 구역 편집기

현재 `MainWindow::createSettingsPage()`에는
`주차 구역·번호판·전기차 판정` 안내 카드만 있고 편집기, polygon overlay, hit-test,
주차 API client는 없다.

승인된 구현 방향은 다음과 같다.

- 주 진입점은 `설정 → 주차 구역 관리` 카드의 `관리 화면 열기`이다.
- 설정 카드 안에서 편집기를 펼치지 않고 전용 전체 페이지로 이동한다.
- 상단에서 CH1~CH4, 기준 영상 시각·해상도, geometry 상태, active version을 표시한다.
- Pi가 제공한 고정 기준 snapshot 위에서 정확히 네 점을 직접 지정한다.
- 기존 구역은 녹색, 편집 중인 구역은 Orange polygon과 1~4번 handle로 표시한다.
- 오른쪽 inspector에서 구역명, 유형, 활성 여부와 STM immutable UID/sensor zone을
  매핑한다.
- 로컬 검증, 초안 저장, Pi 검증, 적용, 카메라 apply/read-back을 분리한다.
- active version read-back이 완료되기 전에는 적용 성공으로 표시하지 않는다.
- `409` version/geometry 충돌 때 로컬 초안을 보존하고 자동 덮어쓰기하지 않는다.
- 좌표는 영상 사각형 안에서만 받고 `[0,1]` normalized coordinate로 저장한다.
- letterbox 또는 canvas 여백 클릭은 점으로 인정하지 않는다.
- v1에는 자동 주차선 검출, 3D cuboid, homography를 추가하지 않는다.

실시간 채널 메뉴에서 동일 편집기로 이동하는 shortcut은 이전에 제안됐지만 생성된
콘셉트에는 포함되지 않았고 별도 승인을 받지 않았다. 임의로 추가하지 않는다.

### 3.4 미저장 변경 이탈 방지

사용자는 편집 변경이 있을 때 다음 동작을 경고 popup으로 막아 달라고 요청했다.

- 다른 CH로 전환
- 다른 페이지 또는 설정 화면으로 이동
- 기준 snapshot 새로고침
- 앱 창 닫기

OS의 Alt+Tab까지 막는 요구는 아니다. 중앙 navigation guard가 편집기의 dirty state를
확인하도록 구현한다. popup 버튼의 정확한 문구와 동작은 아직 최종 승인을 받지 않았다.
이전 권장안은 다음 세 가지다.

- `계속 편집`
- `초안 저장 후 이동`
- `변경 폐기 후 이동`

Pi에 apply job이 이미 접수된 상태는 unsaved dirty state와 구분한다. job은 Qt 화면을
벗어나도 Pi에서 계속 처리되며 Qt가 job ID로 상태를 다시 조회해야 한다.

## 4. 관련 Qt 파일과 함수

| 파일 | 현재 책임 | 후속 작업에서 확인·수정할 지점 |
|---|---|---|
| `main_window.cpp` | 앱 셸, 페이지, live/playback worker, 상태 polling | `createUi`, `createSidebar`, `createTopBar`, `createBottomBar`, `createSettingsPage`, `setCurrentPage`, `startStreams`, `stopStreams`, `requestWorkerStop`, `finalizeStoppedState`, `handleChannelStatus`, `refreshConnectionSummary`, `updateTotalStats` |
| `main_window.h` | `MainWindow` 상태와 widget 포인터 | channel별 worker slot/timer/attempt, parking page, dirty guard, footer widget state 추가 |
| `stream_worker.cpp` | FFmpeg RTSPS open/read/decode와 상태 문자열 emit | 비정상 종료와 사용자 stop을 구조적으로 구분할 final result 도입 검토 |
| `stream_worker.h` | `StreamWorker`, `StreamStats` | retry 자체를 worker 안에 넣지 말고 owner가 판단할 수 있는 종료 정보 제공 |
| `video_panel.cpp` | video child geometry, 통계, 상태 label | `재연결 대기`, dimmed last-frame 정책과 향후 read-only polygon overlay 경계 확인 |
| `video_panel.h` | live/playback panel API | 편집기는 별도 canvas로 두고 live overlay와 editing hit-test를 섞지 않음 |
| `playback_api_client.cpp` | 기존 login/status/timeline/thumbnail/playback HTTPS | 공통 인증·TLS·request logging 패턴 재사용, 기존 Timeline 로직 변경 금지 |
| `playback_api_client.h` | 기존 Control API 공개 함수와 signal | 주차 API를 여기에 계속 누적하지 말고 별도 `ParkingApiClient` 분리 권장 |
| `playback_types.h` | playback/status DTO | 주차 DTO는 별도 `parking_types.h` 권장 |
| `theme.cpp` | 공통 QSS와 semantic state | parking canvas/inspector/footer도 기존 token과 dynamic property를 사용 |
| `CMakeLists.txt` | Qt/FFmpeg 및 source 등록 | 새 parking source 추가, 새 외부 라이브러리는 필요하지 않음 |
| `resources.qrc` | TTF와 아이콘 resource | 새 아이콘이 필요하면 기존 icon workflow를 따름 |

권장 신규 Qt 단위는 다음과 같다.

- `parking_types.h`
- `parking_api_client.h/.cpp`
- `parking_zone_editor_widget.h/.cpp`
- `parking_zone_canvas.h/.cpp`

`main_window.cpp`가 이미 약 3,500줄이므로 polygon geometry와 API parser를 다시 이
파일에 직접 쌓지 않는다.

## 5. Pi API 의존성과 ownership 경계

### 5.1 문서에 정의된 예정 endpoint

RPi 설계 문서
`project-docs/rpi-vms/camera-parking-event-integration-plan-ko.md`에는 다음 endpoint가
계획돼 있다.

```text
GET    /api/v1/capabilities
GET    /api/v1/channels
GET    /api/v1/channels/{id}/snapshot
GET    /api/v1/parking-spaces?channel_id=...
POST   /api/v1/parking-space-drafts
PUT    /api/v1/parking-space-drafts/{draft_id}
POST   /api/v1/parking-space-drafts/{draft_id}/validate
POST   /api/v1/parking-space-drafts/{draft_id}/apply
GET    /api/v1/parking-space-apply-jobs/{job_id}
POST   /api/v1/parking-space-configs/{version}/rollback
```

write에는 `base_version` 또는 `If-Match`와 idempotency key가 필요하다. Qt는 camera
endpoint를 직접 호출하지 않는다.

### 5.2 실제 RPi 구현 상태

2026-08-04 정적 확인 기준
`repos/rpi-vms/src/control/control_server.cpp`에는 login, status, timeline,
thumbnail, playback session, export route가 있지만 위 parking-space, draft,
snapshot/capabilities route는 없다. 주차 API의 실제 response schema와 오류 body도
구현돼 있지 않다.

사용자는 Timeline 지연의 원인이 Pi 직렬화와 DB 조회 병목이었고 이를 해결했다고
알렸다. 이는 Qt 저장소 밖의 완료 사실이며, Qt에서는 기존 3배 cache를 유지한다.

### 5.3 Qt 연동 전에 Pi에서 확정할 계약

- `schema_version`
- Qt/VMS channel ID `1..4`와 camera channel `0..3` 변환 ownership
- snapshot JPEG 전달 방식과 `geometry_id`, 원본 width/height, rotation, flip,
  dewarp, transform metadata
- `parking_space_id`, `display_label`, `space_type`, `enabled`
- 정확히 네 점인 clockwise normalized polygon
- `stm_device_uid`와 선택적 `sensor_zone_id` 목록 endpoint
- `base_version`, draft version, active version과 ETag 규칙
- validation machine-readable code와 field/point 위치
- apply job 상태, camera apply와 read-back 세부 상태, poll interval과 terminal state
- `409`, `422`, `401/403`, offline/unsupported 응답 예시
- rollback과 audit 필드
- 관리자 권한과 현재 `operator` 계정의 write 허용 여부

위 schema가 정해지기 전에는 동작하는 것처럼 보이는 fake apply를 만들지 않는다.
Qt canvas와 로컬 validation까지는 독립적으로 구현할 수 있지만 실제 `초안 저장`,
`검증`, `적용` 버튼은 capability가 없으면 비활성화하고 `Pi 지원 필요`를 표시한다.

## 6. 권장 구현 순서

### 단계 1 — channel별 live retry

1. `workers_`의 remove-only collection을 channel index가 고정된 slot 구조로 바꾼다.
2. channel별 retry timer, attempt, last terminal reason과 stable timer를 둔다.
3. `startChannelStream(channel)`과 `stopChannelStream(channel)`을 분리한다.
4. 사용자 stop과 playback 진입은 desired-live state를 false로 바꾸고 모든 retry를
   취소한다.
5. 비정상 종료 때 해당 channel만 backoff로 예약한다.
6. 로그에 channel, attempt, delay, reason, desired state를 남긴다.
7. 기존 전체 `다시 연결`과 RTSPS Base URL 재적용은 모든 slot을 명시적으로 교체한다.
8. 정책 판정 함수를 분리해 후속 30초 status probe 전환에 대비한다.

### 단계 2 — footer와 상태 정보 재배치

세부 배치를 사용자에게 확인한 뒤 구현한다.

1. sidebar의 `LIGHT THEME · M3 UI SHELL` 제거
2. footer 왼쪽 사용자/연결 상태 구현
3. 영상 수와 total Mbps를 live 전용 상단 상태로 이동
4. 서버·저장소 정상/경고 표시 위치 정리
5. footer 오른쪽 서버 시각 우선 표시
6. 기존 장치 상세와 5초 status polling은 유지

### 단계 3 — parking editor 로컬 UI

1. 승인된 설정 진입 카드와 전용 editor page 구현
2. fixed snapshot canvas와 rendered-image rectangle 좌표 변환 구현
3. 정확히 네 점 생성, handle drag, 선택, 삭제, undo/redo 구현
4. 범위, 자기 교차, winding, 최소 edge/area, active polygon overlap 로컬 검증
5. space inspector와 STM mapping placeholder/capability 상태 구현
6. dirty state와 navigation/close guard 구현
7. API 미지원 상태에서는 읽기·적용 가능 여부를 명확히 구분

### 단계 4 — Pi M7.2 configuration API

Pi 저장소 ownership으로 schema, DB, validation, camera apply/read-back과 tests를 먼저
완성한다. 이 단계는 Qt source에서 임의 구현하지 않는다.

### 단계 5 — Qt API 연동

1. `ParkingApiClient`에서 기존 TLS/session 패턴 재사용
2. capabilities/channels/snapshot/active spaces 조회
3. draft create/update/validate
4. idempotent apply와 job polling
5. verified read-back 이후 active UI 갱신
6. 409 conflict, 422 validation, auth/offline/unsupported 처리
7. Qt 재시작 또는 화면 재진입 시 진행 중 job 복구

### 단계 6 — read-only live overlay

편집기와 API가 검증된 뒤에만 active polygon을 live page에 읽기 전용으로 표시한다.
live 영상에서 직접 편집하거나 기존 single/double-click 동작과 hit-test를 섞지 않는다.

## 7. 사용자 빌드와 검증 절차

### 7.1 공통 빌드

- 실행 환경: 사용자 Windows PowerShell 또는 Qt Creator
- 관리자 권한: 불필요
- 시작 경로: `C:/Users/shini/Documents/QtProjects/qt_4ch_viewer`
- 예상 시간: clean configure 포함 약 2~5분

```powershell
cmake -S . -B build-codex-mingw -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:/Qt/6.11.0/mingw_64 -DVCPKG_INSTALLED_DIR=C:/dev/vcpkg/installed -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic
cmake --build build-codex-mingw --parallel 8
```

실행은 사용자가 다음 명령 또는 Qt Creator Run으로 수행한다.

```powershell
.\build-codex-mingw\qt_4ch_viewer.exe
```

### 7.2 channel별 retry 검증

- 예상 시간: 약 10분

1. CH1과 CH3만 제공하고 CH2와 CH4는 연결할 수 없는 상태로 앱을 시작한다.
2. CH1·CH3가 계속 재생되는 동안 CH2·CH4만 1, 2, 4, 8초 순서로 재시도하는지
   확인한다.
3. 정상 채널에 `Stopped`, 영상 초기화 또는 새 RTSP session이 발생하지 않는지 Pi와
   Qt 로그로 확인한다.
4. CH2를 서버에서 활성화하고 현재 backoff 안에 CH2만 연결되는지 확인한다.
5. CH2가 30초 이상 안정적으로 재생된 뒤 다시 끊어 첫 retry가 약 1초부터 시작하는지
   확인한다.
6. retry 대기 중 `중지`를 누르고 더 이상 worker가 생성되지 않는지 확인한다.
7. retry 대기 중 playback 페이지로 이동해 live retry가 취소되고 Timeline 네 요청이
   병렬로 시작되는지 확인한다.
8. live로 돌아와 필요한 네 채널만 각각 한 worker로 연결되는지 확인한다.
9. 앱 종료 시 retry timer나 worker 때문에 종료가 지연되거나 crash하지 않는지 확인한다.

예상 로그 형식은 구현 시 고정한다. 최소한 다음 값은 포함해야 한다.

```text
[live-retry] channel=2 state=scheduled attempt=3 delay_ms=4000 reason=read_timeout
[live-retry] channel=2 state=starting attempt=3
[live-retry] channel=2 state=stable attempts_reset=true
```

### 7.3 footer 검증

- 예상 시간: 약 3분

1. footer 왼쪽의 사용자명과 Control 연결 상태가 콘셉트처럼 짧게 표시되는지 확인한다.
2. live 화면에서 영상 연결 수와 total Mbps를 계속 확인할 수 있는지 확인한다.
3. 정상 서버·저장소 상태가 중복돼 과도한 공간을 차지하지 않는지 확인한다.
4. 서버 또는 저장소가 warning/critical일 때 다른 페이지에서도 경고를 놓치지 않는지
   확인한다.
5. 오른쪽 시각이 PC clock이 아니라 서버 기준 KST이고 polling 사이에도 매초
   증가하는지 확인한다.

### 7.4 parking editor API 없이 가능한 검증

- 예상 시간: 약 10분

1. `설정 → 주차 구역 관리`가 별도 editor page를 여는지 확인한다.
2. 1280×720, 1920×1080, Windows 배율 100/125/150%에서 canvas와 inspector가
   겹치거나 잘리지 않는지 확인한다.
3. snapshot의 실제 rendered image 영역 안에서만 point가 생성되는지 확인한다.
4. 창 크기를 바꿔도 normalized polygon이 같은 영상 위치에 유지되는지 확인한다.
5. 네 점 진행 상태가 `1/4`부터 `4/4 · 구역 지정 완료`까지 정확한지 확인한다.
6. handle drag, undo/redo, 삭제와 선택 변경이 예상대로 동작하는지 확인한다.
7. 자기 교차, 너무 작은 면적/edge와 polygon overlap이 로컬에서 차단되는지 확인한다.
8. dirty 상태에서 채널, 메뉴, 뒤로가기, snapshot 새로고침, 앱 닫기를 시도해 경고
   popup이 뜨고 데이터가 임의로 사라지지 않는지 확인한다.
9. capability/API가 없으면 저장·적용 버튼이 비활성이고 `Pi 지원 필요`가 명확히
   표시되는지 확인한다.

### 7.5 Pi API 통합 후 검증

- 예상 시간: 약 15분

1. 채널 목록, geometry와 snapshot이 동일 coordinate space인지 확인한다.
2. active space를 불러오고 stable ID, label, type, enabled와 STM UID mapping이
   보존되는지 확인한다.
3. 초안 저장 뒤 editor를 나갔다가 돌아와 같은 draft를 복구하는지 확인한다.
4. Pi validate의 field/point 오류가 오른쪽 inspector와 canvas handle에 연결되는지
   확인한다.
5. apply 중 `Pi 저장`, `카메라 적용`, `read-back` 상태가 순서대로 표시되는지 확인한다.
6. read-back 완료 전 성공 badge가 나타나지 않는지 확인한다.
7. 다른 client가 active version을 변경한 뒤 apply해 HTTP 409가 발생하면 로컬 초안이
   보존되고 자동 overwrite하지 않는지 확인한다.
8. HTTP 422, 401/403, Pi offline, camera unsupported/read-back 실패를 각각 확인한다.
9. apply job 중 페이지를 나갔다가 돌아와 job ID로 상태를 복구하는지 확인한다.

### 7.6 기존 회귀 검증

- `.ref/docs/qt-m3-test-plan.md`
- `.ref/docs/qt-m6-playback-test-plan.md`
- `.ref/docs/qt-storage-status-test-plan.md`
- `.ref/docs/qt-next-task-prompt.md`의 사용자 실기 검증 추가 항목

특히 다음을 다시 확인한다.

- 실시간 일반 2×2에서 과도한 검은 영역이 생기지 않음
- 통계가 한 줄이며 작은 panel에서 상세 항목만 순서대로 생략됨
- Pretendard `ㅡ` 가로획과 font weight 표시
- Timeline 네 요청 동시 시작과 3배 rolling cache 유지
- fullscreen 진입 뒤 worker output frame이 큰 surface에 맞게 갱신됨
- 서버 시각 KST와 Timeline 눈금 일치

## 8. 실패 시 수집할 자료

### live retry

- `[chN]`, `[live-retry]`, `[ui-chN]` 전후 로그
- Pi의 해당 channel live client open/close 로그
- 실패 채널, 실패 시각, 재시도 간격
- 정상 채널까지 재연결됐다면 네 channel의 session open/close 순서

### parking API

- 비밀번호, bearer token, 전체 번호판을 제거한 HTTP method/path/status와 JSON
- `schema_version`, `geometry_id`, base/active version, draft/job ID
- Qt request ID와 Pi request ID
- 409/422의 machine-readable error code
- snapshot 원본 크기와 canvas rendered rectangle

### UI

- Windows 배율과 논리 창 크기
- 겹침 또는 좌표 오류가 보이는 screenshot
- footer의 정상/경고 상태 screenshot

## 9. 다음 작업 시작용 요약

```text
VEDA VMS Qt 후속 구현이다.

먼저 AGENTS.md, .ref/docs/README.md,
.ref/docs/antigravity-handoff-2026-08-04.md를 읽고 현재 source와 대조한다.
현재 폴더는 Git 저장소가 아니며 사용자가 직접 빌드·실기 테스트한다. 기존 파일을
되돌리거나 Timeline 3배 rolling cache를 제거하지 않는다.

첫 구현 단위는 live 4채널 중 실패한 channel만 1/2/4/8/15/30초+jitter로 자동
재연결하는 것이다. 정상 channel은 중지하거나 재생성하지 않고, 사용자 stop,
playback 진입, URL 재적용과 앱 종료에서는 retry를 취소한다. 종료 원인 판정,
retry policy, worker 재생성을 분리해 후속 30초 status probe로 교체 가능하게 한다.

그 다음 footer 재배치는 문서의 미결정 세부값을 사용자에게 확인한 뒤 구현한다.
주차 구역 편집기는 승인된 .ref/ui/parking-zone-concepts-v1을 따르되, Pi parking API는
아직 미구현이므로 로컬 canvas/validation과 capability-unavailable 상태를 먼저 만들고
실제 draft/apply/read-back 호출은 Pi M7.2 schema 확정 뒤 연동한다.

직접 빌드·실행하지 말고 정적 검토, 변경 파일, 사용자 테스트 절차와 예상 로그를
인계한다.
```

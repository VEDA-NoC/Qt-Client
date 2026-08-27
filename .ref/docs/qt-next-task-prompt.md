# Qt 다음 작업 시작 프롬프트

- 작성 기준일: 2026-08-25 (`main@4c0dfa9`, working tree clean)
- 이전 판(2026-08-03, 라이브 레이아웃·법적 고지·서버 시각 작업)은 모두 반영됐고 이 문서로 대체된다.

## 1. 현재 기준선

| 항목 | 값 |
|---|---|
| branch / commit | `main` / `4c0dfa9` (PR #2 merge) |
| 최근 merge | PR #1 `41a9a72` STM32 구역 제어기 연동, PR #2 `0fce84b` 구역-STM 매핑 UX·이벤트 상단 고정 |
| 빌드 환경 | Qt 6.11.0 MinGW 64-bit, CMake, vcpkg FFmpeg `x64-mingw-dynamic` |
| 연동 대상 | Pi `rpi-vms` `main@9b6c4e6` (P2.3 + 원격 명령 409 수정까지 merge) |

Pi 쪽 상태와 API 계약은 `rtsps-codex-hanwha-rtsp-raspberry-pi/project-docs/rpi-vms/current-status-ko.md`,
STM 펌웨어 상태는 `stm32-ev-firmware/STM_Sensor.md`를 본다.

## 2. 구현된 범위

| 영역 | 파일 | 내용 |
|---|---|---|
| 실시간 4채널 | `stream_worker.*`, `video_panel.*` | FFmpeg RTSPS 디코딩, 채널별 백오프 재연결, 2x2 / 4x1 / 1x4 자동 전환 |
| 화면 셸 | `main_window.*`, `theme.*` | 5개 페이지, 상·하단 바, 서버 시각 보간(status 5초 + monotonic elapsed) |
| 재생 | `playback_api_client.*`, `timeline_widget.*` | 로그인, status, timeline 윈도우·프리페치, playback session, seek, 속도, 전체 화면 |
| 주차 구역 | `parking_zone_editor.*`, `parking_zone_canvas.*` | 4점 polygon, draft → 검증 → 적용 잡 폴링, undo/redo, STM 장치 매핑 |
| 이벤트 | `parking_event_api_client.*`, `parking_event_store.*`, `parking_event_card_widget.*` | cursor long polling, 카드 패널 + 테이블, CRITICAL 상단 고정(기준선 방식), **EV 구역 주차 위반 카드**(2026-08-25) |
| STM 연동 | `stm_api_client.*`, `stm_device_list_widget.*`, `stm_fire_response_dialog.*`, `stm_types.h` | 장치 목록 5초 폴링, 등록 해제, 진압 명령 제출·상태 추적 |
| 고지 | `legal_notice_widget.*` | 개인정보 요약 상시 표시 + 운영 상세 disclosure |

## 3. 확인된 결함과 정리 대상

아래는 2026-08-25 소스 정적 검토에서 확인된 항목이다. **UI·UX 판단이 필요한 것은 계획 승인 전에
코드를 고치지 않는다.**

### 3-1. 이벤트 페이지 필터가 동작하지 않음 — **2026-08-25 수정 완료**

`refreshEventsTable()`이 "행이 비었으면 전체 재구성, 아니면 최신 1건 prepend"로 갈라져 있어,
행이 쌓인 뒤에는 필터를 바꿔도 목록이 그대로였고 long poll 한 배치에 N건이 와도 1건만 들어갔다.
매번 전체 재구성(행 상한 500, 스크롤 위치 보존)으로 바꾸고 필터 판정을
`eventPassesTableFilter()`로 분리했다. 주차 위반 표시 작업과 함께 반영됐다 —
[decisions/12 §5](decisions/12-parking-event-long-polling-ui.md) 참조.

### 3-2. 특정 PC 고정 경로·주소가 소스에 박혀 있음

- Control 인증서 기본 경로 `C:/Users/shini/Documents/Codex/2026-07-10/server.crt`
  (`createSettingsPage()`)
- 실시간 RTSPS 기본 주소 `rtsps://100.93.115.22:8554` (`main_window.h`의 `applied_base_url_`)

팀 배포 시 매번 설정 화면에서 고쳐야 한다. 설정 영속화(`QSettings`) 도입 여부와 기본값을
무엇으로 둘지는 UX 결정이 필요하므로 계획 단계에서 확정한다.

### 3-3. 문서 대비 코드 주석 불일치 (경미)

`control_task` 등 STM 쪽 이야기가 아니라 Qt 쪽만 보면, `.ref/docs/decisions/03`의 원안(TCP/TLS
프레이밍)은 채택되지 않았고 실제는 HTTPS REST + long polling이다. 해당 문서는 2026-08-25에
"실제 구현된 규약" 절을 추가해 정리했다.

## 4. 미검증 항목 (사용자 실기 수행 대상)

프로젝트 규칙상 빌드·실행·실기 테스트는 사용자가 수행한다. 현재 미검증으로 남은 것:

1. STM32 + Pi + Qt 3자 통합: Qt 진압 승인 → `POST /api/v1/stm-commands` → STM 액추에이터 동작 →
   STAGE 이벤트가 카드에 반영되는 전 구간. Pi 쪽 U6 검증 당시에는 액추에이터가 미장착이라
   명령이 `ACCEPTED`에 머물렀고, 이후 STM에 진압 액추에이터가 구현됐으므로 재확인이 필요하다.
2. 장치 페이지의 STM 목록: 등록된 장치와 "붙었지만 미등록"인 장치가 구분되어 보이는지, 등록
   해제 후 목록과 주차 구역 매핑이 함께 갱신되는지.
3. 구역-STM 매핑 편집 후 draft → 검증 → 적용 → readback 흐름.
4. 이벤트 상단 고정 기준선: 재로그인으로 백로그가 다시 내려와도 과거 CRITICAL이 고정되지 않는지.
5. **주차 위반 표시(2026-08-25 구현)**: EV 전용 구역에 내연기관 주차 → `주차 위반` 앰버 카드,
   전기차 주차 → 정보 카드 유지, `ev=unknown` 판독 대기 구간에서 위반이 먼저 뜨지 않는지,
   위반 차량 출차 시 `출차 완료`로 뜨는지(위반 카드가 다시 뜨면 안 됨), 이벤트 탭에서
   `WARNING (경고)` 필터로 위반만 걸러지는지. 상세는
   [decisions/12 §5](decisions/12-parking-event-long-polling-ui.md).
6. M6 재생 검증 항목 전반은 [qt-m6-playback-test-plan.md](qt-m6-playback-test-plan.md) 기준.

## 5. 다음 작업 후보

| 우선순위 | 작업 | 비고 |
|---|---|---|
| 높음 | ~~3-1 이벤트 필터 결함 수정~~ | 2026-08-25 주차 위반 작업과 함께 완료 |
| 높음 | 주차 위반 표시 실기 검증 | §4-5 항목. 카메라가 `violation=true`를 실제로 올리는 상황을 만들어야 한다 |
| 높음 | 3자 실기 통합 검증 지원 | 로그·재현 절차 정리 |
| 중간 | 3-2 설정 영속화 | 기본값·저장 위치는 UX 결정 필요 |
| 중간 | M7.5 운영 검증 항목 | 권한 분리, 번호판 마스킹, 보존기간 표시 |
| 낮음 | M5 GPU 렌더링 경로 판단 | 먼저 CPU 병목 측정이 선행돼야 한다 |

## 6. 새 작업 시작 프롬프트

```text
VEDA VMS Qt 프로젝트의 후속 작업이다.

먼저 다음 파일을 읽고 현재 source와 대조해라.
- AGENTS.md
- .ref/docs/README.md
- .ref/docs/qt-next-task-prompt.md
- .ref/docs/milestone.md
- 작업과 관련된 .ref/docs/decisions/ 문서

Pi 쪽 계약이 걸린 작업이면 rtsps-codex-hanwha-rtsp-raspberry-pi 저장소의
project-docs/rpi-vms/current-status-ko.md와 해당 API 문서도 확인해라.

현재 working tree의 기존 변경은 다른 작업 소유일 수 있으므로 임의로 되돌리거나
정리하지 마라. 사용자가 직접 빌드·실행·실기 테스트한다. 에이전트는 build command,
실행 파일, GUI, offscreen platform을 포함해 직접 빌드하거나 실행하지 않는다.

이번 작업 범위: [여기에 작업 지정]

UI·UX 동작이나 화면 배치가 걸린 항목이면 첫 답변에서 코드를 수정하지 말고 다음을 제시한다.
- source에서 확인된 정확한 원인과 근거 파일·함수
- 서로 연관된 영향
- 권장 동작과 변경할 파일
- 실제 선택이 필요한 대안
- 정적 검토와 사용자가 수행할 실기 테스트 계획

사용자가 계획을 승인한 뒤에만 코드와 필요한 문서를 수정한다. 승인 범위 안에서 새
UX 결정이 필요해지면 임의 선택하지 말고 다시 계획 단계로 돌아간다.
```

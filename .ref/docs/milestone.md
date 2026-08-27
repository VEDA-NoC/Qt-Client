# Qt VMS 개발 로드맵

마지막 갱신: 2026-08-25 (`main@4c0dfa9` 기준)

## 현재 완료 상태

| 마일스톤 | 상태 |
|---|---|
| M1. RTSPS 4채널 수신 테스트 | 완료 |
| M2. UI/UX 및 통신·정책 사양 확정 | 완료 (외부 연계 검증 제외) |
| M3. Qt UI 셸과 디자인 시스템 | 완료 |
| M4. Qt-Pi 세션·메타데이터·제어 프로토콜 | **대체됨** — 자체 TCP/TLS 프로토콜 대신 Pi의 HTTPS REST + cursor long polling 채택 |
| M5. 실시간 영상 제품화 | 대부분 완료 (채널 재연결, 레이아웃, 통계, 전체 화면). GPU 경로 판단은 미수행 |
| M6. 움직임 기반 녹화와 RTSPS VOD | 대부분 완료 (timeline 조회·프리페치, seek, 재생 세션). 실기 검증 잔여 |
| M7. 이벤트와 화재 대응 제어 | 구현 완료, 운영 검증 잔여 |
| M8. 저장장치·운영·상용화 준비 | 일부 (status/storage 표시, 법적 고지). 다중 HDD·배포 패키지 미착수 |

## 구현 순서

### M1. RTSPS 4채널 수신 테스트 (완료)
라즈베리파이에서 4채널에 대해 RTSP 데이터를 RTSPS로 전송할 때, Qt에서 ffmpeg으로 영상을 디코딩한다.
큐에 전부 저장한 경우 버퍼가 감당하지 못했고 따라서 3프레임 기준으로 여유를 두었다.

### M2. UI/UX 및 통신·정책 사양 확정 (완료)

- UI 콘셉트와 브랜드 톤 확정
- 녹화·VOD·이벤트·제어·권한·시간·장애 정책 검토
- CCTV 스트림 및 이벤트 API 사양 확인
- 결과물: `.ref/docs/decisions/`의 문서를 `확정` 상태로 전환

### M3. Qt UI 셸과 디자인 시스템 (완료)

- Qt Widgets 기반 navigation, 페이지 전환, 공통 컴포넌트 (`main_window.cpp`, `theme.cpp`)
- Pretendard(static TTF 번들), semantic color token, Light theme
- 실시간 화면은 기존 RTSPS 수신을 유지하고, 미구현 서버 데이터는 가짜 값 대신 명확한 빈 상태로 표시
- 페이지 구성: 실시간 / 재생 / 이벤트 / 장치 / 설정 5개

### M4. Qt-Pi 세션·메타데이터·제어 프로토콜 (대체됨)

원안은 장기 연결 TCP/TLS 위의 길이 프레이밍 JSON이었으나 **채택하지 않았다.** Pi가 이미
HTTPS control plane을 운영하고 있어 이벤트만 별도 소켓 프로토콜을 두면 인증·TLS·재접속
로직이 이중이 되기 때문이다. 실제 채택안은 다음과 같다.

- 인증: `POST /api/v1/login` → Bearer 토큰. TLS는 trust chain + hostname/IP SAN +
  leaf certificate SHA-256 pin을 모두 검증한다 (`playback_api_client.cpp`).
- 이벤트: `GET /api/v1/events` cursor long polling (`parking_event_api_client.cpp`).
  `next_after_id`로 유실 없이 이어받으므로 별도 sequence 복구 계층이 필요 없다.
- 명령 idempotency: STM 명령은 Pi가 `command_id` 재사용 규칙으로 처리하고, Qt는
  `slave_address` + `opcode`만 보낸다 (`stm_api_client.cpp`).
- mock server는 만들지 않았다. Pi 실기를 대상으로 검증한다.

근거와 실제 규약표는 [decisions/03](decisions/03-control-and-device-model.md)에 있다.

### M5. 실시간 영상 제품화 (대부분 완료)

- 4채널 RTSPS 수신, 채널별 지수 백오프 재연결 (`startChannelStream`, `scheduleChannelRetry`)
- 일반형 2x2 / 세로형 4x1 / 초광폭 1x4 레이아웃 자동 전환 (`updateLiveLayout`)
- 채널 상태 배지, 프레임 통계 한 줄 표시, 재생 전체 화면
- **잔여**: 1080p 서브스트림 적용은 Pi의 N2 작업에 의존한다. CPU 렌더링 병목 측정과
  GPU 경로 필요 여부 판단은 아직 수행하지 않았다.

### M6. 움직임 기반 녹화와 RTSPS VOD (대부분 완료)

- timeline 윈도우 조회와 방향성 프리페치 (`requestTimelineWindow`, `scheduleTimelinePrefetch`)
- 4채널 병렬 조회, 서버 시각(`server_time.utc_ms` + monotonic elapsed) 기준 KST 표시
- playback session 생성 → 재생 / 일시정지 / seek / 속도 / 전체 화면 / 정지
- **잔여**: 실기 검증은 [qt-m6-playback-test-plan.md](qt-m6-playback-test-plan.md) 기준으로 사용자가 수행한다.

### M7. 이벤트와 화재 대응 제어 (구현 완료, 운영 검증 잔여)

- 이벤트 수신·저장·표시: `parking_event_api_client.*`, `parking_event_store.*`,
  `parking_event_card_widget.*`. 실시간 탭 우측 카드 패널 + 이벤트 탭 리스트.
  CRITICAL 이벤트는 상단 고정하되, 로그인 시 백로그가 다시 내려와도 고정되지 않도록
  폴링 시작 후 첫 배치의 최대 `event_id`를 기준선으로 잡는다.
- STM32 장치 계층: `stm_api_client.*`(REST 4종), `stm_device_list_widget.*`(5초 폴링,
  등록 해제), `stm_types.h`(프로토콜 값 사본).
- 방재 승인: `stm_fire_response_dialog.*` — `stm.fire_started` 카드에서 열리는
  [화재 진압][닫기][무시] 대응창. 진압 명령 제출 후 상태를 추적한다.
- 구역-STM 매핑: `parking_zone_editor.*`의 장치 콤보.
- **잔여**: 권한 분리(이벤트 확인 / 원문 열람 / 방재 승인), 번호판 마스킹과 audit,
  보존기간 자동파기 검증. 그리고 STM32+Pi+Qt 3자 실기 통합 검증.

### M8. 저장장치·운영·상용화 준비 (일부)

- 완료: `/api/v1/status` 5초 폴링 기반 서버·저장소 상태 표시, 장치 상세(CPU·온도·메모리·
  가동 시간·throttling), 정보 및 법적 고지 화면(`legal_notice_widget.*`)
- 잔여: 다중 HDD 등록·보존 정책 UI, 시간 동기화 상태 표시 정교화, 배포 패키지

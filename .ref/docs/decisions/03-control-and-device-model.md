# 03. 제어 규약과 장치 모델

- 상태: **구현 확정 (2026-08-25 갱신)** — 아래 "실제 구현된 규약"이 현재 계약이다.
  본문의 장기 연결 TCP/TLS 프레이밍 안은 **채택되지 않았다**.

## 실제 구현된 규약 (2026-08-25)

| 구간 | 규약 | 비고 |
|---|---|---|
| CCTV → Raspberry Pi | RTSP + SUNAPI(OpenSDK) 이벤트 | Digest `monitordiff` wake-up + `/parking_status` XML snapshot diff |
| Raspberry Pi → Qt | RTSPS | 실시간·VOD 영상. 변경 없음 |
| Raspberry Pi ↔ Qt | **HTTPS REST + Bearer 토큰**, 이벤트는 `GET /api/v1/events` **cursor long polling** | 장기 연결 TCP/TLS 프레이밍은 쓰지 않는다 |
| Raspberry Pi ↔ STM32 | **Modbus RTU over RS-485** (19200 8E1), Pi가 유일한 master | 공용 header `stm_protocol.h` v0.2가 계약 |

- **Qt↔Pi 전송 방식이 바뀐 이유**: Pi가 이미 HTTPS control plane(로그인·status·timeline·
  playback session·parking API)을 운영하고 있어, 이벤트만 별도 소켓 프로토콜을 두면 인증·TLS·
  재접속 로직이 이중이 된다. cursor long polling은 같은 Bearer 경로를 그대로 타면서
  `next_after_id`로 유실 없이 이어받는다. 아래 본문의 JSON envelope·request_id 설계는
  채택되지 않았으므로 참고 기록으로만 본다.
- **Pi↔STM32**: 자체 바이너리 프레임 대신 STM 쪽 기존 `alejoseb/Modbus-STM32-HAL-FreeRTOS`
  라이브러리와 호환되는 Modbus RTU를 채택했다. FC4(입력 레지스터 읽기)와 FC16(command mailbox
  쓰기) 두 경로만 쓰고 custom function code는 추가하지 않는다.
- **화재 대응 단계**: 1단계 방재포 전개, 2단계 살수 펌프. 원격 `START_STAGE1`은 단독 전개가
  아니라 **순차 진압(방재포 → 살수)**으로 처리된다. 로컬 버튼 경로는 화재 경고(DANGER)
  상태에서만 받는 인터록이 걸리고, 원격 경로는 관리자가 Qt에서 승인하는 절차가 인터록을 대신한다.
  한 번 동작한 stage는 `REARM` 전까지 재기동되지 않는다.
- **Qt 구현 위치**: `stm_api_client.*`(REST 4종), `stm_device_list_widget.*`(장치 목록·등록 해제),
  `stm_fire_response_dialog.*`(진압 승인창), `stm_types.h`(프로토콜 값 사본),
  `parking_zone_editor.*`(구역-STM 매핑).
- 상세는 Pi 저장소의 `project-docs/rpi-vms/p2.3-stm-qt-integration-plan-ko.md`와
  `stm-rs485-protocol-design-ko.md`를 본다.

---

## 아래는 2026-07-22 설계 검토 기록 (일부 미채택)

- 상태: 부분 확정 (STM32 실장 피드백 규약 대기)

## 전체 통신 기준안

| 구간 | 규약 | 용도 |
|---|---|---|
| CCTV -> Raspberry Pi | RTSP 및 카메라 이벤트 API | 메인/서브 영상, 움직임·객체 이벤트 |
| Raspberry Pi -> Qt | RTSPS | 실시간 영상과 VOD 영상 |
| Raspberry Pi <-> Qt | 장기 연결 TCP/TLS, 길이 프레이밍 JSON | 로그인 세션, 조회, 이벤트 push, 장치 상태, 명령 |
| Raspberry Pi <-> STM32 | RS-485/UART 바이너리 프레임 | 센서 상태, actuator 제어, ACK |

메타데이터를 RTSP 영상 스트림에 섞지 않는다. 영상과 이벤트의 연결은 UTC timestamp, `camera_id`, `event_id`, `recording_session_id`로 수행한다.

## Qt-Pi 메시지 프레임

TCP/TLS 위에서 `4바이트 big-endian payload length + UTF-8 JSON payload`를 사용한다. 이벤트 규모가 작아 JSON 오버헤드는 무시할 수 있고 개발 중 패킷 확인이 쉽다.

공통 JSON envelope 제안:

```json
{
  "protocol_version": 1,
  "message_type": "command.request",
  "request_id": "UUID",
  "sent_at": "2026-07-22T01:23:45.678Z",
  "payload": {}
}
```

- 최대 payload 크기를 제한한다.
- `protocol_version`과 capability negotiation을 둔다.
- TLS가 무결성을 제공하므로 Qt-Pi 프레임에 별도 CRC는 넣지 않는다.
- 연결 재개 시 마지막 `event_sequence` 이후 이벤트를 SQLite에서 다시 받는다.

## 장치 계층

```text
Site
└─ Charging Zone
   ├─ CCTV
   ├─ STM32 Zone Controller
   │  ├─ Fire Barrier Actuator
   │  ├─ Sprinkler Actuator
   │  ├─ Infrared Sensor
   │  ├─ Ultrasonic Sensor
   │  ├─ Temperature/Fire Sensor
   │  └─ Charging Station (향후)
```

UI의 장치 목록에는 CCTV, STM32 구역 제어기, 저장 볼륨처럼 독립적으로 관리할 대상만 표시한다. 충전 스테이션과 센서·actuator는 해당 STM32 상세 화면의 하위 요소로 표시한다.

## 서버 설정 소유권

- Raspberry Pi가 사이트·구역·채널·녹화 정책 설정의 원본을 파일로 영속화한다.
- Qt는 Pi 파일시스템을 직접 수정하지 않고 TLS 채널의 `config.get`·`config.update` 요청으로 읽고 쓴다.
- 응답에는 `config_revision`을 포함하고 갱신 요청은 `expected_revision`을 보내 충돌을 검출한다.
- 서버는 schema 검증 후 임시 파일 작성과 atomic rename으로 반영하며 직전 정상본을 백업한다.
- 사전/사후 녹화 기본값은 각각 5초/10초로 확정하고, 운영 화면의 상시 조정 항목이 아니라 관리자 설정으로 둔다.

### 실시간 스트림 주소 소유권

- 카메라 원본 RTSP URL과 계정정보는 Raspberry Pi에서만 관리하며 Qt 설정·로그·스트림 목록 응답에 노출하지 않는다.
- Pi 스트림 목록 API가 구현되면 Qt는 Pi가 공개한 RTSPS endpoint와 표시 메타데이터를 조회하여 사용한다.
- Qt M3의 수동 `Pi RTSPS Base URL` 입력은 연결 시험을 위한 임시 기능이다. 현재 실행에만 적용하며 영속 설정의 원본으로 취급하지 않는다.
- 임시 Base URL은 `rtsps://`만 허용하고 URL userinfo, query, fragment를 거부한다. 적용 시 `/ch1`~`/ch4` endpoint를 다시 만들고 네 스트림을 재연결한다.

## 화재 대응 단계

숫자만 전달하는 `STM 번호 + 단계`보다 의미가 명확한 enum과 안정적인 장치 ID를 함께 사용한다.

| 단계 | 명령 | 동작 | 자동 재시도 |
|---:|---|---|---|
| 0 | `FIRE_RESPONSE_RESET` | 점검 후 정상 상태 복귀 요청 | 금지 |
| 1 | `FIRE_BARRIER_DEPLOY` | 해당 충전 구역 방재판 전개 | 금지 |
| 2 | `SPRINKLER_ACTIVATE` | 화재 확정 후 스프링클러 작동 | 금지 |
| - | `ALARM_TEST` | 경고음·표시 시험 | 정책에 따라 가능 |

단계 2는 단계 1의 완료 ACK, 대상 장치 정상, 안전 interlock, 권한, 명시적 확인을 요구한다. 실제 장치가 단계 1 실패 상태일 때 단계 2를 허용할지는 하드웨어 안전 설계와 함께 확정한다.

## 명령 데이터와 상태

필수 필드:

- `command_id`: UUID, 중복 실행 방지 키
- `target_controller_id`, `zone_id`
- `command_type`, `stage`
- `requested_by`, `requested_at`, `reason`
- `expected_state_version`: 오래된 화면에서 명령하는 것을 방지

상태 전이:

`ACCEPTED -> DISPATCHED -> DEVICE_ACKED -> EXECUTING -> SUCCEEDED`

실패 상태는 `REJECTED`, `FAILED`, `TIMED_OUT`, `CANCELLED`로 구분한다. 위험 명령은 timeout 후 동일 ID로 상태를 조회하며 새 ID로 자동 재시도하지 않는다.

## Pi-STM32 프레임 요구사항

- magic, protocol version, controller address, sequence, command, payload length, payload, CRC
- STM32는 같은 sequence/command를 다시 받으면 재실행하지 않고 이전 결과를 반환
- 센서 보고에는 sensor ID, 상태, 원시값, 단위, STM monotonic tick, sequence 포함
- Raspberry Pi가 수신 UTC를 권위 timestamp로 부여
- actuator 명령은 접수 ACK와 실제 완료 ACK를 분리

## 미결정

- 방재판 위치 센서와 스프링클러 동작 완료를 어떤 피드백으로 확인할지 / STM에서 보고
- 현장 비상 정지·수동 조작과 원격 명령의 우선순위 / 논의 필요하나, Qt에서 결정할 사안이 아닌 것 같음.
- 단계 2 실행 권한을 운영자에게 부여할지 별도 승인자를 요구할지 / 운영자면 될 것 같음.
- RS-485 주소 할당과 장치 교체 시 identity 이전 절차 / 기능 구현 이후 고려

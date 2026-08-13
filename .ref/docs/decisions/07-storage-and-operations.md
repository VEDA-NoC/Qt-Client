# 07. 저장장치·장치 관리·운영 상태

- 상태: 확정 (실장 HDD 검증 대기)

## 저장장치 모델

HDD는 파일 경로가 아니라 안정적인 volume UUID로 식별하고 상태를 관리한다.

| 상태 | 의미 | UI 동작 |
|---|---|---|
| `UNINITIALIZED` | VMS 저장소로 준비되지 않음 | 관리자에게 초기화 제공 |
| `FORMATTING` | 포맷 또는 준비 중 | 진행률 표시, 다른 작업 금지 |
| `READY` | 정상 기록 가능 | 사용률·예상 보존일 표시 |
| `DEGRADED` | SMART/입출력 경고 | 교체 권고, 이벤트 발생 |
| `READ_ONLY` | 읽기만 가능 | 녹화 대상에서 제외 |
| `FULL` | 여유 공간 부족 | 보존 정책 실행 또는 녹화 실패 경고 |
| `MISSING` | 등록 볼륨 분리됨 | 영향 채널과 마지막 확인 시각 표시 |

첫 버전에서는 RAID를 애플리케이션이 직접 구현하지 않는다. HDD 추가 시 `recording pool`에 등록하고 새 녹화의 배치 정책을 서버가 결정한다. 기존 파일의 자동 재분배는 초기 범위에서 제외한다.

## 저장소 관리 기능

- HDD 검색, 등록, 초기화, 안전 제거
- 파일시스템·용량·사용량·쓰기 가능 여부 표시
- 채널 또는 저장 pool 배정
- 보존 기간/최소 여유 공간 설정
- 보호된 사건 영상은 자동 삭제에서 제외
- SMART 상태와 입출력 오류 이력 표시
- 포맷·제거는 관리자 재확인과 감사 로그 필수

## STM32 관리

- 충전 구역당 STM32 구역 제어기 1대를 기본 모델로 한다.
- 등록, RS-485 주소 배정, 구역 연결, 펌웨어 버전, heartbeat, 마지막 응답을 관리한다.
- 센서와 actuator는 STM32 상세 화면의 하위 목록에서 상태와 원시값을 표시한다.
- 향후 충전 스테이션은 같은 구역 STM32의 하위 장치로 추가한다.

Qt는 Pi 저장소 상태 API가 연결되기 전까지 저장소 상태를 `확인 대기`로 표시한다. `MISSING`, 실제 용량, SMART, 보존일과 녹화 가능 여부는 API 응답으로 확인된 경우에만 표시한다.

## N4.1 저장소 상태 API

위의 대문자 상태는 장치 등록·수명주기 모델이고, 아래의 소문자
`storage.state`는 현재 서버가 제공하는 런타임 용량·쓰기 상태이다.

- 기존 M6 HTTPS Control Base URL의 `GET /api/v1/status`를 사용한다.
- storage 전용 포트나 TLS 설정을 만들지 않는다. login, timeline, thumbnail,
  export와 동일한 `QNetworkAccessManager`, 인증서 검증, Bearer token을 사용한다.
- Qt polling 기본 주기는 서버 측정 기본 주기와 같은 5초이다.
- 사용자에게 실제 사용 가능한 용량은 `available_bytes`로 표시한다.
  `free_bytes`는 ext4 reserved block을 포함할 수 있으므로 사용자 가용량으로
  대체하지 않는다.
- `HTTP 401`은 token 만료로 처리하고 자동 재로그인 후 status를 한 번 재요청한다.
- TLS·network 실패는 저장장치 상태가 아니라 `Control 연결 오류`로 표시한다.

| API `storage.state` | Qt 표시 | 녹화 해석 |
|---|---|---|
| `normal` | 정상, 전체·사용·사용 가능 용량 | 정상 녹화 |
| `warning` | 노란색 저장공간 경고 | 현재 녹화 계속, 즉시 삭제로 해석하지 않음 |
| `critical` | 빨간색 저장공간 위험 | retention 또는 공간 확보 실패 가능 |
| `read_only` | 저장장치 쓰기 불가 | live·Control offline과 구분 |

`recording_suspended=true`는 state 색상과 별도로
`용량 부족으로 신규 녹화 중단`을 최우선 표시한다. 이 상태에서도 live 화면이나
Pi 전체를 offline으로 표시하지 않는다.

## 통합 장치 상태 계약

저장소 전용 요청을 추가하지 않고 기존 HTTPS Control API의
`GET /api/v1/status` 한 응답에서 다음 묶음을 함께 받는다.

- `server_time`: UTC 서버 시각, NTP 동기화 상태, `boot_id`
- `system`: CPU 사용률, load average, 온도, Pi uptime, 메모리, throttling, 상태
- `storage`: 전체·사용·여유·사용 가능 용량, 상태, 녹화 중단 여부

Qt는 5초마다 status를 요청한다. 모르는 JSON field는 향후 확장으로 보고 무시한다.
`available=false` 또는 값이 `null`인 metric은 해당 측정값 미지원이며 Control 연결
실패로 해석하지 않는다. `sample_age_ms > 10000`이면 마지막 값과 함께 갱신 지연을
표시한다. `boot_id` 변경은 Pi 재부팅으로 처리한다.

`server_time.utc_ms`는 수신 시각에 monotonic 경과 시간을 묶어 저장하고 기존 1초 UI
timer에서 `기준 UTC + 경과시간`으로 표시한다. 다음 status 응답에서 기준을 다시
보정하며, 이를 위해 polling을 1초로 줄이지 않는다. NTP 동기화 여부와 화면 시계의
초 단위 진행은 별도 상태로 다룬다.

## 상태 문구와 지표 의미

- 하단 시스템 배지는 `서버 정상`, `서버 경고`, `서버 위험`, `서버 상태 지연`을
  사용한다. CPU·온도·메모리 수치는 장치 상세 화면에서 표시한다.
- 저장소 배지는 첫 값이 `available_bytes`, 두 번째 값이 `total_bytes`임을 드러내는
  `저장소 여유 388/491 GB` 형식을 우선한다. 단순 `388/491 GB` 표기는 사용량과
  혼동되므로 사용하지 않는다.
- load average는 CPU 사용률이나 이전 1분 대비 사용량이 아니며 코어 수와 I/O 상태
  없이 일반 사용자가 해석하기 어렵다. Qt UI에서는 제외하고 API parsing만 진단용으로
  유지한다. CPU 사용률로 환산하거나 포화 기준을 Qt에 하드코딩하지 않는다.
- 현재 uptime은 Pi OS의 가동 시간으로 취급해 `Pi 가동 시간`으로 표시한다.
  Pi uptime과 VMS service uptime은 서로 대체하지 않는다.

VMS 재시작을 Pi 재부팅과 별도로 식별하려면 Pi API가 향후 다음 값을 제공해야 한다.

- `service.started_at_utc_ms` 또는 `service.uptime_seconds`
- 프로세스 시작마다 바뀌는 `service_instance_id`

API가 제공되기 전에는 Qt가 VMS service uptime을 추정해 표시하지 않는다. load의
코어 수 기준 설명이나 정규화가 필요하면 `logical_cpu_count`도 Pi API 확장 대상으로
검토한다.

## 운영 상태 수집 주기

| 상태 | 수집 주기 제안 | 메인 화면 표시 |
|---|---:|---|
| 서버 CPU 사용률·온도·메모리 | 5초 | 정상/경고 요약만 |
| 디스크 사용량·쓰기 오류 | 5~10초 | 사용률과 경고 표시 |
| SMART 상세 | 10분 | 이상 시만 경고 |
| 네트워크 처리량 | 1초 통계, 5초 표시 | 합계 Mbps |
| STM32 heartbeat | 5초 | 장치 상태 요약 |

`/proc`, `sysfs`, `statvfs` 수준의 상태 수집은 영상 디코딩보다 매우 가벼우므로 Pi 부담을 이유로 제외할 필요는 없다. 다만 SMART 전체 검사는 짧은 주기로 반복하지 않는다.

## UI 노출 원칙

- 메인 화면: 채널 정상 수, 서버 연결, 녹화 상태, 저장공간, 심각 경보
- 시스템 상태 화면: Pi CPU·온도·메모리·네트워크·디스크 상세
- 진단 화면: Qt 클라이언트 CPU, 메모리, 디코드/render FPS, 지연, 드롭 프레임
- Windows GPU 사용률은 운영 필수 지표가 아니므로 메인 화면에서 제외하고 성능 검증 도구 또는 진단 화면에서만 검토

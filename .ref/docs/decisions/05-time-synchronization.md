# 05. 시간 동기화

- 상태: 부분 확정 (현장 NTP·보드 리비전 확인 대기)

## 권장 결론

Raspberry Pi는 VMS 시스템의 논리적 시간 기준이지만, 스스로 정확한 시간을 얻기 위해 외부 NTP 또는 현장 상위 NTP와 동기화해야 한다. 격리망에서는 Raspberry Pi가 CCTV와 다른 장치에 NTP를 제공한다.

모든 저장·전송 timestamp는 UTC를 사용하고 Qt에서만 KST로 표시한다.

## 장치별 정책

| 장치 | 권장 방식 | 주기/판정 제안 |
|---|---|---|
| Raspberry Pi | `chrony` 등으로 상위 NTP에 지속 동기화 | daemon 자동 조절, 동기 상태 상시 감시 |
| CCTV | Pi 또는 동일 상위 NTP 사용 | 카메라 지원 설정 확인, 허용 오차 1초 이내 목표 |
| STM32 | Pi가 부팅·연결 시 기준 UTC 전달, 이후 monotonic tick 사용 | 60초 heartbeat, 10분마다 offset 점검 제안 |
| Qt/Windows | Windows 시계를 강제 변경하지 않고 서버 UTC로 데이터 표시 | 연결 시와 5분마다 offset/RTT 측정 제안 |

STM32 이벤트에는 RTC 시각만 의존하지 않고 `sequence + monotonic tick`을 포함한다. Raspberry Pi는 수신 즉시 UTC를 부여하며 전송 지연이 필요한 경우 두 시각을 함께 저장한다.

## 이상 감지

- Pi가 NTP 비동기 상태면 `time.unsynced` 이벤트를 생성한다.
- CCTV와 Pi의 차이가 1초를 초과하면 경고, 5초를 초과하면 녹화·이벤트 연결 신뢰도 저하로 표시하는 안을 검토한다.
- 시간 보정으로 시각이 뒤로 가더라도 event sequence와 monotonic clock으로 순서를 보존한다.
- Qt의 지연시간 측정은 wall clock 차이가 아니라 로컬 monotonic clock과 서버 왕복시간을 사용한다.

## 확인 필요

- CCTV의 NTP 서버 설정, 갱신 방식, RTSP timestamp 기준 / SUNAPI로 연결 장치 기준으로 설정 가능함
- STM32의 RTC 유무와 oscillator 오차 / RTC 있음. 상세 스펙은 Agent가 확인해야 함. F401RE 모델임.
- 현장 인터넷 연결 시 Pi가 허용된 상위 NTP와 동기화하고 CCTV는 Pi를 NTP 서버로 사용한다. CCTV의 직접 인터넷 접근은 필수 조건으로 두지 않는다.
- STM32F401RE에는 RTC가 있으나 NUCLEO-F401RE의 32.768 kHz LSE 실장 여부는 보드 리비전에 따라 다를 수 있으므로 실제 보드 리비전을 확인한다.

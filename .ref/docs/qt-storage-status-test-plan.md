# Qt 통합 장치 상태 API 테스트 계획

- 대상: Windows Qt VMS Console + Raspberry Pi 통합 `/api/v1/status`
- Qt polling: 5초
- 주의: Qt 빌드·실행과 Pi 상태 조작은 사용자가 수행한다.

## 사전 조건

1. Pi를 기존 M6 HTTPS Control server가 활성화된 포트로 실행한다.
2. Qt 설정의 Control HTTPS Base URL, 사용자, 비밀번호, `server.crt`를 적용한다.
3. live·playback RTSPS 포트와 device status용 포트를 별도로 만들지 않는다.

Control 설정을 아직 적용하지 않은 실행에서는 하단 배지가
`서버 · Control 설정 필요`, `저장소 · Control 설정 필요`로 표시되어야 한다.
live RTSPS 연결 성공만으로는 Bearer token이 없으므로 status를 조회하지 않는다.

## 정상 상태

1. Control 로그인이 완료된 뒤 최대 5초 기다린다.
2. 하단 배지에 `서버 정상`, `저장소 여유 .../... GB`가 표시되는지 확인한다.
3. 장치 및 제어 화면에서 Pi CPU·온도·메모리·uptime과
   `서버 시각 · NTP 동기화`가 표시되는지 확인한다.
4. 저장소 카드에서 `정상 · ...%`와
   `전체 ... · 사용 ... · 사용 가능 ...`이 표시되는지 확인한다.
5. Application Output에서 다음 형식을 확인한다.

```text
[device-status] system=normal storage=normal sync=synchronized stale_ms=...
```

화면의 사용 가능 용량은 `available_bytes`를 10진 GB로 변환한 값이어야 한다.

## 시스템 상태별 표시

| Pi 응답 | 예상 Qt 표시 |
|---|---|
| `system.state=normal` | 서버 정상 |
| `system.state=warning` | 서버 경고 |
| `system.state=critical` | 서버 위험 |
| `system.state=unknown` | 서버 상태 미확인 |
| `sample_age_ms > 10000` | 서버 상태 지연, Control 연결은 유지 |
| metric `null` 또는 `available=false` | 해당 항목만 측정 미지원 |
| `sync_state=unsynchronized` | NTP 미동기화 경고 |
| `sync_state=unknown` | NTP 상태 미확인 |
| 두 번째 이후 응답에서 `boot_id` 변경 | Pi 재부팅 감지 알림 |

현재 throttling flag와 누적 발생 flag는 각각 `현재`, `과거 이력`으로 구분한다.

## 서버 시각 보간

1. 정상 status 응답 직후 서버 시각과 PC 시각을 기록한다.
2. 다음 status polling 전까지 5초 동안 서버 시각 label이 매초 증가하는지 확인한다.
3. 네트워크 요청 로그는 기존 5초 주기를 유지하는지 확인한다.
4. 다음 status 응답에서 새 `server_time.utc_ms`를 기준으로 자연스럽게 보정되는지
   확인한다.
5. NTP 미동기화 상태에서도 시계는 진행하되 `NTP 미동기화` 경고가 별도로 유지되는지
   확인한다.

장치 상세 화면에서는 load average를 표시하지 않고 `Uptime` 대신 `Pi 가동 시간`을
표시한다. API의 load average parsing은 진단용으로 유지하되 CPU 사용률로 환산하지
않는다. API가 service uptime을 제공하지 않는 동안 VMS 서비스 가동 시간을 임의로
표시하지 않는다.

## 저장소 상태별 표시

| Pi 응답 | 예상 Qt 표시 |
|---|---|
| `warning`, suspended false | 노란색 경고, `현재 녹화는 계속됩니다` |
| `critical`, suspended false | 빨간색 위험, retention/공간 확보 실패 가능성 |
| `read_only` | 저장장치 쓰기 불가, live 연결 상태는 유지 |
| suspended true | `용량 부족으로 신규 녹화 중단` 최우선 표시 |

## 인증·연결 실패

1. token을 만료시킨다.
   - 예상: Pi·저장소 `인증 갱신 중` 후 자동 재로그인하고 status를 한 번 재요청한다.
   - playback 화면과 기존 frame을 status 401만으로 제거하면 안 된다.
2. 정상 로그인 후 Control server만 중지한다.
   - 예상: Pi·저장소 `Control 연결 오류` 또는 마지막 정상값이 있으면 `갱신 지연`.
   - HDD `critical`, `read_only`, offline으로 오인 표시하면 안 된다.
3. 다른 인증서나 SAN이 맞지 않는 주소를 적용한다.
   - 예상: 기존 TLS 실패 안내와 함께 보호된 playback 정보가 제거된다.
   - `ignoreSslErrors()` 우회가 없어야 한다.

## 실패 시 수집 자료

- Qt Creator Application Output의 `[device-status]`, `[playback-api]` 전후 로그
- 하단 배지와 장치 및 제어 화면 screenshot
- Pi `/api/v1/status` 응답에서 비밀정보를 제외한 JSON
- Control URL의 host와 port
- 비밀번호, Bearer token, capability는 마스킹

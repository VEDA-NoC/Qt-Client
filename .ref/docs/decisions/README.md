# 의사결정 문서

이 폴더는 구현 사양과 후속 검증 항목을 모은 공간이다. `확정`은 설계 선택이 끝났다는 의미이며 실제 장치·성능 검증 완료를 의미하지 않는다.

상태 표는 2026-08-25(`main@4c0dfa9`) 기준으로 갱신했다.

| 번호 | 문서 | 주요 내용 | 상태 |
|---:|---|---|---|
| 01 | [영상·녹화·재생](01-media-recording-playback.md) | 메인/서브스트림, 움직임 녹화, RTSPS VOD, 용량 | 확정/검증 대기 |
| 02 | [이벤트 정책](02-event-policy.md) | 이벤트 종류, 심각도, 해제와 확인 처리 | 부분 확정 |
| 03 | [제어 및 장치 모델](03-control-and-device-model.md) | Pi-Qt, Pi-STM32 규약과 화재 대응 단계 | 구현 확정 (원안 TCP/TLS 프레이밍은 미채택, HTTPS REST + long polling으로 대체) |
| 04 | [역할과 권한](04-roles-and-permissions.md) | 관리자·운영자·조회 전용 권한 | 확정 |
| 05 | [시간 동기화](05-time-synchronization.md) | UTC 기준, NTP, STM32 시각 처리 | 부분 확정 |
| 06 | [네트워크 장애 대응](06-network-resilience.md) | 재접속, 이벤트 복구, 명령 중복 방지 | 확정 |
| 07 | [저장장치와 운영 상태](07-storage-and-operations.md) | HDD·STM32 계층, 상태 수집, UI 노출 | 확정/검증 대기 |
| 08 | [UI·브랜드·상용화](08-ui-brand-compliance.md) | 테마, 글꼴, 라이선스, 개인정보·보안 | 확정/시각 QA 대기 |
| 09 | [미결정 사항](09-open-questions.md) | 외부·하드웨어 검증과 후속 선택 | 추적 중 |
| 10 | [주차 구역·번호판·전기차](10-parking-zone-lpr-ev.md) | polygon, STM 연결, LPR, 차량 분류 | 부분 확정 |
| 11 | [Control TLS·녹화 재생 UI](11-control-tls-and-playback-ui.md) | 인증서 배포, 오류 분류, 4채널 timeline과 player UX | 검토 대기 |
| 12 | [주차·EV 이벤트 Long Polling·UI](12-parking-event-long-polling-ui.md) | Cursor Long Polling 수신, 1.png 기반 카드 및 리스트 UI | 확정 · 구현 완료 (STM 이벤트 카드 포함) |

## 검토 방법

합의된 항목은 `확정`, 실제 장치나 외부 승인이 필요한 항목은 `검증 대기` 또는 `부분 확정`으로 관리한다.

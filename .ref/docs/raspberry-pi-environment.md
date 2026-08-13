# 프로젝트 전용 Raspberry Pi 환경

## 문서 기준

| 항목 | 값 | 상태 |
|---|---|---|
| 프로젝트명 | VEDA VMS | 확인됨 |
| 문서 위치 | `.ref/docs/raspberry-pi-environment.md` | 확인됨 |
| 확인일 | 2026-07-22 | 확인됨 |
| 확인 방법·출처 | 사용자 제공 정보와 Pi workspace `repos/rpi-vms` main `e163e824` 문서·소스 | 확인됨 |

## 장치 및 하드웨어

| 항목 | 값 | 상태 |
|---|---|---|
| 역할 | VMS 중앙 서버, 이벤트 핸들러, 녹화 저장, 제어 중계 | 확인됨 |
| 제조사·모델 | Raspberry Pi 4 Model B | 확인됨 |
| 보드 리비전 | Rev 1.5 | 확인됨 |
| 메모리 | 8 GB | 확인됨 |
| 저장장치 | 16 GB ext4 시험 USB 검증됨, 500 GB HDD 배송·교환 대기 | 확인됨 |
| 연결 주변기기 | CCTV 4채널, STM32 계열 제어기 | 일부 확인됨 |

## OS 및 런타임

| 항목 | 값 | 상태 |
|---|---|---|
| 배포판·릴리스 | Raspberry Pi OS, Debian GNU/Linux 13.5 (trixie) | 확인됨 |
| 커널 버전 | 6.18.37-v8+ | 확인됨 |
| 아키텍처 | aarch64 | 확인됨 |
| GStreamer 버전 | 1.26.2 | 확인됨 |
| OpenSSL 버전 | 3.5.6 | 확인됨 |
| SQLite 버전 | 3.46.1 | 확인됨 |
| init·서비스 관리자 | 미확인 | - |

## 현재 확인된 처리 방식

| 항목 | 내용 | 상태 |
|---|---|---|
| 실시간 중계 | 활성 `rpi-vms`에는 아직 없음. M2 ChannelIngest 후 M3 live RTSP/RTSPS server 예정 | 확인됨 |
| 녹화 | 단일 채널 연속 MP4 segment 저장 PoC | 실제 검증됨 |
| 메타데이터 | 녹화 segment index를 SQLite WAL DB에 저장. `vms_events` schema는 있으나 event subscriber/gate는 아직 없음 | 실제 검증됨 |
| 영상 처리 제약 | Raspberry Pi에서 디코딩·재인코딩하지 않음 | 확인됨 |
| 채널 규칙 | 카메라 내부 `camera_channel=0..3`, VMS/DB/Qt/storage/public path `channel_id=1..4` | 확정 |
| 녹화 정책 | 움직임 이벤트 기반, 사전 5초·사후 10초, 파일당 최대 약 1분 목표 | 확정/구현 대기 |
| 스트림 정책 | 녹화용 메인스트림 `2592x1520`, 실시간용 서브스트림 1080p | 확정/구현 대기 |

## 후속 확인 명령

Pi 배포본이 변경되면 위 버전과 활성 commit을 다시 확인한다. 이 문서에는 주소, 계정, 인증서 개인 키 등 자격 증명을 기록하지 않는다.

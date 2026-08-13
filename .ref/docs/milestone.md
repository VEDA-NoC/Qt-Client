# Qt VMS 개발 로드맵

## 현재 완료 상태
M1. RTSPS 4채널 수신 테스트 (완료)
M2. UI/UX 및 통신·정책 사양 확정 (완료, 외부 연계 검증 제외)
M3. Qt UI 셸과 디자인 시스템 (구현 완료, 사용자 빌드·시각 QA 대기)

## 구현 순서

### M1. RTSPS 4채널 수신 테스트 (완료)
라즈베리파이에서 4채널에 대해 RTSP 데이터를 RTSPS로 전송할 때, Qt에서 ffmpeg으로 영상을 디코딩한다.
큐에 전부 저장한 경우 버퍼가 감당하지 못했고 따라서 3프레임 기준으로 여유를 두었다.

### M2. UI/UX 및 통신·정책 사양 확정 (완료)

- UI 콘셉트와 브랜드 톤 확정
- 녹화·VOD·이벤트·제어·권한·시간·장애 정책 검토
- CCTV 스트림 및 이벤트 API 사양 확인
- 결과물: `.ref/docs/decisions/`의 문서를 `확정` 상태로 전환

### M3. Qt UI 셸과 디자인 시스템

- Qt Widgets 기반 navigation, 페이지 전환, 공통 컴포넌트
- Pretendard, semantic color token, Light theme
- 실시간 화면은 기존 RTSPS 수신을 유지하고, 미구현 서버 데이터는 가짜 값 대신 명확한 빈 상태로 표시

### M4. Qt-Pi 세션·메타데이터·제어 프로토콜

- TCP/TLS framing, 인증, request/response, event push
- 재접속, event sequence 복구, 명령 idempotency
- mock server 및 프로토콜 단위 테스트

### M5. 실시간 영상 제품화

- 1080p 서브스트림 적용
- 채널 상태, 재접속, 전체 화면, 프레임 통계
- CPU 렌더링 병목 측정 후 GPU 경로 필요 여부 판단

### M6. 움직임 기반 녹화와 RTSPS VOD

- Raspberry Pi 움직임 trigger, 사전·사후 녹화, 60초 MP4 분할
- SQLite recording session/segment index
- Qt 검색, 타임라인, seek, 구간 전환

### M7. 이벤트와 화재 대응 제어

- 이벤트 상태·심각도·확인·해결 처리
- STM32 장치 계층과 명령 상태
- 방재판 단계 1, 스프링클러 단계 2, 감사 로그

### M8. 저장장치·운영·상용화 준비

- HDD 등록·상태·보존 정책
- 서버 health, 장애 복구, 시간 동기화
- 개인정보·보안·오픈소스 고지·배포 패키지

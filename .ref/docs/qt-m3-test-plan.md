# Qt M3/M4 UI 셸·다채널 빌드 테스트 계획

Codex는 이 변경에서 빌드·실행하지 않는다. 아래 절차는 사용자 Windows Qt 환경에서 수행한다.

## 빌드

- 환경: Windows PowerShell 또는 Qt Creator가 설정한 MinGW 터미널
- 관리자 권한: 불필요
- 시작 경로: `C:\Users\shini\Documents\QtProjects\qt_4ch_viewer`
- 확인된 기존 구성: Qt `6.11.0/mingw_64`, `x64-mingw-dynamic`, MinGW Makefiles

```powershell
cmake -S . -B build-codex-mingw -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:/Qt/6.11.0/mingw_64 -DVCPKG_INSTALLED_DIR=C:/dev/vcpkg/installed -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic
cmake --build build-codex-mingw --parallel 8
```

실행:

```powershell
.\build-codex-mingw\qt_4ch_viewer.exe
```

Qt Creator로 빌드할 경우 기존 kit를 유지하고 CMake를 다시 구성한 뒤 Build/Run한다.

## 서버 없이 가능한 테스트

예상 시간: 약 5분

1. 앱이 최대화 상태로 열리고 최소 크기를 1280x720보다 작게 줄일 수 없는지 확인한다.
2. 좌측 Navy sidebar와 Orange 선택선, 밝은 본문, Pretendard 한글이 깨짐 없이 표시되는지 확인한다.
3. 5개 메뉴를 차례로 선택하고 상단 제목·설명과 본문이 함께 전환되는지 확인한다.
4. 저장소 API 연결 전에는 `장치 및 제어`와 하단 상태줄에 `저장소 확인 대기`가 중립 상태로 표시되는지 확인한다.
5. `녹화 재생`·`이벤트`·`장치 및 제어`가 가짜 데이터 대신 서버 연결 대기 빈 상태를 표시하는지 확인한다.
6. RTSPS 서버가 없으면 약 5초 뒤 채널 상태가 `연결 오류`, 상단 badge가 `채널 오류 · 0/4`로 바뀌는지 확인한다.
7. `중지` 후 각 채널이 `중지됨`, 수신량이 `0.00 Mbps`로 초기화되는지 확인한다.
8. 실시간 이외 메뉴에서는 스트림 상태·시작·중지 control이 숨겨지는지 확인한다.
9. 연결 중에는 버튼이 `다시 연결`로 표시되고, 이를 눌러도 UI 시계가 멈추지 않은 채 `재시작 준비 중`을 거쳐 다시 연결하는지 확인한다.

## 기존 RTSPS 서버 연동 테스트

필요 서버: Pi M4 live server가 준비된 채널에 대해 `{base}/ch1`~`{base}/ch4`를 제공해야 한다. 이벤트·저장소·STM API는 이번 실시간 다채널 확인에 필요하지 않다.

예상 시간: 약 10분

1. `설정`에서 Pi RTSPS Base URL을 입력하고 `적용 및 재연결`을 누른다.
2. 입력한 주소가 유효하면 적용 완료 문구가 표시되고 네 채널이 새 주소로 연결되는지 확인한다. 실행 중 수동 재연결은 실시간 화면의 `다시 연결`을 사용한다.
3. 유효하지 않은 주소를 입력했다가 기존 적용 주소로 되돌리면 적용 버튼이 비활성화되는지 확인한다. 이 상태에서 Enter를 눌러도 상태 문구와 스트림 연결이 바뀌지 않아야 한다.
4. 네 채널에 영상이 표시되고 각 상태가 `연결됨`, 상단이 `4채널 연결됨`으로 바뀌는지 확인한다.
5. 하단 수신 Mbps와 각 채널 decode/render fps가 약 1초마다 갱신되는지 확인한다.
6. 창 크기를 바꿔 영상 종횡비가 유지되고 마지막 프레임이 새 패널 크기에 맞게 다시 표시되는지 확인한다.
7. 한 채널 또는 Pi proxy를 중단해 해당 채널만 `연결 오류`가 되는지 확인한다.
8. 연결 오류가 된 채널은 마지막 프레임을 유지하지 않고 `영상 대기 중`으로 돌아가며 채널 통계가 0으로 초기화되는지 확인한다.
9. 통계 문구 위에 마우스를 두어도 `UI 큐` 설명 툴팁이 표시되지 않는지 확인한다.
10. 100%, 125%, 150% Windows 배율과 서로 다른 해상도의 모니터에서 통계가 잘리지 않고 폭이 부족하면 두 줄로 배치되는지 확인한다.
11. Pi RTSPS 포트는 열려 있지만 ingest가 첫 영상 버퍼를 받지 못한 상태로 Qt를 연결해도 앱이 종료되지 않고 채널만 `연결 오류`가 되는지 확인한다.

## M4 순차 채널 준비와 재연결

예상 시간: 약 10분

1. Pi에서 CH1과 CH3만 준비된 상태로 Qt를 연결한다.
2. CH1·CH3는 계속 `연결됨`을 유지하고 CH2·CH4만 `재연결 대기`, `재연결 중`으로
   전환되는지 확인한다.
3. `[live-retry]` 로그에서 실패 채널별 delay가 jitter를 포함한
   1, 2, 4, 8, 15, 30초 상한 순서인지 확인한다.
4. Pi에서 CH2를 준비한다. 현재 backoff 안에 CH2만 연결되고 CH1·CH3 worker가
   종료·재생성되지 않는지 확인한다.
5. CH2가 30초 이상 정상 재생된 뒤 다시 끊어 첫 retry가 약 1초로 초기화되는지
   확인한다.
6. retry 대기 중 `중지`를 누르면 더 이상 worker가 생성되지 않는지 확인한다.
7. retry 대기 중 녹화 재생 화면으로 이동하면 모든 live retry가 취소되고, 기존
   정책대로 live worker가 모두 끝난 뒤 Timeline 요청이 시작되는지 확인한다.
8. 실시간 화면의 `다시 연결`은 기존과 같이 전체 네 channel을 명시적으로 다시
   연결하는지 확인한다.

정상 핵심 로그:

```text
[live-retry] channel=2 state=scheduled attempt=3 delay_ms=... reason=...
[live-retry] channel=2 state=starting attempt=3
[live-retry] channel=2 state=stable attempts_reset=true
```

## 4채널 프레임 페이싱 진단

Qt Creator Application Output에서 10초 이상 다음 두 종류의 로그를 수집한다.

```text
[ch1] recv=<Mbps> packets=<fps>/s decode_fps=<fps> queue_drops=<fps>/s total_frames=<count>
[ui-ch1] display_fps=<fps> max_gap_ms=<ms> ui_queue_ms=<ms> frame=<WxH> panel=<WxH>
```

판단 기준:

- `packets`부터 낮거나 크게 흔들리면 Pi 송출·네트워크·카메라 입력을 먼저 확인한다.
- `packets`는 약 30인데 `decode_fps`가 낮으면 Qt software decode CPU 부하 가능성이 크다.
- `decode_fps`는 약 30인데 `queue_drops`가 증가하거나 `display_fps`가 낮고 `max_gap_ms`가 크면 Qt GUI 변환·크기 조정 부하 가능성이 크다.
- 네 채널의 `frame=2592x1520`이면 현재 2K software decode 시험 결과로 기록하고, M5 1080p 서브스트림 적용 후 같은 조건으로 다시 비교한다.
- 화면 녹화는 추가 부하를 만들 수 있으므로 우선 로그 10초와 Windows 작업 관리자의 Qt CPU·GPU 사용률을 함께 수집한다.

### 지연 수치의 의미

- 화면의 `UI 큐`는 FFmpeg 디코드와 RGB 변환이 완료된 프레임을 worker가 emit한 시점부터 GUI slot이 받은 시점까지의 대기시간이다.
- 카메라 촬영, 카메라 인코딩, CCTV→Pi 전송, Pi 중계, Pi→Qt 전송, FFmpeg 입력 버퍼와 디코드 시간은 포함하지 않으므로 종단 지연으로 해석하지 않는다.
- M3 종단 지연은 우선 카메라가 밀리초 시계를 촬영하게 한 뒤 원본 시각과 Qt 화면 시각을 비교하는 방식으로 측정한다.
- 운영 중 자동 종단 지연 표시는 카메라 capture UTC 또는 RTP timestamp와 RTCP Sender Report의 NTP 매핑, 장치 간 NTP 동기화가 검증된 뒤 구현한다.

## 정상 핵심 출력

Qt Creator Application Output에서 재생 중 채널마다 다음 형태의 통계가 반복된다.

```text
[ui] bundled font families= Pretendard
[ch1] video_format source=yuvj420p conversion=yuv420p range=full colorspace=<값>
[ch1] recv=<양수> Mbps packets=<양수>/s decode_fps=<양수> queue_drops=<값>/s total_frames=<증가값>
[ui-ch1] display_fps=<양수> max_gap_ms=<값> ui_queue_ms=<값> frame=<해상도> panel=<표시크기>
```

정상 재생 중 `deprecated pixel format used` 경고가 반복되면 안 된다. 여러 채널의 Qt/FFmpeg 로그가 한 줄 중간에서 서로 합쳐져서도 안 된다. `video_format`의 source가 `yuvj420p`인 경우 conversion은 `yuv420p`, range는 `full`이어야 하며 영상의 명암 범위가 변경 전과 시각적으로 같아야 한다.

서버가 없으면 약 5초 뒤 다음과 같은 상태가 채널별로 출력된다.

```text
[ch1] status=open timeout after 5000 ms
```

서버가 RTSP 세션과 트랙만 만들고 유효한 영상을 제공하지 못하면 `read timeout`, `read ended`, `first frame timeout`, `decoded frame error` 중 실제 실패 단계에 맞는 오류가 출력될 수 있다. 유효 프레임이 없는 상태가 패킷 수신으로 계속 유지되어도 15초 안에 실패 처리하며, 이 경우에도 FFmpeg assertion으로 앱이 종료되어서는 안 된다.

## 실패 시 수집할 자료

- Qt Creator의 전체 Application Output
- 실패 채널의 Application Output 상태 문구
- `설정` 화면의 Base URL. 자격 증명은 마스킹한다.
- 문제 화면 전체 screenshot과 Windows 배율
- 빌드 실패 시 최초 error 전후 약 30줄과 사용한 Qt kit/CMake configure 명령

## 이번 단계에서 검증하지 않는 항목

- Pi 메타데이터 TCP/TLS 연결, 설정 읽기·쓰기
- 움직임 기반 5초/10초 녹화와 RTSPS VOD
- HDD 실장·SMART·용량
- STM32 제어, 구역 polygon 편집, LPR·전기차 외부 조회

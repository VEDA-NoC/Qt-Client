# Qt M6 Timeline·녹화 재생 테스트 계획

Codex는 Qt 코드를 작성하지만 사용자 Windows 환경에서 빌드·실행하지 않는다.

## 구현 범위

- QNetworkAccessManager 기반 HTTPS Control API
- HTTP Basic 로그인과 bearer token 보관
- `server.crt` trust, hostname/IP SAN 검증, SHA-256 leaf pin
- 동일 시간 범위의 4채널 Timeline 조회
- 공통 시간축에 recording span과 event duration marker 표시
- recording span 클릭으로 채널과 재생 시작 시각 선택
- one-shot playback session 생성
- 반환된 `rtsps_path`와 신뢰된 RTSPS Base URL 결합
- 기존 FFmpeg StreamWorker를 사용한 VOD 재생
- 재선택·일시정지 재개·1초 seek 시 새 session 발급
- `Space`, 좌우 방향키, `F11`, `Esc`, 영상 더블클릭 단축 조작
- Timeline 클릭 시 JPEG thumbnail preview
- playback 중에도 live Mobile baseline 4개 유지 및 live 복귀 즉시 최신 frame 표시

Export, 배속과 역재생은 이번 범위가 아니다.

## 사전 준비

Pi에서 다음 서비스가 동시에 실행되어야 한다.

- Control HTTPS: `https://PI_IP:9443`
- Live/Playback RTSPS: `rtsps://PI_IP:8554`
- `operator` control credential
- Timeline에 조회 가능한 retained segment

Pi의 공개 인증서 `certs/server.crt`를 Windows PC로 복사한다. private key는 복사하지
않는다. 인증서 SAN에는 Qt에서 입력할 Pi hostname 또는 IP가 포함되어야 한다.

## 빌드

환경: Windows Qt Creator, Desktop Qt 6.11.0 MinGW 64-bit  
관리자 권한: 불필요  
시작 경로: `C:\Users\shini\Documents\QtProjects\qt_4ch_viewer`

`CMakeLists.txt`에 Qt Network와 새 소스가 추가됐으므로 Qt Creator에서 `Build > Run
CMake`를 한 번 실행한 뒤 `Ctrl+B`로 빌드한다.

명령행을 사용할 경우:

```powershell
cmake -S . -B build-codex-mingw -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:/Qt/6.11.0/mingw_64 -DVCPKG_INSTALLED_DIR=C:/dev/vcpkg/installed -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic
cmake --build build-codex-mingw --parallel 8
```

## 정상 연동 테스트

예상 시간: 약 10분

1. 앱의 `설정`에서 실시간 RTSPS Base URL이 Pi의 `rtsps://PI_IP:8554`인지 확인한다.
2. `Pi Control HTTPS Base URL`에 `https://PI_IP:9443`을 입력한다.
3. `operator`용 Control 비밀번호를 입력한다.
4. Pi에서 복사한 `server.crt`를 선택하고 `Control API 적용`을 누른다.
5. `인증 완료 · token 유효시간 ...초`가 표시되는지 확인한다.
6. `녹화 재생`에서 기준 날짜·시간과 표시 범위를 선택한다. UI는 서버 UTC를 기준으로
   변환한 KST로 표시하고 API에는 UTC millisecond `[start, end)`로 보낸다.
7. `타임라인 불러오기`를 누르고 CH 1~4가 공통 시간축에 표시되는지 확인한다.
8. Navy recording bar와 Orange event 선, 시간축 눈금이 표시되는지 확인한다.
9. 원하는 채널의 recording bar를 한 번 클릭하면 해당 시각의 썸네일이 표시되는지
   확인하고, 더블클릭 또는 `재생`을 누른다.
10. 새 one-shot session이 생성되고 VOD 영상이 표시되는지 확인한다.
11. 다른 구간을 선택해 다시 재생한다. 기존 worker가 종료된 뒤 새 session을 발급해야
    한다.
12. `재생 중지`를 눌러 영상과 통계가 대기 상태로 초기화되는지 확인한다.
13. 재생 중 `Space`로 일시정지하고 다시 `Space`로 재개한다. 새 session 연결 후
    중지한 시각 부근에서 이어져야 한다.
14. 좌우 방향키로 1초씩 이동한다. 녹화가 없는 시각은 경고를 표시한다.
15. `F11` 또는 영상 더블클릭으로 전체화면을 전환하고 `Esc`로 복귀한다.
16. 재생 화면에 들어가도 live `/ch1/mobile`~`/ch4/mobile` worker 네 개가
    `Playing`을 유지하는지 확인한다. live 화면으로 돌아오면 새 `Opening` 없이
    보관한 최신 Mobile frame이 즉시 표시되어야 한다.
17. 타임라인의 서로 다른 시각을 빠르게 클릭해도 마지막으로 선택한 시각의
    thumbnail만 표시되는지 확인한다.
18. 인증서를 적용하지 않은 새 실행에서는 날짜·시간·범위·이동·조회 control이 모두
    비활성 상태이고, 타임라인 중앙에 Control API 설정 안내가 먼저 보이는지 확인한다.
19. 좌우 방향키를 빠르게 여러 번 누르고 session 생성 로그가 키 입력마다 생기지
    않고 마지막 위치에 한 번만 생성되는지 확인한다.
20. playback이 끝났거나 일시정지된 뒤 재생 화면에 다시 들어왔을 때 수신·패킷·
    decode·UI 지표가 0인지 확인한다.
21. fullscreen에서 하단 재생 control이 보이고 mouse로 재생·정지·seek·종료할 수
    있는지 확인한다.
22. `Ctrl+마우스 휠`, 확대·축소 아이콘, `캐시 전체`로 timeline 배율을 바꾸고 선택 시각과
    이벤트 위치가 같은 시각을 유지하는지 확인한다. 배율에 따라 10분·1시간 등
    세로 눈금 급간이 바뀌고 모든 급간선이 보이며 날짜 경계는 더 진해야 한다.
23. 날짜 버튼의 어느 부분을 눌러도 달력이 열리고 날짜를 keyboard로 직접 편집할
    수 없는지, 시간은 `HH:mm:ss`로 입력되는지 확인한다.
24. seek 후 새 session이 만들어져도 player bar의 시작·종료점이 최초 선택 구간을
    유지하는지 확인한다.
25. `이전 구간`, `다음 구간`, `현재`가 선택한 표시 범위만큼 pivot을 이동해 자동
    재조회하는지 확인한다.
26. 재생 중 다른 녹화 시각을 한 번 클릭하면 기존 재생이 멈추고 새 시각의 썸네일이
    표시되는지, 더블클릭하면 새 playback session으로 재생되는지 확인한다.
27. 실시간 화면을 세로로 긴 창, 일반 가로 창, 초광폭 창으로 바꾼다. 각각
    `4행×1열`, `2행×2열`, `1행×4열`이어야 한다. 세로 화면에서는 왼쪽 카메라
    목록만 scroll되고 각 영상이 가로폭에 맞는 `2592:1520` 비율로 충분히 크게
    표시되어야 한다. 최근 이벤트 panel은 오른쪽에 고정되고 내용과 tooltip이
    잘리지 않아야 한다.
28. 일반 playback 화면의 검은 surface가 `2592:1520` 비율을 유지하고 통계 끝에
    `원본 ... · 표시 ...` 해상도가 출력되는지 확인한다.
29. playback 영상을 한 번 클릭하면 double-click 판정 시간 후 재생/일시정지하고,
    빠르게 두 번 클릭하면 재생 상태를 바꾸지 않고 fullscreen만 전환하는지 확인한다.
30. `Space`를 누를 때 일반 창과 fullscreen의 재생/일시정지 버튼 text와 enabled
    상태가 즉시 함께 바뀌는지 확인한다.
31. Timeline 일반 wheel과 navigator로 cache 안을 이동하고 상단 기준 날짜·시간이
    같이 갱신되는지 확인한다. `Ctrl+wheel`은 확대·축소만 해야 한다.
32. 표시 window가 cache 경계 반 화면 이내에 도달하면 인접 범위의 4채널 요청이 한
    batch만 발생하고, 응답 후 기존 span/event가 유지되는지 확인한다.
33. 움직임 녹화가 없는 channel은 `녹화 없음`, 응답 실패 channel은 `조회 실패`,
    아직 응답하지 않은 channel만 `조회 중`으로 표시되는지 확인한다.
34. 정상 영상을 표시한 뒤 잘못된·만료된 인증서를 적용한다. TLS 실패 메시지와 함께
    이전 frame, thumbnail, timeline, 통계가 즉시 제거되는지 확인한다.
35. `server.crt` 또는 비밀번호가 없거나 잘못된 경우에만 타임라인 안내 바로 아래
    `설정으로 이동`이 보이는지 확인한다. 정상 설정으로 로그인한 뒤 서버만 중단하거나
    token을 만료시키면 `설정으로 이동` 대신 `다시 연결`이 보이고, 비밀번호를 다시
    입력하지 않아도 재로그인을 시도하는지 확인한다.
36. 재생·일시정지 버튼, 영상 단일 클릭, `Space`를 번갈아 사용한다. `연결 중`,
    `일시정지 중`, `중지 중`에는 중복 조작이 막히고 worker 종료 뒤 `일시정지` 또는
    `미리보기`로 일관되게 전환되는지 확인한다.

## 성능 회귀 확인

예상 시간: 약 5분

1. live 화면에서 4채널을 60초 유지한다.
2. 각 채널의 `decode_fps`, `ui_fps`, `paint_fps`, `UI 큐`를 기록한다.
3. Mobile baseline 네 개가 유지된 상태로 playback VOD를 60초 재생한다.
4. live 화면으로 돌아와 다시 60초 기록한다.

목표는 30 fps source에서 `paint_fps`가 지속적으로 decode의 절반 이하로 떨어지지
않고, `UI 큐`가 수백 ms로 계속 누적되지 않는 것이다. 화면별 worker 격리 후에도
`paint_fps 11~16`, `UI 큐 200 ms 이상`이 반복되면 Qt GUI scaling 경로와
hardware decode 전환을 다음 병목으로 분석한다.

## N2 live 품질 전환 테스트

1. 실시간 2×2 화면에서 CH1~CH4가 `/chN/high`로 먼저 영상을 표시하는지 확인한다.
   이후 각 채널이 기존 영상을 유지한 채 `Standard 준비 중 · High 표시`,
   `Mobile 준비 중 · Standard 표시` 순서로 전환되고 최종적으로 네
   `/chN/mobile` session만 유지되어야 한다.
2. CH1 영상을 더블클릭한다. Mobile 영상이 지워지지 않은 채
   `High 준비 중 · Mobile 표시`가 보이고, `/ch1/high` 첫 frame 뒤 High로 바뀌어야 한다.
3. `Esc` 또는 영상을 다시 더블클릭한다. 전체화면을 닫자마자 최신 Mobile frame이
   표시되어야 한다.
4. `/ch1/high` route를 실패시키고 전체화면에 진입한다. 검은 화면 없이 Mobile을
   계속 표시하며 foreground retry가 backoff로 반복되어야 한다.
5. `/ch1/standard` 또는 `/ch1/mobile` 준비를 지연·실패시킨다. candidate retry 중에도
   직전 High 또는 Standard 영상이 계속 표시되고 late frame이 화면을 덮지 않아야 한다.
   CH2 Standard처럼 일부 H.264 packet에서 `AVERROR_INVALIDDATA`가 발생하는 경우에는
   `[stream-decode] ... state=packet_dropped`가 출력되더라도 정상 frame이 이어지는 동안
   worker retry와 화면 정지가 없어야 한다. 정상 decoded frame이 5초간 완전히 끊기면
   `decode stall timeout after 5000 ms` 뒤 해당 worker만 retry해야 한다.
6. 녹화 재생에 들어가 VOD를 재생하는 동안 Pi에서 Mobile session 네 개가 유지되는지
   확인한다. live 복귀 시 2~3초 재접속 대기 없이 영상이 보여야 한다.
7. `중지`, RTSPS URL 재적용, 로그아웃을 각각 수행해 관련 live worker와 retry가
   종료되는지 확인한다.
8. High 연결 중 또는 retry 대기 중 앱을 종료한다. 종료 지연 후 crash, 남은 thread,
   종료 뒤 retry 로그가 없어야 한다.

변경 후 `[ui-chN]` 로그의 `frame=`은 원본 `2592x1520`이 아니라 panel에 맞춘
출력 크기여야 한다. 원본 크기가 계속 표시되면 worker output size 전달이 적용되지
않은 것이다.

## 정상 핵심 출력

```text
[live-profile] channel=1 role=baseline quality=High state=starting
[stream-open] channel=1 mode=live route=/ch1/high state=open_success
[stream-open] channel=1 mode=live route=/ch1/high state=stream_info codec=h264
[stream-open] channel=1 mode=live route=/ch1/high state=decoder_open codec=h264
[stream-open] channel=1 mode=live route=/ch1/high state=first_video_packet bytes=...
[stream-open] channel=1 mode=live route=/ch1/high state=first_decoded_frame elapsed_ms=... recoverable_errors=...
[live-profile] channel=1 role=candidate quality=Standard state=starting
[live-profile] channel=1 quality=Standard state=promoted_after_first_frame
[live-profile] channel=1 role=candidate quality=Mobile state=starting
[live-profile] channel=1 quality=Mobile state=promoted_after_first_frame
[playback-api] login succeeded expires_in=...
[playback-api] timeline channel=1 spans=... events=...
[playback-api] session created channel=1 duration_ms=... segments=...
[playback] status=Connecting
[playback] status=Opening
[playback] status=Playing ...
[playback] status=PlaybackEnded
```

Application Output에 Control 비밀번호, bearer token, 전체 playback capability가
출력되면 안 된다.

## TLS 실패 테스트

다음 경우 요청이 실패해야 한다.

- 다른 Pi의 `server.crt` 선택
- 인증서 SAN에 없는 IP 또는 hostname으로 접속
- 만료되었거나 손상된 인증서
- `http://` Control URL

예상 UI/로그:

```text
TLS 검증 실패: ...
TLS 인증서 pin이 설정된 server.crt와 일치하지 않습니다.
```

`ignoreSslErrors()`로 우회하거나 Control API를 평문 HTTP로 재시도하면 안 된다.

## API 실패 테스트

- 잘못된 비밀번호: HTTP 401 `invalid_credentials`
- token 만료: HTTP 401, token 폐기 후 다음 사용자 요청에서 재로그인
- 녹화가 없는 범위: Timeline에는 gap만 표시되고 재생 버튼은 활성화되지 않음
- session 대상 미존재: HTTP 404 `playable_media_not_found`
- playback 미지원 상태: HTTP 503 `playback_unavailable`
- Control server 미응답: 10초 timeout

## 실패 시 수집 자료

- Qt Creator Application Output 전체
- 실패 시각과 선택한 채널·시간 범위
- Timeline 화면 screenshot
- Control URL과 RTSPS Base URL
- 인증서의 subject, issuer, SAN, 유효기간
- 비밀번호, bearer token, playback capability는 마스킹
- 빌드 실패 시 최초 error 전후 약 30줄과 Qt kit 정보

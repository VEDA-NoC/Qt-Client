# VEDA VMS Qt Client

Windows용 Qt Widgets 클라이언트입니다. 현재 구현은 4채널 RTSPS 실시간 영상, Control API 기반 재생/타임라인, 주차 영역 편집, 이벤트 long polling, 그리고 RS-485로 붙은 STM32 구역 제어기의 상태 조회·화재 진압 제어·등록 관리를 포함합니다.

마지막 갱신: 2026-08-25 (`main@4c0dfa9`). 진행 상황과 다음 작업은 [.ref/docs/qt-next-task-prompt.md](.ref/docs/qt-next-task-prompt.md)를 봅니다.

## 개발 환경

현재 확인된 기준 환경은 다음과 같습니다.

- Windows 10/11
- Qt 6.11.0 MinGW 64-bit (`C:/Qt/6.11.0/mingw_64`)
- CMake 3.30.x 이상
- MinGW 64-bit 및 `MinGW Makefiles` generator
- vcpkg (`C:/dev/vcpkg`)
- FFmpeg 8.1.2#1, vcpkg triplet `x64-mingw-dynamic`

경로는 각 PC에 맞게 바꾸십시오. 특히 `C:/dev/vcpkg`는 현재 개발 PC의 경로일 뿐, 팀원의 경로가 반드시 같다는 보장은 없습니다.

## FFmpeg 설치

vcpkg가 설치되어 있지 않다면 먼저 공식 vcpkg 설치 절차를 완료한 뒤, 다음 패키지를 설치합니다.

```powershell
C:/dev/vcpkg/vcpkg.exe install ffmpeg:x64-mingw-dynamic
```

설치된 vcpkg와 FFmpeg 버전은 다음으로 확인합니다.

```powershell
C:/dev/vcpkg/vcpkg.exe version
C:/dev/vcpkg/vcpkg.exe list | Select-String 'ffmpeg:x64-mingw-dynamic'
```

이 저장소에는 현재 `vcpkg.json` 또는 `vcpkg-configuration.json`이 없으므로 vcpkg baseline은 고정되어 있지 않습니다. baseline을 사용하는 저장소인지 확인하려면 다음을 실행합니다.

```powershell
Get-ChildItem vcpkg.json,vcpkg-configuration.json -ErrorAction SilentlyContinue |
  Select-String -Pattern 'builtin-baseline|baseline'
```

출력이 없으면 baseline 미사용 상태이며, 현재는 `vcpkg list`의 FFmpeg 버전과 triplet을 인계 기준으로 사용합니다.

현재 CMake는 FFmpeg를 vcpkg manifest로 자동 설치하지 않습니다. `CMakeLists.txt`의 `VCPKG_INSTALLED_DIR`와 `VCPKG_TARGET_TRIPLET`를 기준으로 다음 디렉터리를 직접 찾습니다.

```text
<vcpkg>/installed/x64-mingw-dynamic/include
<vcpkg>/installed/x64-mingw-dynamic/lib
<vcpkg>/installed/x64-mingw-dynamic/bin
```

따라서 팀원의 vcpkg 위치가 다르면 configure 단계에서 `-DVCPKG_INSTALLED_DIR=...`를 지정해야 합니다.

## CMake 빌드

Qt와 vcpkg의 실제 설치 경로에 맞춰 새 build 디렉터리에서 configure합니다.

```powershell
cmake -S . -B build-team -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.11.0/mingw_64 `
  -DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_INSTALLED_DIR=C:/dev/vcpkg/installed `
  -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic

cmake --build build-team --config Debug
```

빌드 후 CMake가 vcpkg의 FFmpeg runtime DLL을 실행 파일 옆으로 복사합니다. Qt DLL까지 포함한 배포 디렉터리가 필요하면 다음을 사용합니다.

```powershell
cmake --install build-team --prefix deploy
```

Qt Creator를 사용하는 경우 `Desktop Qt 6.11.0 MinGW 64-bit` kit를 선택하고, 위 configure 값에 `CMAKE_TOOLCHAIN_FILE`, `VCPKG_INSTALLED_DIR`, `VCPKG_TARGET_TRIPLET`를 추가합니다. 기존 `CMakeCache.txt`를 다른 PC에서 재사용하지 마십시오.

## 실행 전 운영 설정

### RTSPS

- 설정 화면의 실시간 Base URL은 `rtsps://host:8554` 형식입니다.
- 현재 소스에는 개발 환경의 기본 RTSPS 주소가 들어 있으므로 팀원 환경에 맞게 설정 화면에서 변경해야 합니다.
- 카메라 원본 RTSP URL과 카메라 계정은 Qt에 넣지 않고 Pi/스트림 서버에서 관리합니다.

### Control API

- 기본 포트 규칙은 RTSPS 호스트의 `9443`입니다. 실제 서버 구성에 맞는 `https://host:port`를 입력합니다.
- 사용자 이름은 현재 `operator`로 표시되며, 비밀번호는 실행 중 입력합니다.
- 로그인 후 status, timeline, thumbnail, playback session, parking API, event long polling 및 STM 장치·명령 API가 동작합니다.

### STM32 구역 제어기

- Pi를 `--enable-stm-bus` 계열 플래그로 기동해야 STM API가 응답합니다. 꺼져 있으면 Qt에는
  `503 stm_bus_disabled`가 표시됩니다.
- 장치 페이지의 충전 스테이션 목록은 그 페이지에 있을 때만 5초 주기로 폴링합니다.
- 진압 명령은 실시간 화면의 `stm.fire_started` 카드에서 여는 대응창을 통해서만 보냅니다.
  Qt는 `slave_address`와 opcode만 보내고, `origin`은 Pi가 `QT_ADMIN`으로 고정합니다.
- 원격 1단계 명령은 STM 펌웨어에서 **순차 진압(방재포 전개 → 살수)**으로 처리됩니다.

### TLS 인증서

개발/실기 테스트 시 Pi의 공개 인증서 `server.crt`만 `certs/server.crt`에 복사하거나 설정 화면에서 다른 경로를 선택합니다. `server.key`는 복사하지 않습니다. 인증서 파일은 저장소에 커밋하지 않으며, Qt는 trust chain·hostname/IP SAN·leaf certificate SHA-256 pin을 모두 검증합니다.

> 현재 소스의 인증서 기본 경로와 RTSPS 기본 주소는 특정 개발 PC 값이 그대로 들어 있습니다.
> 설정을 저장하는 기능이 없어 실행할 때마다 설정 화면에서 바꿔야 합니다(알려진 정리 대상 —
> [.ref/docs/qt-next-task-prompt.md](.ref/docs/qt-next-task-prompt.md) §3-2).

## 코드 구조

- `main_window.*`: 화면 구성(실시간·재생·이벤트·장치·설정 5개 페이지), 4채널 상태, 재생/주차/이벤트 흐름 조정
- `stream_worker.*`: FFmpeg 기반 RTSPS 수신·디코딩 worker
- `video_panel.*`: 채널별 영상 표시
- `playback_api_client.*`: Control API 로그인, 상태, timeline, thumbnail, playback session, parking API
- `parking_event_api_client.*`: `/api/v1/events` cursor long polling
- `parking_event_store.*`, `parking_event_card_widget.*`, `parking_event_types.h`: 이벤트 저장소와 카드 UI, 이벤트 DTO(카메라·STM 양쪽 payload)
- `parking_zone_editor.*`, `parking_zone_canvas.*`: 주차 영역 편집 UI와 STM 장치 매핑
- `stm_api_client.*`: STM 장치 조회·명령 제출·명령 상태·등록 해제 REST 클라이언트
- `stm_device_list_widget.*`: 장치 페이지의 충전 스테이션 목록(5초 폴링, 등록 해제)
- `stm_fire_response_dialog.*`: 화재 진압 승인 대응창
- `stm_types.h`: `stm_protocol.h`의 opcode·상태값 사본 (프로토콜 변경 시 함께 고쳐야 함)
- `timeline_widget.*`: 녹화 timeline 렌더링과 구간 선택
- `legal_notice_widget.*`: 정보 및 법적 고지 화면
- `theme.*`: 공통 UI 토큰과 QSS
- `resources.qrc`, `resources/`: 아이콘·폰트·앱 리소스
- `.ref/docs/`: 설계 결정, 마일스톤, 테스트 계획 및 이전 작업 기록

## 인계 시 확인할 외부 의존성

코드만 clone한다고 바로 실기 동작이 재현되지는 않습니다. 다음 정보는 별도 보안 채널로 전달해야 합니다.

1. Qt 설치 경로와 MinGW kit
2. vcpkg 설치 경로, FFmpeg triplet 및 설치 버전
3. Pi/Control API 호스트·포트와 네트워크 접근 방법
4. 테스트용 `server.crt`와 인증서 교체 절차
5. 테스트용 Control API 계정 발급/회수 담당자
6. 4개 채널의 실제 스트림 매핑과 playback channel ID
7. 기대하는 서버 API 버전 및 현재 알려진 서버 제약

실제 파일로 전달해야 하는 것은 기본적으로 Pi의 공개 인증서 `server.crt`입니다. 개발 PC의 `certs/server.crt`에 복사하거나 실행 후 설정 화면에서 경로를 선택합니다. `server.key`, 비밀번호, access token은 Qt 클라이언트에 전달하거나 저장소에 넣지 않습니다.

비밀번호, access token, private key는 README·GitHub·소스에 기록하지 않습니다.

## 검증 순서

팀원은 다음 순서로 확인하는 것을 권장합니다.

1. 빈 build 디렉터리에서 CMake configure 성공
2. Debug build 성공 및 실행 파일 옆 FFmpeg DLL 생성
3. 인증서 없이 Control API 연결 시 의도한 TLS 오류가 표시되는지 확인
4. `server.crt`와 올바른 API 계정으로 로그인 및 status 확인
5. 1개 채널 RTSPS 연결 후 4개 채널 재연결 동작 확인
6. timeline 조회 → thumbnail → playback session → 재생 확인
7. 이벤트 수신과 주차 영역 조회/검증/적용 흐름 확인
8. 장치 페이지에서 STM 목록 표시(등록 / 미등록 구분)와 등록 해제 확인
9. `stm.fire_started` 이벤트 카드 → 진압 대응창 → 명령 제출 → 액추에이터 동작 → STAGE 이벤트
   반영까지 STM32+Pi+Qt 3자 통합 확인

상세 검증 기준은 `.ref/docs/qt-m6-playback-test-plan.md`와 관련 테스트 계획 문서를 참조합니다.
미검증으로 남은 항목은 `.ref/docs/qt-next-task-prompt.md` §4에 정리돼 있습니다.

## 저장소 주의사항

`build/`, `build-*`, `CMakeCache.txt`, Qt Creator 사용자 설정은 로컬 산출물입니다. 현재 저장소에는 과거 `build-codex-mingw` 산출물이 이미 추적되어 있으므로, 팀 저장소 정리 시 해당 디렉터리를 Git에서 제거하고 새 build 디렉터리만 사용해야 합니다. 파일을 로컬에서 삭제할 필요는 없으며, 저장소 정리 commit에서 추적만 해제하면 됩니다.

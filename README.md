# VEDA VMS Qt Client

Windows용 Qt Widgets 클라이언트입니다. 현재 구현은 4채널 RTSPS 실시간 영상, Control API 기반 재생/타임라인, 주차 영역 편집, 이벤트 long polling을 포함합니다.

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
- 로그인 후 status, timeline, thumbnail, playback session, parking API 및 event long polling이 동작합니다.

### TLS 인증서

개발/실기 테스트 시 Pi의 공개 인증서 `server.crt`만 `certs/server.crt`에 복사하거나 설정 화면에서 다른 경로를 선택합니다. `server.key`는 복사하지 않습니다. 인증서 파일은 저장소에 커밋하지 않으며, Qt는 trust chain·hostname/IP SAN·leaf certificate SHA-256 pin을 모두 검증합니다.

## 코드 구조

- `main_window.*`: 화면 구성, 4채널 상태, 재생/주차/이벤트 흐름 조정
- `stream_worker.*`: FFmpeg 기반 RTSPS 수신·디코딩 worker
- `video_panel.*`: 채널별 영상 표시
- `playback_api_client.*`: Control API 로그인, 상태, timeline, thumbnail, playback session, parking API
- `parking_event_api_client.*`: `/api/v1/events` long polling
- `parking_zone_editor.*`, `parking_zone_canvas.*`: 주차 영역 편집 UI
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

상세 검증 기준은 `.ref/docs/qt-m6-playback-test-plan.md`와 관련 테스트 계획 문서를 참조합니다.

## 저장소 주의사항

`build/`, `build-*`, `CMakeCache.txt`, Qt Creator 사용자 설정은 로컬 산출물입니다. 현재 저장소에는 과거 `build-codex-mingw` 산출물이 이미 추적되어 있으므로, 팀 저장소 정리 시 해당 디렉터리를 Git에서 제거하고 새 build 디렉터리만 사용해야 합니다. 파일을 로컬에서 삭제할 필요는 없으며, 저장소 정리 commit에서 추적만 해제하면 됩니다.

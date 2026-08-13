# 01. 영상·녹화·재생

- 상태: 확정 (성능 검증 대기)
- 기준일: 2026-07-22

## 권장 결론

실시간과 녹화 재생 모두 RTSPS를 사용한다. Raspberry Pi는 저장된 MP4를 디코딩하거나 재인코딩하지 않고 `demux -> parser -> RTP payloader -> RTSPS`로 전송한다. Qt는 현재 FFmpeg 입력·디코딩 코드를 공통 기반으로 사용하되, 재생 전용 seek·pause·속도·파일 전환 상태를 별도 컨트롤러로 구현한다.

HTTPS/HLS는 1차안에서 제외한다. 추후 동시 재생 세션 수 증가 또는 RTSPS VOD seek 구현 문제가 확인될 때 `HTTPS로 MP4 조각 직접 제공` 방식을 대안으로 측정한다. HLS는 별도 플레이리스트와 세그먼트 관리가 필요하므로 자동 채택하지 않는다.

## 스트림 구성

| 용도 | 입력 | Raspberry Pi 처리 | Qt 처리 | 상태 |
|---|---|---|---|---|
| 실시간 | CCTV 서브스트림 1080p, 최대 30 fps 목표 | TLS 중계, 무변환 | RTSPS 수신·디코딩·표시 | 승인 |
| 녹화 | CCTV 메인스트림 `2592x1520`, 최대 30 fps | MP4 분할 저장, 무변환 | 직접 수신하지 않음 | 승인 |
| 녹화 재생 | 저장된 MP4 조각 | RTSPS VOD로 재패킷화, 무변환 | RTSPS 수신·디코딩·seek | 승인 |

카메라가 메인·서브스트림을 동시에 안정적으로 제공하는지, 코덱·비트레이트·GOP·가변 프레임률 설정 범위는 CCTV 사양서로 확인한다.

## 움직임 기반 녹화 규칙

| 항목 | 제안값 | 비고 |
|---|---:|---|
| 녹화 시작 | 움직임 이벤트 수신 즉시 | 영상 자체를 Pi에서 분석하지 않고 CCTV 이벤트 메타데이터 사용을 우선 |
| 사전 녹화 | 5초 | 메모리 링 버퍼 또는 카메라 이벤트 선행 버퍼 필요 |
| 사후 녹화 | 마지막 움직임 후 10초 | 반복 이벤트 발생 시 연장 |
| 이벤트 병합 | 사후 녹화 구간 안의 새 움직임은 같은 녹화 세션으로 병합 | 파일은 여러 조각일 수 있음 |
| 파일 길이 | 최대 60초 목표 | 실제 분할은 keyframe 경계이므로 GOP에 따라 초과 가능 |
| 무움직임 구간 | 영상 파일을 만들지 않음 | 타임라인에는 녹화 공백으로 표시 |
| 컨테이너 | MP4 | 비정상 종료 복구 전략 별도 검증 필요 |

GStreamer `splitmuxsink`는 시간 기준을 넘긴 뒤 keyframe 경계에서 파일을 분리하므로 카메라 GOP가 지나치게 길면 60초를 초과할 수 있다. 독립 재생 가능한 파일을 위해 closed GOP 또는 이에 준하는 카메라 설정을 확인한다.

## 녹화와 이벤트 데이터 모델

- `recording_session`: 움직임 시작부터 사후 녹화 종료까지의 논리 세션
- `recording_segment`: 실제 MP4 파일 하나. 한 세션이 여러 세그먼트를 참조할 수 있음
- `event`: 주정차·화재·장애 등 사건. 하나 이상의 녹화 세션/세그먼트와 연결 가능
- 모든 시각은 UTC로 저장하고 Qt에서 KST로 표시
- 세그먼트는 `camera_id`, 시작·종료 UTC, 경로, 크기, 코덱, 해상도, 평균 비트레이트, 무결성 상태를 저장

## RTSPS VOD 흐름

1. Qt가 TLS 메타데이터 채널로 카메라와 시간 범위를 조회한다.
2. Raspberry Pi가 SQLite에서 세그먼트를 선택하고 임시 `playback_session_id`와 RTSPS URL을 반환한다.
3. Qt가 RTSPS VOD URL을 열고 RTSP `PLAY/PAUSE/Range`를 사용한다.
4. 서버는 `splitmuxsrc` 또는 동등한 동적 소스로 연속 세그먼트를 제공한다.
5. 녹화 공백은 빈 영상으로 채우지 않고 Qt 타임라인에서 명시적으로 표시한다.

## 500 GB 용량 산정

다음 식으로 실제 보존 기간을 계산한다.

`일일 사용량(GB) = 전체 녹화 비트레이트(Mbps) x 움직임 점유율 x 10.8`

`예상 보존일 = 녹화 사용 가능 용량(GB) / 일일 사용량(GB)`

안전 여유 10%를 두어 450 GB를 녹화 가능 용량으로 가정한 예시는 다음과 같다.

| 채널별 비트레이트 | 4채널 전체 | 움직임 점유율 | 일일 사용량 | 예상 보존일 |
|---:|---:|---:|---:|---:|
| 8 Mbps | 32 Mbps | 10% | 약 34.6 GB | 약 13.0일 |
| 8 Mbps | 32 Mbps | 20% | 약 69.1 GB | 약 6.5일 |
| 12 Mbps | 48 Mbps | 10% | 약 51.8 GB | 약 8.7일 |

실제 비트레이트와 시간대별 움직임 비율을 최소 24시간 측정한 후 보존 기간을 확정한다.

## 성능 검증 기준

- 4채널 녹화 중 VOD 1세션을 열어 Pi CPU, 메모리, 디스크 읽기·쓰기, 네트워크, 온도를 측정한다.
- VOD 경로에 decoder/encoder 요소가 생성되지 않는지 GStreamer 파이프라인과 로그로 확인한다.
- 60초 파일 경계에서 영상 끊김, timestamp 역행, 프레임 손실이 없어야 한다.
- seek 후 첫 화면이 설정한 허용 오차 안에 표시되어야 한다. 허용 오차는 CCTV GOP 확인 후 결정한다.

## 근거 자료

- [GStreamer splitmuxsink](https://gstreamer.freedesktop.org/documentation/multifile/splitmuxsink.html)
- [GStreamer splitmuxsrc](https://gstreamer.freedesktop.org/documentation/multifile/splitmuxsrc.html)
- [GStreamer RTSP media seek](https://gstreamer.freedesktop.org/documentation/gst-rtsp-server/rtsp-media.html)

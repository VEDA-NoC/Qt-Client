# Raspberry Pi 다음 마일스톤 전달 프롬프트

현재 Pi 기준선은 단일 채널 연속 녹화 PoC이며 live RTSP/RTSPS 송출은 아직 없다. 다음 작업은 Pi 로드맵 순서에 맞춰 M2 `ChannelIngest`부터 진행한다. 설정 API·이벤트·LPR은 이 작업에서 앞당겨 구현하지 않는다.

아래 내용을 Raspberry Pi 서버 workspace의 별도 Codex 작업에 전달한다.

```text
workspace 루트의 AGENTS.md와 agent-rules/active-worktree-ko.md,
agent-rules/workflow-ko.md를 순서대로 읽어라.

active-worktree-ko.md가 지정한 저장소에서 git status와 최근 commit을 확인하고,
agent-rules/rpi-vms-router-ko.md,
project-docs/rpi-vms/current-status-ko.md,
project-docs/rpi-vms/vms-development-roadmap-ko.md,
project-docs/rpi-vms/project-constraints-ko.md를 읽어라.

이미 Raspberry Pi에서 검증한 단일 채널 녹화 PoC를 다시 만들지 말고 현재 main 기준에서
[RPi-VMS M2] 단일 채널 ChannelIngest를 구현해라.

[이번 작업 범위]
1. RTSP 연결을 프로세스 실행 시 열고 client session과 독립적으로 계속 유지하는
   ChannelIngest ownership 구조를 만든다.
2. RTP depay/codec parse 이후 tee로 recording branch와 live branch를 분리한다.
3. recording queue는 frame을 drop하지 않고 기존 MP4 segment와 SQLite index 동작을
   회귀 없이 유지한다.
4. live queue만 leaky/latest 정책을 적용할 수 있게 하되, 이번 M2에서는 외부 RTSP/RTSPS
   server를 완성하지 않는다. M3가 구독할 명확한 interface와 test sink 경계를 만든다.
5. 첫 buffer, reconnect, pipeline error/EOS, live drop 상태를 구분해 기록한다.
6. camera_channel=0..3은 카메라 RTSP/SUNAPI 내부 namespace로만 유지하고,
   VMS/DB/Qt/storage/public RTSP path는 channel_id=1..4를 사용한다.
   기본 mapping은 camera_channel 0 -> channel_id 1이다.
7. password, camera URI userinfo, TLS key를 source나 로그에 노출하지 않는다.

[후속 단계에서 유지할 확정 요구사항]
- M3 public live path는 /ch1 .. /ch4로 제공한다.
- Pi에서 영상 디코딩/재인코딩을 기본으로 하지 않는다.
- 녹화 메인스트림은 2592x1520, 최대 30 fps다.
- 실시간은 향후 1080p substream을 사용한다.
- 움직임 녹화는 향후 사전 5초, 마지막 움직임 후 10초다.
- Qt-Pi control은 향후 TCP/TLS와 4-byte big-endian length + UTF-8 JSON을 사용한다.
- 설정 원본은 향후 Pi가 소유하며 config revision 충돌 검출과 atomic write를 사용한다.
- SUNAPI CGI 2.6.6과 참고 문서 2.6.8은 우선 호환으로 가정하고 문제 발생 시
  모델/버전 차이를 진단한다.
- 카메라 LPR, 차량 분류 provider, STM32, playback/control은 이번 M2 범위가 아니다.

[검증과 인계]
- 수정 파일만 format하고 기존 단일 채널 녹화 unit/CLI/CI 회귀 검증을 수행한다.
- 가능하면 실제 Pi + camera + 현재 ext4 시험 저장소에서 segment와 DB complete row를
  확인한다. 장치가 없으면 실행하지 않은 검증으로 명시한다.
- recording branch가 live branch의 느린 consumer 때문에 막히지 않는 구조적 근거와
  테스트 결과를 제시한다.
- 의미 있는 구현·검증 후 project-docs/rpi-vms/current-status-ko.md를 사실에 맞게 갱신한다.
- 수정 파일, Pi 반영 여부, 정확한 build/test/실행 명령, 정상 핵심 출력,
  실패 시 로그, 미검증 항목을 인계한다.

다른 task의 변경을 수정·stage·commit하거나 되돌리지 마라.
```

## 이후 순서

1. M2 단일 채널 `ChannelIngest`
2. M3 shared live RTSP/RTSPS server (`/ch1`)
3. M4 4채널 `ChannelManager`와 `/ch1..ch4`
4. M5 1080p live substream
5. M7 SUNAPI 움직임·LPR event와 5초/10초 event recording
6. M8 playback·export·control plane

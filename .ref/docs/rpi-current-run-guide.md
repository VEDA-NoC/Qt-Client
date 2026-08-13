# 현재 Raspberry Pi 녹화 PoC 실행 안내

## 현재 기능

활성 기준은 Pi workspace의 `repos/rpi-vms` main `e163e824`다. 현재 `app`은 카메라 한 채널을 RTSP over TCP로 받아 MP4 segment와 SQLite index로 저장한다.

현재 구현하지 않은 기능:

- Qt에 제공할 live RTSP/RTSPS server
- recording/live `tee`와 shared `ChannelIngest`
- 4채널 `ChannelManager`
- 움직임 event gate, playback, control API

따라서 이 `app`을 실행해도 Qt의 `/ch1..ch4`에 영상이 송출되지는 않는다.

## 채널 번호

- `camera_channel=0..3`: Hanwha 카메라 내부 RTSP/SUNAPI namespace
- `channel_id=1..4`: VMS, DB, Qt, 저장 경로, 향후 public RTSP path
- 기본 mapping: 카메라 `0` → VMS `1`

## 빌드

- 환경: Raspberry Pi
- 관리자 권한: package 설치에만 필요, 빌드에는 불필요
- 시작 경로: `~/rpi-vms`

```bash
make clean
make
./app --help
```

## 녹화 실행

외장 ext4 시험 저장소가 `/mnt/vms-storage`에 실제 mount된 경우에만 실행한다. HDD와 시험 USB가 모두 없다면 SD카드 오기록을 피하기 위해 실행하지 않는다.

```bash
findmnt -T /mnt/vms-storage
read -rsp 'Camera password: ' CAMERA_PASSWORD; echo
./app \
  --camera-host CAMERA_IP \
  --camera-port 554 \
  --camera-user USER \
  --camera-password "$CAMERA_PASSWORD" \
  --camera-path-template '/{camera_channel}/profile2/media.smp' \
  --camera-channel 0 \
  --storage-root /mnt/vms-storage \
  --channel-id 1 \
  --codec h264 \
  --segment-seconds 60 \
  --require-storage-mount \
  --log-level info
unset CAMERA_PASSWORD
```

현재 CLI는 비밀번호를 argv로 전달하므로 실행 중 같은 장치의 process 목록에서 보일 가능성이 있다. source나 shell history에 직접 적지 말고, 후속 config/secret 작업에서 protected credential 입력으로 교체한다.

## 정상 결과

약 65~75초 실행 후 `Ctrl+C`로 종료한다.

핵심 로그 형태:

```text
[storage] state=ready mount_point=yes ...
[channel] channel_id=1 camera_channel=0 storage=/mnt/vms-storage/recordings/ch1
[gst] recording started
[gst] eos
[gst] recording stopped
```

파일과 DB 확인:

```bash
find /mnt/vms-storage/recordings/ch1 -maxdepth 1 -type f -name 'ch1_*.mp4' -printf '%TY-%Tm-%Td %TH:%TM:%TS %s %p\n' | sort
sqlite3 /mnt/vms-storage/index/media.db \
  'SELECT channel_id, start_wall_time_utc, end_wall_time_utc, file_path, size_bytes, complete FROM recording_segments ORDER BY id DESC LIMIT 5;'
```

정상 예상값은 `channel_id=1`, `complete=1`, 0 byte가 아닌 MP4 파일이다.

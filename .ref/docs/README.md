# VEDA VMS 문서 안내

이 문서는 프로젝트 문서의 출처와 성격을 구분하기 위한 색인이다. 논의 중인 제안은 확정 사양으로 취급하지 않는다.

**현재 상태와 다음 작업의 진입점은 [Qt 다음 작업 시작 프롬프트](qt-next-task-prompt.md)다** (2026-08-25 기준).
Pi 쪽 상태는 `rtsps-codex-hanwha-rtsp-raspberry-pi/project-docs/rpi-vms/current-status-ko.md`,
STM 펌웨어 상태는 `stm32-ev-firmware/STM_Sensor.md`에 있다.

## 전역 규칙

- 기본 응답과 프로젝트 문서는 한국어로 작성한다.
- 불확실한 장치 동작, 라이선스, 법령 및 버전별 API는 확인된 근거와 미확인 항목을 구분한다.
- 사용자가 직접 빌드·실행·실기 테스트한다. Codex는 명시적 요청이 없으면 직접
  빌드하거나 프로그램을 실행하지 않고 코드, 정확한 테스트 방법과 예상 결과만
  제공한다.
- 다른 작업의 파일은 임의로 수정하거나 되돌리지 않는다.
- 프로젝트 자동 작업 규칙의 진입점은 루트 `AGENTS.md`이다.

## 전역 스킬

| 스킬 | 이 프로젝트에서의 적용 범위 |
|---|---|
| `development-workflow` | 코드·문서 변경 범위 확인, 검증, 사용자 인계 형식 |
| `user-environment` | Raspberry Pi/Linux 환경을 전역 값이 아닌 프로젝트 문서에만 기록 |
| `pdf` | 브랜드 가이드 PDF의 텍스트 추출과 렌더링 검증 |
| `imagegen` | 구현 전 검토용 UI 콘셉트 이미지 생성 |

## 프로젝트 규칙

- Qt 클라이언트의 현재 RTSPS 4채널 수신 구현을 M1 기준선으로 유지한다.
- 우선 대상은 Windows 데스크톱, 기준 해상도는 1920x1080이다.
- UI는 Qt Widgets와 C++를 유지하고, 공통 디자인 토큰과 QSS로 테마를 분리한다.
- 실시간 영상은 카메라 서브스트림 1080p, 녹화는 메인스트림 `2592x1520`·최대 30 fps를 사용한다.
- Raspberry Pi에서는 영상 디코딩·재인코딩을 하지 않는 구성을 우선한다.
- 설계가 합의된 의사결정은 `확정`, 외부 승인·실장 검증이 남은 항목은 `부분 확정` 또는 `검증 대기`로 관리한다.
- UI·UX 문제점이나 고민 사항은 코드부터 수정하지 않는다. 원인·영향·권장 동작과
  선택지를 먼저 제시하고 사용자가 계획을 승인한 뒤 구현한다.

## 프로젝트 내용

- [마일스톤](milestone.md)
- [Raspberry Pi 환경](raspberry-pi-environment.md)
- [의사결정 문서 색인](decisions/README.md)
- [Raspberry Pi 다음 마일스톤 전달 프롬프트](rpi-next-milestone-prompt.md)
- [현재 Raspberry Pi 녹화 PoC 실행 안내](rpi-current-run-guide.md)
- [Qt M3 UI 셸 빌드·테스트 계획](qt-m3-test-plan.md)
- [Qt M6 녹화 재생 빌드·테스트 계획](qt-m6-playback-test-plan.md)
- [Qt 저장소 상태 API 테스트 계획](qt-storage-status-test-plan.md)
- [Qt 주차 구역 편집기 구현 및 실기 검증 계획](qt-parking-zone-editor-test-plan.md)
- [Qt 다음 작업 시작 프롬프트](qt-next-task-prompt.md)
- [Antigravity 작업 인계 — 채널 재연결·주차 구역 편집기·하단 상태 UI](antigravity-handoff-2026-08-04.md)
- [UI 아이콘 디자인 작업 순서](ui-icon-workflow.md)
- 기존 시스템 흐름도: `.ref/flow/`
- 기존 UI 와이어프레임: `.ref/ui/*.jpg`
- 검토 승인 방향의 UI 콘셉트: `.ref/ui/concepts-v1/`
- 브랜드 자료: `.ref/brand/`

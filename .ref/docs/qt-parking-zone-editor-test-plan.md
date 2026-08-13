# Qt 주차 구역 편집기 구현 및 실기 검증 계획

- 상태: Qt 구현 완료 / 사용자 빌드·Pi 연동 검증 대기
- 작성 기준일: 2026-08-04

## 구현 범위

- 설정 화면의 `주차 구역 관리 열기`에서 전용 전체 페이지로 진입한다.
- 채널별 현재 라이브 프레임을 고정 기준 이미지로 사용하고, 실제 이미지 영역 안에서만
  정규화 좌표 `0.0..1.0`의 점을 받는다. 구역 하나는 정확히 4점이다.
- 구역 이름, `general`/`ev_only`, 활성 여부, STM `device_uid`와
  `sensor_zone_id`를 함께 편집한다.
- 로컬 변경, Draft 저장, Pi 검증, Apply/readback을 분리한다.
- Apply마다 새 `Idempotency-Key`를 만들며 `applying`은 job API를 1초마다 조회한다.
  `succeeded`만 활성 적용 완료로 표시한다. `failed`와 `unavailable`은 활성 설정을
  바꾸지 않은 실패 상태로 표시한다.
- 편집 중 설정·다른 메뉴·다른 채널·앱 종료로 이동하면 기본 동작이 `계속 편집`인
  경고 창으로 막는다.
- polygon 점 추가/이동, 구역 추가/삭제에는 실행 취소/다시 실행 기록을 제공한다.
- 기존 채널별 라이브 재연결의 중복 `RetryConnecting` 상태 로그를 억제했다.

## 사용 API

| 동작 | 메서드와 경로 |
|---|---|
| 활성 설정 조회 | `GET /api/v1/parking-spaces?channel_id=...` |
| Draft 생성 | `POST /api/v1/parking-space-drafts` |
| Draft 전체 교체 | `PUT /api/v1/parking-space-drafts/{draft_id}` |
| 검증 | `POST /api/v1/parking-space-drafts/{draft_id}/validate` |
| 적용 | `POST /api/v1/parking-space-drafts/{draft_id}/apply` |
| 적용 상태 조회 | `GET /api/v1/parking-space-apply-jobs/{job_id}` |

모든 요청은 기존 Control HTTPS 인증서 pin과 Bearer token을 재사용한다. `409`는
`base_version_conflict` 또는 idempotency 충돌로 취급하고 활성 설정 재조회 안내를
표시한다.

## 현재 Pi API 의존성과 제한

1. 활성 설정이 없는 `active_version=0` 응답은 `geometry_id=null`이다. 별도의 카메라
   geometry 조회 API가 아직 없으므로 편집기에 카메라 측에서 받은 Geometry ID를 직접
   입력해야 최초 Draft를 저장할 수 있다. 임의 기본값은 만들지 않는다.
2. STM 장치/센서 구역 목록 API가 없어 두 식별자는 원문 입력으로 제공한다. 목록 API가
   확정되면 이 입력을 검색 가능한 선택 컨트롤로 교체한다.
3. 현재 Pi 카메라 어댑터가 없으면 Apply는 `202`와 함께
   `state=unavailable`, `camera_parking_target_unavailable`을 반환한다. Qt는 이를 성공으로
   표시하지 않는다.
4. 활성 설정 조회 API는 저장된 미적용 Draft의 `draft_id`와 내용을 반환하지 않는다.
   앱 재진입 후 기존 Draft 이어쓰기는 현재 계약으로 불가능하다.

## 사용자 빌드·실기 검증

1. 기존 방식으로 빌드한 뒤 Control API를 적용하고 인증 완료 상태를 확인한다.
2. 설정 → 주차 구역 관리에서 CH1~CH4 기준 이미지가 채널별 현재 라이브 프레임으로
   보이는지 확인한다. 영상 바깥 검은 여백 클릭은 점을 만들면 안 된다.
3. 새 구역을 추가하고 점 4개를 지정한다. 다섯 번째 점이 생기지 않고 기존 점 drag만
   가능한지, 실행 취소/다시 실행이 동작하는지 확인한다.
4. `geometry_id`, 이름, 유형, STM 식별자를 채우기 전 Draft 저장이 비활성인지 확인한다.
5. Draft 저장 로그에서 `parking.draft.create`가 시작/완료되고 응답 `draft_version`이
   표시되는지 확인한다. 추가 변경 후 저장하면 `parking.draft.update`와 증가한 버전을
   확인한다.
6. 검증 실패 시 `validation_errors[].code`가 표시되고 Apply가 비활성인지 확인한다.
   검증 성공 뒤에만 Apply가 활성화되어야 한다.
7. 현재 카메라 어댑터 미완성 환경에서는 Apply가 `unavailable`로 끝나며 UI가
   “활성 설정은 변경되지 않았다”고 표시하는지 확인한다. 이후 어댑터가 구현되면
   `applying` polling과 최종 `succeeded`, 증가한 `active_version`을 확인한다.
8. 편집 후 메뉴, 설정 뒤로가기, 다른 채널, 창 닫기를 시도한다. `계속 편집`이 기본이고
   해당 버튼을 누르면 이동이 취소되는지 확인한다. `변경 폐기 후 이동`만 이동해야 한다.
9. Pi에서 다른 관리자가 먼저 활성 버전을 바꾼 뒤 저장/적용하여 HTTP 409와 재조회
   안내가 표시되는지 확인한다.
10. 일부 라이브 채널을 끊었다 복구하고 각 채널만 독립 재연결되는지, 같은 상태의
    `RetryConnecting` 로그가 연속 중복 출력되지 않는지 확인한다.

프로젝트 규칙에 따라 Codex는 빌드·실행·GUI 검증을 수행하지 않았다.

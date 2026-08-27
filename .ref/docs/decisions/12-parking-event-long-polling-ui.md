# 12. 주차·EV 이벤트 Long Polling 수신 및 1.png 기반 UI 구현 계획

- 상태: 확정 (구현 진행)
- 작성일: 2026-08-05

## 1. 개요

Pi의 Cursor 기반 Long Polling API (`GET /api/v1/events`)를 통해 실시간 입출차, EV 판정 및 비전기차 점유 위반 이벤트를 수신하고, 이를 **실시간 모니터링 탭 우측 패널 (`.ref/ui/concepts-v1/1.png` 디자인 기준)** 및 **이벤트 탭 관리 리스트**에 반영한다.

---

## 2. API 명세 및 수신 메커니즘 (`GET /api/v1/events`)

### 2.1 API Spec
- **Path**: `GET /api/v1/events?after_id=<next_after_id>&wait_ms=25000&limit=50`
- **Headers**: `Authorization: Bearer <token>`
- **Response**: `200 OK` (JSON)
  - `schema_version`, `server_time_utc_ms`, `next_after_id`
  - `events`: Array of Event DTO (`event_id`, `severity`, `correlation_id`, `payload` 등)

### 2.2 클라이언트 수신 루프
1. 시작 시 `after_id=0`으로 요청 전송.
2. 응답 수신 시 `next_after_id` 저장 후 UI 이벤트 처리.
3. 곧바로 `after_id=<next_after_id>`로 다음 Long Polling 재요청 전송.
4. 네트워크 타임아웃/오류 발생 시 1~2초 지연(Backoff) 후 재요청하여 이벤트 수신 복구.

---

## 3. UI/UX 구현 기준 (`.ref/ui/concepts-v1/1.png` 스타일)

### 3.1 실시간 모니터링 탭 — 우측 최근 이벤트 패널
- **패널 타이틀**: `실시간 이벤트` (상단 옵션 및 수신 상태 아이콘)
- **카드 디자인 구조 (`1.png` 시안 표준)**:
  - **`CRITICAL` (적색)**: 긴급 배지, 화재/중대 경보, **`[대응 화면 열기]`** 대형 액션 버튼 포함
  - **`WARNING` (주황색)**: 경고 배지, 비전기차 충전구역 점유 위반, 차량 번호판 및 내연기관 태그
  - **`INFO` (녹색)**: 정보 배지, EV 차량 입차/출차 완료, 차량 번호판 및 EV 태그
- **하단 요소**: `더 보기 ∨` 버튼으로 이력 확장/접기

### 3.2 이벤트 탭 — 전체 이벤트 관리 리스트
- **상단 필터**: 심각도(전체/INFO/WARNING/CRITICAL), 채널(전체/CH1~CH4), ACK 상태(전체/미확인/확인됨)
- **테이블 리스트**: `발생 시각` | `심각도` | `구역/채널` | `이벤트 내용` | `차량 정보` | `상태` | `확인(ACK)`

---

## 4. 모듈 구조 및 C++ 클래스 설계

```
[NEW] parking_event_types.h        : Event/Session DTO 및 JSON 파싱 함수 (header-only / no throw)
[NEW] parking_event_api_client.h/.cpp : GET /api/v1/events Long Polling QNetworkAccessManager 연동
[NEW] parking_event_store.h/.cpp   : 메모리 내 세션/이벤트 관리 및 Qt Signal 방출
[NEW] parking_event_card_widget.h/.cpp : 1.png 디자인 기반 개별 이벤트 카드 위젯
[MODIFY] main_window.h/.cpp        : 우측 실시간 이벤트 패널 및 이벤트 탭 TableWidget 연결
[MODIFY] CMakeLists.txt            : 신규 소스 파일 추가
```

### 엄격한 C++ / Qt 안정성 원칙:
1. **메모리 안전성**: 모든 UI 커스텀 위젯은 QObject 부모-자식 소유권(Parent ownership)을 엄격히 준수하여 댕글링 포인터 및 메모리 누수를 완전히 방지한다.
2. **QNetworkReply 안전성**: `reply->deleteLater()`를 항상 사용하며, `reply->error()` 발생 시 NULL Dereference가 발생하지 않도록 비동기 안전 검사를 필수 수행한다.
3. **JSON 파싱 안전성**: `QJsonObject`, `QJsonValue` 접근 시 `.isObject()`, `.isString()` 타입 검증 후 읽기를 수행하여 Crash를 방지한다.
4. **C++17 / Qt 6.11 호환성**: 빌드 실패나 미정의 심볼이 전혀 발생하지 않도록 헤더 포원드 선언 및 인클루드를 철저히 검증한다.

---

## 5. 주차 위반 표시 (2026-08-25 추가·확정)

`1.png` 시안의 `WARNING` 카드(비전기차 충전구역 점유 위반)가 실제로는 한 번도 뜨지 않는
것을 확인하고 그 경로를 살렸다.

### 5.1 왜 안 떴는가

1. **서버가 카메라 이벤트의 `severity`를 비워서 보낸다.** rtsps `include/rtsps/event.h:44`에
   "camera sources currently leave this empty"로 명시돼 있다. Qt의 `parseSeverity("")`는
   `INFO`를 돌려주므로 위반 이벤트도 초록 정보 카드가 됐다.
2. **payload 키 이름이 위 §2.1 예시와 다르다.** 서버가 실제로 보내는 것은
   `masked_plate` / `ev_state` / `occupied` / `space_id`이고, 이 문서와
   `outputs/qt-event-long-polling-api-guide.md`가 적어 둔 `plate` / `ev` / `state` /
   `space_label`이 아니다. 그래서 `violation` 하나만 파싱되고 나머지는 전부 빈 값이었다.

**서버 구현이 계약이다.** 위 §2.1의 예시 JSON은 초안이며, 실제 필드는
`parking_camera_observation.cpp`의 `parking_transition_to_event()`가 만든다.

### 5.2 확정한 동작

| 항목 | 결정 |
|---|---|
| 수정 범위 | **Qt만.** 서버 severity는 건드리지 않고 Qt가 표시할 때만 등급을 올린다. 나중에 Pi가 `WARNING`을 채워도 결과가 같다 |
| 판정 | `source_type != "stm" && payload.violation && phase != Ended` (`ParkingEventItem::isParkingViolation()`) |
| 표시 등급 | `effectiveSeverity()` — 위반이면 `WARNING`으로 승격. 서버 원본 `severity`는 보존 |
| 카드 | 아이콘 `⚠️`, 제목 `주차 위반`, 배지 `위반`(앰버), 상세 `전기차 전용 구역에 비전기차 주차 · N분 경과`, 보조 `내연기관 · 번호판 ****` |
| 카드 중복 | 전이마다 1장 유지. `correlation_id` 기준 병합은 하지 않는다 (시간순 로그 성격 유지) |
| 상단 고정 | **하지 않는다.** 고정은 즉시 현장 대응이 필요한 화재 전용으로 남기고, 위반은 앰버 테두리·배경으로만 구분한다 |
| 구역 표시 | `space_id`를 그대로 쓴다. 주차 구역 설정에서 사람이 읽는 라벨을 조회하지 않는다 |
| 번호판 | 원문은 Pi가 `****`로 지우고 보내므로 화면에도 마스킹만 표시한다(결정 08과 일치) |

### 5.3 함께 고친 것

- **`ended`(출차) 전이가 `violation=true`를 물고 온다.** Pi의 ended 전이는 직전 관측을
  복사한 뒤 `occupied`/`plate`만 지우고 `violation`·`ev_state`는 그대로 둔다
  (`parking_camera_observation.cpp:485-490`). 그래서 위반 차량이 나가면 "출차 완료"여야
  할 이벤트가 위반으로 뜬다 — 판정에서 `Ended`를 제외해 막았다. 같은 이유로 출차 카드에는
  차량/EV 요약을 붙이지 않는다.
- **이벤트 탭 필터가 동작하지 않던 결함.** `refreshEventsTable()`이 "행이 비었으면 전체
  재구성, 아니면 최신 1건만 prepend"로 갈라져 있어, 행이 쌓인 뒤에는 필터를 바꿔도 목록이
  그대로였고 한 배치에 N건이 와도 1건만 들어갔다. 매번 전체 재구성으로 바꾸고
  (행 상한 500, 스크롤 위치 보존) 필터 판정을 `eventPassesTableFilter()`로 분리했다.
- 카드와 테이블이 같은 문구를 쓰도록 표시 헬퍼를 `parking_event_text` 네임스페이스로 공유한다
  (`vehicleSummary` / `parkedDuration` / `detailLine`).

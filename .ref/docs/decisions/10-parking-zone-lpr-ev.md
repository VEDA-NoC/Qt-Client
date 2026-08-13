# 10. 주차 구역·번호판·전기차 판정

- 상태: 부분 확정 (외부 API 이용 승인·카메라 LPR 검증 대기)

## 확정 구조

카메라 영상 좌표에 주차 구역을 polygon으로 지정하고 각 구역을 STM32 구역 제어기와 연결한다. 차량 번호판 인식은 카메라 앱을 우선하며, Raspberry Pi가 인식 결과와 점유 이벤트를 결합하고 외부 차량 분류 어댑터를 호출한다. Qt는 외부 서비스나 카메라를 직접 호출하지 않고 Pi가 확정한 상태만 표시한다.

```text
Camera LPR / occupancy
        ↓
Raspberry Pi zone correlator ── vehicle-classification adapter
        ↓                              ↓
zone event + EV state          approved external API
        ↓
Qt event / live overlay / operator action
```

## 주차 구역 설정 모델

| 필드 | 의미 |
|---|---|
| `zone_id` | 서버가 발급하는 안정적인 구역 ID |
| `display_name` | 운영자에게 표시할 구역명 |
| `camera_id`, `channel_id` | 구역을 관찰하는 카메라·채널 |
| `polygon` | 영상 크기에 독립적인 0.0~1.0 정규화 좌표 배열 |
| `lpr_roi` | 선택적 번호판 인식 관심 영역 |
| `controller_id` | 연결된 STM32 구역 제어기 |
| `zone_type` | `GENERAL`, `CHARGING`, `FIRE_RESPONSE` 등 |
| `enabled`, `config_revision` | 적용 여부와 설정 충돌 검출 값 |

- 한 카메라는 여러 구역을 관찰할 수 있다.
- 충전 구역은 기본적으로 STM32 한 대와 연결한다.
- 카메라 이동·줌·교체 뒤에는 polygon calibration이 필요하며 변경 이력을 남긴다.
- 설정 원본은 Pi에 저장하고 Qt 관리자 화면에서 `config.get/config.update`로 편집한다.

## 차량 분류 상태

| 상태 | Qt 동작 |
|---|---|
| `EV_CONFIRMED` | 충전구역 점유 허용, 정상 상태 표시 |
| `NON_EV_CONFIRMED` | 비전기차 충전구역 점유 WARNING 생성 |
| `PLATE_UNREADABLE` | 재인식 대기 후 분류 실패 WARNING |
| `LOOKUP_FAILED` | 외부 조회 실패 표시, 자동 허용하지 않음 |
| `UNKNOWN` | 판정 전 또는 근거 부족 상태를 명시 |

같은 번호판의 중복 조회를 줄이기 위해 서버에 TTL cache를 둘 수 있으나, 저장 목적·보존 기간·암호화·접근 권한을 개인정보 정책과 함께 정한다. Qt 로그에는 전체 번호판을 남기지 않는다.

## 자동차365 연계 제약

자동차365 웹 화면을 자동화 도구로 수집하는 방식은 사용하지 않는다. 국토교통부 자동차종합정보 API는 이용 신청이 필요하고 일부 항목은 차량번호, 소유자명 및 제3자 제공 동의 조건이 안내되어 있으므로, `번호판만으로 연료 종류 조회`가 계약상 제공되는지는 운영 주체가 공식 승인 과정에서 확인해야 한다.

승인 전에는 `VehicleClassificationProvider` 서버 인터페이스와 테스트용 provider만 구현한다. 실제 API 자격 증명은 Pi의 secret 저장소에서 관리하고 Qt·카메라·로그에 노출하지 않는다.

## 외부 검증 필요

- 실제 카메라 모델의 LPR 앱 지원, 이벤트 필드와 정확도
- 번호판과 구역 점유 이벤트의 timestamp·track ID 연결 방식
- 자동차365 또는 승인된 대체 데이터 제공자의 차량 연료 분류 항목, 동의·보존 조건
- 미인식·조회 실패 차량의 현장 운영 절차와 관리자 예외 허용 시간

## 참고

- [국토교통부 자동차종합정보 API서비스](https://www.data.go.kr/data/15071233/openapi.do)
- [자동차365 자동차종합정보개방 서비스 안내](https://www.car365.go.kr/ccpt/comm/ntcn/ntcDetailView.do?bbsId=123&pstId=5550)
- [자동차365 자동화 도구 접근 제한 안내](https://www.car365.go.kr/ccpt/comm/ntcn/ntcDetailView.do?_menuId=M640101000&bbsId=123&pstId=7165)

# UI 아이콘 디자인 작업 순서

## 권장 순서

1. 앱 아이콘 후보로 형태, 선 굵기, 모서리 반경, Navy/Orange 비율을 정한다.
2. 선택된 시각 언어로 sidebar용 5개 아이콘을 한 배치로 제작한다.
3. 16/20/24 px에서 선이 뭉개지지 않는지 함께 검토한다.
4. 선택·비선택·hover 상태를 QSS semantic color로 연결한다.
5. 이후 toolbar, 이벤트 심각도, 장치 종류 아이콘으로 같은 규칙을 확장한다.

앱 아이콘 한 개를 먼저 확정하고 sidebar는 일괄 작업하는 순서가 적절하다. sidebar를 Qt 기본 아이콘이나 서로 다른 출처의 아이콘으로 임시 채우면 선 굵기와 optical size가 달라져 최종 교체 비용이 커진다.

## sidebar 첫 배치

| 메뉴 | 기본 모티프 |
|---|---|
| 실시간 모니터링 | 2x2 화면 grid 또는 live lens |
| 녹화 재생 | timeline + play |
| 이벤트 | alert pulse |
| 장치 및 제어 | controller/module, 센서는 제외 |
| 설정 | 단순 sliders |

## 공통 규칙 초안

- 기본 canvas: 24x24, 실제 도형 optical box 약 20x20
- 선 굵기: 1.75~2 px, round cap/join
- 비선택: sidebar light gray
- hover: white
- 선택: white icon + Orange 좌측 indicator
- 아이콘 전체를 Orange로 칠하지 않아 경고·위험 의미와 혼동하지 않는다.
- 센서 아이콘은 sidebar에 두지 않고 STM32 상세 화면용 장치 icon set에서 별도로 만든다.

현재 `.ref/ui/icon-concepts-v2/`는 검토 후보이며 빌드 리소스가 아니다. 후보를 고른 뒤 SVG를 다듬고, 그때 PNG/ICO 파생 파일과 Windows resource를 연결한다.

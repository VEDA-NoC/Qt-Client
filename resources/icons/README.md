# VEDA VMS 아이콘 자산

## 편집 원본

- 앱 아이콘 C안: `veda-vms-app-icon-v2.svg`
- 사이드바 아이콘: `sidebar/*.svg`

SVG가 편집 원본이다. 색상, 선 굵기, 모서리 또는 도형을 바꿀 때에는 SVG를 먼저 수정한다.

## 빌드용 파생 파일

- `veda-vms-app-icon-v2.png`: Qt 창 아이콘 및 미리보기용 1024 px PNG
- `veda-vms-app-icon-v2.ico`: Windows 실행 파일 및 작업 표시줄용 다중 해상도 ICO
- `sidebar/*.png`: 선택·Hover 상태용 96 px 흰색 투명 PNG
- `sidebar/*-muted.png`: 비선택 상태용 96 px 회색 투명 PNG

파생 파일을 직접 디자인 원본으로 편집하지 않는다. 앱 아이콘 SVG를 바꾼 경우 먼저 같은 이름의 PNG로 1024x1024 내보낸 뒤 아래 스크립트를 실행한다.

```powershell
python tools/generate_icon_derivatives.py
```

스크립트는 ICO의 16/20/24/32/40/48/64/128/256 px 레이어와 사이드바 PNG, 검토용 미리보기를 다시 만든다. 사이드바 도형을 변경할 때에는 SVG와 스크립트의 해당 `draw_*` 함수도 함께 갱신한다.

## 적용 위치

- `app_icon.rc`: Windows 실행 파일 아이콘
- `main.cpp`: Qt 창 및 작업 표시줄 런타임 아이콘
- `resources.qrc`: 앱 및 사이드바 이미지 포함
- `main_window.cpp`: 메뉴별 아이콘 배치와 선택/비선택 색상 처리

현재 앱 아이콘 파일명에 `v2`를 유지하여 이후 시안 교체 시 이전 자산과 구분할 수 있게 했다.

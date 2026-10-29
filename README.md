# DaBa — 가볍고 빠른 동영상 플레이어

순수 **C 언어(Win32 API)** 로 작성한 Windows용 동영상 플레이어입니다.
재생 엔진은 [mpv](https://mpv.io)(libmpv)를 사용해 MP4/MKV/AVI/HEVC/AV1/자막(SMI/SRT/ASS) 등 거의 모든 형식을 재생합니다.
UI는 팟플레이어 스타일의 다크 테마(커스텀 타이틀바·컨트롤바·메뉴)로 직접 그렸습니다.

## 실행
`bin\DaBa.exe` 실행 (같은 폴더에 `libmpv-2.dll` 필요). 파일을 창에 끌어다 놓거나, exe에 파일을 드래그하거나, `연결 프로그램`으로 지정해서 쓰면 됩니다.

## 폴더 구성
| 경로 | 내용 |
|---|---|
| `src/daba.c` | 플레이어 전체 소스 (단일 파일) |
| `src/daba.rc`, `daba.manifest`, `daba.ico` | 아이콘·매니페스트(Per-Monitor DPI)·버전 정보 |
| `src/mpv/client.h` | libmpv API 헤더 (ISC 라이선스) |
| `build.bat` | 빌드 스크립트 (MSVC 또는 MinGW-w64 자동 감지) |
| `bin/` | 빌드 결과 `DaBa.exe` + `libmpv-2.dll` |

## 빌드
- **Visual Studio**: "x64 Native Tools Command Prompt for VS" 에서 `build.bat`
- **MinGW-w64** (MSYS2 / w64devkit): gcc 가 PATH 에 있으면 `build.bat`
- libmpv 는 실행 시 동적으로 불러오므로 .lib 파일 없이 빌드됩니다.
  DLL 업데이트: https://github.com/shinchiro/mpv-winbuild-cmake/releases 의 `mpv-dev-x86_64-*.7z` 안 `libmpv-2.dll`

## 주요 기능
- 다크 타이틀바: 메뉴(DaBa ▾), 형식(MP4), 파일명, 항상 위/최소화/최대화/전체 화면/닫기 (Windows 11 스냅 레이아웃 지원)
- 컨트롤바: 탐색바(호버 시간 표시, 챕터·구간반복 표시), 볼륨, 재생/정지/이전/다음/열기, 시간(클릭 시 남은 시간), H/W·코덱·오디오·채널 배지, 재생목록/설정/메뉴
- 우클릭 메뉴: 열기 / 재생 / 자막 / 영상 / 소리 / 환경 / 화면 비율 / 화면 크기 / 전체·꽉찬 화면 / 재생목록 / 재생 정보
- 같은 폴더 파일 자동 재생목록(자연 정렬), 반복·셔플, 최근 파일, 재생 위치 기억, 파일 열 때 창 크기 자동 맞춤
- 자막 드래그 추가, 자막 싱크/크기/위치, 소리 싱크·정규화, 영상 회전·반전·명도/대비/채도/색상, 화면 캡처(사진\DaBa)
- 전체 화면에서 마우스를 아래로 내리면 컨트롤바 표시, 재생 중 커서 자동 숨김, 마우스 휠 볼륨, 더블클릭 전체 화면

## 단축키 (팟플레이어와 유사)
| 키 | 기능 | 키 | 기능 |
|---|---|---|---|
| Space | 재생/일시 중지 | Enter / Ctrl+Enter | 전체 화면 / 꽉찬 화면 |
| ← → | 5초 이동 (Ctrl 30초, Alt 1분) | ↑ ↓ / M | 볼륨 / 음소거 |
| PgUp PgDn | 이전/다음 파일 | Ctrl+PgUp/PgDn | 이전/다음 챕터 |
| C X Z | 속도 빠르게/느리게/기본 | D F | 이전/다음 프레임 |
| B | 구간 반복 (시작→끝→해제) | , . / | 자막 싱크 −/+/초기화 |
| L / A | 자막 / 소리 트랙 전환 | Alt+H | 자막 보이기 |
| Q W E R T Y U I O | 영상 속성 초기화·명도·대비·채도·색상 | - = | 소리 싱크 |
| Ctrl+S / K | 화면 캡처 / 영상만 캡처 | Alt+1~4, Alt+0 | 창 크기 0.5~2배 / 맞춤 |
| F2 / F3·Ctrl+O | 폴더 / 파일 열기 | Ctrl+U / Ctrl+V | 주소 / 클립보드 열기 |
| Alt+O | 자막 열기 | F4 | 닫기 |
| F5 / F6 | 환경 설정 / 재생목록 | Ctrl+F1 / Tab / F1 | 재생 정보 / 통계 / 정보 |

## 설정 파일
`DaBa.ini` 와 `watch_later\`(재생 위치)는 exe 폴더에 저장됩니다 (쓰기 불가 시 `%APPDATA%\DaBa`).

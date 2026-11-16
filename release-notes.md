# DaBa v0.0.2

관리자 권한을 이용한 설치 및 제거 기능을 추가했습니다.

- `install.bat`: UAC 승인 후 Program Files에 모든 사용자용 설치
- 시작 메뉴 바로가기 및 Windows 설치된 앱 항목 등록
- Windows 설정 또는 `uninstall.bat`에서 관리자 권한으로 제거
- 제거 시 개인 설정, 재생 기록 및 사용자가 추가한 파일 보존
- 실행 중인 설치본 감지 및 설치·제거 중단 안내

## 사용 방법

`DaBa-v0.0.2-windows-x64.zip`을 압축 해제한 뒤 `install.bat`을 실행하고 관리자 권한 요청을 승인하세요. 설치 후 시작 메뉴에서 DaBa를 실행할 수 있습니다.

설치 없이 사용하려면 압축을 푼 폴더에서 `DaBa.exe`를 실행하세요. 일반 재생에는 관리자 권한이 필요하지 않습니다.

제거는 Windows 설정 → 앱 → 설치된 앱 → DaBa → 제거 또는 `uninstall.bat`으로 진행합니다.

## 검증

- Windows x64 MinGW 빌드 및 PowerShell 구문 검사
- 관리자 권한으로 수행하는 실제 설치·제거는 미검증

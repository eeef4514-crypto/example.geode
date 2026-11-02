# Dmm Photo Decor v0.1.0 — Windows x64 소스 패키지

PNG/JPG 사진을 Geometry Dash 에디터에서 불러와 기본 게임 오브젝트로 변환하는 Geode 모드의 초기 소스 프로젝트입니다.

## 구현된 초기 기능

- 에디터 화면에 `PHOTO` 버튼 표시
- Windows 파일 선택 창에서 PNG/JPG 불러오기
- 긴 변 기준 최대 48픽셀로 축소
- 64색 팔레트로 양자화
- 투명 픽셀 제외
- 같은 색상의 이웃 픽셀을 직사각형 단위로 병합
- 기본 GD 오브젝트(ID 1)를 생성하고 No Touch / Passable 속성을 설정
- 이미지가 맵에 기본 오브젝트로 남도록 변환하는 방식

## 중요한 현재 상태

- 이 압축 파일은 **소스 프로젝트**이며, 컴파일된 `.geode` 바이너리가 아닙니다.
- 이 실행 환경에는 Geode v6.0.0-alpha.1 SDK와 Windows Geode 빌드 도구가 없어 실제 빌드/게임 내 테스트를 완료하지 못했습니다.
- 특히 생성 오브젝트의 RGB 사용자 색상이 저장 후 재실행/다른 PC에서도 동일하게 유지되는지 검증이 필요합니다. 현재 코드는 `GameObject::m_baseColor`의 custom color와 런타임 색상을 설정하지만, 최종 맵 저장/재로드 및 오브젝트의 색상 직렬화는 게임에서 반드시 테스트해야 합니다.
- 이 첫 프로토타입은 Windows x64 전용입니다. 한 번에 최대 48×48 픽셀, 2,304개 이하의 색상 직사각형 오브젝트를 만듭니다.
- 기본 블록을 사용하되 No Touch 및 Passable로 플레이어 충돌을 방지하도록 설정합니다. 실제 게임에서 충돌/레이어 동작을 검증해야 합니다.

## `.geode` 빌드 방법 (GitHub Actions)

1. GitHub에서 새 저장소를 만듭니다. Public 또는 Private 모두 가능합니다.
2. 이 ZIP을 풀고, 안의 파일과 폴더(`mod.json`, `CMakeLists.txt`, `src`, `.github`)를 저장소 최상위에 올립니다.
3. `main` 또는 `master` 브랜치에 커밋합니다.
4. 저장소의 **Actions** 탭에서 `Build Dmm Photo Decor (Windows)` 작업을 확인합니다.
5. 작업이 성공하면 `geode-build-win` 아티팩트를 다운로드합니다.
6. 아티팩트 안의 `dmm.photodecor.geode` 파일을 Geometry Dash의 `geode/mods` 폴더에 넣고 게임을 다시 실행합니다.

빌드 워크플로는 Geode SDK `6.0.0-alpha.1`을 `mod.json`에서 읽어 사용하고, Geode 공식 `build-geode-mod` 액션으로 Windows x64 패키지를 빌드합니다.

## 로컬 빌드

Geode CLI와 v6.0.0-alpha.1 SDK를 설치한 Windows x64 개발 환경에서:

```powershell
geode sdk install 6.0.0-alpha.1
$env:GEODE_SDK
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

`build` 폴더 아래 생성된 `.geode` 파일을 설치합니다.

## 참고

- `mod.json` 대상 게임 버전은 Windows `2.209`로 설정했습니다.
- 이 버전은 Geode v6 알파용 초기 구현이며, 배포 전 실제 빌드와 테스트가 필요합니다.

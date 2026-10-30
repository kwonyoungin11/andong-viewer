# 안동뷰어 (andong-viewer)

안동시 문화유산 발표용 지도 뷰어. 원래 ka-hgis 저장소 안에서 함께 빌드되던 것을
2026-09-08 에 별도 프로젝트로 분리했다.

## ka-hgis 와의 관계

코드 의존은 끊었다. 예전에는 ka-hgis 의 `ka_core` 를 통째로 링크했지만 실제로 쓰던
것은 `LayerOps` 함수 세 개(`applyCanvasScreenDpi`, `ensureOtfEnabled`,
`setLabelsVisible`)뿐이라, 그 셋만 `src/core/LayerOps.cpp` 로 옮겼다.
`AndongPack` 은 애초에 안동 전용이라 그대로 가져왔다(ka-hgis 쪽에서 참조하는 곳이 없었다).

남은 공통점은 **QGIS/Qt 런타임**뿐이다. `publish-andong.ps1` 은 ka-hgis 의 포터블
런타임이 있으면 재활용하고, 없으면 `make-portable.ps1` 로 새로 만든다.

## 빌드

```powershell
cmake --preset vs                 # 구성 (VS 2022 x64)
cmake --build --preset release    # 빌드
ctest --preset release            # 테스트
```

## 자료

| 경로 | 설명 |
|---|---|
| `data/andong/andong_city.geojson` | 시 경계 |
| `data/andong/andong_mark.png` | 마크 |
| `data/andong/satellite.mbtiles` | 위성 배경 (275MB, git 제외 — `pack-andong-basemap` 으로 생성) |
| `data/theme/andong.qss` | 화면 테마 |

## 배포

```powershell
.\scripts\publish-andong.ps1      # dist\andong-portable 생성
```

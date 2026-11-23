# MW3 SP Mods — 한글패치 + 패드 호환 브리지 & 패드 진동

**Call of Duty: Modern Warfare 3 (2011) 싱글플레이(Steam)** 전용 ASI 플러그인 2종입니다.

| 플러그인 | 하는 일 |
|---|---|
| **MW3KoreanBridge.asi** | 한글패치와 [MW3GamepadFix](https://github.com/TheTronuo/MW3GamepadFix)를 같이 깔면 한글이 사라지는 문제를 해결합니다. 두 모드가 같은 게임 함수를 후킹하려다 충돌하던 것을 순서대로 이어 붙여 줍니다. |
| **MW3Rumble.asi** | PC판에서 빠진 **패드 진동**을 추가합니다. 사격, 근접공격, 폭발(거리 비례), 피격(데미지 비례), 지진·화면 흔들림, 낙하 착지 시 진동합니다. |

> ⚠️ **싱글플레이 전용입니다.** 멀티플레이(iw5mp)에서는 사용하지 마세요. Activision과 무관한 비공식 팬 모드입니다.

---

## 설치 (복붙 3분 컷)

### 0. 준비물 (각 원작자 배포처에서 받으세요)
1. **MW3 한글패치** — 먼저 설치해서 한글이 나오는지 확인
2. **[MW3GamepadFix](https://github.com/TheTronuo/MW3GamepadFix/releases)** — 패드 지원 모드 (0.8.9에서 확인)

### 1. 이 모드 설치
1. [Releases](../../releases)에서 `MW3-SP-Mods-vX.X.zip` 다운로드
2. 압축을 풀고 **안에 있는 파일 전부를** 게임 폴더(`iw5sp.exe`가 있는 곳)에 복사
   - 기본 경로: `C:\Program Files (x86)\Steam\steamapps\common\Call of Duty Modern Warfare 3`
   - `winmm.dll`을 덮어쓸지 물으면 **덮어쓰기** 해도 됩니다 (GamepadFix에 든 것과 같은 Ultimate ASI Loader 9.7.4)
3. 게임 실행 → 끝

설치 후 게임 폴더는 대략 이렇게 됩니다:
```
iw5sp.exe
d3d9.dll                 ← 한글패치
KoreanLocalization.json  ← 한글패치
winmm.dll                ← ASI 로더
MW3GamepadFix.asi / .ini ← 패드 모드
MW3KoreanBridge.asi      ← 이 모드
MW3Rumble.asi / .ini     ← 이 모드
```

> 한글패치만 쓰거나 GamepadFix만 쓰는 경우에도 넣어둬도 문제없습니다. 브리지는 충돌이 없으면 아무것도 하지 않습니다.

### 제거
`MW3KoreanBridge.asi`, `MW3Rumble.asi`, `MW3Rumble.ini`를 지우면 됩니다. 게임 파일은 전혀 수정하지 않습니다.

---

## 진동 설정 (`MW3Rumble.ini`)
수정 후 게임을 재시작하세요.

| 항목 | 기본값 | 설명 |
|---|---|---|
| `Strength` | 1.0 | 전체 세기 (0 = 끔, 최대 2) |
| `Fire` / `Explosion` / `Damage` / `Shake` / `Landing` | 1.0 | 종류별 세기 배율 |
| `ExplosionRadius` | 1600 | 이 거리(게임 단위, 약 40 = 1m)보다 먼 폭발은 진동 없음 |
| `Pad` | -1 | XInput 패드 번호 0~3, -1 = 처음 연결된 패드 |
| `Debug` | 0 | 1이면 감지한 이벤트를 `MW3Rumble.log`에 기록 |

- 진동은 **XInput**으로 출력됩니다. Xbox 패드는 바로 되고, 듀얼센스/듀얼쇼크는 Steam 입력(Xbox 패드로 에뮬레이션)을 켜야 합니다.
- 일시정지·메뉴에서는 자동으로 멈춥니다.

## 지원 버전
- Steam판 `iw5sp.exe` SHA256 `a97d2bbc7e495e4cf1a3b7e9a021e25b2c7b461d63f2b88d3cfe8782e8dc1023`
- 버전이 다르면 각 플러그인이 스스로 비활성화됩니다(로그에 기록). 게임은 정상 실행됩니다.

## 문제 해결
게임 폴더의 로그를 확인하세요.
- `MW3KoreanBridge.log` — `chained gamepad -> Korean` 두 줄이 보이면 정상
- `MW3Rumble.log` — `event hook installed`, `pad 0 connected`가 보이면 정상

---

## 작동 원리 (요약)

**KoreanBridge** — GamepadFix(MinHook)는 ASI 로드 시점에, 한글패치(d3d9 프록시)는 `Direct3DCreate9` 시점에 같은 함수 두 개(`FindAsset`, `CountChars`)를 후킹합니다. 한글패치는 원본 바이트를 검사하므로 이미 후킹돼 있으면 포기합니다. 브리지는 `Direct3DCreate9`를 가로채 잠시 원본 바이트를 복원 → 한글패치 후킹 → MinHook 트램펄린을 한글패치 훅으로 연결해 `게임 → 패드 훅 → 한글 훅 → 원본` 체인을 만듭니다. 두 모드의 파일은 건드리지 않습니다.

**Rumble** — PC판 실행파일에는 진동 코드가 없어서, 게임이 이미 알고 있는 정보를 읽어 진동으로 번역합니다.
- `CG_EntityEvent`(`iw5sp+0x42540`) 후킹: 사격·근접·폭발·착지 이벤트. 내 캐릭터 이벤트만 골라내고, 폭발은 카메라와의 거리로 세기 결정
- 읽기 전용 폴링: 플레이어 상태의 데미지 카운터(피격), 카메라 흔들림 슬롯 4개(지진)
- 저주파 모터 = 폭발/피격/착지, 고주파 모터 = 사격

## 직접 빌드
[Zig](https://ziglang.org) 설치 후:
```powershell
winget install zig.zig
.\build.ps1
```
`dist\`에 `.asi`가 생성됩니다.

---

## 크레딧 🙏
- **MW3 한글패치** — 한글패치 제작자님 *(이름/링크 추가 예정)*. 이 모드는 한글패치 없이는 의미가 없습니다.
- **[MW3GamepadFix](https://github.com/TheTronuo/MW3GamepadFix)** — **TheTronuo**님. PC판 MW3에 제대로 된 패드 지원을 만들어 주신 분.
- **[Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)** — **ThirteenAG**님 (MIT). 배포 zip에 포함된 `winmm.dll`.
- **Claude** (Anthropic) — 리버스 엔지니어링, 브리지·진동 코드 작성.
- 기획·테스트 — [huuuuni123](https://github.com/huuuuni123)

## 라이선스
이 저장소의 코드는 [MIT](LICENSE). 포함된 Ultimate ASI Loader는 원 라이선스(`licenses/Ultimate-ASI-Loader.txt`)를 따릅니다. 한글패치와 MW3GamepadFix는 각 원작자의 저작물이며 이 저장소에 포함되어 있지 않습니다.

---

## English
Two ASI plugins for **Call of Duty: Modern Warfare 3 (2011) single-player (Steam)**:
- **MW3KoreanBridge.asi** — lets the Korean translation patch and [MW3GamepadFix](https://github.com/TheTronuo/MW3GamepadFix) hook the same functions without conflicting.
- **MW3Rumble.asi** — adds controller vibration (firing, melee, explosions by distance, damage, earthquakes/camera shake, landing) via XInput. Configure in `MW3Rumble.ini`.

Install: download the release zip and copy all files next to `iw5sp.exe`. Single-player only; not affiliated with Activision.

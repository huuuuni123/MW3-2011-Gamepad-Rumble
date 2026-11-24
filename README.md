# MW3 2011 Gamepad + 진동 완벽 호환 모드

**Call of Duty: Modern Warfare 3 (2011) 싱글플레이 · Steam** — 한글패치 + 패드 모드 호환 브리지 & 패드 진동

[한국어](#한국어) · [English](#english)

---

## 한국어

한글패치와 패드 모드(MW3GamepadFix)를 같이 깔면 한글이 사라지던 문제를 해결하고, PC판에 없던 **패드 진동**을 추가합니다.
**세 모드(한글패치 · 패드 모드 · 진동)가 서로 충돌하지 않도록 포팅**했습니다.

> [!IMPORTANT]
> **게임은 꼭 [Steam에서 정품 구매](https://store.steampowered.com/app/115300/)하세요.** 이 모드는 Steam 정품 `iw5sp.exe`에서만 작동하며, 불법 복제판은 지원하지 않습니다.

> [!CAUTION]
> **면책 고지 — 꼭 읽어 주세요**
> - **싱글플레이 전용**입니다. 멀티플레이(`iw5mp.exe`)에서 사용하다 **VAC 밴 등 계정 제재를 받아도 제작자는 책임지지 않습니다.**
> - 이 모드는 비공식 팬 모드로, Activision·Infinity Ward·Microsoft와 무관합니다. 사용으로 생기는 게임 오류·데이터 손실·계정 문제 등 **모든 결과는 사용자 본인 책임**입니다.
> - **잘 모르겠으면 설치하지 마세요.** 제작자는 어떤 문제도 책임질 수 없습니다.

### 설치 (3단계)
1. **한글패치** 설치 → [gamemixblog 님 블로그](https://blog.naver.com/gamemixblog/222982734386)
2. **패드 모드** 설치 → [MW3GamepadFix-0.8.9-ASI.zip 바로 받기](https://github.com/TheTronuo/MW3GamepadFix/releases/download/v0.8.9/MW3GamepadFix-0.8.9-ASI.zip) 압축 풀어 게임 폴더에 복사
3. **이 모드** 설치 → [Releases](../../releases/latest)에서 `MW3-2011-Gamepad-Rumble-v1.0.zip` 받아 압축 풀어 게임 폴더에 복사
   (`winmm.dll` 덮어쓰기 물으면 **예** — 같은 파일입니다)

게임 폴더 = `iw5sp.exe`가 있는 곳. 보통 `C:\Program Files (x86)\Steam\steamapps\common\Call of Duty Modern Warfare 3`

끝! 게임을 실행하면 한글 + 패드 + 진동이 모두 작동합니다.

### 진동 설정
`MW3Rumble.ini`를 메모장으로 열어 숫자만 바꾸세요 (게임 재시작 필요).
- `Strength=1.0` — 전체 세기. `0`이면 진동 끔
- `Fire` 사격 · `Explosion` 폭발 · `Damage` 피격 · `Shake` 화면 흔들림 · `Landing` 착지 — 종류별 세기

듀얼센스/듀얼쇼크는 Steam 입력을 켜 주세요 (진동은 XInput으로 나갑니다).

### 잘 안 되면
- 한글이 안 나옴 → 1번(한글패치)만 깔고 한글이 나오는지 먼저 확인
- 게임 폴더의 `MW3KoreanBridge.log`, `MW3Rumble.log`를 이슈에 첨부해 주세요
- Steam판 최신 `iw5sp.exe`만 지원합니다. 버전이 다르면 모드가 스스로 꺼지고 게임은 정상 실행됩니다

### 제거
`MW3KoreanBridge.asi`, `MW3Rumble.asi`, `MW3Rumble.ini` 삭제. 게임 파일은 수정하지 않습니다.

---

## English

Makes the Korean translation patch and MW3GamepadFix work together (the Korean text used to vanish), and adds **controller rumble** the PC version never had.
**All three mods — Korean patch, gamepad mod, rumble — are ported to run side by side without conflicts.**

> [!IMPORTANT]
> **Please [buy the game on Steam](https://store.steampowered.com/app/115300/).** This mod only works with the genuine Steam `iw5sp.exe`; pirated copies are not supported.

> [!CAUTION]
> **Disclaimer**
> - **Single-player only.** If you use it in multiplayer (`iw5mp.exe`) and get a **VAC ban or any other account action, the authors are not responsible.**
> - Unofficial fan mod, not affiliated with Activision, Infinity Ward or Microsoft. **You use it entirely at your own risk** — crashes, data loss, account issues, anything.
> - **If you're not sure what you're doing, don't install it.** The authors cannot take responsibility for any problem.

### Install
1. Korean patch → [gamemixblog](https://blog.naver.com/gamemixblog/222982734386) (skip if you play in English)
2. Gamepad mod → [download MW3GamepadFix-0.8.9-ASI.zip](https://github.com/TheTronuo/MW3GamepadFix/releases/download/v0.8.9/MW3GamepadFix-0.8.9-ASI.zip), extract into the game folder
3. This mod → grab `MW3-2011-Gamepad-Rumble-v1.0.zip` from [Releases](../../releases/latest), extract into the game folder (overwriting `winmm.dll` is fine — same file)

Game folder = where `iw5sp.exe` lives. Done — launch the game.

### Rumble settings
Edit `MW3Rumble.ini`, restart the game. `Strength=0` turns rumble off; `Fire`, `Explosion`, `Damage`, `Shake`, `Landing` scale each effect. Output is XInput — enable Steam Input for PlayStation pads.

### Troubleshooting / uninstall
Attach `MW3KoreanBridge.log` / `MW3Rumble.log` from the game folder to an issue. Only the current Steam `iw5sp.exe` is supported; other builds make the plugins disable themselves. To uninstall, delete `MW3KoreanBridge.asi`, `MW3Rumble.asi`, `MW3Rumble.ini`.

---

## Credits / 크레딧 🙏
- **MW3 한글패치 / Korean patch** — [gamemixblog](https://blog.naver.com/gamemixblog/222982734386)
- **[MW3GamepadFix](https://github.com/TheTronuo/MW3GamepadFix)** — **TheTronuo**
- **[Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)** — **ThirteenAG** (MIT, bundled as `winmm.dll`)
- **Claude** (Anthropic) — compatibility bridge & rumble code / 호환 브리지·진동 코드
- **[huuuuni123](https://github.com/huuuuni123)** — idea & testing / 기획·테스트

The Korean patch and MW3GamepadFix belong to their authors and are **not** included here — please download them from the links above.
한글패치와 MW3GamepadFix는 원작자의 저작물이라 포함하지 않았습니다. 위 링크에서 받아 주세요.

## Build
```powershell
winget install zig.zig
.\build.ps1                                   # dist\*.asi
.\build.ps1 -AsiLoader <path\to\winmm.dll>    # + release zip
```

## License
Code: [MIT](LICENSE). Ultimate ASI Loader: its own MIT license (`licenses/Ultimate-ASI-Loader.txt`).

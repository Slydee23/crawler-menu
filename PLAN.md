# PLAN — CrawlerMenu

Status legend: `[ ]` open · `[x]` done · `[~]` in progress · `(?)` needs decision/info from user

## Phase 0 — Setup
- [x] Decisions: name CrawlerMenu, base = user's APK v1.6.5, floating button menu, mod code on GitHub, patched APK only for the user
- [x] Project repo (`Slydee23/crawler-menu`), `.gitignore` for game files / dumps / builds, LICENSE (MIT)
- [x] Toolchain on the PC (in `C:\Android`): JDK 17, SDK cmdline-tools, NDK r27c, build-tools 35 (apksigner), platform-tools (adb), apktool 3.0.3
- [x] Phone: USB debugging on, `adb devices` sees it (23124RA7EO, Android 15, arm64; game 1.6.5 installed via APKVision repack)
- [x] Save backup pulled to `build/save-backup/` (adb pull works on Android 15; in Git Bash prefix paths break — use PowerShell for `/sdcard/...`)

## Phase 1 — Analysis (local only, nothing committed)
- [x] IL2CPP dump with Cpp2IL 2022.1.0-pre-release.21 (`--force-unity-version 6000.0.62 --output-as diffable-cs`) → `dump/out/` (142,063 methods, metadata v31)
- [x] Find the classes/methods for each cheat (candidates found; semantics to verify on device in Phase 2)
- [x] Document findings in `docs/GAME_CODE.md` (names only, no dumped code)

## Phase 2 — Proof of concept
- Ignore the repack's "Downloaded from APKVISION.ORG" start toast (decided with the user)
- [x] Native lib loads in the game (log line via logcat): loaded from a smali patch in `Application.<clinit>`
- [x] Hook library: ShadowHook v2.0.1 (submodule), CMake build (`tools/build-native.ps1`); `shadowhook_init` OK in the game (needs `libshadowhook_nothing.so` shipped next to our lib)
- [x] Resolve a method by name via the IL2CPP API: background thread waits for `libil2cpp.so` + assembly `Pancake`, then finds `PlayerModel.AddCoins` (`native/src/il2cpp.*`)
- [x] Hook a resolved game method: pass-through hook on `PlayerModel.AddCoins` logs every call (coin pickup = `coins=1, useGreed=0`; many `coins=0` calls during play). Readiness via a pass-through hook on `il2cpp_init` (calling `il2cpp_domain_get` before init finishes aborts the game)
- [x] First cheat end-to-end: Edit Money (PoC: one-time +1000 bonus on the first coin pickup via the game's own `AddCoins(n, useGreed=false)`; verified in game 1465 → 2466). Real value input comes with the menu in Phase 3
- [x] Local patch script: `tools/patch-apk.ps1` (user's APK → `build/patched.apk`, debug-signed)

## Phase 3 — Menu
- [x] Floating button + empty menu panel (Java, `java/src/crawlermenu/`), added to the activity decor view; started from a smali patch in `Application.attachBaseContext`, shipped as an extra `classesN.dex` (`tools/build-java.ps1`)
- [x] Panel draggable by its title, accent color = blue of the game's menu buttons (#263FCD), background opacity slider (20-100 %, saved in SharedPreferences)
- [x] JNI bridge: `crawlermenu.NativeBridge.setFeature(id, on)` → `native/src/feature_flags.*`; first switch “Coins ×10” (verified 1465 → 1475 for one coin), now “Coins × [factor]” with an editable, saved factor via `setFeatureValue` (custom factor verified: x16 gave 16 per coin). Cheats are off on every game start
- [x] Edit money: “Coins [amount] [Add]”. During a run → run coins (PlayerModel.AddCoins, original code, no Greed/multiplier); outside a run (village) → village gold (`Globals.Progression` → `ProgressionData.AddCoins(long)`). Requests are queued and carried out on the game thread from `PlayerModel.Update` / `InputManager.Update` hooks. Verified in game
- [x] Infinite rerolls, skips & banishes (one switch): hooks on `SimpleStat.AddValue`/`SetValue` drop decreases of the player's `PlayerStats.Rerolls`/`CardSkips`/`Banishes` (fields resolved by name), counters at 0 are raised to 1. Rerolls, skips and banishes verified in game
- [x] Godmode: same stat protection for `PlayerStats.Health` (damage is still calculated by `PlayerModel.ResolveDamage`, health then drops point by point via SimpleStat). Verified in game
- [x] No mana cost: `PlayerModel.CanAffordManaCost` → always true, `PlayerModel.TryUseMana` → success without spending (observed first: playing a card = `TryUseMana(cost)` → `Mana.AddValue(-cost)`). Verified in game (no mana spent)
- [x] XP multiplier: “XP × [factor]”, applied at the entry points `PlayerModel.AddEnemyXp`/`AddGrowthXp` (multiplying inside `AddXp` raised the level silently without level-up choices). Verified in game (x50, level-ups with card choices)
- [x] Robustness: the game thread waits up to 30 min for `il2cpp_init` (Unity only initializes once the game is visible; with the screen off it took 52 s, the old 60 s limit would have disabled all cheats)
- [x] One-hit kill: `EnemyModel.ResolveDamage` (returns `DamageResult` = 3 doubles) gets at least enemy health + armor + 1 as damage, so the game's armor/overkill handling still runs. Verified in game
- [x] Combo never breaks: keeps the developer property `PlayerModel.DebugIsInCombo` set from the Update hook (every card continues the combo; ending the turn still ends it). Verified in game
- [x] Menu redesign “gothic gaming”: near-black panel with double gold frame, blood red accents, serif headings; fixed compact size (380×240 dp, scrolls); icon sidebar COMBAT / ECONOMY / RUN / SETTINGS (last tab remembered). Code split into `Theme`, `Widgets`, `FloatingMenu`
- [x] Own vector line icons for the sidebar (crossed swords, coin, skull, gear) in `Icons.java`; font glyphs looked different per device
- [x] Run stats editor: section “Run stats” on the RUN page, live values (polled 2×/s), SET per stat via original `SimpleStat.SetValue` (+ `SetMax` if capped), queued to the game thread. Verified in game (values show live, SET works; Greed/Luck/Growth persist through the run, fight-only stats reset after fights exactly like the game's own cards). Charm left out (developer leftover without effect)

## Later — QoL
- [ ] (?) More QoL ideas from the user, to be collected once the cheats work

## Phase 4 — Release (code only)
- [x] README: features, compatibility, build instructions, install with save backup, legal notes (no screenshots: they would show game art)
- [x] Link from InventoryCrawler (footer of index.html/howto.html + README), pushed there

## v0.2.0 — feedback
- [x] Combo never breaks → COMBAT; infinite rerolls / skips / banishes → ECONOMY as three separate switches (feature ids 2, 8, 9); RUN = run stats editor only
- [x] Visible credit “© 2026 Slydee” in the title bar, version/license line in SETTINGS
- [x] User check of the new layout on the phone (Add coins moved to the top of ECONOMY)
- [x] Private APK for the user's brother (owns the game) — local file only: `build/CrawlerMenu-v0.2.0.apk`, signed with `build/debug.keystore` (keep it for updates over the top)
- [x] Tag v0.2.0 + GitHub release (source only): https://github.com/Slydee23/crawler-menu/releases/tag/v0.2.0

## Showcase trailer (30–60 s)
- [ ] Shot list for the user → user records clips on the phone
- [ ] Edit/cut the clips into the trailer (ffmpeg), title + end card
- [x] Tag v0.1.0 + GitHub release (source only, no APK): https://github.com/Slydee23/crawler-menu/releases/tag/v0.1.0

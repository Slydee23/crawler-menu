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
- [ ] Draggable panel, colors matching the game
- [ ] JNI bridge to native toggles/values
- [ ] Cheats: Infinite rerolls · Combo never breaks · Godmode · No mana cost · One-hit kill · XP multiplier · Edit money · Edit run stats (Health, Armor, Might, Amount, Area, Growth, …)

## Phase 4 — Release (code only)
- [ ] README: features, build instructions, legal notes
- [ ] Optional: link from InventoryCrawler page
- [ ] Tag v0.1.0 (source only, no APK)

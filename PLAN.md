# PLAN — CrawlerMenu

Status legend: `[ ]` open · `[x]` done · `[~]` in progress · `(?)` needs decision/info from user

## Phase 0 — Setup
- [x] Decisions: name CrawlerMenu, base = user's APK v1.6.5, floating button menu, mod code on GitHub, patched APK only for the user
- [ ] Project repo (`Slydee23/crawler-menu`), `.gitignore` for game files / dumps / builds, LICENSE (MIT)
- [ ] Toolchain on the PC: JDK 17, Android SDK command-line tools + NDK + platform-tools (adb), apktool, apksigner
- [ ] Phone: USB debugging on, `adb devices` sees it

## Phase 1 — Analysis (local only, nothing committed)
- [ ] IL2CPP dump of `libil2cpp.so` + `global-metadata.dat` (Unity 6 → metadata v31+; tool: Cpp2IL or a recent Il2CppDumper fork)
- [ ] Find the classes/methods for each cheat: gold, rerolls, combo break, damage taken, damage dealt, mana cost, XP gain, player stats
- [ ] Document findings in `docs/GAME_CODE.md` (names only, no dumped code)

## Phase 2 — Proof of concept
- [ ] Native lib loads in the game (log line via logcat)
- [ ] Resolve one method by name via the IL2CPP API and hook it
- [ ] First cheat end-to-end: Edit Money
- [ ] Local patch script: user's APK → patched, signed APK in `build/`

## Phase 3 — Menu
- [ ] Floating button + menu (Java), JNI bridge to native toggles/values
- [ ] Cheats: Infinite rerolls · Combo never breaks · Godmode · No mana cost · One-hit kill · XP multiplier · Edit money · Edit run stats (Health, Armor, Might, Amount, Area, Growth, …)

## Phase 4 — Release (code only)
- [ ] README: features, build instructions, legal notes
- [ ] Optional: link from InventoryCrawler page
- [ ] Tag v0.1.0 (source only, no APK)

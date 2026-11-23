# CrawlerMenu — a mod menu for Vampire Crawlers (Android)

In-game mod menu (floating button + menu) for *Vampire Crawlers* on Android (Unity 6000.0.62f1, IL2CPP, arm64). Sister project of the save editor **InventoryCrawler** (`d:\Vibecoding\Projekte\Slydees Vampire Crawler Save Editor`, repo `Slydee23/inventory-crawler`) — reuse its knowledge (`docs/SAVE_FORMAT.md`, `data/ids.json`, `tools/extract-apk-ids.mjs`).

## Working with the user
- The user speaks German: talk to them in German. Everything in the product (code, comments, UI, commits, README) is **English**.
- Act with sensible defaults; ask only real decisions. When the user must do something on the phone, give **one step at a time** and wait for feedback/screenshots.
- Track progress in `PLAN.md`.

## Legal / distribution rules (decided with the user)
- **Only our own code goes to GitHub.** Never commit: APKs, `.so`/`.dat`/asset files from the game, IL2CPP dumps (`dump.cs`, generated headers, Cpp2IL output), patched builds. All of that lives in gitignored folders (`game-files/`, `dump/`, `build/`).
- The patched APK is built **locally, for the user only** — never published or attached to releases.
  - Exception decided by the user (2026-11-23): a private copy for the user's brother, who owns the game. Still never uploaded, published or attached anywhere.
- The repo ships source + a local build/patch script that works on the user's own APK.
- Unofficial fan project, not affiliated with poncle / Nosebleed Interactive. Single-player only.

## Technical approach
- Native hook library `libcrawlermenu.so` (C++, Android NDK, arm64-v8a). Hooks via the inline-hook library ShadowHook (ByteDance, MIT; git submodule `native/third_party/shadowhook`, pinned to v2.0.1). Dobby was the first plan but its current sources do not build with NDK r27.
- Resolve game methods **by name at runtime** through the exported IL2CPP API (`il2cpp_domain_get`, `il2cpp_class_from_name`, `il2cpp_class_get_method_from_name`, …) instead of hard-coded offsets → survives minor game updates.
- Floating menu in Java (own minimal implementation, no GPL templates), talking to the native lib via JNI.
- Injection for personal use: decompile APK (apktool), add our `.so` + a small loader in the launcher activity, rebuild, sign with a local debug key.
- A patched APK has a different signature → the original game must be **uninstalled first, which deletes its data**. Always back up `Android/data/com.poncle.vampirecrawlers/files/Save/` before installing.

## Game facts (from InventoryCrawler)
- Package `com.poncle.vampirecrawlers`, game version 1.6.5, save format 0.30.0.
- Run state classes/fields seen in saves: player state (`PlayerLevel`, `CurrentCoins`, `CurrentXp`, `PlayerStats.Snapshot` with Health/Armor/Might/Amount/Area/Growth/Luck/Greed/Mana/…), card piles, combo pile (`CurrentCombo`), rerolls/skips/banishes (`RemainingCardRerolls`, …).

## Layout (target)
```
CLAUDE.md, PLAN.md, README.md, LICENSE
native/        C++ hook library (CMake), ShadowHook as submodule
java/          floating menu + loader (smali or compiled dex)
tools/         build + patch scripts (run on the user's own APK)
game-files/    (gitignored) user's APK
dump/          (gitignored) IL2CPP dump for analysis
build/         (gitignored) outputs, patched APK
```

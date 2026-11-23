# CrawlerMenu

An in-game mod menu for **Vampire Crawlers** on Android: a floating button that opens a small
cheat panel on top of the game.

> **Unofficial fan project** for single-player use. Not affiliated with or endorsed by poncle or
> Nosebleed Interactive. This repository contains **source code only** — no game files, no APK.
> You build the modded game yourself from **your own copy** of the game with the script in this repo.

Sister project: [InventoryCrawler — a Vampire Crawlers save editor](https://github.com/Slydee23/inventory-crawler).

## Features

| Category | Feature |
| --- | --- |
| **Combat** | Godmode · One-hit kill · No mana cost · Combo never breaks |
| **Economy** | Add coins (run coins during a run, village gold outside) · Coin multiplier (custom factor) · XP multiplier (custom factor, level-ups still offer card choices) · Infinite rerolls · Infinite skips · Infinite banishes |
| **Run** | Run stats editor (Health, Max health, Armor, Might, Amount, Area, Duration, Growth, Greed, Luck, …) with live values |
| **Settings** | Panel background opacity |

- Draggable round **CM** button and a compact panel with a sidebar, gothic look.
- Every cheat is **off** each time the game starts. Multiplier factors and the opacity are remembered.
- Stats behave like the game's own: run-long stats (Greed, Luck, Growth, …) stay for the run,
  fight-only stats reset after each fight.

## Compatibility

- **Game version 1.6.5**, arm64 (`arm64-v8a`) devices. Tested on Android 15.
- Game methods are looked up **by name** at runtime, not by memory offsets, so small game updates
  may keep working — but only 1.6.5 is tested.
- Store builds of the game are protected by Google Play's integrity check (PairIP). A re-signed copy
  of a protected APK does not start, so the patch script only works with an APK where that check is
  not active. The project was developed and tested against such a v1.6.5 APK.

## How it works

1. `tools/patch-apk.ps1` decodes your APK with apktool, adds `libcrawlermenu.so` and a small menu
   dex, and patches the game's `Application` class to load them. It then rebuilds the APK and signs it
   with a local debug key.
2. The native library waits for the game's IL2CPP runtime, finds game methods by name through the
   IL2CPP API and hooks them with [ShadowHook](https://github.com/bytedance/android-inline-hook).
3. The menu (plain Android views, no extra permissions) talks to the native library via JNI.
   Requests from the menu are carried out on the game's own thread.

Details and the list of hooked game methods: [docs/GAME_CODE.md](docs/GAME_CODE.md).

## Build it yourself (Windows)

### 1. Tools

| Tool | Version used | Notes |
| --- | --- | --- |
| JDK | 17 | e.g. Eclipse Temurin |
| Android SDK command-line tools | latest | install to `C:\Android` |
| SDK packages | `build-tools;35.0.0`, `platforms;android-35`, `ndk;27.2.12479018`, `cmake;3.22.1`, `platform-tools` | `sdkmanager --sdk_root=C:\Android "<package>"` |
| apktool | 3.0.3 | `C:\Android\tools\apktool.jar` |

The scripts expect everything under `C:\Android`; pass `-AndroidHome <path>` to use another folder.

### 2. Source

```powershell
git clone --recurse-submodules https://github.com/Slydee23/crawler-menu.git
cd crawler-menu
# already cloned without submodules?
git submodule update --init
```

### 3. Patch your APK

Put your copy of the game into `game-files/` (ignored by git), then:

```powershell
powershell -ExecutionPolicy Bypass -File tools\patch-apk.ps1 -Apk game-files\<your-game>.apk
```

The result is `build\patched.apk`. It is for **your own use only** — do not share it, it contains the game.

### 4. Install

The patched APK has a different signature than the installed game, so the game has to be
**uninstalled first — which deletes its data. Back up your save first:**

```powershell
adb pull /sdcard/Android/data/com.poncle.vampirecrawlers/files/Save     # creates .\Save
adb uninstall com.poncle.vampirecrawlers
adb install build\patched.apk
# start the game once, close it, then restore the save:
adb push Save /sdcard/Android/data/com.poncle.vampirecrawlers/files/
```

Later builds can be installed over it with `adb install -r build\patched.apk` (keeps the save).

## Usage

Tap the **CM** button to open the panel, drag it to move it. Drag the panel by its title bar.
Pick a category in the sidebar. Values like *Add coins* and the run stats are applied on the next
game frame; outside a run the menu tells you when something is not available.

## Project layout

```text
native/   C++ library (CMake): IL2CPP lookup, hooks, JNI bridge; ShadowHook as git submodule
java/     the menu (plain Android views), compiled to a dex by tools/build-java.ps1
tools/    build-native.ps1, build-java.ps1, patch-apk.ps1
docs/     GAME_CODE.md — hooked game methods and findings
```

## Credits

- [ShadowHook](https://github.com/bytedance/android-inline-hook) (MIT) — inline hooking
- [apktool](https://apktool.org/) (Apache-2.0) — decoding and rebuilding the APK (not included)

## License

[MIT](LICENSE) © 2026 Slydee — applies to the code in this repository only.
Vampire Crawlers and all of its content belong to their respective owners.

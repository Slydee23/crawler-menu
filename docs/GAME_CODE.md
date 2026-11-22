# Game code map (v1.6.5)

Names of classes, methods and fields that CrawlerMenu hooks or reads. Found by inspecting
the game's IL2CPP metadata locally. No dumped code is stored in this repo.

- Game logic lives in assembly **`Pancake`** (not `Assembly-CSharp`).
- Namespaces: `Nosebleed.Pancake.Models`, `Nosebleed.Pancake.GameLogic`.
- Metadata version 31, Unity 6000.0.62f1, arm64-v8a, `libil2cpp.so` exports the normal IL2CPP API.
- Methods are resolved **by name** at runtime (`il2cpp_class_from_name` + `il2cpp_class_get_method_from_name`), no offsets.

## How to reach the live objects

| What | How |
| --- | --- |
| Run state | `GameModel.Instance` (singleton, `SingletonMonoBehaviour<GameModel>`), property `PlayerModel` |
| Player | `PlayerModel` (MonoBehaviour) |
| Player stats | `PlayerModel.PlayerStats` (`PlayerStats`, one `SimpleStat` per stat) |
| Enemy | `EnemyModel` (MonoBehaviour, one per enemy in the encounter) |

## Cheat -> candidate hook points

| Feature | Class | Member | Notes |
| --- | --- | --- | --- |
| Edit money | `PlayerModel` | `AddCoins(int, bool useGreed)`, `CurrentCoinCount` (backed by `_currentCoinCount` `IntAsset`), `MAX_COINS = 2147000000` | Add via `AddCoins(n, false)` to avoid Greed scaling. |
| Infinite rerolls / skips / banishes | `PlayerStats` | `Rerolls`, `CardSkips`, `Banishes` (`SimpleStat`) | Set `Value` high, or hook the consuming code in `ChooseCardModal` (`OnReroll`, `Button_Reroll`) / `ChooseGemModal`. |
| Run stats (Health, Armor, Might, Amount, Area, Growth, Greed, Luck, Recovery, Mana, Magnet, Duration, Curse, Charm, ...) | `PlayerStats` | `SimpleStat` fields; `SimpleStat.Value`, `AddValue(float, bool notify)`, `AddRunBaseModifierValue`, `AddMax` | `PlayerModel.SetStatsDirty()` / `StatChanged` event to refresh the UI. |
| Godmode | `PlayerModel` | `ResolveDamage(int, bool)` (player takes damage), `CheckHealthForDeath(float)`, `OnPlayerDeath()` | Hook `ResolveDamage` to return 0, or `CheckHealthForDeath` to no-op. |
| One-hit kill | `EnemyModel` | `ResolveDamage(double, ...)`, `Kill()`, `SetHealth(double)`, `Health`, `MaxHealth` | Hook `ResolveDamage` to scale damage, or call `Kill()` on hit. |
| No mana cost | `PlayerModel` | `SubtractMana(int)`, `SubtractManaWithoutNotify(int)`, `TryUseMana(int, bool)`, `CanAffordManaCost(int)` | Hook `CanAffordManaCost` -> true and `SubtractMana*` / `TryUseMana` to cost 0. |
| Combo never breaks | `PlayerModel` | `ResetCardCombo()`, `ResetCardComboDelayed()`, `ResetComboValues()`, `DiscardComboPile()`, field `CardCombo` (`IntVariable`), `ComboPile` | Hook the reset methods to no-op. Needs in-game testing. |
| XP multiplier | `PlayerModel` | `AddXp(int, bool)`, `AddEnemyXp`, `AddGrowthXp`, `CalculateEnemyXp`, `AddLevels_DEBUG(int)` | Hook `AddXp` to multiply the argument. `AddLevels_DEBUG` is a ready-made level-up. |

## Debug code already inside the game

- `DebugMenuModal` (`Nosebleed.Pancake.Modal`) is the game's own developer menu (health fields, enemy health/stats multiplier,
  hand size, dungeon/deck dropdowns, "add all cards", "max out gems").
- It is a UI prefab in a release build; whether it can be opened is open and not needed for our menu.
- `PlayerModel.AddLevels_DEBUG` exists and can be called directly.

## Open questions (need testing on the device)

- Exact semantics of `ResolveDamage` return values (godmode vs. armor).
- Which of the combo reset methods is the one that actually breaks a combo.
- Where Rerolls/Skips are decremented (stat value vs. a counter in the modal).
- Whether APKVision / PairIP (`libpairipcore.so`) blocks loading a patched build (see PLAN.md, Phase 2).

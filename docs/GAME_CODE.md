# Game code map (v1.6.5)

Names of classes, methods and fields that CrawlerMenu hooks or reads. Found by inspecting
the game's IL2CPP metadata locally and by logging calls in game. No dumped code is stored in this repo.

- Game logic lives in assembly **`Pancake`** (not `Assembly-CSharp`).
- Namespaces: `Nosebleed.Pancake.Models`, `Nosebleed.Pancake.GameLogic`, `Nosebleed.Pancake.MetaProgression`,
  `Nosebleed.Pancake.InputSystem`, `Nosebleed.Pancake.GameConfig.Accessors`.
- Metadata version 31, Unity 6000.0.62f1, arm64-v8a, `libil2cpp.so` exports the normal IL2CPP API.
- Methods and fields are resolved **by name** at runtime (`il2cpp_class_get_method_from_name`,
  `il2cpp_class_get_field_from_name` + `il2cpp_field_get_offset`), no hard-coded offsets.

## Startup and threading

- Our library is loaded first (smali patch in `Application.<clinit>`); `libil2cpp.so` comes later.
- Wait for a pass-through hook on `il2cpp_init` before touching the runtime: calling `il2cpp_domain_get`
  earlier aborts the game. Unity only initializes once the game is visible (screen on).
- Game objects are only touched on the game thread. The menu queues requests; per-frame hooks
  carry them out: `PlayerModel.Update()` (during a run), `InputManager.Update()` (always, incl. village).

## How to reach the live objects

| What | How |
| --- | --- |
| Player | `self` of `PlayerModel.Update()` / any `PlayerModel` hook |
| Player stats | `PlayerModel.get_PlayerStats()` → `PlayerStats`, one `SimpleStat` per field (`Health`, `Rerolls`, …) |
| Stat value | `SimpleStat.get_Value()`, `AddValue(float, bool notify)`, `SetValue(float, bool notify)`, `GetMaxValue()`, `SetMax(float, bool)` |
| Village gold | static `Globals.get_Progression()` → `ProgressionData.AddCoins(long)` (`TotalCoins`) |
| Enemy | `self` of `EnemyModel.ResolveDamage(...)`; `get_Health()`, `get_Armor()` (double) |

## Implemented cheats (verified in game)

| Feature | Hook point | How |
| --- | --- | --- |
| Coin multiplier | `PlayerModel.AddCoins(int, bool useGreed)` | multiply positive amounts (the game calls `AddCoins(0)` constantly) |
| Add coins (run) | `PlayerModel.AddCoins` original, from `PlayerModel.Update` | `AddCoins(n, false)` – no Greed, no multiplier |
| Add coins (village) | `ProgressionData.AddCoins(long)`, from `InputManager.Update` | outside a run |
| XP multiplier | `PlayerModel.AddEnemyXp(int, bool)`, `AddGrowthXp(int, bool)` | multiply the base XP at the entry point; multiplying in `AddXp` raised the level silently without level-up choices |
| Infinite rerolls / skips / banishes | `SimpleStat.AddValue` / `SetValue` | drop decreases of `PlayerStats.Rerolls` / `CardSkips` / `Banishes`; keep them ≥ 1 |
| Godmode | `SimpleStat.AddValue` / `SetValue` | drop decreases of `PlayerStats.Health`. `PlayerModel.ResolveDamage(int, bool)` only calculates the hit; health then drops point by point |
| No mana cost | `PlayerModel.CanAffordManaCost(int)` → true, `PlayerModel.TryUseMana(int, bool)` → true without spending | playing a card = `TryUseMana(cost)` → `Mana.AddValue(-cost)` |
| One-hit kill | `EnemyModel.ResolveDamage(double, CardModel, bool, int, bool, string)` → `DamageResult {double ArmorDamage, HealthDamage, OverkillDamage}` | damage ≥ health + armor + 1, so armor/overkill handling still runs |
| Combo never breaks | `PlayerModel.set_DebugIsInCombo(bool)` | developer flag; kept set from `PlayerModel.Update`. Ending the turn still ends the combo |

## Run stats editor (verified in game)

Edits `PlayerStats` fields `Health` (current and max), `Armor`, `Might`, `Amount`, `Area`, `Duration`, `Growth`,
`Greed`, `Luck`, `Recovery`, `Magnet`, `Mana`, `DealCount`, `Curse`, `Risk`, `Scavenge`, `RevivalStacks`,
`Rerolls`, `CardSkips`, `Banishes` via the original `SimpleStat.SetValue` (and `SetMax` if the value is capped).
`Charm` exists but has no effect (developer leftover), so it is not offered.

## Other debug code inside the game

- `DebugMenuModal` (`Nosebleed.Pancake.Modal`) is the game's own developer menu; not used.
- `PlayerModel.AddLevels_DEBUG(int)` exists; not used.

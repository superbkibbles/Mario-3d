# Super Mario – World 1 (C++ / raylib)

Four levels, played in order. Score, lives and power-ups carry over between levels.

| Level | Theme |
| --- | --- |
| 1-1 | Overworld: pipes, pits, brick/? blocks, Goombas, Koopas, flagpole |
| 1-2 | Underground: brick ceiling, coin rooms, pipes, plateaus |
| 1-3 | Treetops: platform jumping over open pits, ledge-aware Koopas |
| 1-4 | Castle: lava pits, rotating firebars, fire-breathing boss on a bridge, axe ending |

On the title screen, press ← → to pick the starting level.

## 3D Arena mode

On the title screen, press ↑/↓ to switch between **2D CAMPAIGN** and **3D ARENA**, then press Enter.

The arena is a third-person wave-survival mode on an open 3D map with hills, buildings, walls, trees and a helipad.
Some buildings have stepped blocks up to the roof, which make good sniper spots. Mario has 100 HP.
After each cleared wave you get a short break and 3 parachuted supply crates. The run ends when Mario dies.

| Key | Action |
| --- | --- |
| Mouse | Look and aim |
| WASD / Shift / Space | Move / sprint / jump |
| Left mouse | Fire (hold for the AK) |
| Right mouse | Aim down sights; with the sniper rifle this opens the scope (hold Shift to steady your breath) |
| R | Reload |
| 1–5 or mouse wheel | Knife, pistol, AK-47, shotgun, sniper rifle (only the weapons you own) |
| V | Quick knife slash without switching weapon |
| F | Open a crate (the knife also smashes crates open) |
| G | Throw a grenade |
| Q | Airstrike on the spot you aim at (needs a flare) |
| E | FPV drone: mouse to look, WASD to fly, Space/Ctrl up/down, left mouse to drop the bomb and again to crash it, E to come back |
| H | Board or exit the helicopter: WASD to fly, Space/Ctrl up/down, left mouse minigun (overheats), right mouse rockets |
| Esc | Pause (Enter in the pause menu quits to the title) |

**Loot and ammo.** You start with a knife, a pistol, 12+24 rounds of 9mm and one grenade. Everything else comes from
loot: the AK, shotgun and sniper rifle; ammo per caliber (9mm, 7.62, 12G, .308, rockets), each with a carrying cap;
medkits; grenades; airstrike flares; drones. Loot comes from crates and from killed enemies. Tanks and helicopters
always drop good loot. Walk over loot to pick it up; in the helicopter, fly low over it. Each loot item has a glow
pillar and a name label, and crates and loot are shown on the minimap.

**Enemies.**
- Zombies close in and hit you up close.
- Armed Goombas keep their distance, strafe and shoot.
- Tanks track you with the turret and fire shells.
- Attack helicopters circle you and fire bursts and homing rockets.

Small arms barely dent armor, so use explosives, rockets or the minigun on vehicles. Headshots do double damage.
Knife kills and explosions cut enemies in half with blood.

## Weapons

| Key | Action |
| --- | --- |
| 1 | Knife |
| 2 | AK |
| F | Attack. The knife slashes and cuts enemies in half; hold F with the AK for automatic fire. |
| R | Reload the AK (30-round magazine, also reloads automatically when empty) |

Knife kills split the enemy in two and spray blood, which sticks to floors and walls. They also
play a gore sound and send a shockwave that distorts the screen around the hit.
The AK has muzzle flash, recoil, ejected casings and ricochet sparks.
The castle boss takes 4 knife hits or 20 bullets. You can also drop it into the lava by touching the axe.

## Explosives and support

Each level starts with 5 grenades, 2 airstrikes and 3 FPV drones.

| Key | Action |
| --- | --- |
| G | Throw a grenade. It bounces and explodes after 1.7 s. |
| Q | Call an airstrike. A jet crosses the screen and drops 6 bombs ahead of you. |
| E | Launch the FPV drone. |

While flying the drone:

| Key | Action |
| --- | --- |
| Arrows / WASD | Fly. The camera follows the drone, and the view switches to a camera feed with a crosshair and a drop marker. |
| F | Drop its bomb. Press F again once it's empty to crash the drone into the target. |
| E | Fly back to Mario |

The drone has a 15 s battery and explodes when the battery runs out. Mario stands still and can
still be hit while you fly.

Explosions blow enemies apart, break bricks, damage the boss, and hurt Mario if he is too close.

## Helicopter

Each level has a parked helicopter near the start. Walk up to it and press **H** to board.

| Key | Action |
| --- | --- |
| Arrows / WASD | Fly. The helicopter tilts with speed. |
| F (hold) | Minigun. It overheats if you hold it too long. |
| G | Rocket (8 per helicopter) |
| H | Get out. You can board again later. |

The helicopter has 12 HP. Enemy fire, fire bars and blasts hit the helicopter instead of Mario.
When it is destroyed it explodes and throws Mario out. Get out before the flagpole or the axe.

## Enemy firepower

- Every second Goomba and Koopa carries a pistol and shoots at you. A knife slash knocks their bullets out of the air.
- Tanks (2 per level) patrol, aim their turret at you or at your helicopter, and fire arcing shells.
  They take 8 hits; a knife or bullet does 1, an explosion 4. They are immune to stomps, and you can stand on top of them.
  Destroyed tanks leave a burning wreck.
- Attack helicopters (in 1-1 and 1-3) hover above you, fire 5-round bursts and rockets, and take 10 hits.
  When destroyed they spin down trailing smoke and explode on impact.

## Zombies

Zombies climb out of the ground, walk toward you, hop over small obstacles and wait at ledges.
Only weapons kill them: one knife slash, 3 AK rounds, or an explosion. Stomping, shells and
hitting blocks from below do nothing to them.

## Build & run

```sh
brew install cmake          # once
cmake -S . -B build
cmake --build build -j8
./build/mario
```

CMake downloads and builds raylib 5.5 automatically (first configure needs internet).

## Controls

| Key | Action |
| --- | --- |
| ← → / A D | move |
| Z / Space / ↑ / W | jump (hold for higher) |
| X / Shift | run |
| P | pause |
| M | mute |
| Enter | start / continue |
| Esc | quit |

## Sprites

The game loads PNG sprite sheets from `assets/`. Any sheet that is missing is generated
as placeholder pixel art at startup, so the game always runs.

Export the placeholders as editable templates:

```sh
./build/mario --dump-assets   # writes every sheet into assets/
```

Each sheet is a single horizontal strip of equal-size frames. Your PNG must be exactly
the size below or a uniform multiple of it (for example 2x, which gets scaled down nearest-neighbor).
Use a transparent background. Characters face **right**; the game mirrors them.

| File | Frame size | Frames (left → right) |
| --- | --- | --- |
| `player_small.png` | 16×16 | idle, walk1, walk2, walk3, skid, jump, dead |
| `player_big.png` | 16×32 | idle, walk1, walk2, walk3, skid, jump |
| `goomba.png` | 16×16 | walk1, walk2, squished |
| `koopa.png` | 16×24 | walk1, walk2 |
| `shell.png` | 16×16 | shell |
| `tiles.png` | 16×16 | ground, brick, ?-block ×3 (flash), used block, hard block, pipe top-left, pipe top-right, pipe body-left, pipe body-right, underground ground, underground brick, castle stone, lava ×2, treetop left/middle/right, trunk, bridge |
| `items.png` | 16×16 | mushroom, coin ×4 (spin), axe, firebar ball, boss flame, grenade, bomb (pointing right) |
| `zombie.png` | 16×24 | walk1, walk2 |
| `jet.png` | 32×16 | jet (facing right) |
| `drone.png` | 16×8 | drone, propellers frame A/B |
| `tank.png` | 32×16 | hull tread A, tread B, wreck (faces right; the barrel is drawn by the game) |
| `heli.png` | 48×24 | body, rotor blur A, rotor blur B (faces right; rotor frames are drawn over the body) |
| `weapons.png` | 24×8 | knife, AK (pointing right, grip at left) |
| `boss.png` | 32×32 | walk1, walk2, breathing fire |
| `flag.png` | 16×16 | flag, pole top ball, pole segment |
| `castle.png` | 80×80 | castle (door centered at bottom) |
| `cloud.png` | 32×24 | cloud |
| `hill.png` | 80×40 | hill |
| `bush.png` | 32×16 | bush |

A sheet with the wrong size is ignored (with a warning in the console) and the placeholder is used.

Sound effects are synthesized at startup, so no audio files are needed.

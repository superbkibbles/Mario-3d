#pragma once

// Sound effects the 3D arena can request; the 2D game owns the synthesized sounds and maps these.
enum class Sfx3 {
  Shot, Pistol, Shotgun, Sniper, Boom, Slice, Splat, Groan, Clank, Reload, Empty, Switch, Pickup, Heli, Rocket,
  Cannon, Hurt, Swish, Drone, Jet, Whistle, Radio, Throw, Ricochet, Squeal, Coin
};

void PlaySfx3(Sfx3 s);
void StopSfx3(Sfx3 s);
bool SfxPlaying3(Sfx3 s);
void ToggleMuteShared();

// Runs the third-person 3D arena in the already-open window until the player quits to the title.
// Returns the final score.
int RunArena3D();

// Super Mario style platformer - World 1 (four levels) using raylib.
// Sprites load from assets/*.png; any missing sheet is generated as placeholder pixel art.
// Run with --dump-assets to export the placeholder sheets as templates.

#include "mode3d.h"
#include "raylib.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <vector>

#ifndef ASSET_DIR
#define ASSET_DIR "assets/"
#endif

namespace {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
constexpr int TILE = 16;
constexpr int VW = 256;  // virtual (NES-like) resolution
constexpr int VH = 224;
constexpr int SCALE = 3;
constexpr float DT = 1.0f / 60.0f;

constexpr float GRAVITY = 2000.0f;
constexpr float GRAVITY_HOLD = 700.0f;  // while rising and holding jump
constexpr float MAX_FALL = 360.0f;
constexpr float ENEMY_SPEED = 30.0f;
constexpr float SHELL_SPEED = 200.0f;

constexpr int LH = 14;
constexpr int GROUND_Y = 12 * TILE;  // top of the ground
constexpr int POLE_TOP_Y = 32;

constexpr float KNIFE_TIME = 0.22f;
constexpr float KNIFE_COOLDOWN = 0.3f;
constexpr float AK_RATE = 0.09f;
constexpr int AK_MAG = 30;
constexpr float AK_RELOAD = 1.3f;
constexpr float BULLET_SPEED = 480.0f;
constexpr int BOSS_HP = 20;
constexpr int START_GRENADES = 5;
constexpr int START_AIRSTRIKES = 2;
constexpr int START_DRONES = 3;
constexpr float GRENADE_FUSE = 1.7f;
constexpr float DRONE_BATTERY = 15.0f;
constexpr int ZOMBIE_HP = 3;
constexpr float ZOMBIE_SPEED = 22.0f;
constexpr int TANK_HP = 8;
constexpr float TANK_SPEED = 12.0f;
constexpr int HELI_HP = 12;
constexpr int HELI_ROCKETS = 8;
constexpr float HELI_W = 40, HELI_H = 16;  // hitbox; sprite is 48x24

// ---------------------------------------------------------------------------
// Palette (placeholder art)
// ---------------------------------------------------------------------------
const Color C_SKY{92, 148, 252, 255};
const Color C_RED{216, 40, 0, 255};
const Color C_SKIN{252, 188, 116, 255};
const Color C_HAIR{136, 76, 12, 255};
const Color C_BLUE{32, 56, 236, 255};
const Color C_YEL{252, 216, 0, 255};
const Color C_BLK{0, 0, 0, 255};
const Color C_WHITE{252, 252, 252, 255};
const Color C_BRICK{200, 76, 12, 255};
const Color C_LTBRICK{252, 188, 176, 255};
const Color C_DKBRICK{120, 40, 0, 255};
const Color C_GBROWN{172, 80, 36, 255};
const Color C_TAN{240, 208, 176, 255};
const Color C_GREEN{0, 168, 0, 255};
const Color C_LTGREEN{128, 208, 16, 255};
const Color C_DKGREEN{0, 104, 0, 255};
const Color C_KSKIN{252, 200, 96, 255};
const Color C_COIN{252, 188, 60, 255};
const Color C_HILL{0, 168, 68, 255};
const Color C_UG{0, 112, 136, 255};
const Color C_LTUG{156, 252, 240, 255};
const Color C_DKUG{0, 56, 72, 255};
const Color C_STONE{160, 160, 168, 255};
const Color C_LTSTONE{220, 220, 228, 255};
const Color C_DKSTONE{88, 88, 96, 255};
const Color C_LAVA{228, 92, 16, 255};
const Color C_LAVATOP{252, 188, 60, 255};
const Color C_STEEL{220, 220, 232, 255};
const Color C_DKSTEEL{150, 150, 165, 255};
const Color C_GUN{60, 60, 66, 255};
const Color C_WOOD{170, 90, 30, 255};
const Color C_ZSKIN{120, 170, 90, 255};
const Color C_ZDARK{70, 110, 60, 255};
const Color C_ZSHIRT{90, 60, 120, 255};
const Color C_ZPANTS{60, 60, 90, 255};
const Color C_OLIVE{80, 100, 40, 255};
const Color C_DKOLIVE{50, 64, 24, 255};
const Color C_JET{150, 160, 170, 255};

// ---------------------------------------------------------------------------
// Sprite sheets
// ---------------------------------------------------------------------------
enum SheetId {
  S_PSMALL, S_PBIG, S_GOOMBA, S_KOOPA, S_SHELL, S_TILES, S_ITEMS, S_FLAG, S_CASTLE, S_CLOUD, S_HILL, S_BUSH,
  S_WEAPONS, S_BOSS, S_ZOMBIE, S_JET, S_DRONE, S_TANK, S_HELI, S_COUNT
};

enum TileFrame {
  TF_GROUND, TF_BRICK, TF_Q0, TF_Q1, TF_Q2, TF_USED, TF_HARD, TF_PIPE_TL, TF_PIPE_TR, TF_PIPE_BL, TF_PIPE_BR,
  TF_GROUND_UG, TF_BRICK_UG, TF_STONE, TF_LAVA0, TF_LAVA1, TF_TREE_L, TF_TREE_M, TF_TREE_R, TF_TRUNK, TF_BRIDGE,
  TF_COUNT
};
enum ItemFrame { IF_MUSHROOM, IF_COIN0, IF_AXE = 5, IF_FIREBALL, IF_FLAME, IF_GRENADE, IF_BOMB, IF_COUNT };
enum PlayerPose { POSE_IDLE, POSE_WALK1, POSE_WALK2, POSE_WALK3, POSE_SKID, POSE_JUMP, POSE_DEAD };
enum Weapon { W_KNIFE, W_AK };

struct Sheet {
  Texture2D tex{};
  int fw = 0, fh = 0, frames = 0;
};

struct SheetSpec {
  const char* file;
  int fw, fh, frames;
  void (*gen)(Image&);
};

void R(Image& im, int x, int y, int w, int h, Color c) { ImageDrawRectangle(&im, x, y, w, h, c); }

// Small player is 16x16, big player is the same design stretched vertically (sy = 2).
void GenPlayerFrame(Image& im, int ox, int sy, int pose) {
  auto r = [&](int x, int y, int w, int h, Color c) { R(im, ox + x, y * sy, w, h * sy, c); };
  if (pose == POSE_DEAD) {
    r(4, 1, 8, 2, C_RED);
    r(3, 3, 10, 4, C_SKIN);
    r(3, 3, 2, 2, C_HAIR);
    r(11, 3, 2, 2, C_HAIR);
    r(5, 4, 1, 2, C_BLK);
    r(10, 4, 1, 2, C_BLK);
    r(5, 6, 6, 1, C_HAIR);
    r(4, 7, 8, 4, C_RED);
    r(5, 8, 6, 5, C_BLUE);
    r(1, 3, 2, 4, C_SKIN);
    r(13, 3, 2, 4, C_SKIN);
    r(2, 7, 2, 2, C_RED);
    r(12, 7, 2, 2, C_RED);
    r(4, 13, 3, 3, C_HAIR);
    r(9, 13, 3, 3, C_HAIR);
    return;
  }
  // head
  r(5, 1, 6, 1, C_RED);
  r(4, 2, 10, 1, C_RED);
  r(4, 3, 8, 4, C_SKIN);
  r(12, 4, 1, 2, C_SKIN);
  r(3, 3, 3, 3, C_HAIR);
  r(10, 4, 1, 1, C_BLK);
  r(9, 6, 4, 1, C_HAIR);
  // body
  r(4, 7, 8, 5, C_RED);
  r(5, 8, 6, 5, C_BLUE);
  r(6, 9, 1, 1, C_YEL);
  r(9, 9, 1, 1, C_YEL);
  // arms
  if (pose == POSE_JUMP) {
    r(12, 1, 2, 3, C_SKIN);
    r(12, 4, 2, 4, C_RED);
    r(2, 9, 2, 2, C_SKIN);
  } else if (pose == POSE_SKID) {
    r(1, 7, 2, 2, C_SKIN);
    r(12, 9, 2, 2, C_SKIN);
  } else {
    r(2, 9, 2, 2, C_SKIN);
    r(12, 9, 2, 2, C_SKIN);
  }
  // legs + shoes
  switch (pose) {
    case POSE_IDLE:
      r(5, 12, 2, 2, C_BLUE); r(9, 12, 2, 2, C_BLUE);
      r(4, 14, 4, 2, C_HAIR); r(8, 14, 4, 2, C_HAIR);
      break;
    case POSE_WALK1:
    case POSE_SKID:
      r(4, 12, 2, 2, C_BLUE); r(10, 12, 2, 2, C_BLUE);
      r(2, 14, 4, 2, C_HAIR); r(10, 14, 4, 2, C_HAIR);
      break;
    case POSE_WALK2:
      r(6, 12, 4, 2, C_BLUE);
      r(5, 14, 6, 2, C_HAIR);
      break;
    case POSE_WALK3:
      r(5, 12, 2, 2, C_BLUE); r(9, 12, 2, 1, C_BLUE);
      r(3, 14, 4, 2, C_HAIR); r(9, 13, 4, 2, C_HAIR);
      break;
    case POSE_JUMP:
      r(4, 12, 2, 2, C_BLUE); r(10, 11, 2, 2, C_BLUE);
      r(2, 13, 4, 2, C_HAIR); r(11, 11, 4, 2, C_HAIR);
      break;
  }
}

void GenPlayerSmall(Image& im) {
  for (int p = 0; p <= POSE_DEAD; p++) GenPlayerFrame(im, p * 16, 1, p);
}

void GenPlayerBig(Image& im) {
  for (int p = 0; p < POSE_DEAD; p++) GenPlayerFrame(im, p * 16, 2, p);
}

// frames: walk1, walk2, squished
void GenGoomba(Image& im) {
  for (int f = 0; f < 2; f++) {
    int o = f * 16;
    R(im, o + 4, 1, 8, 2, C_GBROWN);
    R(im, o + 2, 3, 12, 3, C_GBROWN);
    R(im, o + 1, 6, 14, 5, C_GBROWN);
    R(im, o + 3, 4, 4, 1, C_BLK);
    R(im, o + 9, 4, 4, 1, C_BLK);
    R(im, o + 4, 5, 3, 3, C_WHITE);
    R(im, o + 9, 5, 3, 3, C_WHITE);
    R(im, o + 5, 6, 1, 2, C_BLK);
    R(im, o + 10, 6, 1, 2, C_BLK);
    R(im, o + 4, 11, 8, 3, C_TAN);
    if (f == 0) {
      R(im, o + 1, 13, 6, 3, C_BLK);
      R(im, o + 10, 14, 5, 2, C_BLK);
    } else {
      R(im, o + 1, 14, 5, 2, C_BLK);
      R(im, o + 9, 13, 6, 3, C_BLK);
    }
  }
  int o = 32;
  R(im, o + 2, 8, 12, 2, C_GBROWN);
  R(im, o + 1, 10, 14, 3, C_GBROWN);
  R(im, o + 4, 10, 2, 1, C_WHITE);
  R(im, o + 10, 10, 2, 1, C_WHITE);
  R(im, o + 3, 13, 10, 1, C_TAN);
  R(im, o + 1, 14, 14, 2, C_BLK);
}

// 16x24, faces right; frames: walk1, walk2
void GenKoopa(Image& im) {
  for (int f = 0; f < 2; f++) {
    int o = f * 16;
    R(im, o + 10, 1, 5, 6, C_KSKIN);
    R(im, o + 12, 3, 2, 2, C_WHITE);
    R(im, o + 13, 3, 1, 2, C_BLK);
    R(im, o + 10, 7, 3, 5, C_KSKIN);
    R(im, o + 2, 9, 10, 11, C_GREEN);
    R(im, o + 4, 11, 6, 6, C_LTGREEN);
    R(im, o + 6, 11, 2, 6, C_DKGREEN);
    R(im, o + 1, 18, 12, 2, C_WHITE);
    R(im, o + 12, 12, 2, 3, C_KSKIN);
    if (f == 0) {
      R(im, o + 3, 20, 4, 4, C_KSKIN);
      R(im, o + 9, 20, 4, 3, C_KSKIN);
    } else {
      R(im, o + 1, 20, 4, 3, C_KSKIN);
      R(im, o + 7, 20, 4, 4, C_KSKIN);
    }
  }
}

void GenShell(Image& im) {
  R(im, 2, 3, 12, 10, C_GREEN);
  R(im, 4, 5, 8, 6, C_LTGREEN);
  R(im, 7, 3, 2, 10, C_DKGREEN);
  R(im, 2, 8, 12, 1, C_DKGREEN);
  R(im, 1, 12, 14, 2, C_WHITE);
  R(im, 3, 14, 10, 1, C_GREEN);
}

void GenGroundTile(Image& im, int o, Color body, Color light, Color dark) {
  R(im, o, 0, 16, 16, body);
  R(im, o, 0, 15, 1, light);
  R(im, o, 0, 1, 15, light);
  R(im, o, 15, 16, 1, dark);
  R(im, o + 15, 0, 1, 16, dark);
  R(im, o + 9, 1, 1, 7, dark);
  R(im, o + 1, 9, 6, 1, dark);
  R(im, o + 10, 8, 5, 1, dark);
  R(im, o + 6, 10, 1, 5, dark);
}

void GenBrickTile(Image& im, int o, Color body, Color light, Color mortar) {
  R(im, o, 0, 16, 16, body);
  for (int row = 0; row < 4; row++) {
    int y = row * 4;
    R(im, o, y + 3, 16, 1, mortar);
    int off = (row % 2 == 0) ? 7 : 3;
    R(im, o + off, y, 1, 3, mortar);
    R(im, o + off + 8, y, 1, 3, mortar);
    if (row == 0) R(im, o, y, 16, 1, light);
  }
}

void GenTreeTop(Image& im, int o, int part) {
  R(im, o, 0, 16, 16, BLANK);
  if (part == 0) {
    ImageDrawCircle(&im, o + 8, 8, 8, C_DKGREEN);
    ImageDrawCircle(&im, o + 8, 8, 7, C_LTGREEN);
    R(im, o + 8, 0, 8, 16, C_DKGREEN);
    R(im, o + 8, 1, 8, 14, C_LTGREEN);
  } else if (part == 2) {
    ImageDrawCircle(&im, o + 7, 8, 8, C_DKGREEN);
    ImageDrawCircle(&im, o + 7, 8, 7, C_LTGREEN);
    R(im, o, 0, 8, 16, C_DKGREEN);
    R(im, o, 1, 8, 14, C_LTGREEN);
  } else {
    R(im, o, 0, 16, 16, C_DKGREEN);
    R(im, o, 1, 16, 14, C_LTGREEN);
  }
  R(im, o + 4, 4, 2, 2, C_GREEN);
  R(im, o + 10, 9, 2, 2, C_GREEN);
}

void GenTiles(Image& im) {
  for (int f = 0; f < TF_COUNT; f++) {
    int o = f * 16;
    switch (f) {
      case TF_GROUND: GenGroundTile(im, o, C_BRICK, C_LTBRICK, C_DKBRICK); break;
      case TF_GROUND_UG: GenGroundTile(im, o, C_UG, C_LTUG, C_DKUG); break;
      case TF_BRICK: GenBrickTile(im, o, C_BRICK, C_LTBRICK, C_BLK); break;
      case TF_BRICK_UG: GenBrickTile(im, o, C_UG, C_LTUG, C_DKUG); break;
      case TF_Q0:
      case TF_Q1:
      case TF_Q2: {
        const Color shades[3] = {{252, 152, 56, 255}, {220, 120, 40, 255}, {180, 92, 20, 255}};
        Color body = shades[f - TF_Q0];
        R(im, o, 0, 16, 16, C_DKBRICK);
        R(im, o + 1, 1, 14, 14, body);
        R(im, o + 2, 2, 1, 1, C_BLK); R(im, o + 13, 2, 1, 1, C_BLK);
        R(im, o + 2, 13, 1, 1, C_BLK); R(im, o + 13, 13, 1, 1, C_BLK);
        R(im, o + 5, 3, 6, 2, C_HAIR);
        R(im, o + 10, 4, 2, 3, C_HAIR);
        R(im, o + 4, 4, 2, 2, C_HAIR);
        R(im, o + 7, 7, 3, 2, C_HAIR);
        R(im, o + 7, 9, 2, 1, C_HAIR);
        R(im, o + 7, 11, 2, 2, C_HAIR);
        break;
      }
      case TF_USED:
        R(im, o, 0, 16, 16, C_BLK);
        R(im, o + 1, 1, 14, 14, C_HAIR);
        R(im, o + 2, 2, 1, 1, C_BLK); R(im, o + 13, 2, 1, 1, C_BLK);
        R(im, o + 2, 13, 1, 1, C_BLK); R(im, o + 13, 13, 1, 1, C_BLK);
        break;
      case TF_HARD:
        R(im, o, 0, 16, 16, C_DKBRICK);
        R(im, o, 0, 15, 15, C_LTBRICK);
        R(im, o + 1, 1, 14, 14, C_BRICK);
        R(im, o + 3, 3, 10, 10, C_GBROWN);
        break;
      case TF_PIPE_TL:
        R(im, o, 0, 16, 16, C_BLK);
        R(im, o + 1, 1, 15, 14, C_GREEN);
        R(im, o + 3, 1, 2, 14, C_LTGREEN);
        R(im, o + 6, 1, 1, 14, C_LTGREEN);
        break;
      case TF_PIPE_TR:
        R(im, o, 0, 16, 16, C_BLK);
        R(im, o, 1, 15, 14, C_GREEN);
        R(im, o + 9, 1, 4, 14, C_DKGREEN);
        break;
      case TF_PIPE_BL:
        R(im, o + 2, 0, 14, 16, C_BLK);
        R(im, o + 3, 0, 13, 16, C_GREEN);
        R(im, o + 5, 0, 2, 16, C_LTGREEN);
        R(im, o + 8, 0, 1, 16, C_LTGREEN);
        break;
      case TF_PIPE_BR:
        R(im, o, 0, 14, 16, C_BLK);
        R(im, o, 0, 13, 16, C_GREEN);
        R(im, o + 7, 0, 4, 16, C_DKGREEN);
        break;
      case TF_STONE:
        R(im, o, 0, 16, 16, C_STONE);
        R(im, o, 0, 16, 1, C_LTSTONE);
        R(im, o, 0, 1, 16, C_LTSTONE);
        R(im, o, 15, 16, 1, C_DKSTONE);
        R(im, o + 15, 0, 1, 16, C_DKSTONE);
        R(im, o, 7, 16, 1, C_DKSTONE);
        R(im, o + 7, 0, 1, 7, C_DKSTONE);
        R(im, o + 11, 8, 1, 7, C_DKSTONE);
        R(im, o + 3, 8, 1, 7, C_DKSTONE);
        break;
      case TF_LAVA0:
      case TF_LAVA1:
        for (int x = 0; x < 16; x++) {
          int top = 3 + (int)roundf(2.0f * sinf((x + (f - TF_LAVA0) * 8) * PI / 8.0f));
          R(im, o + x, top, 1, 16 - top, C_LAVA);
          R(im, o + x, top, 1, 1, C_LAVATOP);
        }
        R(im, o + 3, 9, 2, 1, C_LAVATOP);
        R(im, o + 11, 12, 2, 1, C_LAVATOP);
        break;
      case TF_TREE_L: GenTreeTop(im, o, 0); break;
      case TF_TREE_M: GenTreeTop(im, o, 1); break;
      case TF_TREE_R: GenTreeTop(im, o, 2); break;
      case TF_TRUNK:
        R(im, o + 2, 0, 12, 16, {200, 140, 80, 255});
        R(im, o + 5, 0, 1, 16, {150, 96, 48, 255});
        R(im, o + 10, 0, 1, 16, {150, 96, 48, 255});
        break;
      case TF_BRIDGE:
        R(im, o, 2, 16, 6, C_BRICK);
        R(im, o, 2, 16, 1, C_LTBRICK);
        R(im, o, 7, 16, 1, C_DKBRICK);
        R(im, o + 7, 2, 1, 6, C_DKBRICK);
        R(im, o + 15, 2, 1, 6, C_DKBRICK);
        R(im, o + 2, 0, 2, 2, C_DKSTONE);
        R(im, o + 10, 0, 2, 2, C_DKSTONE);
        break;
    }
  }
}

// frames: mushroom, coin x4 (spin), axe, firebar ball, boss flame
void GenItems(Image& im) {
  R(im, 4, 1, 8, 2, C_RED);
  R(im, 2, 3, 12, 3, C_RED);
  R(im, 1, 6, 14, 3, C_RED);
  R(im, 6, 2, 4, 3, C_WHITE);
  R(im, 2, 5, 3, 3, C_WHITE);
  R(im, 11, 5, 3, 3, C_WHITE);
  R(im, 4, 9, 8, 6, C_TAN);
  R(im, 6, 10, 1, 3, C_BLK);
  R(im, 9, 10, 1, 3, C_BLK);
  const int widths[4] = {8, 4, 2, 4};
  for (int f = 0; f < 4; f++) {
    int o = 16 + f * 16, w = widths[f];
    R(im, o + 8 - w / 2, 2, w, 12, C_HAIR);
    R(im, o + 8 - w / 2 + (w > 2 ? 1 : 0), 3, std::max(1, w - 2), 10, C_COIN);
    if (w >= 4) R(im, o + 8 - w / 2 + 1, 4, 1, 6, C_WHITE);
  }
  int o = IF_AXE * 16;
  R(im, o + 7, 3, 2, 13, C_HAIR);
  R(im, o + 1, 1, 7, 8, C_DKSTEEL);
  R(im, o + 2, 2, 5, 6, C_STEEL);
  R(im, o + 9, 2, 3, 4, C_DKSTEEL);
  o = IF_FIREBALL * 16;
  ImageDrawCircle(&im, o + 8, 8, 4, C_LAVA);
  ImageDrawCircle(&im, o + 8, 8, 2, C_LAVATOP);
  o = IF_FLAME * 16;
  R(im, o + 0, 7, 4, 2, C_LAVA);
  R(im, o + 3, 6, 6, 4, C_LAVA);
  R(im, o + 8, 4, 7, 8, C_LAVA);
  R(im, o + 9, 6, 5, 4, C_LAVATOP);
  R(im, o + 11, 7, 3, 2, C_WHITE);
  o = IF_GRENADE * 16;
  ImageDrawCircle(&im, o + 8, 9, 4, C_DKOLIVE);
  ImageDrawCircle(&im, o + 8, 9, 3, C_OLIVE);
  R(im, o + 6, 8, 5, 1, C_DKOLIVE);
  R(im, o + 7, 4, 3, 2, C_DKSTEEL);
  R(im, o + 10, 4, 2, 4, C_DKSTEEL);
  ImageDrawCircle(&im, o + 5, 4, 1, C_STEEL);
  o = IF_BOMB * 16;  // points right; rotated along its velocity
  R(im, o + 4, 6, 9, 5, C_GUN);
  R(im, o + 12, 7, 2, 3, C_GUN);
  R(im, o + 5, 7, 6, 1, C_DKSTEEL);
  R(im, o + 1, 4, 3, 2, C_GUN);
  R(im, o + 1, 11, 3, 2, C_GUN);
  R(im, o + 3, 6, 1, 5, C_GUN);
  R(im, o + 8, 6, 1, 5, C_YEL);
}

// frames: flag, pole top ball, pole segment
void GenFlag(Image& im) {
  for (int y = 0; y < 16; y++) {
    int w = 16 - std::abs(2 * y - 15);
    R(im, 16 - w, y, w, 1, C_WHITE);
  }
  ImageDrawCircle(&im, 11, 8, 3, C_GREEN);
  ImageDrawCircle(&im, 16 + 8, 8, 4, C_DKGREEN);
  ImageDrawCircle(&im, 16 + 7, 7, 2, C_LTGREEN);
  R(im, 32 + 7, 0, 2, 16, C_LTGREEN);
}

void BrickFill(Image& im, int x, int y, int w, int h) {
  R(im, x, y, w, h, C_BRICK);
  for (int yy = y, row = 0; yy < y + h; yy += 8, row++) {
    R(im, x, yy + 7, w, 1, C_BLK);
    for (int xx = x + ((row % 2) ? 4 : 0); xx < x + w; xx += 8) R(im, xx, yy, 1, 7, C_BLK);
  }
}

void GenCastle(Image& im) {
  BrickFill(im, 0, 40, 80, 40);
  for (int x = 0; x < 80; x += 16) BrickFill(im, x, 32, 8, 8);
  BrickFill(im, 24, 8, 32, 32);
  for (int x = 24; x < 56; x += 12) BrickFill(im, x, 0, 8, 8);
  R(im, 30, 16, 6, 12, C_BLK);
  R(im, 44, 16, 6, 12, C_BLK);
  R(im, 32, 60, 16, 20, C_BLK);
  ImageDrawCircle(&im, 40, 60, 8, C_BLK);
}

void GenCloud(Image& im) {
  ImageDrawCircle(&im, 8, 15, 7, C_WHITE);
  ImageDrawCircle(&im, 16, 10, 9, C_WHITE);
  ImageDrawCircle(&im, 24, 15, 7, C_WHITE);
  R(im, 6, 15, 20, 7, C_WHITE);
}

void GenHill(Image& im) {
  for (int y = 0; y < 40; y++) {
    int hw = std::min(40, 8 + y);
    R(im, 40 - hw, y, hw * 2, 1, C_HILL);
  }
  ImageDrawCircle(&im, 40, 8, 8, C_HILL);
  R(im, 32, 16, 3, 7, C_DKGREEN);
  R(im, 46, 14, 3, 7, C_DKGREEN);
  R(im, 39, 26, 3, 7, C_DKGREEN);
}

void GenBush(Image& im) {
  ImageDrawCircle(&im, 8, 10, 6, C_LTGREEN);
  ImageDrawCircle(&im, 16, 7, 7, C_LTGREEN);
  ImageDrawCircle(&im, 24, 10, 6, C_LTGREEN);
  R(im, 2, 10, 28, 6, C_LTGREEN);
}

// 24x8, pointing right; frames: knife, AK
void GenWeapons(Image& im) {
  R(im, 0, 3, 6, 3, C_WOOD);
  R(im, 1, 4, 4, 1, C_HAIR);
  R(im, 6, 1, 2, 6, C_DKSTEEL);
  R(im, 8, 3, 11, 2, C_STEEL);
  R(im, 8, 5, 9, 1, C_DKSTEEL);
  R(im, 19, 3, 2, 1, C_STEEL);
  R(im, 21, 3, 1, 1, C_WHITE);
  int o = 24;
  R(im, o + 0, 2, 6, 3, C_WOOD);
  R(im, o + 0, 4, 3, 2, C_WOOD);
  R(im, o + 6, 2, 9, 3, C_GUN);
  R(im, o + 7, 5, 2, 3, C_WOOD);
  R(im, o + 11, 5, 3, 1, C_HAIR);
  R(im, o + 12, 6, 3, 2, C_HAIR);
  R(im, o + 15, 3, 5, 2, C_WOOD);
  R(im, o + 15, 2, 6, 1, C_GUN);
  R(im, o + 20, 3, 4, 1, C_GUN);
  R(im, o + 22, 1, 1, 2, C_GUN);
}

// 32x32, faces right; frames: walk1, walk2, breathing fire
void GenBoss(Image& im) {
  for (int f = 0; f < 3; f++) {
    int o = f * 32;
    ImageDrawCircle(&im, o + 12, 17, 11, C_DKGREEN);
    ImageDrawCircle(&im, o + 12, 17, 9, C_GREEN);
    R(im, o + 5, 6, 3, 3, C_WHITE);
    R(im, o + 11, 4, 3, 3, C_WHITE);
    R(im, o + 17, 6, 3, 3, C_WHITE);
    R(im, o + 2, 11, 3, 3, C_WHITE);
    R(im, o + 3, 25, 20, 2, C_WHITE);
    R(im, o + 18, 13, 7, 13, C_KSKIN);
    R(im, o + 19, 15, 5, 1, C_GBROWN);
    R(im, o + 19, 19, 5, 1, C_GBROWN);
    R(im, o + 20, 4, 10, 10, C_GREEN);
    R(im, o + 21, 0, 2, 4, C_WHITE);
    R(im, o + 26, 0, 2, 4, C_WHITE);
    R(im, o + 18, 3, 3, 8, C_RED);
    R(im, o + 25, 6, 3, 2, C_WHITE);
    R(im, o + 27, 6, 1, 2, C_RED);
    if (f == 2) {
      R(im, o + 24, 10, 8, 5, C_BLK);
      R(im, o + 25, 12, 6, 2, C_RED);
      R(im, o + 25, 10, 1, 1, C_WHITE);
      R(im, o + 29, 10, 1, 1, C_WHITE);
    } else {
      R(im, o + 25, 11, 6, 2, C_KSKIN);
      R(im, o + 26, 11, 1, 1, C_WHITE);
      R(im, o + 29, 11, 1, 1, C_WHITE);
    }
    R(im, o + 24, 17, 4, 3, C_KSKIN);
    if (f == 1) {
      R(im, o + 6, 27, 6, 5, C_KSKIN);
      R(im, o + 16, 28, 6, 4, C_KSKIN);
    } else {
      R(im, o + 5, 28, 6, 4, C_KSKIN);
      R(im, o + 15, 27, 6, 5, C_KSKIN);
    }
  }
}

// 16x24, faces right, arms reaching forward; frames: walk1, walk2
void GenZombie(Image& im) {
  for (int f = 0; f < 2; f++) {
    int o = f * 16;
    R(im, o + 5, 1, 7, 7, C_ZSKIN);
    R(im, o + 4, 1, 3, 3, C_ZDARK);
    R(im, o + 9, 3, 2, 2, C_RED);
    R(im, o + 8, 6, 4, 1, C_ZDARK);
    R(im, o + 4, 8, 8, 9, C_ZSHIRT);
    R(im, o + 5, 15, 2, 2, C_ZSKIN);  // torn shirt
    R(im, o + 6, 10, 2, 2, C_DKBRICK);
    R(im, o + 9, 9, 7, 2, C_ZSKIN);
    R(im, o + 9, 12, 6, 2, C_ZSKIN);
    R(im, o + 5, 17, 6, 3, C_ZPANTS);
    if (f == 0) {
      R(im, o + 4, 20, 3, 4, C_ZPANTS);
      R(im, o + 9, 20, 3, 3, C_ZPANTS);
      R(im, o + 3, 23, 4, 1, C_HAIR);
      R(im, o + 9, 22, 4, 2, C_HAIR);
    } else {
      R(im, o + 6, 20, 4, 3, C_ZPANTS);
      R(im, o + 5, 22, 5, 2, C_HAIR);
    }
  }
}

// 32x16, faces right
void GenJet(Image& im) {
  R(im, 3, 6, 25, 4, C_JET);
  R(im, 28, 7, 3, 2, C_JET);
  R(im, 31, 8, 1, 1, C_DKSTEEL);
  R(im, 19, 5, 6, 2, {80, 160, 220, 255});
  R(im, 2, 1, 5, 5, C_DKSTEEL);
  R(im, 3, 2, 3, 4, C_JET);
  R(im, 10, 9, 12, 3, C_DKSTEEL);
  R(im, 12, 12, 6, 2, C_DKSTEEL);
  R(im, 0, 7, 3, 2, C_LAVATOP);
}

// 16x8; frames: props blur A/B
void GenDrone(Image& im) {
  for (int f = 0; f < 2; f++) {
    int o = f * 16;
    R(im, o + 1, 2, 14, 1, C_DKSTEEL);
    R(im, o + 5, 2, 6, 4, C_GUN);
    R(im, o + 10, 4, 2, 1, C_RED);
    R(im, o + 2, 3, 1, 2, C_GUN);
    R(im, o + 13, 3, 1, 2, C_GUN);
    Color prop{200, 200, 210, 160};
    if (f == 0) {
      R(im, o + 0, 1, 5, 1, prop);
      R(im, o + 11, 1, 5, 1, prop);
    } else {
      R(im, o + 1, 0, 3, 1, prop);
      R(im, o + 12, 0, 3, 1, prop);
    }
  }
}

// 32x16 hull with turret, faces right (barrel is drawn separately); frames: tread A, tread B, wreck
void GenTank(Image& im) {
  for (int f = 0; f < 3; f++) {
    int o = f * 32;
    bool wreck = f == 2;
    Color body = wreck ? Color{50, 40, 34, 255} : C_OLIVE;
    Color dark = wreck ? Color{28, 24, 20, 255} : C_DKOLIVE;
    Color tread = wreck ? Color{30, 30, 30, 255} : C_GUN;
    R(im, o + 1, 10, 30, 6, tread);
    R(im, o + 2, 10, 28, 1, C_DKSTEEL);
    for (int w = 0; w < 5; w++) {
      ImageDrawCircle(&im, o + 5 + w * 5, 13, 2, wreck ? dark : C_DKSTEEL);
      R(im, o + 5 + w * 5 + (f == 1 ? 1 : -1), 13, 1, 1, tread);
    }
    R(im, o + 3, 6, 26, 4, body);
    R(im, o + 3, 6, 26, 1, dark);
    R(im, o + 26, 7, 4, 3, body);
    R(im, o + 10, 1, 12, 5, body);
    R(im, o + 12, 0, 6, 1, dark);
    R(im, o + 13, 2, 3, 2, dark);
    if (wreck) {
      R(im, o + 8, 7, 4, 2, {120, 40, 10, 255});
      R(im, o + 18, 3, 3, 2, {120, 40, 10, 255});
    } else {
      R(im, o + 6, 7, 3, 2, C_LTGREEN);  // star decal
    }
  }
}

// 48x24, faces right; frames: body, rotor blur A, rotor blur B (rotor frames are drawn over the body)
void GenHeli(Image& im) {
  Color body{60, 90, 60, 255}, light{100, 140, 90, 255}, dark{35, 55, 35, 255};
  R(im, 14, 9, 24, 10, body);
  ImageDrawCircle(&im, 36, 14, 6, body);
  R(im, 32, 9, 8, 6, {120, 190, 230, 255});  // canopy
  R(im, 34, 10, 2, 3, C_SKIN);                // pilot
  R(im, 33, 9, 3, 1, C_RED);
  R(im, 0, 11, 16, 3, body);                  // tail boom
  R(im, 0, 7, 3, 7, dark);                    // tail fin
  ImageDrawCircle(&im, 2, 9, 2, C_DKSTEEL);   // tail rotor hub
  R(im, 14, 9, 24, 1, light);
  R(im, 20, 12, 8, 3, dark);                  // door
  R(im, 22, 6, 6, 3, dark);                   // rotor mast
  R(im, 16, 21, 24, 1, C_GUN);                // skids
  R(im, 19, 19, 1, 2, C_GUN);
  R(im, 35, 19, 1, 2, C_GUN);
  R(im, 38, 16, 6, 2, C_GUN);                 // nose gun
  Color blade{40, 40, 40, 200};
  R(im, 48 + 2, 4, 44, 1, blade);
  R(im, 48 + 22, 3, 6, 2, C_GUN);
  R(im, 96 + 12, 4, 24, 1, blade);
  R(im, 96 + 20, 3, 8, 2, C_GUN);
  R(im, 96 + 0, 6, 4, 1, blade);
}

const SheetSpec kSpecs[S_COUNT] = {
    {"player_small.png", 16, 16, 7, GenPlayerSmall},
    {"player_big.png", 16, 32, 6, GenPlayerBig},
    {"goomba.png", 16, 16, 3, GenGoomba},
    {"koopa.png", 16, 24, 2, GenKoopa},
    {"shell.png", 16, 16, 1, GenShell},
    {"tiles.png", 16, 16, TF_COUNT, GenTiles},
    {"items.png", 16, 16, IF_COUNT, GenItems},
    {"flag.png", 16, 16, 3, GenFlag},
    {"castle.png", 80, 80, 1, GenCastle},
    {"cloud.png", 32, 24, 1, GenCloud},
    {"hill.png", 80, 40, 1, GenHill},
    {"bush.png", 32, 16, 1, GenBush},
    {"weapons.png", 24, 8, 2, GenWeapons},
    {"boss.png", 32, 32, 3, GenBoss},
    {"zombie.png", 16, 24, 2, GenZombie},
    {"jet.png", 32, 16, 1, GenJet},
    {"drone.png", 16, 8, 2, GenDrone},
    {"tank.png", 32, 16, 3, GenTank},
    {"heli.png", 48, 24, 3, GenHeli},
};

Sheet sheets[S_COUNT];
std::string assetDir;

void ResolveAssetDir() {
  const std::string candidates[] = {"assets/", std::string(GetApplicationDirectory()) + "assets/", ASSET_DIR};
  for (const auto& d : candidates) {
    if (DirectoryExists(d.c_str())) {
      assetDir = d;
      return;
    }
  }
  assetDir = ASSET_DIR;
}

void LoadSheets(bool dump) {
  if (dump) std::filesystem::create_directories(assetDir);
  for (int i = 0; i < S_COUNT; i++) {
    const SheetSpec& s = kSpecs[i];
    const int w = s.fw * s.frames, h = s.fh;
    std::string path = assetDir + s.file;
    Image img{};
    bool loaded = false;
    if (!dump && FileExists(path.c_str())) {
      img = LoadImage(path.c_str());
      // accept exact size or any uniform upscale of the expected layout
      if (img.data && img.width * h == img.height * w && img.width >= w) {
        if (img.width != w) ImageResizeNN(&img, w, h);
        loaded = true;
      } else {
        TraceLog(LOG_WARNING, "%s: expected %dx%d (or a uniform multiple), got %dx%d - using placeholder", path.c_str(), w, h,
                 img.width, img.height);
        UnloadImage(img);
      }
    }
    if (!loaded) {
      img = GenImageColor(w, h, BLANK);
      s.gen(img);
      if (dump) {
        ExportImage(img, path.c_str());
        printf("wrote %s\n", path.c_str());
      }
    }
    sheets[i].tex = LoadTextureFromImage(img);
    sheets[i].fw = s.fw;
    sheets[i].fh = s.fh;
    sheets[i].frames = s.frames;
    UnloadImage(img);
  }
}

void DrawFrame(SheetId id, int frame, float x, float y, bool flipX = false, bool flipY = false, Color tint = WHITE) {
  const Sheet& s = sheets[id];
  Rectangle src{(float)(frame * s.fw), 0, (float)s.fw, (float)s.fh};
  if (flipX) src.width = -src.width;
  if (flipY) src.height = -src.height;
  DrawTextureRec(s.tex, src, {floorf(x), floorf(y)}, tint);
}

// Draws a frame centered at (cx, cy), rotated by deg.
void DrawFrameRot(SheetId id, int frame, float cx, float cy, float deg, Color tint = WHITE) {
  const Sheet& s = sheets[id];
  Rectangle src{(float)(frame * s.fw), 0, (float)s.fw, (float)s.fh};
  DrawTexturePro(s.tex, src, {cx, cy, (float)s.fw, (float)s.fh}, {s.fw / 2.f, s.fh / 2.f}, deg, tint);
}

float Randf(float a, float b) { return a + (b - a) * (GetRandomValue(0, 10000) / 10000.0f); }

// ---------------------------------------------------------------------------
// Shockwave post-process shader (applied when blitting the virtual screen)
// ---------------------------------------------------------------------------
const char* kWaveFs = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec4 waves[4];   // xy = center (uv), z = radius, w = strength (in v units)
uniform int waveCount;
uniform float aspect;
out vec4 finalColor;
void main() {
  vec2 offset = vec2(0.0);
  for (int i = 0; i < 4; i++) {
    if (i >= waveCount) break;
    vec2 d = fragTexCoord - waves[i].xy;
    d.x *= aspect;
    float dist = length(d);
    float diff = dist - waves[i].z;
    float width = 0.045;
    if (dist > 0.0001 && abs(diff) < width) {
      float k = 1.0 - abs(diff) / width;
      k = k * k * (3.0 - 2.0 * k);
      vec2 dir = d / dist;
      dir.x /= aspect;
      offset += dir * k * waves[i].w;
    }
  }
  vec2 uv = fragTexCoord - offset;
  float r = texture(texture0, uv - offset * 0.35).r;
  vec4 g = texture(texture0, uv);
  float b = texture(texture0, uv + offset * 0.35).b;
  finalColor = vec4(r, g.g, b, g.a) * colDiffuse * fragColor;
}
)";

Shader waveShader{};
bool waveShaderOk = false;
int locWaves = -1, locWaveCount = -1, locAspect = -1;

// ---------------------------------------------------------------------------
// Sound (synthesized at startup, no audio files)
// ---------------------------------------------------------------------------
enum Sfx {
  SFX_JUMP, SFX_COIN, SFX_STOMP, SFX_KICK, SFX_BUMP, SFX_BREAK, SFX_SPROUT, SFX_POWERUP, SFX_SHRINK, SFX_DIE, SFX_FLAG,
  SFX_TICK, SFX_1UP, SFX_SWISH, SFX_SLICE, SFX_SQUEAL, SFX_SPLAT, SFX_SHOT, SFX_RELOAD, SFX_EMPTY, SFX_SWITCH,
  SFX_RICOCHET, SFX_ROAR, SFX_SIZZLE, SFX_BOOM, SFX_THROW, SFX_CLINK, SFX_JET, SFX_WHISTLE, SFX_RADIO, SFX_DRONE,
  SFX_GROAN, SFX_CANNON, SFX_CLANK, SFX_ROCKET, SFX_HELI, SFX_EPOP, SFX_SNIPER, SFX_SHOTGUN, SFX_PICKUP, SFX_HURT,
  SFX_COUNT
};

Sound sfx[SFX_COUNT];
bool audioOk = false;
bool muted = false;

// Layered synth: each tone() mixes into the buffer at the cursor and advances it; at() moves the cursor.
struct Synth {
  static constexpr int SR = 22050;
  enum { SQUARE, TRIANGLE, NOISE };
  std::vector<float> buf;
  size_t cur = 0;
  float phase = 0, lp = 1.0f, lpState = 0;
  unsigned rng = 12345;

  Synth& at(float sec) {
    cur = (size_t)(sec * SR);
    phase = 0;
    return *this;
  }
  Synth& lowpass(float a) {
    lp = a;
    lpState = 0;
    return *this;
  }
  Synth& rest(float dur) {
    cur += (size_t)(dur * SR);
    if (buf.size() < cur) buf.resize(cur, 0.f);
    return *this;
  }
  Synth& tone(float f0, float f1, float dur, float vol, int wave = SQUARE, float duty = 0.5f, float decay = 0.0f) {
    size_t n = (size_t)(dur * SR);
    if (buf.size() < cur + n) buf.resize(cur + n, 0.f);
    for (size_t i = 0; i < n; i++) {
      float t = (float)i / n;
      phase += (f0 + (f1 - f0) * t) / SR;
      phase -= floorf(phase);
      float v;
      if (wave == SQUARE) {
        v = phase < duty ? 1.f : -1.f;
      } else if (wave == TRIANGLE) {
        v = 4.f * fabsf(phase - 0.5f) - 1.f;
      } else {
        rng = rng * 1103515245u + 12345u;
        v = ((rng >> 16) & 0x7fff) / 16383.5f - 1.f;
        lpState += lp * (v - lpState);
        v = lpState / std::max(0.25f, sqrtf(lp));
      }
      buf[cur + i] += v * vol * (1.f - decay * t);
    }
    cur += n;
    return *this;
  }
  Synth& notes(std::initializer_list<float> freqs, float each, float vol, float duty = 0.5f) {
    for (float f : freqs) {
      if (f <= 0) rest(each);
      else tone(f, f, each, vol, SQUARE, duty, 0.3f);
    }
    return *this;
  }
  Sound build() {
    std::vector<short> pcm(buf.size());
    for (size_t i = 0; i < buf.size(); i++) pcm[i] = (short)(std::clamp(buf[i], -1.f, 1.f) * 32767);
    Wave w{};
    w.frameCount = (unsigned)pcm.size();
    w.sampleRate = SR;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = pcm.data();
    return LoadSoundFromWave(w);  // copies the samples
  }
};

void BuildSounds() {
  using S = Synth;
  sfx[SFX_JUMP] = S().tone(250, 700, 0.16f, 0.22f, S::SQUARE, 0.25f, 0.6f).build();
  sfx[SFX_COIN] = S().tone(988, 988, 0.06f, 0.2f).tone(1319, 1319, 0.35f, 0.2f, S::SQUARE, 0.5f, 1.0f).build();
  sfx[SFX_STOMP] = S().tone(600, 150, 0.1f, 0.3f, S::SQUARE, 0.5f, 0.5f).build();
  sfx[SFX_KICK] = S().tone(1000, 400, 0.06f, 0.25f, S::SQUARE, 0.5f, 0.5f).build();
  sfx[SFX_BUMP] = S().tone(160, 90, 0.08f, 0.5f, S::TRIANGLE, 0.5f, 0.5f).build();
  sfx[SFX_BREAK] = S().tone(0, 0, 0.25f, 0.3f, S::NOISE, 0.5f, 1.0f).build();
  sfx[SFX_SPROUT] = S().notes({300, 400, 500, 600, 700, 800}, 0.04f, 0.2f, 0.25f).build();
  sfx[SFX_POWERUP] = S().notes({523, 659, 784, 1047, 659, 784, 1047, 1319}, 0.05f, 0.2f).build();
  sfx[SFX_SHRINK] = S().notes({1047, 784, 659, 523, 392, 330}, 0.05f, 0.2f).build();
  sfx[SFX_DIE] = S().notes({784, 740, 698, 0, 587, 523, 466, 440, 392, 330, 262}, 0.1f, 0.22f).build();
  sfx[SFX_FLAG] = S().tone(1200, 200, 1.0f, 0.2f, S::SQUARE, 0.5f, 0.3f).build();
  sfx[SFX_TICK] = S().tone(1800, 1800, 0.02f, 0.1f).build();
  sfx[SFX_1UP] = S().notes({1319, 1568, 2637, 2093, 2349, 3136}, 0.07f, 0.2f).build();
  sfx[SFX_SWISH] = S().lowpass(0.45f).tone(0, 0, 0.14f, 0.5f, S::NOISE, 0.5f, 1.0f)
                       .at(0).tone(1800, 500, 0.1f, 0.06f, S::TRIANGLE, 0.5f, 1.0f).build();
  sfx[SFX_SLICE] = S().lowpass(0.7f).tone(0, 0, 0.05f, 0.6f, S::NOISE, 0.5f, 0.6f)
                       .lowpass(0.1f).tone(0, 0, 0.32f, 0.9f, S::NOISE, 0.5f, 1.0f)
                       .at(0.03f).tone(200, 45, 0.28f, 0.4f, S::TRIANGLE, 0.5f, 1.0f).build();
  sfx[SFX_SQUEAL] = S().at(0.04f).tone(900, 1700, 0.07f, 0.13f, S::SQUARE, 0.25f)
                        .tone(1700, 600, 0.16f, 0.13f, S::SQUARE, 0.25f, 1.0f).build();
  sfx[SFX_SPLAT] = S().lowpass(0.08f).tone(0, 0, 0.2f, 1.0f, S::NOISE, 0.5f, 1.0f)
                       .at(0).tone(130, 40, 0.16f, 0.4f, S::TRIANGLE, 0.5f, 1.0f).build();
  sfx[SFX_SHOT] = S().lowpass(0.8f).tone(0, 0, 0.03f, 0.9f, S::NOISE, 0.5f, 0.3f)
                      .lowpass(0.18f).tone(0, 0, 0.2f, 0.9f, S::NOISE, 0.5f, 1.0f)
                      .at(0).tone(170, 45, 0.1f, 0.45f, S::SQUARE, 0.5f, 1.0f).build();
  sfx[SFX_RELOAD] = S().tone(2200, 2200, 0.02f, 0.15f).rest(0.35f).tone(1400, 1400, 0.03f, 0.18f)
                        .rest(0.45f).tone(2600, 2600, 0.025f, 0.15f).rest(0.06f).tone(1800, 1800, 0.03f, 0.15f).build();
  sfx[SFX_EMPTY] = S().tone(2500, 2500, 0.015f, 0.12f).build();
  sfx[SFX_SWITCH] = S().tone(1200, 1200, 0.02f, 0.12f).rest(0.04f).tone(1800, 1800, 0.03f, 0.12f).build();
  sfx[SFX_RICOCHET] = S().tone(3200, 1300, 0.12f, 0.07f, S::TRIANGLE, 0.5f, 1.0f).build();
  sfx[SFX_ROAR] = S().lowpass(0.12f).tone(0, 0, 0.6f, 0.8f, S::NOISE, 0.5f, 0.8f)
                      .at(0).tone(90, 60, 0.6f, 0.25f, S::SQUARE, 0.4f, 0.8f).build();
  sfx[SFX_SIZZLE] = S().lowpass(0.6f).tone(0, 0, 0.5f, 0.35f, S::NOISE, 0.5f, 1.0f).build();
  sfx[SFX_BOOM] = S().lowpass(0.5f).tone(0, 0, 0.08f, 0.7f, S::NOISE, 0.5f, 1.0f)
                      .at(0).lowpass(0.05f).tone(0, 0, 1.2f, 1.0f, S::NOISE, 0.5f, 1.0f)
                      .at(0).tone(90, 28, 0.55f, 0.5f, S::SQUARE, 0.5f, 1.0f).build();
  sfx[SFX_THROW] = S().lowpass(0.3f).tone(0, 0, 0.12f, 0.4f, S::NOISE, 0.5f, 1.0f).build();
  sfx[SFX_CLINK] = S().tone(3000, 2800, 0.02f, 0.08f, S::TRIANGLE).build();
  sfx[SFX_JET] = S().lowpass(0.2f).tone(0, 0, 2.2f, 0.6f, S::NOISE, 0.5f, 0.95f)
                     .at(0).tone(520, 240, 2.2f, 0.05f, S::TRIANGLE, 0.5f, 0.95f).build();
  sfx[SFX_WHISTLE] = S().tone(2400, 700, 0.8f, 0.09f, S::TRIANGLE, 0.5f, 0.3f).build();
  sfx[SFX_RADIO] = S().notes({1200, 0, 1200, 0, 1600}, 0.06f, 0.12f).build();
  sfx[SFX_DRONE] = S().tone(170, 172, 0.5f, 0.06f, S::SQUARE, 0.3f)
                       .at(0).tone(345, 348, 0.5f, 0.04f, S::TRIANGLE).build();
  sfx[SFX_GROAN] = S().lowpass(0.05f).tone(0, 0, 0.7f, 0.5f, S::NOISE, 0.5f, 0.5f)
                       .at(0).tone(110, 75, 0.7f, 0.12f, S::SQUARE, 0.3f, 0.6f).build();
  sfx[SFX_CANNON] = S().lowpass(0.6f).tone(0, 0, 0.05f, 0.8f, S::NOISE, 0.5f, 0.5f)
                        .lowpass(0.08f).tone(0, 0, 0.5f, 1.0f, S::NOISE, 0.5f, 1.0f)
                        .at(0).tone(130, 45, 0.3f, 0.45f, S::SQUARE, 0.5f, 1.0f).build();
  sfx[SFX_CLANK] = S().tone(1100, 700, 0.09f, 0.18f, S::TRIANGLE, 0.5f, 1.0f)
                       .at(0).lowpass(0.7f).tone(0, 0, 0.03f, 0.4f, S::NOISE, 0.5f, 1.0f).build();
  sfx[SFX_ROCKET] = S().lowpass(0.15f).tone(0, 0, 0.5f, 0.7f, S::NOISE, 0.5f, 0.9f)
                        .at(0).tone(300, 900, 0.3f, 0.06f, S::TRIANGLE, 0.5f, 1.0f).build();
  {
    S heli;  // rotor chop loop: 6 thumps per 0.5 s over a low hum
    for (int i = 0; i < 6; i++) heli.lowpass(0.06f).tone(0, 0, 0.04f, 0.9f, S::NOISE, 0.5f, 1.0f).rest(0.0433f);
    heli.at(0).tone(95, 95, 0.5f, 0.05f, S::TRIANGLE);
    sfx[SFX_HELI] = heli.build();
  }
  sfx[SFX_EPOP] = S().lowpass(0.5f).tone(0, 0, 0.09f, 0.5f, S::NOISE, 0.5f, 1.0f)
                      .at(0).tone(500, 200, 0.05f, 0.15f, S::SQUARE, 0.5f, 1.0f).build();
  sfx[SFX_SNIPER] = S().lowpass(0.95f).tone(0, 0, 0.02f, 1.0f, S::NOISE, 0.5f, 0.2f)
                        .lowpass(0.12f).tone(0, 0, 0.9f, 0.9f, S::NOISE, 0.5f, 1.0f)
                        .at(0).tone(220, 40, 0.18f, 0.5f, S::SQUARE, 0.5f, 1.0f).build();
  sfx[SFX_SHOTGUN] = S().lowpass(0.6f).tone(0, 0, 0.04f, 1.0f, S::NOISE, 0.5f, 0.4f)
                         .lowpass(0.1f).tone(0, 0, 0.4f, 1.0f, S::NOISE, 0.5f, 1.0f)
                         .at(0).tone(140, 40, 0.2f, 0.5f, S::SQUARE, 0.5f, 1.0f).build();
  sfx[SFX_PICKUP] = S().notes({880, 1320}, 0.05f, 0.18f).build();
  sfx[SFX_HURT] = S().tone(320, 110, 0.15f, 0.25f, S::SQUARE, 0.4f, 0.8f)
                      .at(0).lowpass(0.2f).tone(0, 0, 0.12f, 0.5f, S::NOISE, 0.5f, 1.0f).build();
}

void Play(Sfx s) {
  if (audioOk && !muted) PlaySound(sfx[s]);
}

// ---------------------------------------------------------------------------
// Level grid
// ---------------------------------------------------------------------------
// Tile chars: '.' empty, G ground, B brick, ? coin block, M mushroom block, U used block, X hard block,
// < > pipe top, ( ) pipe body, o floating coin, S castle stone, L lava, [ = ] treetop, | trunk,
// b bridge, A axe.
int LW = 212;
std::vector<char> tiles;
std::vector<float> bumpT;  // block bounce animation timers

char& Tile(int x, int y) { return tiles[y * LW + x]; }
char TileAt(int x, int y) { return (x < 0 || x >= LW || y < 0 || y >= LH) ? '.' : Tile(x, y); }

bool IsSolidChar(char c) { return c != '.' && c != 'o' && c != 'L' && c != '|' && c != 'A'; }

bool Solid(int tx, int ty) {
  if (tx < 0 || tx >= LW) return true;
  if (ty < 0 || ty >= LH) return false;
  return IsSolidChar(Tile(tx, ty));
}

bool SolidAtPx(float x, float y) { return Solid((int)floorf(x / TILE), (int)floorf(y / TILE)); }

// Move along X with tile collision; returns true when blocked.
bool MoveX(float& x, float y, float w, float h, float dx) {
  x += dx;
  int top = (int)floorf(y / TILE), bot = (int)floorf((y + h - 0.01f) / TILE);
  if (dx > 0) {
    int tx = (int)floorf((x + w - 0.01f) / TILE);
    for (int ty = top; ty <= bot; ty++)
      if (Solid(tx, ty)) {
        x = (float)(tx * TILE) - w;
        return true;
      }
  } else if (dx < 0) {
    int tx = (int)floorf(x / TILE);
    for (int ty = top; ty <= bot; ty++)
      if (Solid(tx, ty)) {
        x = (float)((tx + 1) * TILE);
        return true;
      }
  }
  return false;
}

// Move along Y with tile collision; reports the tile hit (center column preferred when rising).
bool MoveY(float x, float& y, float w, float h, float dy, int& hitTx, int& hitTy) {
  y += dy;
  int l = (int)floorf(x / TILE), r = (int)floorf((x + w - 0.01f) / TILE);
  if (dy > 0) {
    int ty = (int)floorf((y + h - 0.01f) / TILE);
    for (int tx = l; tx <= r; tx++)
      if (Solid(tx, ty)) {
        y = (float)(ty * TILE) - h;
        hitTx = tx;
        hitTy = ty;
        return true;
      }
  } else if (dy < 0) {
    int ty = (int)floorf(y / TILE);
    int cx = (int)floorf((x + w / 2) / TILE);
    int cand[3] = {cx, l, r};
    for (int tx : cand)
      if (Solid(tx, ty)) {
        y = (float)((ty + 1) * TILE);
        hitTx = tx;
        hitTy = ty;
        return true;
      }
  }
  return false;
}

bool TouchesTile(Rectangle r, char c) {
  int l = (int)floorf(r.x / TILE), rr = (int)floorf((r.x + r.width - 0.01f) / TILE);
  int t = (int)floorf(r.y / TILE), b = (int)floorf((r.y + r.height - 0.01f) / TILE);
  for (int ty = t; ty <= b; ty++)
    for (int tx = l; tx <= rr; tx++)
      if (TileAt(tx, ty) == c) return true;
  return false;
}

// ---------------------------------------------------------------------------
// Entities
// ---------------------------------------------------------------------------
enum EType { GOOMBA, KOOPA, ZOMBIE, TANK };
enum EState { WALK, SQUISHED, SHELL, SHELL_MOVING, FLIPPED, WRECK };

struct Enemy {
  EType type = GOOMBA;
  EState st = WALK;
  float x = 0, y = 0, vx = -ENEMY_SPEED, vy = 0;
  bool active = false, alive = true, onGround = false;
  bool smart = false;  // turns around at ledges
  float t = 0, kickGrace = 0, animT = 0;
  // zombies and tanks
  int hp = 1;
  float flashT = 0, groanT = 0, emerge = 0;
  bool faceRight = false;
  // tanks: barrel angle in degrees (0 = straight ahead, negative = raised) and recoil kick
  float aim = 0, recoil = 0;
  // goombas/koopas carrying a pistol
  bool armed = false;
  float gunT = 0, gunFlash = 0;
};

struct EnemyBullet {
  float x, y, vx, vy, life;
};

struct EnemyHeli {
  float x = 0, y = 0, vx = 0, vy = 0;  // hitbox top-left, same size as the player's
  int hp = 10;
  bool alive = true, active = false, crashing = false, faceRight = false;
  float fireT = 2, rocketT = 4, burstT = 0, flashT = 0, tilt = 0, spin = 0;
  int burstLeft = 0;
};

struct Item {  // mushroom
  float x, y, vx = 0, vy = 0;
  float emerge = 16;
  bool alive = true;
};

enum FxType { FX_COIN, FX_DEBRIS };
struct Effect {
  FxType type;
  float x, y, vx, vy, t = 0;
  int part = 0;
};

struct Popup {
  float x, y, t = 0;
  std::string text;
};

enum PKind { P_BLOOD, P_SPARK, P_CASING, P_LAVA, P_SMOKE, P_FIRE };
struct Particle {
  PKind kind;
  float x, y, vx, vy, life, size;
  Color c;
  bool stuck = false;
};

// One half of a sliced enemy, rendered as a rotated sub-rectangle of its sprite.
struct Half {
  SheetId sheet;
  int frame;
  float srcY, srcH, fw;
  float x, y, vx, vy, rot, vrot;  // x,y = center
  bool flip, top;
  float t = 0, gushT = 0;
  bool landed = false, bounced = false;
};

struct Bullet {
  float x, y, vx, vy, life;
};

struct Shockwave {
  float x, y, t, dur, maxR, strength;
};

struct Firebar {
  float cx, cy, angle, speed;
  int n;
};

struct Flame {
  float x, y, vx, targetY, t = 0;
  bool alive = true;
};

struct Boss {
  bool present = false, alive = false, active = false, onGround = false, faceRight = false, splashed = false;
  float x = 0, y = 0, vx = -20, vy = 0;
  int hp = BOSS_HP;
  float jumpT = 2, fireT = 2.5f, mouthT = 0, flashT = 0, animT = 0;
  float minX = 0, maxX = 0;
};

enum ProjKind { PJ_GRENADE, PJ_BOMB, PJ_DRONE_BOMB, PJ_ROCKET, PJ_TANK_SHELL, PJ_ENEMY_ROCKET };
struct Projectile {
  ProjKind kind;
  float x, y, vx, vy, fuse;
  float rot = 0;
  bool alive = true;
};

struct Explosion {
  float x, y, t, r;
};

struct Jet {
  bool active = false;
  float x = 0, y = 0, vx = 0;
  float targets[6] = {};
  int next = 0;
};

struct Drone {
  bool active = false;
  float x = 0, y = 0, vx = 0, vy = 0, battery = 0, t = 0;
  int payload = 0;
};

struct Heli {
  bool present = false, alive = true, onGround = false, faceRight = true, overheated = false;
  float x = 0, y = 0, vx = 0, vy = 0;  // hitbox top-left
  int hp = HELI_HP, rockets = HELI_ROCKETS;
  float heat = 0, fireCD = 0, rocketCD = 0, hitT = 0, rotor = 0, tilt = 0, flashT = 0;
};

struct Player {
  float x = 0, y = 0, vx = 0, vy = 0;
  bool big = false, onGround = false, faceRight = true;
  float invuln = 0, animT = 0;
  int pose = POSE_IDLE;
  float attackT = 0, attackCD = 0, fireCD = 0, flashT = 0, reloadT = 0;
  int ammo = AK_MAG;
  int grenades = START_GRENADES, airstrikes = START_AIRSTRIKES, drones = START_DRONES;
};

std::vector<Enemy> enemies;
std::vector<Item> items;
std::vector<Effect> effects;
std::vector<Popup> popups;
std::vector<Particle> particles;
std::vector<Half> halves;
std::vector<Bullet> bullets;
std::vector<Shockwave> shockwaves;
std::vector<Firebar> firebars;
std::vector<Flame> flames;
std::vector<Projectile> projectiles;
std::vector<Explosion> explosions;
Jet jet;
Drone drone;
Heli heli;
bool inHeli = false;
std::vector<EnemyBullet> enemyBullets;
std::vector<EnemyHeli> enemyHelis;
int armCounter = 0;  // every second goomba/koopa gets a pistol
Boss boss;
Player p;
Weapon weapon = W_KNIFE;

float PW() { return 12.0f; }
float PH() { return p.big ? 32.0f : 16.0f; }

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------
enum State { ST_TITLE, ST_INTRO, ST_PLAYING, ST_DYING, ST_GOAL, ST_CLEAR, ST_GAMEOVER };
enum GoalPhase { GP_SLIDE, GP_WAIT, GP_WALK, GP_TALLY, GP_DONE, GP_COLLAPSE, GP_AFTER_COLLAPSE };

State state = ST_TITLE;
float stateT = 0, globalT = 0;
int score = 0, coins = 0, lives = 3, timeLeft = 400;
int levelIdx = 0, titleLevel = 0;
int titleMode = 0;  // 0 = 2D campaign, 1 = 3D arena
bool runArena = false;
float timeAcc = 0, camX = 0;
float transformT = 0;
bool transformToBig = false;
bool deathByFall = false, deathJumped = false;
int goalPhase = GP_SLIDE;
float flagY = 0;
bool playerHidden = false;
bool paused = false;
bool jumpLatch = false, enterLatch = false, attackLatch = false, reloadLatch = false;
bool grenadeLatch = false, airstrikeLatch = false, droneLatch = false, heliLatch = false;
int switchLatch = 0;
float poleX = -1000, castleX = -1000;
float hitStop = 0, shakeT = 0, shakeAmp = 0;
bool droneMode = false;   // player is piloting the FPV drone
bool camReturn = false;   // camera gliding back to the player after a drone flight
std::string toast;
float toastT = 0;

// ---------------------------------------------------------------------------
// Level builders
// ---------------------------------------------------------------------------
void InitLevel(int w) {
  LW = w;
  tiles.assign(LW * LH, '.');
  bumpT.assign(LW * LH, 0.f);
  enemies.clear();
  items.clear();
  effects.clear();
  popups.clear();
  particles.clear();
  halves.clear();
  bullets.clear();
  shockwaves.clear();
  firebars.clear();
  flames.clear();
  projectiles.clear();
  explosions.clear();
  jet = Jet{};
  drone = Drone{};
  droneMode = camReturn = false;
  toastT = 0;
  heli = Heli{};
  inHeli = false;
  enemyBullets.clear();
  enemyHelis.clear();
  armCounter = 0;
  if (audioOk) {
    StopSound(sfx[SFX_DRONE]);
    StopSound(sfx[SFX_HELI]);
  }
  boss = Boss{};
  poleX = castleX = -1000;
}

void SetT(int x, int y, char c) {
  if (x >= 0 && x < LW && y >= 0 && y < LH) Tile(x, y) = c;
}
void Row(int x0, int x1, int y, char c) {
  for (int x = x0; x <= x1; x++) SetT(x, y, c);
}
void Fill(int x0, int x1, int y0, int y1, char c) {
  for (int y = y0; y <= y1; y++) Row(x0, x1, y, c);
}
void Ground(int x0, int x1) { Fill(x0, x1, 12, 13, 'G'); }
void Pipe(int x, int h) {
  int top = 12 - h;
  SetT(x, top, '<');
  SetT(x + 1, top, '>');
  for (int y = top + 1; y < 12; y++) {
    SetT(x, y, '(');
    SetT(x + 1, y, ')');
  }
}
void Column(int x, int h, char c = 'X') {
  for (int k = 0; k < h; k++) SetT(x, 11 - k, c);
}
void StairsUp(int x, int n) {
  for (int i = 0; i < n; i++) Column(x + i, i + 1);
}
void StairsDown(int x, int n) {
  for (int i = 0; i < n; i++) Column(x + i, n - i);
}
void Tree(int x, int w, int top) {
  SetT(x, top, '[');
  Row(x + 1, x + w - 2, top, '=');
  SetT(x + w - 1, top, ']');
  int mid = x + w / 2;
  Fill(w >= 4 ? mid - 1 : mid, mid, top + 1, 13, '|');
}
void Arm(Enemy& e) {
  e.armed = (armCounter++ % 2) == 1;
  e.gunT = Randf(1.0f, 2.5f);
}
void Goomba(int tx, int ty = 11) {
  Enemy e;
  e.type = GOOMBA;
  e.x = (float)(tx * TILE);
  e.y = (float)(ty * TILE);
  Arm(e);
  enemies.push_back(e);
}
void Koopa(int tx, int ty = 11, bool smart = false) {
  Enemy e;
  e.type = KOOPA;
  e.x = (float)(tx * TILE);
  e.y = (float)(ty * TILE);
  e.smart = smart;
  Arm(e);
  enemies.push_back(e);
}
void AttackHeli(int tx, int ty) {
  EnemyHeli h;
  h.x = (float)(tx * TILE);
  h.y = (float)(ty * TILE);
  h.fireT = Randf(1.5f, 2.5f);
  h.rocketT = Randf(3.5f, 5.0f);
  enemyHelis.push_back(h);
}
void Zombie(int tx, int ty = 11) {
  Enemy e;
  e.type = ZOMBIE;
  e.x = (float)(tx * TILE);
  e.y = (float)(ty * TILE);
  e.vx = -ZOMBIE_SPEED;
  e.smart = true;
  e.hp = ZOMBIE_HP;
  e.emerge = 0.8f;
  e.groanT = Randf(1, 4);
  enemies.push_back(e);
}
void Tank(int tx, int ty = 11) {
  Enemy e;
  e.type = TANK;
  e.x = (float)(tx * TILE);
  e.y = (float)(ty * TILE);
  e.vx = -TANK_SPEED;
  e.hp = TANK_HP;
  e.t = Randf(1.0f, 2.0f);  // time until first shot
  enemies.push_back(e);
}
void ParkHeli(int tx) {
  heli.present = true;
  heli.x = (float)(tx * TILE);
  heli.y = GROUND_Y - HELI_H;
}
void AddFirebar(int tx, int ty, float speed, int n = 6) {
  SetT(tx, ty, 'U');
  firebars.push_back({(float)(tx * TILE + 8), (float)(ty * TILE + 8), 0, speed, n});
}
void FlagAndCastle(int poleCol, int castleCol) {
  Column(poleCol, 1);
  poleX = (float)(poleCol * TILE + 8);
  castleX = (float)(castleCol * TILE);
}

// 1-1: overworld
void BuildLevel1() {
  InitLevel(212);
  auto isPit = [](int x) { return (x >= 69 && x <= 70) || (x >= 86 && x <= 88) || (x >= 153 && x <= 154); };
  for (int x = 0; x < LW; x++)
    if (!isPit(x)) Ground(x, x);

  SetT(16, 8, '?');
  SetT(20, 8, 'B'); SetT(21, 8, 'M'); SetT(22, 8, 'B'); SetT(23, 8, '?'); SetT(24, 8, 'B');
  SetT(22, 4, '?');
  Pipe(28, 2); Pipe(38, 3); Pipe(46, 4); Pipe(57, 4);
  SetT(69, 8, 'o'); SetT(70, 8, 'o');
  SetT(77, 8, 'B'); SetT(78, 8, 'M'); SetT(79, 8, 'B');
  Row(80, 87, 4, 'B');
  Row(86, 88, 7, 'o');
  Row(91, 93, 4, 'B'); SetT(94, 4, '?');
  SetT(94, 8, '?');
  SetT(100, 8, 'B'); SetT(101, 8, 'B');
  SetT(106, 8, '?'); SetT(109, 8, '?'); SetT(109, 4, 'M'); SetT(112, 8, '?');
  SetT(118, 8, 'B');
  Row(121, 123, 4, 'B');
  SetT(128, 4, 'B'); SetT(129, 4, '?'); SetT(130, 4, '?'); SetT(131, 4, 'B');
  SetT(129, 8, 'B'); SetT(130, 8, 'B');
  StairsUp(134, 4); StairsDown(140, 4);
  StairsUp(148, 4); Column(152, 4);
  StairsDown(155, 4);
  Pipe(163, 2);
  SetT(168, 8, 'B'); SetT(169, 8, 'B'); SetT(170, 8, '?'); SetT(171, 8, 'B');
  Pipe(179, 2);
  StairsUp(181, 8); Column(189, 8);
  FlagAndCastle(198, 202);

  Goomba(22); Goomba(40); Goomba(51); Goomba(53);
  Goomba(80, 3); Goomba(82, 3);
  Goomba(97); Goomba(99);
  Koopa(107);
  Goomba(114); Goomba(116);
  Goomba(124); Goomba(126); Goomba(128); Goomba(130);
  Koopa(160);
  Goomba(174); Goomba(176);
  Zombie(62); Zombie(92); Zombie(145); Zombie(192);
  Tank(73); Tank(165);
  ParkHeli(7);
  AttackHeli(120, 3); AttackHeli(178, 3);
}

// 1-2: underground
void BuildLevel2() {
  InitLevel(196);
  auto isPit = [](int x) { return (x >= 60 && x <= 61) || (x >= 120 && x <= 122) || (x >= 141 && x <= 143); };
  for (int x = 0; x < LW; x++)
    if (!isPit(x)) Ground(x, x);
  Fill(0, 0, 2, 11, 'B');
  Row(6, 160, 1, 'B');

  Row(10, 14, 8, '?'); SetT(10, 8, 'M');
  StairsUp(17, 4);
  Fill(25, 26, 5, 8, 'B');
  Row(34, 38, 8, 'B'); Row(34, 38, 7, 'o');
  Row(40, 44, 4, 'B'); Row(40, 44, 5, 'o');
  Row(46, 50, 2, 'o');
  Fill(52, 53, 8, 11, 'B');
  Pipe(64, 3); Pipe(70, 4); Pipe(76, 2);
  Row(84, 95, 7, 'B'); SetT(90, 7, 'M');
  Row(85, 94, 3, 'o');
  Row(108, 109, 9, 'B');
  Row(111, 112, 6, 'B'); Row(111, 112, 5, 'o');
  Row(120, 122, 8, 'o');
  StairsUp(130, 4);
  Fill(134, 140, 8, 11, 'X');
  Fill(144, 147, 8, 11, 'X');
  StairsDown(148, 4);
  SetT(151, 8, '?');
  Pipe(158, 2);
  StairsUp(165, 8); Column(173, 8);
  FlagAndCastle(182, 186);

  Goomba(15); Goomba(23); Goomba(33); Goomba(36, 7); Goomba(44); Goomba(48); Goomba(56); Goomba(68);
  Goomba(86, 6); Goomba(89, 6);
  Goomba(98); Koopa(100); Koopa(104); Goomba(106);
  Goomba(115); Goomba(117); Goomba(126);
  Goomba(137, 7); Koopa(145, 7);
  Goomba(154); Goomba(156);
  Zombie(30); Zombie(81); Zombie(126); Zombie(162);
  Tank(112); Tank(176);
  ParkHeli(5);
}

// 1-3: treetops
void BuildLevel3() {
  InitLevel(182);
  Ground(0, 15);
  Ground(160, 181);
  struct T { int x, w, top; };
  const T trees[] = {{18, 4, 10}, {24, 6, 7},  {32, 5, 9},   {39, 3, 6},   {44, 8, 10}, {55, 4, 8},
                     {61, 5, 5},  {69, 7, 9},  {79, 4, 7},   {86, 6, 10},  {95, 3, 8},  {100, 5, 6},
                     {108, 8, 9}, {119, 4, 7}, {126, 5, 10}, {134, 6, 8},  {143, 4, 6}, {150, 6, 9}};
  for (const T& t : trees) Tree(t.x, t.w, t.top);

  Row(25, 28, 5, 'o'); Row(45, 50, 7, 'o'); Row(87, 90, 6, 'o');
  Row(109, 114, 5, 'o'); Row(135, 138, 4, 'o'); Row(151, 154, 5, 'o');
  SetT(47, 6, '?'); SetT(72, 5, 'M'); SetT(110, 4, '?'); SetT(128, 6, '?');
  FlagAndCastle(170, 174);

  Koopa(26, 6, true); Koopa(57, 7, true); Koopa(63, 4, true);
  Koopa(102, 5, true); Koopa(121, 6, true); Koopa(152, 8, true);
  Goomba(34, 8); Goomba(46, 9); Goomba(48, 9); Goomba(70, 8); Goomba(88, 9);
  Goomba(110, 8); Goomba(112, 8); Goomba(128, 9); Goomba(136, 7);
  Goomba(164); Goomba(166);
  Zombie(50, 9); Zombie(89, 9); Zombie(137, 7); Zombie(168);
  Tank(110, 8); Tank(162);
  ParkHeli(5);
  AttackHeli(95, 2); AttackHeli(150, 2);
}

// 1-4: castle with lava, firebars, boss and axe
void BuildLevel4() {
  InitLevel(160);
  Fill(0, LW - 1, 0, 1, 'S');
  auto isPit = [](int x) {
    return (x >= 14 && x <= 16) || (x >= 30 && x <= 33) || (x >= 52 && x <= 55) || (x >= 75 && x <= 78) ||
           (x >= 100 && x <= 103);
  };
  for (int x = 0; x <= 121; x++) {
    if (isPit(x)) SetT(x, 13, 'L');
    else Fill(x, x, 12, 13, 'S');
  }
  Fill(0, 1, 2, 11, 'S');

  Row(24, 28, 8, 'S'); SetT(26, 4, 'M'); Row(25, 27, 7, 'o');
  Row(44, 48, 6, 'S'); SetT(46, 2, '?');
  Fill(66, 72, 2, 6, 'S');
  Row(84, 90, 8, 'S');
  Row(108, 113, 7, 'S'); SetT(110, 3, '?');
  AddFirebar(20, 8, 2.0f);
  AddFirebar(40, 7, -2.2f);
  AddFirebar(62, 8, 2.4f);
  AddFirebar(87, 4, -1.8f);
  AddFirebar(118, 8, -2.6f);

  // bridge over lava, boss, axe
  Fill(122, 124, 9, 13, 'S');
  Fill(125, 147, 12, 13, 'L');
  Row(125, 147, 9, 'b');
  Fill(148, 151, 9, 13, 'S');
  SetT(148, 8, 'A');
  Fill(152, 159, 2, 13, 'S');

  boss.present = boss.alive = true;
  boss.x = 140.0f * TILE;
  boss.y = 9.0f * TILE - 32;
  boss.minX = 130.0f * TILE;
  boss.maxX = 145.0f * TILE;

  Goomba(22); Goomba(36); Goomba(46, 5); Koopa(58); Goomba(66); Goomba(80); Goomba(88, 7);
  Koopa(95); Goomba(106); Goomba(111, 6);
  Zombie(47, 5); Zombie(70); Zombie(92); Zombie(115);
  Tank(94); Tank(106);
  ParkHeli(4);
}

struct LevelInfo {
  const char* name;
  Color sky;
  int groundFrame, brickFrame;
  bool hills, clouds, axeGoal;
  int time;
  void (*build)();
};

const LevelInfo kLevels[] = {
    {"1-1", C_SKY, TF_GROUND, TF_BRICK, true, true, false, 400, BuildLevel1},
    {"1-2", BLACK, TF_GROUND_UG, TF_BRICK_UG, false, false, false, 400, BuildLevel2},
    {"1-3", C_SKY, TF_GROUND, TF_BRICK, false, true, false, 300, BuildLevel3},
    {"1-4", BLACK, TF_STONE, TF_STONE, false, false, true, 300, BuildLevel4},
};
constexpr int LEVEL_COUNT = (int)(sizeof(kLevels) / sizeof(kLevels[0]));

const LevelInfo& Lvl() { return kLevels[levelIdx]; }

void ResetWorld(int level, bool keepBig) {
  levelIdx = level;
  Lvl().build();
  bool big = keepBig && p.big;
  p = Player{};
  p.big = big;
  p.x = 3 * TILE;
  p.y = GROUND_Y - PH();
  camX = 0;
  timeLeft = Lvl().time;
  timeAcc = 0;
  transformT = 0;
  flagY = POLE_TOP_Y + 4;
  playerHidden = false;
  paused = false;
  hitStop = shakeT = 0;
}

void StartLevel(int level, bool keepBig) {
  ResetWorld(level, keepBig);
  state = ST_INTRO;
  stateT = 0;
}

void NewGame(int level) {
  score = 0;
  coins = 0;
  lives = 3;
  weapon = W_KNIFE;
  p.big = false;
  StartLevel(level, false);
}

// ---------------------------------------------------------------------------
// Effects helpers
// ---------------------------------------------------------------------------
void AddScore(int pts, float x, float y) {
  score += pts;
  popups.push_back({x, y, 0, std::to_string(pts)});
}

void AddCoin() {
  Play(SFX_COIN);
  if (++coins >= 100) {
    coins -= 100;
    lives++;
    Play(SFX_1UP);
  }
}

void Toast(const char* msg) {
  toast = msg;
  toastT = 1.6f;
}

void Shake(float t, float amp) {
  shakeT = std::max(shakeT, t);
  shakeAmp = std::max(shakeAmp, amp);
}

void AddShockwave(float x, float y, float maxR, float strength, float dur) {
  if (shockwaves.size() >= 4) shockwaves.erase(shockwaves.begin());
  shockwaves.push_back({x, y, 0, dur, maxR, strength});
}

Color BloodColor() {
  static const Color reds[4] = {{150, 0, 0, 255}, {190, 10, 10, 255}, {120, 0, 8, 255}, {220, 30, 20, 255}};
  return reds[GetRandomValue(0, 3)];
}

void AddParticle(PKind kind, float x, float y, float vx, float vy, float life, float size, Color c) {
  particles.push_back({kind, x, y, vx, vy, life, size, c});
}

void BloodBurst(float x, float y, float dir, int n, float power = 1.0f) {
  for (int i = 0; i < n; i++) {
    float vx = (dir * Randf(20, 230) + Randf(-70, 70)) * power;
    float vy = Randf(-340, -30) * power;
    AddParticle(P_BLOOD, x + Randf(-3, 3), y + Randf(-3, 3), vx, vy, Randf(3.0f, 5.5f), Randf(1.0f, 2.6f), BloodColor());
  }
}

void Sparks(float x, float y, float dir, int n) {
  for (int i = 0; i < n; i++)
    AddParticle(P_SPARK, x, y, -dir * Randf(30, 160), Randf(-120, 120), Randf(0.08f, 0.22f), 1, {255, 230, 120, 255});
}

void LavaSplash(float x, float y) {
  for (int i = 0; i < 18; i++)
    AddParticle(P_LAVA, x + Randf(-6, 6), y, Randf(-80, 80), Randf(-260, -80), Randf(0.6f, 1.2f), Randf(1, 2.5f),
                GetRandomValue(0, 1) ? C_LAVA : C_LAVATOP);
  Play(SFX_SIZZLE);
}

// Split a sprite (frame of `sheet`, drawn with top-left at sx,sy) into two flying halves.
void SpawnHalves(SheetId sheet, int frame, float sx, float sy, float fw, float fh, bool flip, float dir, float power) {
  float cut = floorf(fh / 2);
  Half top{sheet, frame, 0, cut, fw, sx + fw / 2, sy + cut / 2, dir * Randf(70, 120) * power, -Randf(240, 300) * power,
           0, dir * Randf(7, 11), flip, true};
  Half bot{sheet, frame, cut, fh - cut, fw, sx + fw / 2, sy + cut + (fh - cut) / 2, dir * 25, -60, 0, dir * 1.5f, flip,
           false};
  bot.gushT = 0.9f;
  halves.push_back(top);
  halves.push_back(bot);
}

// ---------------------------------------------------------------------------
// Gameplay
// ---------------------------------------------------------------------------
void KillEnemy(Enemy& e, float dir) {
  e.st = FLIPPED;
  e.vy = -250;
  e.vx = dir * 50;
  AddScore(100, e.x, e.y);
  Play(SFX_KICK);
}

Rectangle EnemyBody(const Enemy& e) {
  if (e.type == TANK) return {e.x, e.y, 32, 16};
  if ((e.type == KOOPA || e.type == ZOMBIE) && e.st == WALK) return {e.x, e.y - 8, 16, 24};
  return {e.x, e.y, 16, 16};
}

bool EnemyHittable(const Enemy& e) {
  return e.alive && e.active && e.st != FLIPPED && e.st != SQUISHED && e.st != WRECK;
}

void Explode(float x, float y, float radius, bool hurtsPlayer, bool damages = true);

// Armor: sparks and a clank per hit; at 0 HP the tank blows up and leaves a burning wreck.
void DamageTank(Enemy& e, int dmg, float dir) {
  e.hp -= dmg;
  e.flashT = 0.1f;
  Play(SFX_CLANK);
  for (int i = 0; i < 6; i++)
    AddParticle(P_SPARK, e.x + 16 - dir * 14, e.y + Randf(2, 12), -dir * Randf(40, 160), Randf(-140, 40),
                Randf(0.1f, 0.25f), 1, {255, 230, 120, 255});
  if (e.hp > 0) return;
  e.st = WRECK;
  e.t = 0;
  e.vx = 0;
  AddScore(1000, e.x + 8, e.y - 8);
  Explode(e.x + 16, e.y + 6, 36, true, false);
  for (int i = 0; i < 8; i++)
    AddParticle(P_CASING, e.x + 16, e.y + 4, Randf(-160, 160), Randf(-320, -120), 3.0f, 2.5f, C_DKOLIVE);
}

Rectangle EnemyHeliRect(const EnemyHeli& h) { return {h.x, h.y, HELI_W, HELI_H}; }

bool EnemyHeliHittable(const EnemyHeli& h) { return h.alive && h.active && !h.crashing; }

// At 0 HP the attack helicopter spins down trailing smoke and explodes on impact.
void DamageEnemyHeli(EnemyHeli& h, int dmg, float dir) {
  if (!EnemyHeliHittable(h)) return;
  h.hp -= dmg;
  h.flashT = 0.1f;
  Play(SFX_CLANK);
  for (int i = 0; i < 5; i++)
    AddParticle(P_SPARK, h.x + HELI_W / 2 - dir * 16, h.y + Randf(2, 14), -dir * Randf(40, 160), Randf(-120, 60),
                Randf(0.1f, 0.25f), 1, {255, 230, 120, 255});
  if (h.hp > 0) return;
  h.crashing = true;
  h.vy = -40;
  h.vx = dir * 50;
  AddScore(2000, h.x + 12, h.y);
  Play(SFX_ROAR);
}

// Cut an enemy in two (knife) or blow it apart (explosive).
void SliceEnemy(Enemy& e, float dir, bool explosive = false) {
  SheetId sheet;
  int frame = ((int)(e.animT * 6)) % 2;
  float sy = e.y, fh = 16;
  bool flip = false;
  if (e.type == GOOMBA) {
    sheet = S_GOOMBA;
  } else if (e.type == ZOMBIE) {
    sheet = S_ZOMBIE;
    frame = ((int)(e.animT * 3)) % 2;
    sy = e.y - 8;
    fh = 24;
    flip = !e.faceRight;
  } else if (e.st == WALK) {
    sheet = S_KOOPA;
    sy = e.y - 8;
    fh = 24;
    flip = e.vx < 0;
  } else {
    sheet = S_SHELL;
    frame = 0;
  }
  SpawnHalves(sheet, frame, e.x, sy, 16, fh, flip, dir, explosive ? 1.7f : 1.0f);
  float cutY = sy + floorf(fh / 2);
  BloodBurst(e.x + 8, cutY, dir, explosive ? 70 : 50, explosive ? 1.4f : 1.0f);
  AddScore(e.type == ZOMBIE ? 300 : 200, e.x, e.y);
  if (explosive) {
    Play(SFX_SPLAT);
  } else {
    AddShockwave(e.x + 8, cutY, 64, 5.0f, 0.45f);
    Play(SFX_SLICE);
  }
  Play(e.type == ZOMBIE ? SFX_GROAN : SFX_SQUEAL);
  e.alive = false;
}

void ShootEnemy(Enemy& e, float dir) {
  if (e.type == TANK) {
    DamageTank(e, 1, dir);
    return;
  }
  if (e.type == ZOMBIE && --e.hp > 0) {  // zombies soak a few rounds
    e.flashT = 0.1f;
    e.x += dir * 2;
    BloodBurst(e.x + 8, e.y, dir, 10, 0.7f);
    Play(SFX_SPLAT);
    return;
  }
  BloodBurst(e.x + 8, e.y + 8, dir, 22, 0.8f);
  e.st = FLIPPED;
  e.vy = -200;
  e.vx = dir * 70;
  AddScore(e.type == ZOMBIE ? 300 : 100, e.x, e.y);
  Play(SFX_SPLAT);
}

void DamageBoss(int dmg, float dir, bool knife) {
  if (!boss.alive) return;
  boss.hp -= dmg;
  boss.flashT = 0.12f;
  BloodBurst(boss.x + 16, boss.y + 14, dir, knife ? 25 : 6);
  if (knife) AddShockwave(boss.x + 16, boss.y + 16, 70, 5.0f, 0.45f);
  if (boss.hp > 0) {
    Play(knife ? SFX_SLICE : SFX_SPLAT);
    return;
  }
  boss.alive = false;
  AddScore(5000, boss.x, boss.y);
  Play(SFX_ROAR);
  Play(SFX_SLICE);
  int frame = ((int)(boss.animT * 4)) % 2;
  SpawnHalves(S_BOSS, frame, boss.x, boss.y, 32, 32, !boss.faceRight, dir, knife ? 1.0f : 0.6f);
  BloodBurst(boss.x + 16, boss.y + 16, dir, 140, 1.3f);
  AddShockwave(boss.x + 16, boss.y + 16, 120, 8.0f, 0.7f);
  Shake(0.5f, 4);
  hitStop = 0.12f;
}

void EndDrone(bool explode);
void ExitHeli();
void DamageHeli(int dmg);

void KillPlayer(bool fell) {
  if (droneMode) EndDrone(false);
  if (inHeli) ExitHeli();
  state = ST_DYING;
  stateT = 0;
  deathByFall = fell;
  deathJumped = false;
  if (p.big) {
    p.big = false;
    p.y += 16;
  }
  p.vx = p.vy = 0;
  p.pose = POSE_DEAD;
  Play(SFX_DIE);
}

void HurtPlayer() {
  if (state != ST_PLAYING) return;
  if (inHeli) {  // the helicopter takes the hit
    DamageHeli(3);
    return;
  }
  if (p.invuln > 0 || transformT > 0) return;
  Shake(0.2f, 2);
  if (p.big) {
    p.big = false;
    p.y += 16;
    transformT = 0.8f;
    transformToBig = false;
    p.invuln = 2.0f;
    Play(SFX_SHRINK);
  } else {
    KillPlayer(false);
  }
}

void BumpTile(int tx, int ty) {
  if (tx < 0 || tx >= LW || ty < 0 || ty >= LH) return;
  char& c = Tile(tx, ty);
  float bx = (float)(tx * TILE), by = (float)(ty * TILE);
  if (c == '?' || c == 'M') {
    if (c == '?') {
      effects.push_back({FX_COIN, bx, by - 16, 0, -300});
      AddCoin();
    } else {
      items.push_back({bx, by});
      Play(SFX_SPROUT);
    }
    c = 'U';
    bumpT[ty * LW + tx] = 0.15f;
  } else if (c == 'B') {
    if (p.big) {
      c = '.';
      const float dvx[4] = {-60, 60, -60, 60}, dvy[4] = {-320, -320, -220, -220};
      for (int i = 0; i < 4; i++)
        effects.push_back({FX_DEBRIS, bx + (i % 2) * 8, by + (i / 2) * 8, dvx[i], dvy[i], 0, i});
      score += 50;
      Play(SFX_BREAK);
    } else {
      bumpT[ty * LW + tx] = 0.15f;
      Play(SFX_BUMP);
    }
  } else {
    Play(SFX_BUMP);
    return;
  }
  // whatever stands on the bumped block gets knocked
  Rectangle top{bx, by - 8, 16, 8};
  for (auto& e : enemies) {
    if (EnemyHittable(e) && e.type != ZOMBIE && e.type != TANK && CheckCollisionRecs(top, {e.x, e.y, 16, 16}))
      KillEnemy(e, e.x + 8 < bx + 8 ? -1.f : 1.f);
  }
  for (auto& it : items) {
    if (it.alive && it.emerge <= 0 && CheckCollisionRecs(top, {it.x, it.y, 16, 16})) {
      it.vy = -250;
      it.vx = (it.x + 8 < bx + 8) ? -fabsf(it.vx) : fabsf(it.vx);
    }
  }
  if (TileAt(tx, ty - 1) == 'o') {
    Tile(tx, ty - 1) = '.';
    effects.push_back({FX_COIN, bx, by - 16, 0, -300});
    AddCoin();
  }
}

float Approach(float v, float target, float step) {
  if (v < target) return std::min(v + step, target);
  return std::max(v - step, target);
}

void UpdateCamera() {
  float maxCam = (float)(LW * TILE - VW);
  if (droneMode) {  // free camera that follows the drone both ways
    camX = Approach(camX, std::clamp(drone.x - VW / 2.f, 0.f, maxCam), 420 * DT);
    return;
  }
  if (inHeli) {  // look ahead in the flight direction
    float t = heli.x + HELI_W / 2 - VW / 2.f + (heli.faceRight ? 40.f : -40.f);
    camX = Approach(camX, std::clamp(t, 0.f, maxCam), 600 * DT);
    return;
  }
  float target = p.x + PW() / 2 - VW * 0.45f;
  if (camReturn) {
    float t = std::clamp(target, 0.f, maxCam);
    camX = Approach(camX, t, 520 * DT);
    if (fabsf(camX - t) < 1) camReturn = false;
    return;
  }
  if (target > camX) camX = target;
  camX = std::clamp(camX, 0.f, maxCam);
}

Vector2 HandPos();

// Left edge past which off-screen things are discarded; stays near the player while the drone roams.
float CullLeft() { return std::min(camX, p.x - VW) - 48; }

// ---------------------------------------------------------------------------
// Explosives: grenades, airstrikes, FPV drone
// ---------------------------------------------------------------------------
// `damages` = false for purely cosmetic blasts (a dying tank) that still singe the player.
void Explode(float x, float y, float radius, bool hurtsPlayer, bool damages) {
  explosions.push_back({x, y, 0, radius});
  for (int i = 0; i < 40; i++) {
    float a = Randf(0, 2 * PI), s = Randf(20, radius * 5);
    AddParticle(P_FIRE, x, y, cosf(a) * s, sinf(a) * s - 40, Randf(0.25f, 0.6f), Randf(2, 4.5f),
                GetRandomValue(0, 1) ? C_LAVATOP : C_LAVA);
  }
  for (int i = 0; i < 14; i++)
    AddParticle(P_SMOKE, x + Randf(-radius / 2, radius / 2), y + Randf(-radius / 2, radius / 2), Randf(-20, 20),
                Randf(-50, -10), Randf(0.7f, 1.2f), Randf(3, 6), {70, 70, 70, 180});
  for (int i = 0; i < 14; i++)  // scorched debris that sticks where it lands
    AddParticle(P_BLOOD, x, y, Randf(-200, 200), Randf(-300, -60), Randf(4, 7), Randf(1, 2), {35, 30, 30, 255});
  AddShockwave(x, y, radius * 2.5f, 8.0f, 0.6f);
  Shake(0.4f, 4.5f);
  hitStop = std::max(hitStop, 0.04f);
  Play(SFX_BOOM);

  auto dist = [&](float px, float py) { return hypotf(px - x, py - y); };
  if (hurtsPlayer && state == ST_PLAYING && dist(p.x + PW() / 2, p.y + PH() / 2) < radius * 0.7f) HurtPlayer();
  if (!damages) return;
  for (auto& e : enemies) {
    if (!EnemyHittable(e)) continue;
    Rectangle b = EnemyBody(e);
    float ex = b.x + b.width / 2, ey = b.y + b.height / 2;
    if (dist(ex, ey) >= radius + (e.type == TANK ? 16 : 8)) continue;
    if (e.type == TANK) DamageTank(e, 4, ex < x ? -1.f : 1.f);
    else SliceEnemy(e, ex < x ? -1.f : 1.f, true);
  }
  for (auto& h : enemyHelis)
    if (EnemyHeliHittable(h) && dist(h.x + HELI_W / 2, h.y + HELI_H / 2) < radius + 20)
      DamageEnemyHeli(h, 5, h.x + HELI_W / 2 < x ? -1.f : 1.f);
  if (boss.alive && boss.active && dist(boss.x + 16, boss.y + 16) < radius + 16)
    DamageBoss(6, boss.x + 16 < x ? -1.f : 1.f, true);
  for (auto& f : flames)
    if (f.alive && dist(f.x + 8, f.y + 8) < radius) f.alive = false;
  for (auto& h : halves) {
    float d = dist(h.x, h.y);
    if (d < radius * 1.5f) {
      h.landed = false;
      h.bounced = false;
      h.vx += (h.x < x ? -1.f : 1.f) * 150;
      h.vy = -250;
      h.vrot = Randf(-12, 12);
    }
  }
  // blast bricks
  int r = (int)ceilf(radius / TILE);
  int tx0 = (int)floorf(x / TILE), ty0 = (int)floorf(y / TILE);
  for (int ty = ty0 - r; ty <= ty0 + r; ty++)
    for (int tx = tx0 - r; tx <= tx0 + r; tx++) {
      if (TileAt(tx, ty) != 'B' || dist(tx * TILE + 8.f, ty * TILE + 8.f) > radius) continue;
      Tile(tx, ty) = '.';
      const float dvx[4] = {-80, 80, -80, 80}, dvy[4] = {-340, -340, -240, -240};
      for (int i = 0; i < 4; i++)
        effects.push_back({FX_DEBRIS, tx * TILE + (i % 2) * 8.f, ty * TILE + (i / 2) * 8.f, dvx[i], dvy[i], 0, i});
      score += 50;
    }
}

// ---------------------------------------------------------------------------
// Helicopter
// ---------------------------------------------------------------------------
Rectangle HeliRect() { return {heli.x, heli.y, HELI_W, HELI_H}; }
Vector2 HeliNose() { return {heli.faceRight ? heli.x + HELI_W + 4 : heli.x - 4, heli.y + 11}; }

bool NearHeli() {
  if (!heli.present || !heli.alive || inHeli) return false;
  Rectangle r = HeliRect();
  return CheckCollisionRecs({p.x - 6, p.y, PW() + 12, PH()}, {r.x, r.y - 8, r.width, r.height + 8});
}

void SyncPlayerToHeli() {
  p.x = heli.x + HELI_W / 2 - PW() / 2;
  p.y = heli.y + HELI_H - PH();
  p.vx = p.vy = 0;
  p.faceRight = heli.faceRight;
}

void BoardHeli() {
  inHeli = true;
  heli.faceRight = p.faceRight;
  p.attackT = 0;
  Play(SFX_SWITCH);
  Toast("ARROWS FLY  F GUN  G ROCKET  H EXIT");
}

void ExitHeli() {
  if (!inHeli) return;
  inHeli = false;
  p.x = heli.x + HELI_W / 2 - PW() / 2;
  p.y = heli.y + HELI_H - PH();
  p.vx = heli.vx * 0.5f;
  p.vy = std::min(heli.vy, 0.f);
  p.onGround = false;
  p.invuln = std::max(p.invuln, 0.5f);
  camReturn = true;
  if (audioOk) StopSound(sfx[SFX_HELI]);
}

void DamageHeli(int dmg) {
  if (!heli.alive || heli.hitT > 0) return;
  heli.hp -= dmg;
  heli.hitT = 0.5f;
  heli.flashT = 0.12f;
  Shake(0.25f, 3);
  Play(SFX_CLANK);
  Rectangle r = HeliRect();
  for (int i = 0; i < 10; i++)
    AddParticle(P_SPARK, r.x + Randf(0, r.width), r.y + Randf(0, r.height), Randf(-150, 150), Randf(-150, 50),
                Randf(0.1f, 0.3f), 1, {255, 230, 120, 255});
  if (heli.hp > 0) return;
  heli.alive = false;
  bool wasInside = inHeli;
  ExitHeli();
  Explode(r.x + r.width / 2, r.y + r.height / 2, 44, false, true);
  for (int i = 0; i < 12; i++)
    AddParticle(P_CASING, r.x + r.width / 2, r.y + 8, Randf(-200, 200), Randf(-340, -120), 3.0f, 2.5f, {60, 90, 60, 255});
  Toast("HELICOPTER DOWN!");
  if (wasInside) {
    p.vy = -300;
    HurtPlayer();
  }
}

void HeliPhysics(float dir, float lift) {
  heli.vx = std::clamp((heli.vx + dir * 320 * DT) * 0.97f, -150.f, 150.f);
  heli.vy = std::clamp((heli.vy + (lift + 60) * DT) * 0.95f, -130.f, 200.f);
  if (heli.onGround && lift >= 0) heli.vx *= 0.85f;  // skids drag on the ground
  if (MoveX(heli.x, heli.y, HELI_W, HELI_H, heli.vx * DT)) heli.vx = 0;
  int a, b;
  bool hit = MoveY(heli.x, heli.y, HELI_W, HELI_H, heli.vy * DT, a, b);
  heli.onGround = hit && heli.vy > 0;
  if (hit) heli.vy = 0;
  if (heli.y < 2) {
    heli.y = 2;
    heli.vy = 0;
  }
  heli.tilt = Approach(heli.tilt, heli.vx / 150.f * 14.f, 60 * DT);
}

void UpdateHeli() {
  if (!heli.present || !heli.alive) return;
  if (heli.hitT > 0) heli.hitT -= DT;
  if (heli.flashT > 0) heli.flashT -= DT;
  if (heli.fireCD > 0) heli.fireCD -= DT;
  if (heli.rocketCD > 0) heli.rocketCD -= DT;
  heli.heat = std::max(0.f, heli.heat - 0.45f * DT);
  if (heli.overheated && heli.heat < 0.35f) heli.overheated = false;

  if (!inHeli) {  // parked or abandoned: settle to the ground, rotor spinning down
    heli.rotor = std::max(0.f, heli.rotor - DT * 0.5f);
    HeliPhysics(0, 0);
    if (TouchesTile(HeliRect(), 'L') || heli.y > VH) {
      heli.hp = 0;
      heli.hitT = 0;
      DamageHeli(1);
    }
    return;
  }
  heli.rotor = std::min(1.f, heli.rotor + DT * 2);
  bool left = IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A), right = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
  bool up = IsKeyDown(KEY_UP) || IsKeyDown(KEY_W) || IsKeyDown(KEY_SPACE), down = IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S);
  float dir = (right ? 1.f : 0.f) - (left ? 1.f : 0.f);
  if (dir != 0) heli.faceRight = dir > 0;
  HeliPhysics(dir, up ? -280.f : down ? 200.f : 0.f);
  SyncPlayerToHeli();
  if (audioOk && !muted && !IsSoundPlaying(sfx[SFX_HELI])) PlaySound(sfx[SFX_HELI]);

  float fdir = heli.faceRight ? 1.f : -1.f;
  if ((IsKeyDown(KEY_F) || attackLatch) && heli.fireCD <= 0 && !heli.overheated) {
    Vector2 n = HeliNose();
    float droop = heli.tilt * fdir * 0.02f;  // nose-down tilt angles the gun downward
    bullets.push_back({n.x, n.y, fdir * 520, Randf(-18, 18) + 520 * droop + 30, 1.0f});
    AddParticle(P_CASING, heli.x + HELI_W / 2, heli.y + HELI_H, Randf(-30, 30), Randf(-40, 0), 1.5f, 1.2f,
                {230, 190, 60, 255});
    heli.fireCD = 0.05f;
    heli.heat += 0.045f;
    if (heli.heat >= 1) {
      heli.overheated = true;
      Toast("GUN OVERHEATED");
    }
    Play(SFX_SHOT);
    Shake(0.04f, 0.8f);
  }
  if (grenadeLatch) {
    if (heli.rockets <= 0) {
      Toast("NO ROCKETS");
    } else if (heli.rocketCD <= 0) {
      Vector2 n = HeliNose();
      projectiles.push_back({PJ_ROCKET, n.x, n.y + 3, fdir * 160 + heli.vx * 0.5f, 25, 0});
      heli.rockets--;
      heli.rocketCD = 0.3f;
      Play(SFX_ROCKET);
    }
  }
  // grab coins with the airframe
  Rectangle r = HeliRect();
  for (int ty = (int)(r.y / TILE); ty <= (int)((r.y + r.height - 1) / TILE); ty++)
    for (int tx = (int)(r.x / TILE); tx <= (int)((r.x + r.width - 1) / TILE); tx++)
      if (TileAt(tx, ty) == 'o') {
        Tile(tx, ty) = '.';
        score += 200;
        AddCoin();
      }
  if (TouchesTile(r, 'L')) {
    heli.hitT = 0;
    DamageHeli(HELI_HP);
  }
}

// ---------------------------------------------------------------------------
// Tanks
// ---------------------------------------------------------------------------
void UpdateTank(Enemy& e) {
  if (e.flashT > 0) e.flashT -= DT;
  if (e.recoil > 0) e.recoil = std::max(0.f, e.recoil - 12 * DT);
  if (e.st == WRECK) {  // burn for a while, then cool down
    e.t += DT;
    if (e.t < 8 && GetRandomValue(0, 2) == 0)
      AddParticle(P_SMOKE, e.x + Randf(8, 24), e.y + 2, Randf(-8, 8), Randf(-40, -20), Randf(0.6f, 1.0f), Randf(2, 4),
                  {40, 40, 40, 170});
    if (e.t < 5 && GetRandomValue(0, 3) == 0)
      AddParticle(P_FIRE, e.x + Randf(6, 26), e.y + 4, Randf(-10, 10), Randf(-40, -10), Randf(0.2f, 0.4f), 2, C_LAVA);
    if (e.x < CullLeft()) e.alive = false;
    return;
  }
  e.animT += fabsf(e.vx) * DT * 0.2f;
  Vector2 target = inHeli ? Vector2{heli.x + HELI_W / 2, heli.y + HELI_H / 2} : Vector2{p.x + PW() / 2, p.y + PH() / 2};
  Vector2 pivot{e.x + 16, e.y + 3};
  float dx = target.x - pivot.x, dy = target.y - pivot.y;
  bool engaging = fabsf(dx) < 230 && e.x > camX - 32 && e.x < camX + VW && state == ST_PLAYING;

  if (engaging) {
    e.faceRight = dx > 0;
    e.vx = 0;  // stop to fire
    // barrel elevation, measured from the facing direction; can't depress much below level
    float desired = atan2f(dy, fabsf(dx)) * RAD2DEG;
    e.aim = Approach(e.aim, std::clamp(desired, -70.f, 8.f), 60 * DT);
    e.t -= DT;
    if (e.t <= 0) {
      e.t = Randf(2.2f, 3.2f);
      float d = e.faceRight ? 1.f : -1.f;
      float a = e.aim * DEG2RAD;
      Vector2 muzzle{pivot.x + d * cosf(a) * 15, pivot.y + sinf(a) * 15};
      projectiles.push_back({PJ_TANK_SHELL, muzzle.x, muzzle.y, d * cosf(a) * 210, sinf(a) * 210, 0});
      e.recoil = 3;
      for (int i = 0; i < 6; i++)
        AddParticle(P_FIRE, muzzle.x, muzzle.y, d * Randf(20, 80), Randf(-30, 30), Randf(0.1f, 0.2f), 2, C_LAVATOP);
      AddParticle(P_SMOKE, muzzle.x, muzzle.y, d * 20, -10, 0.6f, 3, {120, 120, 120, 170});
      Play(SFX_CANNON);
      Shake(0.12f, 1.5f);
    }
  } else {
    e.aim = Approach(e.aim, 0, 30 * DT);
    if (e.vx == 0) e.vx = e.faceRight ? TANK_SPEED : -TANK_SPEED;
    e.faceRight = e.vx > 0;
  }

  e.vy = std::min(e.vy + 1400 * DT, MAX_FALL);
  if (e.vx != 0 && e.onGround && !SolidAtPx(e.vx > 0 ? e.x + 33 : e.x - 1, e.y + 17)) e.vx = -e.vx;  // ledge
  if (MoveX(e.x, e.y, 32, 16, e.vx * DT)) e.vx = -e.vx;
  int a, b;
  e.onGround = MoveY(e.x, e.y, 32, 16, e.vy * DT, a, b) && e.vy > 0;
  if (e.onGround) e.vy = 0;
  if (TouchesTile({e.x + 2, e.y + 4, 28, 12}, 'L')) {
    LavaSplash(e.x + 16, e.y + 12);
    e.alive = false;
  }
  if (e.y > VH + 16 || e.x < CullLeft()) e.alive = false;
}

// ---------------------------------------------------------------------------
// Enemy fire: pistols, attack helicopters
// ---------------------------------------------------------------------------
Vector2 EnemyTarget() {
  if (inHeli) return {heli.x + HELI_W / 2, heli.y + HELI_H / 2};
  return {p.x + PW() / 2, p.y + PH() / 2};
}

void FireEnemyBullet(float x, float y, float speed, float spread) {
  Vector2 t = EnemyTarget();
  float a = atan2f(t.y - y, t.x - x) + Randf(-spread, spread);
  enemyBullets.push_back({x, y, cosf(a) * speed, sinf(a) * speed, 2.0f});
  AddParticle(P_FIRE, x, y, cosf(a) * 40, sinf(a) * 40, 0.06f, 1.5f, C_LAVATOP);
  Play(SFX_EPOP);
}

// Armed goombas/koopas stop for nothing; they pop a shot whenever the player is in range.
void UpdateEnemyGun(Enemy& e) {
  if (e.gunFlash > 0) e.gunFlash -= DT;
  if (!e.armed || e.st != WALK || state != ST_PLAYING) return;
  Vector2 t = EnemyTarget();
  float gx = e.x + 8, gy = e.y + (e.type == KOOPA ? 4.f : 9.f);
  bool onScreen = e.x > camX && e.x < camX + VW - 16;
  if (!onScreen || fabsf(t.x - gx) > 200) return;
  e.gunT -= DT;
  if (e.gunT > 0) return;
  e.gunT = Randf(1.8f, 2.8f);
  e.gunFlash = 0.08f;
  float d = t.x > gx ? 1.f : -1.f;
  FireEnemyBullet(gx + d * 9, gy, 200, 0.06f);
}

void UpdateEnemyBullets() {
  for (auto& b : enemyBullets) {
    if (b.life <= 0) continue;
    b.life -= DT;
    b.x += b.vx * DT;
    b.y += b.vy * DT;
    if (SolidAtPx(b.x, b.y)) {
      Sparks(b.x, b.y, b.vx > 0 ? 1.f : -1.f, 4);
      b.life = 0;
      continue;
    }
    if (state == ST_PLAYING) {
      if (inHeli && CheckCollisionPointRec({b.x, b.y}, HeliRect())) {
        DamageHeli(1);
        b.life = 0;
      } else if (!inHeli && CheckCollisionPointRec({b.x, b.y}, {p.x, p.y, PW(), PH()})) {
        if (p.invuln <= 0 && transformT <= 0) BloodBurst(b.x, b.y, b.vx > 0 ? 1.f : -1.f, 8, 0.6f);
        HurtPlayer();
        b.life = 0;
      }
    }
    if (b.x < camX - 16 || b.x > camX + VW + 16 || b.y < -16 || b.y > VH + 16) b.life = 0;
  }
  enemyBullets.erase(std::remove_if(enemyBullets.begin(), enemyBullets.end(), [](const EnemyBullet& b) { return b.life <= 0; }),
                     enemyBullets.end());
}

// Attack helicopters keep station above and to one side of the player, firing bursts and rockets.
void UpdateEnemyHelis() {
  for (auto& h : enemyHelis) {
    if (!h.alive) continue;
    if (!h.active) {
      if (h.x < camX + VW + 40) h.active = true;
      else continue;
    }
    if (h.flashT > 0) h.flashT -= DT;
    float cx = h.x + HELI_W / 2, cy = h.y + HELI_H / 2;
    if (h.crashing) {
      h.vy = std::min(h.vy + 300 * DT, 260.f);
      h.x += h.vx * DT;
      h.y += h.vy * DT;
      h.spin += 7 * DT;
      AddParticle(P_SMOKE, cx, cy, Randf(-10, 10), Randf(-30, -10), 0.9f, Randf(3, 5), {40, 40, 40, 190});
      if (GetRandomValue(0, 1)) AddParticle(P_FIRE, cx, cy, Randf(-30, 30), Randf(-30, 10), 0.25f, 2.5f, C_LAVA);
      if (SolidAtPx(cx, h.y + HELI_H) || SolidAtPx(cx + (h.vx > 0 ? 20.f : -20.f), cy) || h.y > VH) {
        h.alive = false;
        if (h.y <= VH) Explode(cx, cy, 44, true);
      }
      continue;
    }
    Vector2 t = EnemyTarget();
    float side = cx < t.x ? -1.f : 1.f;  // hold position on whichever side it is already on
    float gx = t.x + side * 100 - HELI_W / 2, gy = std::clamp(t.y - 80, 48.f, 120.f);
    h.vx = Approach(h.vx, std::clamp((gx - h.x) * 1.5f, -90.f, 90.f), 120 * DT);
    h.vy = Approach(h.vy, std::clamp((gy - h.y) * 1.5f, -70.f, 70.f), 120 * DT);
    if (MoveX(h.x, h.y, HELI_W, HELI_H, h.vx * DT)) h.vx = 0;
    int a, b;
    if (MoveY(h.x, h.y, HELI_W, HELI_H, h.vy * DT, a, b)) h.vy = 0;
    h.faceRight = t.x > cx;
    h.tilt = Approach(h.tilt, h.vx / 90.f * 10.f, 40 * DT);

    bool onScreen = h.x > camX - 8 && h.x < camX + VW - HELI_W + 8;
    if (!onScreen || state != ST_PLAYING) continue;
    Vector2 nose{h.faceRight ? h.x + HELI_W + 4 : h.x - 4, h.y + 11};
    h.fireT -= DT;
    if (h.fireT <= 0) {
      h.burstLeft = 5;
      h.fireT = Randf(2.5f, 3.5f);
    }
    if (h.burstLeft > 0) {
      h.burstT -= DT;
      if (h.burstT <= 0) {
        h.burstT = 0.09f;
        h.burstLeft--;
        FireEnemyBullet(nose.x, nose.y, 260, 0.08f);
      }
    }
    h.rocketT -= DT;
    if (h.rocketT <= 0) {
      h.rocketT = Randf(4.5f, 6.5f);
      float a2 = atan2f(t.y - nose.y, t.x - nose.x);
      projectiles.push_back({PJ_ENEMY_ROCKET, nose.x, nose.y, cosf(a2) * 150, sinf(a2) * 150, 0});
      Play(SFX_ROCKET);
    }
  }
  enemyHelis.erase(std::remove_if(enemyHelis.begin(), enemyHelis.end(), [](const EnemyHeli& h) { return !h.alive; }),
                   enemyHelis.end());
}

void ThrowGrenade() {
  if (p.grenades <= 0) {
    Toast("NO GRENADES");
    return;
  }
  p.grenades--;
  Vector2 h = HandPos();
  float dir = p.faceRight ? 1.f : -1.f;
  projectiles.push_back({PJ_GRENADE, h.x, h.y - 4, dir * 170 + p.vx * 0.4f, -230, GRENADE_FUSE});
  Play(SFX_THROW);
}

void CallAirstrike() {
  if (jet.active) return;
  if (p.airstrikes <= 0) {
    Toast("NO AIRSTRIKES");
    return;
  }
  p.airstrikes--;
  jet = Jet{};
  jet.active = true;
  jet.x = camX - 40;
  jet.y = 52;
  jet.vx = 240;
  float end = camX + VW - 8;
  float start = std::min(std::max(p.x + 56, camX + 64), end - 60);
  for (int i = 0; i < 6; i++) jet.targets[i] = start + (end - start) * i / 5.f;
  Toast("AIRSTRIKE INBOUND");
  Play(SFX_RADIO);
  Play(SFX_JET);
}

void LaunchDrone() {
  if (p.drones <= 0) {
    Toast("NO DRONES");
    return;
  }
  p.drones--;
  Vector2 h = HandPos();
  drone = Drone{};
  drone.active = true;
  drone.x = h.x;
  drone.y = p.y - 4;
  drone.vx = p.faceRight ? 60.f : -60.f;
  drone.vy = -90;
  drone.battery = DRONE_BATTERY;
  drone.payload = 1;
  droneMode = true;
  p.vx = 0;
  Play(SFX_SWITCH);
}

void EndDrone(bool explode) {
  if (!drone.active) return;
  drone.active = false;
  droneMode = false;
  camReturn = true;
  if (audioOk) StopSound(sfx[SFX_DRONE]);
  if (explode) Explode(drone.x, drone.y, 40, true);
}

void UpdateDrone() {
  if (!drone.active) return;
  drone.t += DT;
  drone.battery -= DT;
  bool left = IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A), right = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
  bool up = IsKeyDown(KEY_UP) || IsKeyDown(KEY_W) || IsKeyDown(KEY_SPACE), down = IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S);
  drone.vx += ((right ? 1.f : 0.f) - (left ? 1.f : 0.f)) * 520 * DT;
  drone.vy += ((down ? 1.f : 0.f) - (up ? 1.f : 0.f)) * 520 * DT + 30 * DT;
  drone.vx = std::clamp(drone.vx * 0.94f, -170.f, 170.f);
  drone.vy = std::clamp(drone.vy * 0.94f, -170.f, 170.f);
  float bx = drone.x - 6, by = drone.y - 3;
  if (MoveX(bx, by, 12, 6, drone.vx * DT)) drone.vx = 0;
  int a, b;
  if (MoveY(bx, by, 12, 6, drone.vy * DT, a, b)) drone.vy = 0;
  if (by < 2) {
    by = 2;
    drone.vy = 0;
  }
  drone.x = bx + 6;
  drone.y = by + 3;
  if (audioOk && !muted && !IsSoundPlaying(sfx[SFX_DRONE])) PlaySound(sfx[SFX_DRONE]);

  if (attackLatch) {
    if (drone.payload > 0) {
      drone.payload--;
      projectiles.push_back({PJ_DRONE_BOMB, drone.x, drone.y + 5, drone.vx * 0.6f, std::max(drone.vy, 0.f) + 30, 0});
      Play(SFX_THROW);
    } else {
      EndDrone(true);  // kamikaze
      return;
    }
  }
  if (droneLatch && drone.t > 0.2f) {
    EndDrone(false);
    Play(SFX_SWITCH);
    return;
  }
  if (drone.battery <= 0) {
    Toast("DRONE BATTERY DEAD");
    EndDrone(true);
  }
}

void UpdateJet() {
  if (!jet.active) return;
  jet.x += jet.vx * DT;
  if (GetRandomValue(0, 1)) AddParticle(P_SMOKE, jet.x - 16, jet.y, -30, 0, 0.6f, 2, {230, 230, 230, 120});
  while (jet.next < 6 && jet.x >= jet.targets[jet.next]) {
    projectiles.push_back({PJ_BOMB, jet.x, jet.y + 6, jet.vx * 0.25f, 40, 0});
    Play(SFX_WHISTLE);
    jet.next++;
  }
  if (jet.x > camX + VW + 80 && jet.next >= 6) jet.active = false;
}

void UpdateProjectiles() {
  for (auto& pr : projectiles) {
    if (!pr.alive) continue;
    if (pr.kind == PJ_GRENADE) {
      pr.vy = std::min(pr.vy + 900 * DT, MAX_FALL);
      float nx = pr.x + pr.vx * DT;
      if (SolidAtPx(nx + (pr.vx > 0 ? 3.f : -3.f), pr.y)) {
        pr.vx = -pr.vx * 0.5f;
        Play(SFX_CLINK);
      } else {
        pr.x = nx;
      }
      float ny = pr.y + pr.vy * DT;
      if (SolidAtPx(pr.x, ny + (pr.vy > 0 ? 3.f : -3.f))) {
        if (pr.vy > 60) Play(SFX_CLINK);
        pr.vy = pr.vy > 0 ? -pr.vy * 0.4f : 0;
        if (fabsf(pr.vy) < 30) pr.vy = 0;
        pr.vx *= 0.75f;
      } else {
        pr.y = ny;
      }
      pr.rot += pr.vx * DT * 12;
      pr.fuse -= DT;
      if (pr.fuse <= 0) {
        pr.alive = false;
        Explode(pr.x, pr.y, 40, true);
      }
      continue;
    }
    if (pr.kind == PJ_TANK_SHELL || pr.kind == PJ_ENEMY_ROCKET) {  // enemy fire: bursts on terrain, player or heli
      if (pr.kind == PJ_TANK_SHELL) {
        pr.vy += 150 * DT;  // light arc
      } else {
        float d = pr.vx > 0 ? 1.f : -1.f;
        AddParticle(P_SMOKE, pr.x - d * 5, pr.y, -d * 15, Randf(-8, 8), 0.45f, 2, {200, 200, 200, 140});
      }
      pr.x += pr.vx * DT;
      pr.y += pr.vy * DT;
      pr.rot = atan2f(pr.vy, pr.vx) * RAD2DEG;
      Rectangle victim = inHeli ? HeliRect() : Rectangle{p.x, p.y, PW(), PH()};
      bool hit = SolidAtPx(pr.x, pr.y) || (state == ST_PLAYING && CheckCollisionPointRec({pr.x, pr.y}, victim));
      if (hit) {
        pr.alive = false;
        Explode(pr.x, pr.y, 26, true);
      } else if (pr.y > VH + 16 || pr.x < camX - 64 || pr.x > camX + VW + 64) {
        pr.alive = false;
      }
      continue;
    }
    if (pr.kind == PJ_ROCKET) {
      float d = pr.vx > 0 ? 1.f : -1.f;
      pr.vx = Approach(pr.vx, d * 420, 600 * DT);
      pr.vy *= 0.97f;
      AddParticle(P_SMOKE, pr.x - d * 6, pr.y, -d * 20, Randf(-10, 10), 0.5f, 2, {200, 200, 200, 150});
      AddParticle(P_FIRE, pr.x - d * 7, pr.y, -d * 60, 0, 0.08f, 1.5f, C_LAVATOP);
    } else {
      pr.vy = std::min(pr.vy + 600 * DT, 420.f);
    }
    // bombs and rockets burst on contact; airstrike bombs punch through ceilings near the top
    pr.x += pr.vx * DT;
    pr.y += pr.vy * DT;
    pr.rot = atan2f(pr.vy, pr.vx) * RAD2DEG;
    bool pierce = pr.kind == PJ_BOMB && pr.y < 40;
    bool hit = !pierce && SolidAtPx(pr.x, pr.y + (pr.kind == PJ_ROCKET ? 0.f : 3.f));
    for (auto& e : enemies)
      if (!hit && EnemyHittable(e) && CheckCollisionPointRec({pr.x, pr.y}, EnemyBody(e))) hit = true;
    if (!hit && boss.alive && boss.active && CheckCollisionPointRec({pr.x, pr.y}, {boss.x, boss.y, 32, 32})) hit = true;
    for (auto& h : enemyHelis)
      if (!hit && EnemyHeliHittable(h) && CheckCollisionPointRec({pr.x, pr.y}, EnemyHeliRect(h))) hit = true;
    if (hit) {
      pr.alive = false;
      Explode(pr.x, pr.y, pr.kind == PJ_BOMB ? 36.f : pr.kind == PJ_ROCKET ? 32.f : 34.f, true);
    } else if (pr.y > VH + 16 || (pr.kind == PJ_ROCKET && (pr.x < camX - 32 || pr.x > camX + VW + 32))) {
      pr.alive = false;
    }
  }
  projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(), [](const Projectile& pr) { return !pr.alive; }),
                    projectiles.end());
}

// Hand position in world space, matching the drawn sprite.
Vector2 HandPos() {
  float sy = p.y + PH() - (p.big ? 32.f : 16.f);
  float hy = sy + (p.big ? 19.f : 10.f);
  float hx = p.faceRight ? p.x - 2 + 13 : p.x - 2 + 3;
  return {hx, hy};
}

Vector2 MuzzlePos() {
  Vector2 h = HandPos();
  return {h.x + (p.faceRight ? 17.f : -17.f), h.y - 1};
}

void KnifeAttack() {
  p.attackT = KNIFE_TIME;
  p.attackCD = KNIFE_COOLDOWN;
  Play(SFX_SWISH);
  float dir = p.faceRight ? 1.f : -1.f;
  Rectangle hb = p.faceRight ? Rectangle{p.x + PW() - 4, p.y - 6, 26, PH() + 8} : Rectangle{p.x - 22, p.y - 6, 26, PH() + 8};
  bool hit = false;
  for (auto& e : enemies) {
    if (EnemyHittable(e) && CheckCollisionRecs(hb, EnemyBody(e))) {
      if (e.type == TANK) DamageTank(e, 1, dir);  // a knife barely scratches armor
      else SliceEnemy(e, dir);
      hit = true;
    }
  }
  for (auto& f : flames) {
    if (f.alive && CheckCollisionRecs(hb, {f.x, f.y + 4, 16, 8})) {
      f.alive = false;
      Sparks(f.x + 8, f.y + 8, -dir, 12);
      Play(SFX_SIZZLE);
      hit = true;
    }
  }
  if (boss.alive && boss.active && CheckCollisionRecs(hb, {boss.x, boss.y, 32, 32})) {
    DamageBoss(5, dir, true);
    hit = true;
  }
  for (auto& h : enemyHelis) {
    if (EnemyHeliHittable(h) && CheckCollisionRecs(hb, EnemyHeliRect(h))) {
      DamageEnemyHeli(h, 2, dir);
      hit = true;
    }
  }
  for (auto& b : enemyBullets) {  // a well-timed slash knocks bullets out of the air
    if (b.life > 0 && CheckCollisionPointRec({b.x, b.y}, hb)) {
      Sparks(b.x, b.y, -dir, 5);
      b.life = 0;
      hit = true;
    }
  }
  if (hit) {
    hitStop = std::max(hitStop, 0.07f);
    Shake(0.18f, 3);
  }
}

void StartReload() {
  if (p.reloadT > 0 || p.ammo >= AK_MAG) return;
  p.reloadT = AK_RELOAD;
  Play(SFX_RELOAD);
}

void FireAK() {
  if (p.reloadT > 0) return;
  if (p.ammo <= 0) {
    if (attackLatch) Play(SFX_EMPTY);
    StartReload();
    return;
  }
  if (p.fireCD > 0) return;
  float dir = p.faceRight ? 1.f : -1.f;
  p.fireCD = AK_RATE;
  p.flashT = 0.05f;
  p.ammo--;
  Vector2 m = MuzzlePos();
  bullets.push_back({m.x, m.y, dir * BULLET_SPEED, Randf(-12, 12), 1.0f});
  Vector2 h = HandPos();
  AddParticle(P_CASING, h.x, h.y - 2, -dir * Randf(40, 90), Randf(-210, -140), 2.5f, 1.5f, {230, 190, 60, 255});
  AddParticle(P_SMOKE, m.x, m.y, dir * 20, -15, 0.35f, 2, {200, 200, 200, 160});
  Play(SFX_SHOT);
  Shake(0.06f, 1.2f);
  if (p.ammo == 0) StartReload();
}

void UpdateWeapon() {
  if (p.attackT > 0) p.attackT -= DT;
  if (p.attackCD > 0) p.attackCD -= DT;
  if (p.fireCD > 0) p.fireCD -= DT;
  if (p.flashT > 0) p.flashT -= DT;
  if (p.reloadT > 0) {
    p.reloadT -= DT;
    if (p.reloadT <= 0) p.ammo = AK_MAG;
  }
  if (switchLatch == 1 && weapon != W_KNIFE) {
    weapon = W_KNIFE;
    p.reloadT = 0;
    Play(SFX_SWITCH);
  } else if (switchLatch == 2 && weapon != W_AK) {
    weapon = W_AK;
    p.attackT = 0;
    Play(SFX_SWITCH);
  }
  if (weapon == W_KNIFE) {
    if (attackLatch && p.attackCD <= 0) KnifeAttack();
  } else {
    if (reloadLatch) StartReload();
    if (IsKeyDown(KEY_F) || attackLatch) FireAK();
  }
  if (grenadeLatch) ThrowGrenade();
  if (airstrikeLatch) CallAirstrike();
  if (droneLatch) LaunchDrone();
}

void PlayerPhysics(float dir, bool run, bool jumpPressed, bool jumpDown) {
  float maxV = run ? 150.f : 90.f;
  if (dir != 0) {
    bool skid = p.vx * dir < 0;
    float acc = skid ? (p.onGround ? 650.f : 300.f) : (p.onGround ? 320.f : 240.f);
    p.vx += dir * acc * DT;
    if (fabsf(p.vx) > maxV) p.vx = Approach(p.vx, dir * maxV, 400 * DT);
    if (p.onGround) p.faceRight = dir > 0;
  } else if (p.onGround) {
    p.vx = Approach(p.vx, 0, 380 * DT);
  }

  if (jumpPressed && p.onGround) {
    p.vy = -(310.f + fabsf(p.vx) * 0.2f);
    p.onGround = false;
    Play(SFX_JUMP);
  }
  p.vy += ((p.vy < 0 && jumpDown) ? GRAVITY_HOLD : GRAVITY) * DT;
  p.vy = std::min(p.vy, MAX_FALL);

  if (MoveX(p.x, p.y, PW(), PH(), p.vx * DT)) p.vx = 0;
  if (p.x < camX && !droneMode && !camReturn) {
    p.x = camX;
    if (p.vx < 0) p.vx = 0;
  }

  int hx = 0, hy = 0;
  bool rising = p.vy < 0;
  bool hit = MoveY(p.x, p.y, PW(), PH(), p.vy * DT, hx, hy);
  p.onGround = false;
  if (hit) {
    if (rising) {
      if (state == ST_PLAYING) BumpTile(hx, hy);
    } else {
      p.onGround = true;
    }
    p.vy = 0;
  }

  if (!p.onGround) {
    p.pose = POSE_JUMP;
  } else if (dir != 0 && p.vx * dir < 0 && fabsf(p.vx) > 30) {
    p.pose = POSE_SKID;
  } else if (fabsf(p.vx) > 4) {
    p.animT += fabsf(p.vx) * DT;
    p.pose = POSE_WALK1 + ((int)(p.animT / 7)) % 3;
  } else {
    p.pose = POSE_IDLE;
  }
}

bool JumpDown() { return IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_Z) || IsKeyDown(KEY_UP) || IsKeyDown(KEY_W); }

void StartFlag() {
  state = ST_GOAL;
  goalPhase = GP_SLIDE;
  stateT = 0;
  p.vx = p.vy = 0;
  p.x = poleX - PW();
  p.faceRight = true;
  int pts = p.y < 48 ? 5000 : p.y < 80 ? 2000 : p.y < 112 ? 800 : p.y < 144 ? 400 : 100;
  AddScore(pts, poleX + 4, p.y);
  Play(SFX_FLAG);
}

void StartAxe() {
  state = ST_GOAL;
  goalPhase = GP_COLLAPSE;
  stateT = 0;
  p.vx = p.vy = 0;
  for (int y = 0; y < LH; y++)
    for (int x = 0; x < LW; x++)
      if (Tile(x, y) == 'A') Tile(x, y) = '.';
  Play(SFX_BREAK);
}

void UpdatePlayer() {
  bool left = IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A);
  bool right = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
  bool run = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) || IsKeyDown(KEY_X);
  float dir = (right ? 1.f : 0.f) - (left ? 1.f : 0.f);
  if (heliLatch && !droneMode) {
    if (inHeli) ExitHeli();
    else if (NearHeli()) BoardHeli();
  }
  if (inHeli) {  // UpdateHeli flies and keeps the player in the seat
    if (p.invuln > 0) p.invuln -= DT;
    return;
  }
  if (droneMode) PlayerPhysics(0, false, false, false);  // standing still while piloting
  else PlayerPhysics(dir, run, jumpLatch, JumpDown());
  if (state != ST_PLAYING) return;
  if (p.invuln > 0) p.invuln -= DT;
  if (droneMode) UpdateDrone();
  else UpdateWeapon();

  // floating coins
  int l = (int)floorf(p.x / TILE), r = (int)floorf((p.x + PW() - 0.01f) / TILE);
  int t = (int)floorf(p.y / TILE), b = (int)floorf((p.y + PH() - 0.01f) / TILE);
  for (int ty = std::max(t, 0); ty <= std::min(b, LH - 1); ty++)
    for (int tx = std::max(l, 0); tx <= std::min(r, LW - 1); tx++)
      if (Tile(tx, ty) == 'o') {
        Tile(tx, ty) = '.';
        score += 200;
        AddCoin();
      }

  if (TouchesTile({p.x + 2, p.y + 4, PW() - 4, PH() - 4}, 'L')) {
    LavaSplash(p.x + PW() / 2, p.y + PH());
    KillPlayer(true);
    return;
  }
  if (p.y > VH + 16) {
    KillPlayer(true);
    return;
  }
  if (Lvl().axeGoal) {
    if (TouchesTile({p.x, p.y, PW(), PH()}, 'A')) StartAxe();
  } else if (p.x + PW() >= poleX - 8) {
    StartFlag();
  }
}

void UpdateItems() {
  for (auto& it : items) {
    if (!it.alive) continue;
    if (it.emerge > 0) {
      float d = 20 * DT;
      it.y -= d;
      it.emerge -= d;
      if (it.emerge <= 0) it.vx = 60;
    } else {
      it.vy = std::min(it.vy + 1400 * DT, MAX_FALL);
      if (MoveX(it.x, it.y, 16, 16, it.vx * DT)) it.vx = -it.vx;
      int a, b;
      if (MoveY(it.x, it.y, 16, 16, it.vy * DT, a, b)) it.vy = 0;
      if (it.y > VH + 16 || it.x < CullLeft()) it.alive = false;
    }
    if (it.alive && CheckCollisionRecs({p.x, p.y, PW(), PH()}, {it.x + 2, it.y + 2, 12, 14})) {
      it.alive = false;
      AddScore(1000, it.x, it.y);
      Play(SFX_POWERUP);
      if (!p.big) {
        p.big = true;
        p.y -= 16;
        transformT = 0.8f;
        transformToBig = true;
      }
    }
  }
}

// Zombies claw out of the ground, then shamble toward the player, hop small obstacles and wait at ledges.
void UpdateZombie(Enemy& e) {
  if (e.flashT > 0) e.flashT -= DT;
  if (e.emerge > 0) {
    e.emerge -= DT;
    if (GetRandomValue(0, 1))
      AddParticle(P_CASING, e.x + Randf(2, 14), e.y + 15, Randf(-50, 50), Randf(-150, -60), 0.8f, 1.5f, C_HAIR);
    return;
  }
  float dx = (p.x + PW() / 2) - (e.x + 8);
  float dir = dx > 0 ? 1.f : -1.f;
  if (fabsf(dx) < 240) e.vx = fabsf(dx) < 4 ? 0 : dir * ZOMBIE_SPEED;
  else if (e.vx == 0) e.vx = -ZOMBIE_SPEED;
  if (e.onGround && e.vx != 0 && !SolidAtPx(e.vx > 0 ? e.x + 17 : e.x - 1, e.y + 17)) e.vx = 0;
  if (e.vx != 0) e.faceRight = e.vx > 0;
  else e.faceRight = dir > 0;

  e.vy = std::min(e.vy + 1400 * DT, MAX_FALL);
  float before = e.x;
  if (MoveX(e.x, e.y, 16, 16, e.vx * DT) && e.onGround) e.vy = -260;  // hop
  if (e.vx == 0) e.x = before;
  int a, b;
  e.onGround = MoveY(e.x, e.y, 16, 16, e.vy * DT, a, b) && e.vy > 0;
  if (e.onGround) e.vy = 0;

  e.groanT -= DT;
  if (e.groanT <= 0) {
    if (e.x > camX - 16 && e.x < camX + VW) Play(SFX_GROAN);
    e.groanT = Randf(3, 7);
  }
  if (TouchesTile({e.x + 2, e.y + 4, 12, 12}, 'L')) {
    LavaSplash(e.x + 8, e.y + 12);
    e.alive = false;
  }
  if (e.y > VH + 16 || e.x < CullLeft()) e.alive = false;
}

void UpdateEnemies() {
  for (auto& e : enemies) {
    if (!e.alive) continue;
    if (!e.active) {
      if (e.x < camX + VW + 16) e.active = true;
      else continue;
    }
    e.animT += DT;
    if (e.type == ZOMBIE && e.st == WALK) {
      UpdateZombie(e);
      continue;
    }
    if (e.type == TANK) {
      UpdateTank(e);
      continue;
    }
    UpdateEnemyGun(e);
    switch (e.st) {
      case FLIPPED:
        e.vy += 1000 * DT;
        e.x += e.vx * DT;
        e.y += e.vy * DT;
        if (e.y > VH + 32) e.alive = false;
        break;
      case SQUISHED:
        e.t -= DT;
        if (e.t <= 0) e.alive = false;
        break;
      default: {
        if (e.st == SHELL) {
          e.t += DT;
          if (e.t > 7.0f) {  // koopa climbs back out
            e.st = WALK;
            e.vx = (p.x < e.x) ? -ENEMY_SPEED : ENEMY_SPEED;
          }
        }
        if (e.kickGrace > 0) e.kickGrace -= DT;
        if (e.smart && e.st == WALK && e.onGround) {
          float aheadX = e.vx > 0 ? e.x + 17 : e.x - 1;
          if (!SolidAtPx(aheadX, e.y + 17)) e.vx = -e.vx;
        }
        e.vy = std::min(e.vy + 1400 * DT, MAX_FALL);
        if (MoveX(e.x, e.y, 16, 16, e.vx * DT)) {
          e.vx = -e.vx;
          if (e.st == SHELL_MOVING && e.x > camX - 16 && e.x < camX + VW) Play(SFX_BUMP);
        }
        int a, b;
        e.onGround = MoveY(e.x, e.y, 16, 16, e.vy * DT, a, b) && e.vy > 0;
        if (e.onGround) e.vy = 0;
        if (TouchesTile({e.x + 2, e.y + 4, 12, 12}, 'L')) {
          LavaSplash(e.x + 8, e.y + 12);
          e.alive = false;
        }
        if (e.y > VH + 16 || e.x < CullLeft() || (e.st == SHELL_MOVING && e.x > camX + VW + 48 && !droneMode))
          e.alive = false;
        break;
      }
    }
  }

  // enemy vs enemy
  for (size_t i = 0; i < enemies.size(); i++) {
    Enemy& a = enemies[i];
    if (!EnemyHittable(a)) continue;
    if (a.type == ZOMBIE || a.type == TANK) continue;  // only weapons hurt these; they ignore other enemies
    for (size_t j = i + 1; j < enemies.size(); j++) {
      Enemy& b = enemies[j];
      if (!EnemyHittable(b) || b.type == ZOMBIE || b.type == TANK) continue;
      if (!CheckCollisionRecs({a.x, a.y, 16, 16}, {b.x, b.y, 16, 16})) continue;
      bool am = a.st == SHELL_MOVING, bm = b.st == SHELL_MOVING;
      if (am || bm) {
        if (am) KillEnemy(b, a.vx > 0 ? 1.f : -1.f);
        if (bm) KillEnemy(a, b.vx > 0 ? 1.f : -1.f);
      } else if (a.x < b.x) {
        a.vx = -fabsf(a.vx);
        b.vx = fabsf(b.vx);
      } else {
        a.vx = fabsf(a.vx);
        b.vx = -fabsf(b.vx);
      }
    }
  }
}

void UpdateBoss() {
  if (!boss.present || !boss.alive) return;
  if (!boss.active) {
    if (boss.x < camX + VW) boss.active = true;
    else return;
  }
  boss.animT += DT;
  if (boss.flashT > 0) boss.flashT -= DT;
  boss.faceRight = p.x > boss.x + 16;
  bool bridgeDown = state == ST_GOAL;
  if (!bridgeDown) {
    if (boss.x < boss.minX) boss.vx = fabsf(boss.vx);
    if (boss.x > boss.maxX) boss.vx = -fabsf(boss.vx);
    boss.jumpT -= DT;
    if (boss.jumpT <= 0 && boss.onGround) {
      boss.vy = -300;
      boss.jumpT = Randf(1.5f, 3.0f);
    }
    boss.fireT -= DT;
    if (boss.fireT <= 0) {
      boss.mouthT = 0.45f;
      boss.fireT = Randf(2.0f, 3.2f);
    }
    if (boss.mouthT > 0) {
      boss.mouthT -= DT;
      if (boss.mouthT <= 0) {
        float d = boss.faceRight ? 1.f : -1.f;
        Flame f{boss.faceRight ? boss.x + 30 : boss.x - 14, boss.y + 6, d * 110, std::clamp(p.y + PH() - 14, 96.f, 136.f)};
        flames.push_back(f);
        Play(SFX_ROAR);
      }
    }
  }
  boss.vy = std::min(boss.vy + 1200 * DT, MAX_FALL);
  if (!bridgeDown) MoveX(boss.x, boss.y, 32, 32, boss.vx * DT);
  int a, b;
  boss.onGround = MoveY(boss.x, boss.y, 32, 32, boss.vy * DT, a, b) && boss.vy > 0;
  if (boss.onGround) boss.vy = 0;
  if (!boss.splashed && TouchesTile({boss.x, boss.y, 32, 32}, 'L')) {
    boss.splashed = true;
    LavaSplash(boss.x + 16, boss.y + 28);
    Play(SFX_ROAR);
  }
  if (boss.y > VH + 16) {
    boss.alive = false;
    AddScore(5000, boss.x, VH - 40);
  }
  if (state == ST_PLAYING && CheckCollisionRecs({p.x, p.y, PW(), PH()}, {boss.x + 4, boss.y + 4, 24, 28})) HurtPlayer();
}

void UpdateHazards() {
  for (auto& fb : firebars) {
    fb.angle += fb.speed * DT;
    for (int i = 0; i < fb.n; i++) {
      Vector2 c{fb.cx + cosf(fb.angle) * i * 8, fb.cy + sinf(fb.angle) * i * 8};
      if (CheckCollisionCircleRec(c, 3, {p.x + 1, p.y + 2, PW() - 2, PH() - 2})) HurtPlayer();
    }
  }
  for (auto& f : flames) {
    if (!f.alive) continue;
    f.t += DT;
    f.x += f.vx * DT;
    f.y = Approach(f.y, f.targetY, 30 * DT);
    if (f.t > 8 || f.x < camX - 32 || f.x > camX + VW + 32) f.alive = false;
    if (CheckCollisionRecs({p.x, p.y, PW(), PH()}, {f.x + 2, f.y + 5, 12, 6})) HurtPlayer();
  }
}

void UpdateBullets() {
  for (auto& b : bullets) {
    if (b.life <= 0) continue;
    b.life -= DT;
    b.x += b.vx * DT;
    b.y += b.vy * DT;
    float dir = b.vx > 0 ? 1.f : -1.f;
    if (SolidAtPx(b.x, b.y)) {
      Sparks(b.x, b.y, dir, 6);
      if (GetRandomValue(0, 2) == 0) Play(SFX_RICOCHET);
      b.life = 0;
      continue;
    }
    for (auto& e : enemies) {
      if (EnemyHittable(e) && CheckCollisionPointRec({b.x, b.y}, EnemyBody(e))) {
        ShootEnemy(e, dir);
        b.life = 0;
        break;
      }
    }
    if (b.life > 0 && boss.alive && boss.active && CheckCollisionPointRec({b.x, b.y}, {boss.x, boss.y, 32, 32})) {
      DamageBoss(1, dir, false);
      b.life = 0;
    }
    for (auto& h : enemyHelis) {
      if (b.life > 0 && EnemyHeliHittable(h) && CheckCollisionPointRec({b.x, b.y}, EnemyHeliRect(h))) {
        DamageEnemyHeli(h, 1, dir);
        b.life = 0;
      }
    }
    if (b.x < camX - 8 || b.x > camX + VW + 8) b.life = 0;
  }
  bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [](const Bullet& b) { return b.life <= 0; }), bullets.end());
}

void PlayerVsEnemies() {
  if (state != ST_PLAYING || transformT > 0 || inHeli) return;
  Rectangle pr{p.x, p.y, PW(), PH()};
  bool stomped = false;
  for (auto& e : enemies) {
    if (!EnemyHittable(e) && !(e.alive && e.st == WRECK)) continue;
    if (e.type == TANK) {  // hull is a platform; the treads hurt from the side
      Rectangle tr{e.x, e.y, 32, 16};
      if (!CheckCollisionRecs(pr, tr)) continue;
      if (p.vy >= 0 && (p.y + PH()) - e.y < 8) {
        p.y = e.y - PH();
        p.vy = 0;
        p.onGround = true;
        p.x += e.vx * DT;
      } else if (e.st != WRECK) {
        HurtPlayer();
        if (state != ST_PLAYING) return;
        p.vx = (p.x + PW() / 2 < e.x + 16) ? -80.f : 80.f;
      } else {
        p.x = (p.x + PW() / 2 < e.x + 16) ? e.x - PW() : e.x + 32;  // wreck blocks like a wall
      }
      continue;
    }
    if (!EnemyHittable(e)) continue;
    if (e.type == ZOMBIE) {  // stomping just bounces off; only weapons kill zombies
      if (e.emerge > 0 || !CheckCollisionRecs(pr, {e.x + 3, e.y - 6, 10, 22})) continue;
      if (p.vy > 0 && (p.y + PH()) - (e.y - 8) < 10) {
        p.vy = JumpDown() ? -340.f : -220.f;
        p.y = e.y - 8 - PH();
        p.onGround = false;
        Play(SFX_BUMP);
      } else {
        HurtPlayer();
        if (state != ST_PLAYING) return;
      }
      continue;
    }
    if (!CheckCollisionRecs(pr, {e.x + 2, e.y + 3, 12, 13})) continue;
    bool stomp = stomped || (p.vy > 0 && (p.y + PH()) - e.y < 10);
    auto bounce = [&]() {
      p.vy = JumpDown() ? -360.f : -220.f;
      p.y = e.y - PH();
      p.onGround = false;
      stomped = true;
    };
    switch (e.st) {
      case WALK:
        if (stomp) {
          if (e.type == GOOMBA) {
            e.st = SQUISHED;
            e.t = 0.5f;
          } else {
            e.st = SHELL;
            e.vx = 0;
            e.t = 0;
          }
          AddScore(100, e.x, e.y);
          Play(SFX_STOMP);
          bounce();
        } else {
          HurtPlayer();
        }
        break;
      case SHELL: {
        float dir = (p.x + PW() / 2 < e.x + 8) ? 1.f : -1.f;
        e.st = SHELL_MOVING;
        e.vx = dir * SHELL_SPEED;
        e.kickGrace = 0.3f;
        AddScore(400, e.x, e.y);
        Play(SFX_KICK);
        if (stomp) bounce();
        break;
      }
      case SHELL_MOVING:
        if (stomp) {
          e.st = SHELL;
          e.vx = 0;
          e.t = 0;
          AddScore(100, e.x, e.y);
          Play(SFX_STOMP);
          bounce();
        } else if (e.kickGrace <= 0) {
          HurtPlayer();
        }
        break;
      default:
        break;
    }
    if (state != ST_PLAYING) return;
  }
}

void UpdateParticles() {
  for (auto& pt : particles) {
    pt.life -= DT;
    if (pt.stuck) continue;
    switch (pt.kind) {
      case P_BLOOD:
      case P_CASING: pt.vy += 900 * DT; break;
      case P_LAVA: pt.vy += 600 * DT; break;
      case P_SPARK: pt.vx *= 0.9f; pt.vy *= 0.9f; break;
      case P_SMOKE: pt.vy -= 20 * DT; break;
      case P_FIRE: pt.vx *= 0.9f; pt.vy = pt.vy * 0.9f - 60 * DT; break;
    }
    float nx = pt.x + pt.vx * DT, ny = pt.y + pt.vy * DT;
    bool collides = pt.kind == P_BLOOD || pt.kind == P_CASING || pt.kind == P_LAVA;
    if (collides && SolidAtPx(nx, ny)) {
      bool vertical = !SolidAtPx(nx, pt.y);
      if (pt.kind == P_BLOOD) {
        // stick to the surface it hit (floor, ceiling or wall)
        if (vertical) pt.y = pt.vy > 0 ? floorf(ny / TILE) * TILE - 0.5f : (floorf(ny / TILE) + 1) * TILE;
        else pt.x = pt.vx > 0 ? floorf(nx / TILE) * TILE - 0.5f : (floorf(nx / TILE) + 1) * TILE;
        pt.stuck = true;
        pt.vx = pt.vy = 0;
      } else if (pt.kind == P_CASING) {
        if (vertical) {
          pt.vy = -pt.vy * 0.35f;
          pt.vx *= 0.6f;
        } else {
          pt.vx = -pt.vx * 0.5f;
        }
      } else {
        pt.life = 0;
      }
      continue;
    }
    pt.x = nx;
    pt.y = ny;
  }
  particles.erase(std::remove_if(particles.begin(), particles.end(),
                                 [](const Particle& pt) { return pt.life <= 0 || pt.y > VH + 20; }),
                  particles.end());
  if (particles.size() > 1600) particles.erase(particles.begin(), particles.begin() + (particles.size() - 1600));
}

void UpdateHalves() {
  for (auto& h : halves) {
    h.t += DT;
    if (h.gushT > 0) {  // the bottom half keeps spurting for a moment
      h.gushT -= DT;
      for (int i = 0; i < 2; i++)
        AddParticle(P_BLOOD, h.x + Randf(-3, 3), h.y - h.srcH / 2, Randf(-45, 45), Randf(-230, -110), Randf(3, 5),
                    Randf(1, 2), BloodColor());
    }
    if (h.landed) continue;
    if (h.top && h.t < 0.8f && GetRandomValue(0, 1))
      AddParticle(P_BLOOD, h.x, h.y + h.srcH / 2, Randf(-20, 20), Randf(-20, 20), Randf(3, 5), 1.2f, BloodColor());
    h.vy = std::min(h.vy + 1200 * DT, MAX_FALL);
    h.x += h.vx * DT;
    h.y += h.vy * DT;
    h.rot += h.vrot * DT;
    if (SolidAtPx(h.x + (h.vx > 0 ? 5.f : -5.f), h.y)) h.vx = -h.vx * 0.3f;
    float bottom = h.y + h.srcH / 2;
    if (h.vy > 0 && SolidAtPx(h.x, bottom)) {
      h.y = floorf(bottom / TILE) * TILE - h.srcH / 2;
      if (!h.bounced && h.vy > 150) {
        h.bounced = true;
        h.vy = -h.vy * 0.3f;
        h.vx *= 0.5f;
        h.vrot *= 0.4f;
        BloodBurst(h.x, bottom - 1, h.vx > 0 ? 1.f : -1.f, 8, 0.5f);
        if (h.top) Play(SFX_SPLAT);
      } else {
        h.landed = true;
        h.vx = h.vy = 0;
        h.rot = roundf(h.rot / PI) * PI;  // settle flat, either side up
      }
    }
  }
  halves.erase(std::remove_if(halves.begin(), halves.end(), [](const Half& h) { return h.t > 6.0f || h.y > VH + 40; }),
               halves.end());
}

void UpdateFx() {
  for (auto& t : bumpT)
    if (t > 0) t = std::max(0.f, t - DT);
  for (auto& f : effects) {
    f.t += DT;
    f.vy += (f.type == FX_COIN ? 900.f : 1200.f) * DT;
    f.x += f.vx * DT;
    f.y += f.vy * DT;
    if (f.type == FX_COIN && f.t > 0.45f) {
      popups.push_back({f.x, f.y, 0, "200"});
      score += 200;
    }
  }
  effects.erase(std::remove_if(effects.begin(), effects.end(),
                               [](const Effect& f) { return (f.type == FX_COIN && f.t > 0.45f) || f.y > VH + 16; }),
                effects.end());
  for (auto& pu : popups) {
    pu.t += DT;
    pu.y -= 30 * DT;
  }
  popups.erase(std::remove_if(popups.begin(), popups.end(), [](const Popup& pu) { return pu.t > 0.8f; }), popups.end());
  for (auto& ex : explosions) ex.t += DT;
  explosions.erase(std::remove_if(explosions.begin(), explosions.end(), [](const Explosion& ex) { return ex.t > 0.45f; }),
                   explosions.end());
  UpdateParticles();
  UpdateHalves();
  enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](const Enemy& e) { return !e.alive; }), enemies.end());
  items.erase(std::remove_if(items.begin(), items.end(), [](const Item& it) { return !it.alive; }), items.end());
  flames.erase(std::remove_if(flames.begin(), flames.end(), [](const Flame& f) { return !f.alive; }), flames.end());
}

void UpdatePlaying() {
  if (transformT > 0) {  // world freezes while growing/shrinking
    transformT -= DT;
    return;
  }
  timeAcc += DT;
  if (timeAcc >= 0.4f) {
    timeAcc -= 0.4f;
    if (--timeLeft <= 0) {
      timeLeft = 0;
      KillPlayer(false);
      return;
    }
  }
  UpdatePlayer();
  if (state != ST_PLAYING) return;
  UpdateHeli();
  if (state != ST_PLAYING) return;
  UpdateItems();
  UpdateEnemies();
  UpdateBoss();
  UpdateHazards();
  UpdateBullets();
  UpdateJet();
  UpdateProjectiles();
  if (state != ST_PLAYING) return;
  UpdateEnemyHelis();
  UpdateEnemyBullets();
  if (state != ST_PLAYING) return;
  PlayerVsEnemies();
  UpdateCamera();
}

void UpdateDying() {
  stateT += DT;
  if (deathByFall) {
    p.y += 25 * DT;  // sink (lava) or keep falling off-screen
  } else if (stateT > 0.5f) {
    if (!deathJumped) {
      p.vy = -380;
      deathJumped = true;
    }
    p.vy += 1000 * DT;
    p.y += p.vy * DT;
  }
  if (stateT > 3.2f) {
    if (--lives <= 0) {
      state = ST_GAMEOVER;
      stateT = 0;
    } else {
      StartLevel(levelIdx, false);
    }
  }
}

void FinishLevel() {
  if (levelIdx + 1 < LEVEL_COUNT) {
    StartLevel(levelIdx + 1, true);
  } else {
    state = ST_CLEAR;
    stateT = 0;
  }
}

void UpdateGoal() {
  const float baseTop = GROUND_Y - 16;  // top of the block under the pole
  switch (goalPhase) {
    case GP_SLIDE: {
      bool done = true;
      if (p.y + PH() < baseTop) {
        p.y = std::min(p.y + 120 * DT, baseTop - PH());
        done = false;
      }
      if (flagY < baseTop - 16) {
        flagY = std::min(flagY + 120 * DT, baseTop - 16);
        done = false;
      }
      p.pose = POSE_JUMP;
      if (done) {
        goalPhase = GP_WAIT;
        stateT = 0;
      }
      break;
    }
    case GP_WAIT:
      stateT += DT;
      if (stateT > 0.35f) {
        goalPhase = GP_WALK;
        p.x = poleX + 1;
      }
      break;
    case GP_WALK:
      PlayerPhysics(1, false, false, false);
      if (p.x + PW() / 2 >= castleX + 40) {
        playerHidden = true;
        goalPhase = GP_TALLY;
        stateT = 0;
      }
      break;
    case GP_COLLAPSE: {
      stateT += DT;
      p.pose = POSE_IDLE;
      if (stateT > 0.05f) {
        stateT = 0;
        int rightmost = -1, row = 0;
        for (int y = 0; y < LH; y++)
          for (int x = LW - 1; x > rightmost; x--)
            if (Tile(x, y) == 'b') {
              rightmost = x;
              row = y;
              break;
            }
        if (rightmost < 0) {
          goalPhase = GP_AFTER_COLLAPSE;
        } else {
          Tile(rightmost, row) = '.';
          Play(SFX_BUMP);
        }
      }
      UpdateBoss();
      break;
    }
    case GP_AFTER_COLLAPSE:
      stateT += DT;
      UpdateBoss();
      if (stateT > 1.5f) {
        goalPhase = GP_TALLY;
        stateT = 0;
      }
      break;
    case GP_TALLY:
      if (timeLeft > 0) {
        int n = std::min(timeLeft, 2);
        timeLeft -= n;
        score += 50 * n;
        if (((int)(globalT * 60)) % 4 == 0) Play(SFX_TICK);
      } else {
        goalPhase = GP_DONE;
        stateT = 0;
      }
      break;
    default:
      stateT += DT;
      if (stateT > 2.0f) FinishLevel();
      break;
  }
}

void UpdateTimers() {
  if (toastT > 0) toastT -= DT;
  if (shakeT > 0) {
    shakeT -= DT;
    if (shakeT <= 0) shakeAmp = 0;
  }
  for (auto& w : shockwaves) w.t += DT;
  shockwaves.erase(std::remove_if(shockwaves.begin(), shockwaves.end(), [](const Shockwave& w) { return w.t >= w.dur; }),
                   shockwaves.end());
}

void Step() {
  globalT += DT;
  UpdateTimers();
  if (hitStop > 0) {  // brief freeze-frame on heavy hits
    hitStop -= DT;
  } else {
    switch (state) {
      case ST_TITLE:
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
          titleLevel = (titleLevel + LEVEL_COUNT - 1) % LEVEL_COUNT;
          ResetWorld(titleLevel, false);
        }
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
          titleLevel = (titleLevel + 1) % LEVEL_COUNT;
          ResetWorld(titleLevel, false);
        }
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_TAB)) {
          titleMode = 1 - titleMode;
          Play(SFX_SWITCH);
        }
        if (enterLatch) {
          if (titleMode == 0) NewGame(titleLevel);
          else runArena = true;
        }
        break;
      case ST_INTRO:
        stateT += DT;
        if (stateT > 2.5f) state = ST_PLAYING;
        break;
      case ST_PLAYING:
        UpdatePlaying();
        UpdateFx();
        break;
      case ST_DYING:
        UpdateDying();
        UpdateJet();
        UpdateProjectiles();
        UpdateEnemyBullets();
        UpdateFx();
        break;
      case ST_GOAL:
        UpdateGoal();
        UpdateJet();
        UpdateProjectiles();
        UpdateHazards();
        UpdateCamera();
        UpdateFx();
        break;
      case ST_CLEAR:
        stateT += DT;
        if (enterLatch && stateT > 1.0f) {
          titleLevel = 0;
          ResetWorld(0, false);
          state = ST_TITLE;
        }
        break;
      case ST_GAMEOVER:
        stateT += DT;
        if (stateT > 4.0f || (enterLatch && stateT > 1.0f)) {
          ResetWorld(titleLevel, false);
          state = ST_TITLE;
        }
        break;
    }
  }
  jumpLatch = enterLatch = attackLatch = reloadLatch = false;
  grenadeLatch = airstrikeLatch = droneLatch = heliLatch = false;
  switchLatch = 0;
}

// ---------------------------------------------------------------------------
// Rendering (all in virtual 256x224 coordinates)
// ---------------------------------------------------------------------------
void DrawTextC(const char* text, int y, int size, Color c) { DrawText(text, (VW - MeasureText(text, size)) / 2, y, size, c); }

void DrawScenery(int cx) {
  const LevelInfo& L = Lvl();
  if (!L.hills && !L.clouds) return;
  for (int base = 0; base < LW * TILE; base += 48 * TILE) {
    auto sc = [&](SheetId id, int x, int y) {
      int sx = base + x - cx;
      if (sx > -100 && sx < VW + 100) DrawFrame(id, 0, (float)sx, (float)y);
    };
    if (L.hills) {
      sc(S_HILL, 0, GROUND_Y - 40);
      sc(S_HILL, 16 * TILE, GROUND_Y - 40);
      sc(S_BUSH, 11 * TILE, GROUND_Y - 16);
      sc(S_BUSH, 23 * TILE, GROUND_Y - 16);
      sc(S_BUSH, 41 * TILE, GROUND_Y - 16);
    }
    if (L.clouds) {
      sc(S_CLOUD, 8 * TILE, 32);
      sc(S_CLOUD, 19 * TILE, 16);
      sc(S_CLOUD, 27 * TILE, 40);
      sc(S_CLOUD, 36 * TILE, 24);
    }
  }
}

void DrawTiles(int cx) {
  static const int qSeq[6] = {0, 0, 0, 1, 2, 1};
  static const int coinSeq[4] = {1, 2, 3, 2};
  const LevelInfo& L = Lvl();
  int qFrame = TF_Q0 + qSeq[(int)(globalT * 8) % 6];
  int coinFrame = coinSeq[(int)(globalT * 6) % 4];
  int lavaFrame = TF_LAVA0 + ((int)(globalT * 3)) % 2;
  int t0 = std::max(0, cx / TILE), t1 = std::min(LW - 1, (cx + VW) / TILE + 1);
  for (int ty = 0; ty < LH; ty++) {
    for (int tx = t0; tx <= t1; tx++) {
      char c = Tile(tx, ty);
      if (c == '.') continue;
      float x = (float)(tx * TILE - cx), y = (float)(ty * TILE);
      float bt = bumpT[ty * LW + tx];
      if (bt > 0) y -= sinf((1.f - bt / 0.15f) * PI) * 5.f;
      int frame = -1;
      switch (c) {
        case 'G': frame = L.groundFrame; break;
        case 'B': frame = L.brickFrame; break;
        case '?':
        case 'M': frame = qFrame; break;
        case 'U': frame = TF_USED; break;
        case 'X': frame = TF_HARD; break;
        case '<': frame = TF_PIPE_TL; break;
        case '>': frame = TF_PIPE_TR; break;
        case '(': frame = TF_PIPE_BL; break;
        case ')': frame = TF_PIPE_BR; break;
        case 'S': frame = TF_STONE; break;
        case 'L': frame = (TileAt(tx, ty - 1) == 'L') ? -2 : lavaFrame; break;
        case '[': frame = TF_TREE_L; break;
        case '=': frame = TF_TREE_M; break;
        case ']': frame = TF_TREE_R; break;
        case '|': frame = TF_TRUNK; break;
        case 'b': frame = TF_BRIDGE; break;
        case 'o': DrawFrame(S_ITEMS, coinFrame, x, y); break;
        case 'A': DrawFrame(S_ITEMS, IF_AXE, x, y + sinf(globalT * 4) * 1.5f); break;
      }
      if (frame >= 0) DrawFrame(S_TILES, frame, x, y);
      else if (frame == -2) DrawRectangle((int)x, (int)y, TILE, TILE, C_LAVA);  // lava below the surface row
    }
  }
}

void DrawWeapon(int frame, Vector2 pos, float angDeg, bool faceRight, Vector2 origin) {
  Rectangle src{frame * 24.f, 0, 24, 8};
  float rot = angDeg;
  if (!faceRight) {
    src.width = -24;
    origin.x = 24 - origin.x;
    rot = -angDeg;
  }
  DrawTexturePro(sheets[S_WEAPONS].tex, src, {floorf(pos.x), floorf(pos.y), 24, 8}, origin, rot, WHITE);
}

// Helicopter sprite for a 40x16 hitbox at (x,y): body + rotor frame, tilted around its center.
void DrawHeliSprite(float x, float y, bool faceRight, float tiltDeg, bool rotorOn, Color tint) {
  const Sheet& s = sheets[S_HELI];
  Rectangle dst{x + HELI_W / 2, y + 4, 48, 24};
  Vector2 origin{24, 12};
  float w = faceRight ? 48.f : -48.f;
  DrawTexturePro(s.tex, {0, 0, w, 24}, dst, origin, tiltDeg, tint);
  int rotor = rotorOn ? 1 + ((int)(globalT * 30)) % 2 : 1;
  DrawTexturePro(s.tex, {rotor * 48.f, 0, w, 24}, dst, origin, tiltDeg, WHITE);
}

void DrawPlayer(int cx) {
  if (playerHidden || inHeli) return;
  if (p.invuln > 0 && transformT <= 0 && ((int)(p.invuln * 20)) % 2) return;
  bool drawBig = p.big;
  if (transformT > 0) drawBig = (((int)(transformT * 10)) % 2 == 0) ? transformToBig : !transformToBig;
  SheetId id = drawBig ? S_PBIG : S_PSMALL;
  float fh = drawBig ? 32.f : 16.f;
  int frame = (state == ST_DYING && !drawBig) ? POSE_DEAD : p.pose;
  if (drawBig && frame == POSE_DEAD) frame = POSE_JUMP;
  bool flip = state == ST_DYING ? false : !p.faceRight;
  DrawFrame(id, frame, p.x - 2 - cx, p.y + PH() - fh, flip);

  if (state == ST_DYING || transformT > 0) return;
  Vector2 h = HandPos();
  h.x -= cx;
  if (weapon == W_KNIFE) {
    float ang = -25;
    if (p.attackT > 0) {
      float t = 1 - p.attackT / KNIFE_TIME;
      float e = 1 - (1 - t) * (1 - t) * (1 - t);
      ang = -120 + 190 * e;
      // slash arc trail
      float a0 = -120, a1 = ang;
      if (!p.faceRight) {
        a0 = 180 - ang;
        a1 = 300;
      }
      unsigned char alpha = (unsigned char)(220 * (p.attackT / KNIFE_TIME));
      DrawRing(h, 12, 22, a0, a1, 24, {255, 255, 255, alpha});
      DrawRing(h, 9, 13, a0, a1, 24, {140, 220, 255, (unsigned char)(alpha / 2)});
    }
    DrawWeapon(0, h, ang, p.faceRight, {3, 4});
  } else {
    float recoil = p.fireCD > 0 ? 2.f * (p.fireCD / AK_RATE) : 0.f;
    float ang = p.reloadT > 0 ? 35.f : 0.f;
    DrawWeapon(1, {h.x - (p.faceRight ? recoil : -recoil), h.y}, ang, p.faceRight, {7, 4});
    if (p.flashT > 0) {
      Vector2 m = MuzzlePos();
      m.x -= cx;
      float d = p.faceRight ? 1.f : -1.f;
      DrawTriangle({m.x, m.y - 4}, {m.x, m.y + 4}, {m.x + d * 9, m.y}, {255, 200, 60, 255});
      DrawTriangle({m.x, m.y + 4}, {m.x, m.y - 4}, {m.x + d * 9, m.y}, {255, 200, 60, 255});
      DrawCircleV(m, 3, {255, 240, 150, 255});
      DrawCircleV(m, 1.5f, WHITE);
    }
  }
}

void DrawEnemies(int cx) {
  for (const auto& e : enemies) {
    if (!e.alive || !e.active) continue;
    float x = e.x - cx;
    if (x < -32 || x > VW + 32) continue;
    int walkFrame = ((int)(e.animT * 6)) % 2;
    if (e.type == TANK) {
      Color tint = e.flashT > 0 ? Color{255, 120, 120, 255} : WHITE;
      if (e.st != WRECK) {  // barrel pivots on the turret
        float d = e.faceRight ? 1.f : -1.f;
        float deg = e.faceRight ? e.aim : 180 - e.aim;
        Vector2 pivot{e.x + 16 - cx - d * e.recoil * 0.5f, e.y + 3};
        DrawRectanglePro({pivot.x, pivot.y, 15, 3}, {0, 1.5f}, deg, C_DKOLIVE);
        DrawRectanglePro({pivot.x, pivot.y, 15, 1}, {0, 1.5f}, deg, C_OLIVE);
      }
      int frame = e.st == WRECK ? 2 : ((int)e.animT) % 2;
      DrawFrame(S_TANK, frame, x, e.y, !e.faceRight, false, tint);
      continue;
    }
    if (e.armed && e.st == WALK) {  // pistol pointed at the player
      Vector2 t = EnemyTarget();
      float gx = e.x + 8 - cx, gy = e.y + (e.type == KOOPA ? 4.f : 9.f);
      float d = t.x > e.x + 8 ? 1.f : -1.f;
      float deg = atan2f(t.y - gy, (t.x - cx) - gx) * RAD2DEG;
      DrawRectanglePro({gx + d * 3, gy, 8, 2}, {0, 1}, deg, C_GUN);
      if (e.gunFlash > 0) DrawCircleV({gx + d * 3 + cosf(deg * DEG2RAD) * 9, gy + sinf(deg * DEG2RAD) * 9}, 2, C_LAVATOP);
    }
    if (e.type == ZOMBIE) {
      int zf = ((int)(e.animT * 3)) % 2;
      if (e.st == FLIPPED) {
        DrawFrame(S_ZOMBIE, zf, x, e.y - 8, !e.faceRight, true);
      } else if (e.emerge > 0) {  // rising out of the ground
        float visible = 24.f * (1.f - e.emerge / 0.8f);
        Rectangle src{zf * 16.f, 0, e.faceRight ? 16.f : -16.f, visible};
        DrawTextureRec(sheets[S_ZOMBIE].tex, src, {floorf(x), floorf(e.y + 16 - visible)}, WHITE);
      } else {
        DrawFrame(S_ZOMBIE, zf, x, e.y - 8, !e.faceRight, false, e.flashT > 0 ? Color{255, 90, 90, 255} : WHITE);
      }
    } else if (e.type == GOOMBA) {
      if (e.st == SQUISHED) DrawFrame(S_GOOMBA, 2, x, e.y);
      else DrawFrame(S_GOOMBA, walkFrame, x, e.y, false, e.st == FLIPPED);
    } else {
      if (e.st == WALK) {
        DrawFrame(S_KOOPA, walkFrame, x, e.y - 8, e.vx < 0);
      } else {
        float shake = (e.st == SHELL && e.t > 6.0f) ? (((int)(e.t * 30)) % 2 ? 1.f : -1.f) : 0.f;
        DrawFrame(S_SHELL, 0, x + shake, e.y, false, e.st == FLIPPED);
      }
    }
  }
}

void DrawBoss(int cx) {
  if (!boss.present || !boss.alive) return;
  int frame = boss.mouthT > 0 ? 2 : ((int)(boss.animT * 4)) % 2;
  Color tint = boss.flashT > 0 ? Color{255, 90, 90, 255} : WHITE;
  DrawFrame(S_BOSS, frame, boss.x - cx, boss.y, !boss.faceRight, false, tint);
  if (boss.active) {  // health bar
    float bx = boss.x - cx;
    DrawRectangle((int)bx, (int)boss.y - 6, 32, 3, {60, 0, 0, 255});
    DrawRectangle((int)bx, (int)boss.y - 6, (int)(32.f * boss.hp / BOSS_HP), 3, {230, 40, 30, 255});
  }
}

void DrawHalves(int cx) {
  for (const auto& h : halves) {
    unsigned char a = (unsigned char)(255 * std::clamp(6.0f - h.t, 0.f, 1.f));
    Rectangle src{h.frame * h.fw, h.srcY, h.flip ? -h.fw : h.fw, h.srcH};
    Rectangle dst{h.x - cx, h.y, h.fw, h.srcH};
    float deg = h.rot * RAD2DEG;
    DrawTexturePro(sheets[h.sheet].tex, src, dst, {h.fw / 2, h.srcH / 2}, deg, {255, 255, 255, a});
    // raw cut edge
    float w = h.fw - 4;
    Vector2 origin = h.top ? Vector2{w / 2, -(h.srcH / 2 - 2)} : Vector2{w / 2, h.srcH / 2};
    DrawRectanglePro({h.x - cx, h.y, w, 2}, origin, deg, {170, 0, 0, a});
  }
}

void DrawParticles(int cx) {
  for (const auto& pt : particles) {
    Color c = pt.c;
    float fade = std::clamp(pt.life, 0.f, 1.f);
    c.a = (unsigned char)(c.a * fade);
    float x = pt.x - cx;
    if (x < -8 || x > VW + 8) continue;
    if (pt.kind == P_SMOKE) {
      DrawCircleV({x, pt.y}, std::max(1.f, pt.size + (0.35f - pt.life) * 10), c);
    } else if (pt.kind == P_FIRE) {
      Color fc = pt.life > 0.3f ? pt.c : Color{150, 40, 20, (unsigned char)(255 * std::clamp(pt.life * 4, 0.f, 1.f))};
      DrawCircleV({x, pt.y}, pt.size * (0.4f + pt.life * 1.5f), fc);
    } else if (pt.stuck) {
      DrawRectangleV({x - pt.size, pt.y - 0.5f}, {pt.size * 2, std::max(1.f, pt.size * 0.7f)}, c);
    } else {
      DrawRectangleV({x - pt.size / 2, pt.y - pt.size / 2}, {pt.size, pt.size}, c);
    }
  }
}

void DrawHUD() {
  DrawText("MARIO", 16, 6, 10, WHITE);
  DrawText(TextFormat("%06d", score), 16, 16, 10, WHITE);
  DrawRectangle(90, 17, 5, 8, C_COIN);
  DrawText(TextFormat("x%02d", coins), 98, 16, 10, WHITE);
  DrawText("WORLD", 144, 6, 10, WHITE);
  DrawText(Lvl().name, 150, 16, 10, WHITE);
  DrawText("TIME", 206, 6, 10, WHITE);
  if (state != ST_TITLE && state != ST_INTRO) DrawText(TextFormat("%03d", timeLeft), 210, 16, 10, WHITE);
  if (inHeli && state == ST_PLAYING) {
    DrawRectangle(14, 27, 228, 11, {0, 0, 0, 130});
    DrawText("HELI", 16, 28, 10, C_YEL);
    DrawRectangle(42, 30, 50, 5, {60, 0, 0, 255});
    DrawRectangle(42, 30, (int)(50.f * std::max(0, heli.hp) / HELI_HP), 5, heli.hp <= 4 ? RED : GREEN);
    DrawText(TextFormat("RKT %d", heli.rockets), 100, 28, 10, WHITE);
    DrawText("GUN", 150, 28, 10, heli.overheated ? RED : WHITE);
    DrawRectangle(174, 30, 60, 5, {40, 40, 40, 255});
    DrawRectangle(174, 30, (int)(60 * std::min(1.f, heli.heat)), 5, heli.overheated ? RED : ORANGE);
  } else if (state == ST_PLAYING || state == ST_GOAL || state == ST_DYING) {
    DrawRectangle(14, 27, 74, 11, {0, 0, 0, 110});
    DrawTextureRec(sheets[S_WEAPONS].tex, {weapon == W_KNIFE ? 0.f : 24.f, 0, 24, 8}, {16, 29}, WHITE);
    if (weapon == W_KNIFE) {
      DrawText("KNIFE", 44, 28, 10, WHITE);
    } else if (p.reloadT > 0) {
      if (((int)(globalT * 6)) % 2) DrawText("RELOAD", 44, 28, 10, C_YEL);
    } else {
      DrawText(TextFormat("%02d/%d", p.ammo, AK_MAG), 44, 28, 10, p.ammo <= 5 ? Color{255, 90, 90, 255} : WHITE);
    }
    DrawRectangle(92, 27, 150, 11, {0, 0, 0, 110});
    DrawText(TextFormat("GRN %d   AIR %d   FPV %d", p.grenades, p.airstrikes, p.drones), 96, 28, 10, WHITE);
  }
}

void DrawWorld() {
  const LevelInfo& L = Lvl();
  ClearBackground(L.sky);
  int cx = (int)floorf(camX);
  DrawScenery(cx);
  if (!L.axeGoal) {
    DrawFrame(S_CASTLE, 0, castleX - cx, GROUND_Y - 80);
    for (int y = POLE_TOP_Y; y < GROUND_Y - 16; y += 16) DrawFrame(S_FLAG, 2, poleX - 8 - cx, (float)y);
    DrawFrame(S_FLAG, 1, poleX - 8 - cx, POLE_TOP_Y - 12);
    DrawFrame(S_FLAG, 0, poleX - 17 - cx, flagY);
  }

  // mushrooms drawn before tiles so they rise out from behind the block
  for (const auto& it : items) DrawFrame(S_ITEMS, IF_MUSHROOM, it.x - cx, it.y);

  DrawTiles(cx);

  for (const auto& fb : firebars)
    for (int i = 0; i < fb.n; i++)
      DrawFrame(S_ITEMS, IF_FIREBALL, fb.cx + cosf(fb.angle) * i * 8 - 8 - cx, fb.cy + sinf(fb.angle) * i * 8 - 8);

  for (const auto& f : effects)
    if (f.type == FX_COIN) DrawFrame(S_ITEMS, 1 + ((int)(f.t * 20)) % 4, f.x - cx, f.y);

  DrawEnemies(cx);
  DrawBoss(cx);
  DrawHalves(cx);
  for (const auto& h : enemyHelis) {
    if (!h.active) continue;
    Color tint = h.flashT > 0 ? Color{255, 140, 140, 255} : Color{150, 110, 110, 255};
    DrawHeliSprite(h.x - cx, h.y, h.faceRight, h.tilt + h.spin * RAD2DEG, true, tint);
  }
  if (heli.present && heli.alive) {
    Color tint = heli.flashT > 0 ? Color{255, 140, 140, 255} : WHITE;
    DrawHeliSprite(heli.x - cx, heli.y, heli.faceRight, heli.tilt, heli.rotor > 0.05f, tint);
    if (NearHeli() && state == ST_PLAYING && !droneMode)
      DrawText("H BOARD", (int)(heli.x - cx + 2), (int)heli.y - 20, 10, C_YEL);
  }
  DrawPlayer(cx);
  for (const auto& f : flames) DrawFrame(S_ITEMS, IF_FLAME, f.x - cx, f.y, f.vx < 0);
  for (const auto& b : bullets) {
    float x = b.x - cx;
    DrawLineEx({x, b.y}, {x - (b.vx > 0 ? 6.f : -6.f), b.y - b.vy * 0.012f}, 1.5f, {255, 240, 160, 255});
  }
  for (const auto& f : effects) {
    if (f.type == FX_DEBRIS) {
      Rectangle src{(float)(L.brickFrame * 16 + (f.part % 2) * 8), (float)((f.part / 2) * 8), 8, 8};
      if (((int)(f.t * 10)) % 2) src.width = -8;
      DrawTextureRec(sheets[S_TILES].tex, src, {floorf(f.x - cx), floorf(f.y)}, WHITE);
    }
  }
  for (const auto& pr : projectiles) {
    if (pr.kind == PJ_GRENADE) {
      DrawFrameRot(S_ITEMS, IF_GRENADE, pr.x - cx, pr.y - 1, pr.rot);
      if (pr.fuse < 0.6f && ((int)(pr.fuse * 16)) % 2) DrawCircleV({pr.x - cx, pr.y - 6}, 1.5f, RED);
    } else if (pr.kind == PJ_TANK_SHELL) {
      DrawCircleV({pr.x - cx, pr.y}, 3, {255, 160, 60, 120});
      DrawCircleV({pr.x - cx, pr.y}, 2, C_GUN);
    } else {
      Color tint = pr.kind == PJ_ENEMY_ROCKET ? Color{255, 120, 120, 255} : WHITE;
      DrawFrameRot(S_ITEMS, IF_BOMB, pr.x - cx, pr.y, pr.rot, tint);
    }
  }
  for (const auto& b : enemyBullets) {
    float x = b.x - cx;
    float len = std::max(1.f, hypotf(b.vx, b.vy));
    DrawLineEx({x, b.y}, {x - b.vx / len * 5, b.y - b.vy / len * 5}, 1.5f, {255, 110, 80, 255});
  }
  for (const auto& ex : explosions) {
    float k = ex.t / 0.45f;
    Vector2 c{ex.x - cx, ex.y};
    if (k < 0.25f) DrawCircleV(c, ex.r * (0.3f + k * 2), {255, 255, 230, 255});
    DrawCircleV(c, ex.r * (0.5f + 0.5f * k), {255, 150, 40, (unsigned char)(220 * (1 - k))});
    DrawCircleV(c, ex.r * 0.45f * (1 - k), {255, 230, 120, (unsigned char)(255 * (1 - k))});
    DrawRing(c, ex.r * (0.6f + 0.8f * k), ex.r * (0.65f + 0.8f * k), 0, 360, 32, {255, 255, 255, (unsigned char)(160 * (1 - k))});
  }
  DrawParticles(cx);
  if (jet.active) DrawFrame(S_JET, 0, jet.x - 16 - cx, jet.y - 8);
  if (drone.active) {
    DrawFrame(S_DRONE, ((int)(globalT * 30)) % 2, drone.x - 8 - cx, drone.y - 4);
    if (drone.payload > 0) DrawRectangle((int)(drone.x - 2 - cx), (int)drone.y + 2, 4, 4, C_DKOLIVE);
  }
  for (const auto& pu : popups) DrawText(pu.text.c_str(), (int)(pu.x - cx), (int)pu.y, 10, WHITE);
}

// Analog FPV feed look: static, scanlines, vignette, crosshair, drop marker and telemetry.
void DrawFpvOverlay() {
  const Color green{0, 255, 120, 255};
  float cx = floorf(camX);
  float dx = drone.x - cx, dy = drone.y;
  float charge = std::max(0.f, drone.battery / DRONE_BATTERY);

  for (int y = 0; y < VH; y += 2) DrawRectangle(0, y, VW, 1, {0, 0, 0, 45});
  int noise = 60 + (int)((1 - charge) * 260);
  for (int i = 0; i < noise; i++) {
    unsigned char g = (unsigned char)GetRandomValue(120, 255);
    DrawPixel(GetRandomValue(0, VW - 1), GetRandomValue(0, VH - 1), {g, g, g, 150});
  }
  if (GetRandomValue(0, 40) == 0) DrawRectangle(0, GetRandomValue(0, VH), VW, GetRandomValue(1, 3), {255, 255, 255, 60});
  DrawRectangleGradientV(0, 0, VW, 28, {0, 0, 0, 140}, BLANK);
  DrawRectangleGradientV(0, VH - 40, VW, 40, BLANK, {0, 0, 0, 170});
  DrawRectangleGradientH(0, 0, 24, VH, {0, 0, 0, 120}, BLANK);
  DrawRectangleGradientH(VW - 24, 0, 24, VH, BLANK, {0, 0, 0, 120});

  // drop marker: where a released bomb would land
  float gy = dy;
  while (gy < VH && !SolidAtPx(drone.x, gy)) gy += 2;
  for (float y = dy + 8; y < gy - 2; y += 6) DrawLineV({dx, y}, {dx, y + 3}, {255, 80, 80, 160});
  if (gy < VH) {
    DrawCircleLinesV({dx, gy - 2}, 5, {255, 80, 80, 220});
    DrawLineV({dx - 7, gy - 2}, {dx + 7, gy - 2}, {255, 80, 80, 220});
  }
  // crosshair
  DrawCircleLinesV({dx, dy}, 11, green);
  DrawLineV({dx - 16, dy}, {dx - 6, dy}, green);
  DrawLineV({dx + 6, dy}, {dx + 16, dy}, green);
  DrawLineV({dx, dy - 16}, {dx, dy - 6}, green);
  // corner brackets
  const int m = 6, l = 14;
  DrawLine(m, 36, m + l, 36, green); DrawLine(m, 36, m, 36 + l, green);
  DrawLine(VW - m, 36, VW - m - l, 36, green); DrawLine(VW - m, 36, VW - m, 36 + l, green);
  DrawLine(m, VH - 6, m + l, VH - 6, green); DrawLine(m, VH - 6, m, VH - 6 - l, green);
  DrawLine(VW - m, VH - 6, VW - m - l, VH - 6, green); DrawLine(VW - m, VH - 6, VW - m, VH - 6 - l, green);

  DrawText("FPV", 12, VH - 36, 10, green);
  if (((int)(globalT * 2)) % 2 == 0) {
    DrawCircle(40, VH - 31, 2, RED);
    DrawText("REC", 45, VH - 36, 10, RED);
  }
  Color bat = charge < 0.3f ? Color{255, 80, 80, 255} : green;
  DrawRectangleLines(12, VH - 22, 42, 7, bat);
  DrawRectangle(14, VH - 20, (int)(38 * charge), 3, bat);
  DrawRectangle(54, VH - 20, 2, 3, bat);
  DrawText(TextFormat("%d%%", (int)(charge * 100)), 60, VH - 23, 10, bat);
  DrawText(TextFormat("ALT %dM", std::max(0, (int)((GROUND_Y - drone.y) / TILE))), 96, VH - 36, 10, green);
  DrawText(TextFormat("SPD %d", (int)hypotf(drone.vx, drone.vy)), 160, VH - 36, 10, green);
  DrawText(drone.payload > 0 ? "F DROP" : "F KAMIKAZE", 96, VH - 23, 10, drone.payload > 0 ? green : Color{255, 80, 80, 255});
  DrawText("E RETURN", 186, VH - 23, 10, green);
  if (charge < 0.27f && ((int)(globalT * 4)) % 2) DrawTextC("LOW BATTERY", 100, 10, {255, 80, 80, 255});
}

void DrawScene() {
  switch (state) {
    case ST_INTRO:
      ClearBackground(BLACK);
      DrawHUD();
      DrawTextC(TextFormat("WORLD %s", Lvl().name), 80, 10, WHITE);
      DrawFrame(S_PSMALL, POSE_IDLE, 104, 100);
      DrawText(TextFormat("x  %d", lives), 128, 104, 10, WHITE);
      break;
    case ST_GAMEOVER:
      ClearBackground(BLACK);
      DrawHUD();
      DrawTextC("GAME OVER", 104, 10, WHITE);
      break;
    default:
      DrawWorld();
      if (droneMode) DrawFpvOverlay();
      DrawHUD();
      break;
  }
  if (toastT > 0 && (state == ST_PLAYING || state == ST_DYING)) DrawTextC(toast.c_str(), 46, 10, C_YEL);

  if (state == ST_TITLE) {
    DrawRectangle(8, 38, 240, 142, {0, 0, 0, 180});
    DrawRectangleLines(8, 38, 240, 142, C_TAN);
    DrawTextC("SUPER MARIO", 46, 20, C_TAN);
    DrawTextC("WORLD 1  -  RAYLIB REMAKE", 70, 10, WHITE);
    DrawTextC("ARROWS MOVE   Z/SPACE JUMP   X RUN", 86, 10, LIGHTGRAY);
    DrawTextC("F ATTACK   1 KNIFE   2 AK   R RELOAD", 98, 10, LIGHTGRAY);
    DrawTextC("G GRENADE   Q AIRSTRIKE   E FPV DRONE", 110, 10, LIGHTGRAY);
    DrawTextC("H HELICOPTER   P PAUSE   M MUTE", 122, 10, LIGHTGRAY);
    DrawTextC(titleMode == 0 ? "MODE:  > 2D CAMPAIGN <   3D ARENA" : "MODE:    2D CAMPAIGN   > 3D ARENA <", 138, 10,
              C_TAN);
    if (titleMode == 0) DrawTextC(TextFormat("<  START AT %s  >", kLevels[titleLevel].name), 150, 10, WHITE);
    else DrawTextC("THIRD PERSON - MOUSE + KEYBOARD", 150, 10, WHITE);
    if (((int)(globalT * 2)) % 2 == 0) DrawTextC("PRESS ENTER   (UP/DOWN: MODE)", 164, 10, C_YEL);
  } else if (state == ST_GOAL && goalPhase >= GP_TALLY && goalPhase <= GP_DONE && levelIdx + 1 < LEVEL_COUNT) {
    DrawTextC("COURSE CLEAR!", 80, 20, C_YEL);
  } else if (state == ST_CLEAR) {
    DrawRectangle(24, 52, 208, 96, {0, 0, 0, 190});
    DrawTextC("THANK YOU MARIO!", 60, 20, C_YEL);
    DrawTextC("YOUR QUEST IS OVER.", 86, 10, WHITE);
    DrawTextC(TextFormat("FINAL SCORE %06d", score), 104, 10, WHITE);
    DrawTextC("PRESS ENTER", 126, 10, LIGHTGRAY);
  } else if (paused) {
    DrawTextC("PAUSED", 100, 20, WHITE);
  }
}

void LoadWaveShader() {
  waveShader = LoadShaderFromMemory(nullptr, kWaveFs);
  waveShaderOk = IsShaderValid(waveShader) && waveShader.id != rlGetShaderIdDefault();
  if (!waveShaderOk) {
    TraceLog(LOG_WARNING, "shockwave shader unavailable - effect disabled");
    return;
  }
  locWaves = GetShaderLocation(waveShader, "waves");
  locWaveCount = GetShaderLocation(waveShader, "waveCount");
  locAspect = GetShaderLocation(waveShader, "aspect");
  float aspect = (float)VW / VH;
  SetShaderValue(waveShader, locAspect, &aspect, SHADER_UNIFORM_FLOAT);
}

// Push active shockwaves to the shader; returns false when there is nothing to distort.
bool UploadShockwaves() {
  if (!waveShaderOk || shockwaves.empty()) return false;
  float data[16] = {0};
  int n = std::min((int)shockwaves.size(), 4);
  for (int i = 0; i < n; i++) {
    const Shockwave& w = shockwaves[i];
    float k = w.t / w.dur;
    float sx = w.x - floorf(camX), sy = w.y;
    data[i * 4 + 0] = sx / VW;
    data[i * 4 + 1] = 1.0f - sy / VH;
    data[i * 4 + 2] = (w.maxR * (1 - (1 - k) * (1 - k))) / VH;
    data[i * 4 + 3] = (w.strength * (1 - k) * (1 - k)) / VH;
  }
  SetShaderValueV(waveShader, locWaves, data, SHADER_UNIFORM_VEC4, 4);
  SetShaderValue(waveShader, locWaveCount, &n, SHADER_UNIFORM_INT);
  return true;
}

Sfx MapSfx3(Sfx3 s) {
  switch (s) {
    case Sfx3::Shot: return SFX_SHOT;
    case Sfx3::Pistol: return SFX_EPOP;
    case Sfx3::Shotgun: return SFX_SHOTGUN;
    case Sfx3::Sniper: return SFX_SNIPER;
    case Sfx3::Boom: return SFX_BOOM;
    case Sfx3::Slice: return SFX_SLICE;
    case Sfx3::Splat: return SFX_SPLAT;
    case Sfx3::Groan: return SFX_GROAN;
    case Sfx3::Clank: return SFX_CLANK;
    case Sfx3::Reload: return SFX_RELOAD;
    case Sfx3::Empty: return SFX_EMPTY;
    case Sfx3::Switch: return SFX_SWITCH;
    case Sfx3::Pickup: return SFX_PICKUP;
    case Sfx3::Heli: return SFX_HELI;
    case Sfx3::Rocket: return SFX_ROCKET;
    case Sfx3::Cannon: return SFX_CANNON;
    case Sfx3::Hurt: return SFX_HURT;
    case Sfx3::Swish: return SFX_SWISH;
    case Sfx3::Drone: return SFX_DRONE;
    case Sfx3::Jet: return SFX_JET;
    case Sfx3::Whistle: return SFX_WHISTLE;
    case Sfx3::Radio: return SFX_RADIO;
    case Sfx3::Throw: return SFX_THROW;
    case Sfx3::Ricochet: return SFX_RICOCHET;
    case Sfx3::Squeal: return SFX_SQUEAL;
    case Sfx3::Coin: return SFX_COIN;
  }
  return SFX_BUMP;
}

}  // namespace

void PlaySfx3(Sfx3 s) { Play(MapSfx3(s)); }
void StopSfx3(Sfx3 s) {
  if (audioOk) StopSound(sfx[MapSfx3(s)]);
}
bool SfxPlaying3(Sfx3 s) { return audioOk && IsSoundPlaying(sfx[MapSfx3(s)]); }
void ToggleMuteShared() { muted = !muted; }

int main(int argc, char** argv) {
  bool dump = false;
  for (int i = 1; i < argc; i++)
    if (strcmp(argv[i], "--dump-assets") == 0) dump = true;

  SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
  InitWindow(VW * SCALE, VH * SCALE, "Super Mario - World 1 (raylib)");
  SetTargetFPS(60);
  SetExitKey(KEY_ESCAPE);
  InitAudioDevice();
  audioOk = IsAudioDeviceReady();
  if (audioOk) BuildSounds();

  ResolveAssetDir();
  LoadSheets(dump);
  LoadWaveShader();
  RenderTexture2D target = LoadRenderTexture(VW, VH);

  ResetWorld(0, false);
  state = ST_TITLE;

  float acc = 0;
  while (!WindowShouldClose()) {
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) jumpLatch = true;
    if (IsKeyPressed(KEY_ENTER)) enterLatch = true;
    if (IsKeyPressed(KEY_F)) attackLatch = true;
    if (IsKeyPressed(KEY_R)) reloadLatch = true;
    if (IsKeyPressed(KEY_G)) grenadeLatch = true;
    if (IsKeyPressed(KEY_Q)) airstrikeLatch = true;
    if (IsKeyPressed(KEY_E)) droneLatch = true;
    if (IsKeyPressed(KEY_H)) heliLatch = true;
    if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_KP_1)) switchLatch = 1;
    if (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_KP_2)) switchLatch = 2;
    if (IsKeyPressed(KEY_M)) muted = !muted;
    if (IsKeyPressed(KEY_P) && state == ST_PLAYING) {
      paused = !paused;
      if (paused && audioOk) {
        StopSound(sfx[SFX_DRONE]);
        StopSound(sfx[SFX_HELI]);
      }
    }

    acc = std::min(acc + GetFrameTime(), 0.1f);
    while (acc >= DT) {
      acc -= DT;
      if (!paused) Step();
    }
    if (runArena) {  // the 3D arena runs its own loop in this window, then we come back to the title
      runArena = false;
      RunArena3D();
      if (WindowShouldClose()) break;
      ResetWorld(titleLevel, false);
      state = ST_TITLE;
      acc = 0;
      continue;
    }

    BeginTextureMode(target);
    DrawScene();
    EndTextureMode();

    BeginDrawing();
    ClearBackground(BLACK);
    float s = std::min(GetScreenWidth() / (float)VW, GetScreenHeight() / (float)VH);
    if (s >= 1.0f) s = floorf(s);  // integer scaling keeps pixels crisp
    Vector2 shake{0, 0};
    if (shakeT > 0 && !paused) shake = {Randf(-shakeAmp, shakeAmp) * s, Randf(-shakeAmp, shakeAmp) * s};
    Rectangle dst{(GetScreenWidth() - VW * s) / 2 + shake.x, (GetScreenHeight() - VH * s) / 2 + shake.y, VW * s, VH * s};
    bool waves = UploadShockwaves();
    if (waves) BeginShaderMode(waveShader);
    DrawTexturePro(target.texture, {0, 0, (float)VW, -(float)VH}, dst, {0, 0}, 0, WHITE);
    if (waves) EndShaderMode();
    EndDrawing();
  }

  UnloadRenderTexture(target);
  if (waveShaderOk) UnloadShader(waveShader);
  for (auto& sh : sheets) UnloadTexture(sh.tex);
  if (audioOk) {
    for (auto& snd : sfx) UnloadSound(snd);
    CloseAudioDevice();
  }
  CloseWindow();
  return 0;
}

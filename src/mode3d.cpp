// Third-person 3D arena: open map, wave survival, looting with limited ammo, vehicles and support weapons.
// Everything is drawn from shaded primitive boxes, so no 3D model files are needed.

#include "mode3d.h"

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
constexpr float ARENA = 80.0f;  // half extent of the square map
constexpr int HM = 129;         // heightmap vertices per side
constexpr float HMAX = 7.0f;
constexpr float GRAV = 25.0f;
constexpr float PLAYER_R = 0.4f;
constexpr float PLAYER_H = 1.7f;
constexpr float MAX_HP = 100;
constexpr float PHELI_HP = 300;
constexpr float DRONE_BATTERY = 25;
constexpr float WAVE_BREAK = 10;

const Color SKY{120, 175, 235, 255};
const Color C_RED{216, 40, 0, 255};
const Color C_SKIN{252, 188, 116, 255};
const Color C_HAIR{110, 60, 10, 255};
const Color C_BLUE{32, 56, 236, 255};
const Color C_ZSKIN{120, 170, 90, 255};
const Color C_ZSHIRT{90, 60, 120, 255};
const Color C_ZPANTS{60, 60, 90, 255};
const Color C_GBROWN{172, 80, 36, 255};
const Color C_TAN{240, 208, 176, 255};
const Color C_OLIVE{86, 104, 46, 255};
const Color C_DKOLIVE{52, 64, 28, 255};
const Color C_GUN{55, 55, 60, 255};
const Color C_WOOD{150, 96, 40, 255};
const Color C_BLOOD{150, 0, 0, 255};
const Color C_FIRE{255, 170, 40, 255};
const Color C_HUD{0, 255, 120, 255};

float Rf(float a, float b) { return a + (b - a) * (GetRandomValue(0, 10000) / 10000.0f); }
Vector3 Fwd(float yaw) { return {sinf(yaw), 0, cosf(yaw)}; }
Vector3 RightOf(float yaw) { return {-cosf(yaw), 0, sinf(yaw)}; }
Vector3 Dir(float yaw, float pitch) { return {sinf(yaw) * cosf(pitch), sinf(pitch), cosf(yaw) * cosf(pitch)}; }
float AngleDiff(float a, float b) {
  float d = fmodf(b - a + PI, 2 * PI);
  if (d < 0) d += 2 * PI;
  return d - PI;
}
float TurnTo(float a, float b, float step) {
  float d = AngleDiff(a, b);
  return fabsf(d) <= step ? b : a + (d > 0 ? step : -step);
}
Color Shade(Color c, float k) {
  return {(unsigned char)std::clamp(c.r * k, 0.f, 255.f), (unsigned char)std::clamp(c.g * k, 0.f, 255.f),
          (unsigned char)std::clamp(c.b * k, 0.f, 255.f), c.a};
}
Color Alpha(Color c, float a) { return {c.r, c.g, c.b, (unsigned char)std::clamp(a * 255.f, 0.f, 255.f)}; }

// Box with baked per-face shading, drawn in the current rlgl matrix (so it can be rotated/animated).
void Box(float cx, float cy, float cz, float sx, float sy, float sz, Color c) {
  float x0 = cx - sx / 2, x1 = cx + sx / 2, y0 = cy - sy / 2, y1 = cy + sy / 2, z0 = cz - sz / 2, z1 = cz + sz / 2;
  rlBegin(RL_QUADS);
  Color k = Shade(c, 1.0f);
  rlColor4ub(k.r, k.g, k.b, k.a);
  rlVertex3f(x0, y1, z0); rlVertex3f(x0, y1, z1); rlVertex3f(x1, y1, z1); rlVertex3f(x1, y1, z0);
  k = Shade(c, 0.45f);
  rlColor4ub(k.r, k.g, k.b, k.a);
  rlVertex3f(x0, y0, z0); rlVertex3f(x1, y0, z0); rlVertex3f(x1, y0, z1); rlVertex3f(x0, y0, z1);
  k = Shade(c, 0.85f);
  rlColor4ub(k.r, k.g, k.b, k.a);
  rlVertex3f(x0, y0, z1); rlVertex3f(x1, y0, z1); rlVertex3f(x1, y1, z1); rlVertex3f(x0, y1, z1);
  k = Shade(c, 0.62f);
  rlColor4ub(k.r, k.g, k.b, k.a);
  rlVertex3f(x0, y0, z0); rlVertex3f(x0, y1, z0); rlVertex3f(x1, y1, z0); rlVertex3f(x1, y0, z0);
  k = Shade(c, 0.75f);
  rlColor4ub(k.r, k.g, k.b, k.a);
  rlVertex3f(x1, y0, z0); rlVertex3f(x1, y1, z0); rlVertex3f(x1, y1, z1); rlVertex3f(x1, y0, z1);
  k = Shade(c, 0.55f);
  rlColor4ub(k.r, k.g, k.b, k.a);
  rlVertex3f(x0, y0, z0); rlVertex3f(x0, y0, z1); rlVertex3f(x0, y1, z1); rlVertex3f(x0, y1, z0);
  rlEnd();
}
void BoxV(Vector3 c, Vector3 s, Color col) { Box(c.x, c.y, c.z, s.x, s.y, s.z, col); }

// ---------------------------------------------------------------------------
// Terrain
// ---------------------------------------------------------------------------
std::vector<float> hgt;
Model terrain{};
Texture2D terrainTex{};

float HAt(int i, int j) {
  i = std::clamp(i, 0, HM - 1);
  j = std::clamp(j, 0, HM - 1);
  return hgt[j * HM + i];
}

float TerrainH(float x, float z) {
  float u = std::clamp((x + ARENA) / (2 * ARENA) * (HM - 1), 0.f, HM - 1.001f);
  float v = std::clamp((z + ARENA) / (2 * ARENA) * (HM - 1), 0.f, HM - 1.001f);
  int i = (int)u, j = (int)v;
  float fu = u - i, fv = v - j;
  float a = HAt(i, j) + (HAt(i + 1, j) - HAt(i, j)) * fu;
  float b = HAt(i, j + 1) + (HAt(i + 1, j + 1) - HAt(i, j + 1)) * fu;
  return a + (b - a) * fv;
}

void BuildTerrain() {
  Image noise = GenImagePerlinNoise(HM, HM, GetRandomValue(0, 5000), GetRandomValue(0, 5000), 2.5f);
  Color* px = LoadImageColors(noise);
  Image hm = GenImageColor(HM, HM, BLACK);
  hgt.assign(HM * HM, 0);
  for (int j = 0; j < HM; j++) {
    for (int i = 0; i < HM; i++) {
      float x = -ARENA + i * 2 * ARENA / (HM - 1), z = -ARENA + j * 2 * ARENA / (HM - 1);
      float n = px[j * HM + i].r / 255.f;
      float h = n * n * 1.1f;
      float d = sqrtf(x * x + z * z);
      h *= std::clamp((d - 12) / 14, 0.f, 1.f);  // flat middle for the helipad
      float edge = std::max(fabsf(x), fabsf(z));
      if (edge > 64) h += (edge - 64) / 16 * 1.1f;  // hills ring the map
      h = std::clamp(h, 0.f, 1.f);
      unsigned char g = (unsigned char)(h * 255);
      ImageDrawPixel(&hm, i, j, {g, g, g, 255});
      hgt[j * HM + i] = g / 255.f * HMAX;
    }
  }
  UnloadImageColors(px);
  UnloadImage(noise);
  terrain = LoadModelFromMesh(GenMeshHeightmap(hm, {2 * ARENA, HMAX, 2 * ARENA}));
  UnloadImage(hm);

  // baked lighting + grass/rock texture
  const int TS = 512;
  Image tex = GenImageColor(TS, TS, BLACK);
  Color* data = (Color*)tex.data;
  Vector3 light = Vector3Normalize({0.5f, 1.0f, 0.3f});
  for (int ty = 0; ty < TS; ty++) {
    for (int tx = 0; tx < TS; tx++) {
      float x = -ARENA + (tx + 0.5f) / TS * 2 * ARENA, z = -ARENA + (ty + 0.5f) / TS * 2 * ARENA;
      float h = TerrainH(x, z);
      float hx = TerrainH(x + 0.6f, z) - TerrainH(x - 0.6f, z);
      float hz = TerrainH(x, z + 0.6f) - TerrainH(x, z - 0.6f);
      Vector3 nrm = Vector3Normalize({-hx, 1.2f, -hz});
      float k = 0.55f + 0.5f * std::max(0.f, Vector3DotProduct(nrm, light));
      float grain = 0.9f + (GetRandomValue(0, 100) / 100.f) * 0.18f;
      Color base = ((tx / 6 + ty / 6) % 7 == 0) ? Color{78, 130, 52, 255} : Color{92, 148, 62, 255};
      if (h > 4.6f) base = Color{128, 122, 110, 255};
      else if (nrm.y < 0.8f) base = Color{110, 112, 80, 255};
      data[ty * TS + tx] = Shade(base, k * grain);
    }
  }
  terrainTex = LoadTextureFromImage(tex);
  UnloadImage(tex);
  GenTextureMipmaps(&terrainTex);
  SetTextureFilter(terrainTex, TEXTURE_FILTER_TRILINEAR);
  terrain.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = terrainTex;
}

// ---------------------------------------------------------------------------
// World: buildings, cover, trees, crates
// ---------------------------------------------------------------------------
enum SolidKind { SK_BUILDING, SK_STEP, SK_WALL, SK_TRUNK };
struct Solid3 {
  BoundingBox bb;
  Color col;
  SolidKind kind;
};
struct Tree3 {
  Vector3 pos;
  float s;
};
struct Crate3 {
  Vector3 pos;
  bool alive = true;
  float dropY = 0;  // > 0 while parachuting in
};

std::vector<Solid3> solids;
std::vector<Tree3> trees;
std::vector<Crate3> crates;
Vector3 helipad{0, 0, 10};

bool Overlaps2D(const BoundingBox& a, const BoundingBox& b, float pad) {
  return a.min.x - pad < b.max.x && a.max.x + pad > b.min.x && a.min.z - pad < b.max.z && a.max.z + pad > b.min.z;
}

bool PointFree(float x, float z, float pad) {
  for (const auto& s : solids)
    if (x > s.bb.min.x - pad && x < s.bb.max.x + pad && z > s.bb.min.z - pad && z < s.bb.max.z + pad) return false;
  return fabsf(x) < ARENA - 6 && fabsf(z) < ARENA - 6 && Vector2Distance({x, z}, {helipad.x, helipad.z}) > 7;
}

// Highest walkable surface under (x,z) that is not above `fromY` + step height.
float GroundAt(float x, float z, float fromY = 1e9f) {
  float g = TerrainH(x, z);
  for (const auto& s : solids)
    if (x >= s.bb.min.x && x <= s.bb.max.x && z >= s.bb.min.z && z <= s.bb.max.z && s.bb.max.y <= fromY + 0.5f)
      g = std::max(g, s.bb.max.y);
  return g;
}

void AddSolid(Vector3 mn, Vector3 mx, Color c, SolidKind k) { solids.push_back({{mn, mx}, c, k}); }

void BuildWorld() {
  solids.clear();
  trees.clear();
  crates.clear();
  helipad.y = TerrainH(helipad.x, helipad.z);
  const Color palette[4] = {{200, 186, 150, 255}, {150, 150, 158, 255}, {170, 96, 70, 255}, {120, 130, 110, 255}};
  int placed = 0;
  for (int tries = 0; tries < 300 && placed < 13; tries++) {
    float x = Rf(-62, 62), z = Rf(-62, 62);
    if (sqrtf(x * x + z * z) < 24) continue;
    float w = Rf(7, 12), d = Rf(7, 12);
    bool stairs = GetRandomValue(0, 1) == 0;
    float h = stairs ? 4.5f : Rf(4.5f, 8.5f);
    BoundingBox cand{{x - w / 2, 0, z - d / 2}, {x + w / 2, 0, z + d / 2}};
    bool ok = true;
    for (const auto& s : solids)
      if (Overlaps2D(cand, s.bb, 7)) ok = false;
    if (!ok) continue;
    float lo = 1e9f, hi = -1e9f;
    for (float sx : {cand.min.x, x, cand.max.x})
      for (float sz : {cand.min.z, z, cand.max.z}) {
        lo = std::min(lo, TerrainH(sx, sz));
        hi = std::max(hi, TerrainH(sx, sz));
      }
    AddSolid({cand.min.x, lo - 0.5f, cand.min.z}, {cand.max.x, hi + h, cand.max.z}, palette[GetRandomValue(0, 3)],
             SK_BUILDING);
    if (stairs) {  // stepped blocks up the +x side lead onto the roof
      float top = hi + h;
      for (int s = 1; s <= 3; s++) {
        float sy = top - (4 - s) * 1.1f;
        float sz0 = cand.min.z + (s - 1) * 2.0f;
        AddSolid({cand.max.x, lo - 0.5f, sz0}, {cand.max.x + 2.2f, sy, sz0 + 2.0f}, {160, 160, 160, 255}, SK_STEP);
      }
    }
    placed++;
  }
  for (int i = 0, tries = 0; i < 12 && tries < 200; tries++) {  // low walls for cover
    float x = Rf(-65, 65), z = Rf(-65, 65);
    if (!PointFree(x, z, 3)) continue;
    bool alongX = GetRandomValue(0, 1);
    float hw = alongX ? 3.0f : 0.3f, hd = alongX ? 0.3f : 3.0f;
    float g = TerrainH(x, z);
    AddSolid({x - hw, g - 0.6f, z - hd}, {x + hw, g + 1.25f, z + hd}, {140, 136, 128, 255}, SK_WALL);
    i++;
  }
  for (int i = 0, tries = 0; i < 45 && tries < 400; tries++) {
    float x = Rf(-74, 74), z = Rf(-74, 74);
    if (!PointFree(x, z, 2)) continue;
    float g = TerrainH(x, z), s = Rf(0.8f, 1.3f);
    trees.push_back({{x, g, z}, s});
    AddSolid({x - 0.3f, g - 0.2f, z - 0.3f}, {x + 0.3f, g + 2.2f * s, z + 0.3f}, C_WOOD, SK_TRUNK);
    i++;
  }
  for (int i = 0, tries = 0; i < 16 && tries < 400; tries++) {
    float x = Rf(-70, 70), z = Rf(-70, 70);
    if (!PointFree(x, z, 1.5f)) continue;
    crates.push_back({{x, TerrainH(x, z), z}});
    i++;
  }
  for (const auto& s : solids)  // a few crates up on the stair-access roofs
    if (s.kind == SK_BUILDING && s.bb.max.y - s.bb.min.y < 6 && GetRandomValue(0, 1))
      crates.push_back({{(s.bb.min.x + s.bb.max.x) / 2, s.bb.max.y, (s.bb.min.z + s.bb.max.z) / 2}});
}

// Pushes a standing cylinder (feet at p) out of solids; returns the walkable ground height under it.
float CollideSolids(Vector3& p, float r, float h, float stepUp) {
  float ground = TerrainH(p.x, p.z);
  for (const auto& s : solids) {
    const BoundingBox& b = s.bb;
    if (p.y + h <= b.min.y) continue;
    float cx = std::clamp(p.x, b.min.x, b.max.x), cz = std::clamp(p.z, b.min.z, b.max.z);
    float dx = p.x - cx, dz = p.z - cz, d2 = dx * dx + dz * dz;
    if (d2 >= r * r) continue;
    if (p.y >= b.max.y - stepUp) {
      ground = std::max(ground, b.max.y);
      continue;
    }
    if (d2 > 1e-6f) {
      float d = sqrtf(d2);
      p.x = cx + dx / d * r;
      p.z = cz + dz / d * r;
    } else {
      float l = p.x - b.min.x, rr = b.max.x - p.x, n = p.z - b.min.z, f = b.max.z - p.z;
      float m = std::min({l, rr, n, f});
      if (m == l) p.x = b.min.x - r;
      else if (m == rr) p.x = b.max.x + r;
      else if (m == n) p.z = b.min.z - r;
      else p.z = b.max.z + r;
    }
  }
  p.x = std::clamp(p.x, -ARENA + 1, ARENA - 1);
  p.z = std::clamp(p.z, -ARENA + 1, ARENA - 1);
  return ground;
}

bool InsideSolid(Vector3 p) {
  for (const auto& s : solids)
    if (p.x > s.bb.min.x && p.x < s.bb.max.x && p.y > s.bb.min.y && p.y < s.bb.max.y && p.z > s.bb.min.z &&
        p.z < s.bb.max.z)
      return true;
  return false;
}

// ---------------------------------------------------------------------------
// Weapons, ammo, loot
// ---------------------------------------------------------------------------
enum WeaponId { W_KNIFE, W_PISTOL, W_AK, W_SHOTGUN, W_SNIPER, W_COUNT };
enum AmmoId { A_9MM, A_762, A_12G, A_308, A_ROCKET, A_COUNT, A_NONE = A_COUNT };

struct WeaponDef {
  const char* name;
  AmmoId ammo;
  int mag;
  float rate, dmg, spread;
  int pellets;
  float reload;
  bool automatic;
  float range, recoil;
  Sfx3 sound;
};

const WeaponDef WD[W_COUNT] = {
    {"KNIFE", A_NONE, 0, 0.45f, 100, 0, 0, 0, false, 2.6f, 0, Sfx3::Swish},
    {"PISTOL", A_9MM, 12, 0.2f, 24, 0.012f, 1, 1.1f, false, 90, 0.015f, Sfx3::Pistol},
    {"AK-47", A_762, 30, 0.1f, 30, 0.022f, 1, 1.9f, true, 130, 0.012f, Sfx3::Shot},
    {"SHOTGUN", A_12G, 6, 0.85f, 14, 0.075f, 9, 2.3f, false, 40, 0.05f, Sfx3::Shotgun},
    {"SNIPER", A_308, 5, 1.3f, 160, 0.05f, 1, 2.7f, false, 320, 0.06f, Sfx3::Sniper},
};
const char* AMMO_NAME[A_COUNT] = {"9MM", "7.62", "12G", ".308", "ROCKETS"};
const Color AMMO_COL[A_COUNT] = {{220, 200, 80, 255}, {230, 140, 40, 255}, {220, 60, 50, 255}, {80, 170, 230, 255},
                                 {200, 200, 200, 255}};
const int AMMO_BOX[A_COUNT] = {24, 30, 12, 10, 4};
const int AMMO_CAP[A_COUNT] = {120, 210, 48, 40, 24};

enum LootKind { LK_WEAPON, LK_AMMO, LK_HEALTH, LK_GRENADE, LK_FLARE, LK_DRONE };
struct Loot3 {
  LootKind kind;
  int sub, amount;
  Vector3 pos, vel;
  float t = 0;
  bool alive = true;
};

std::string LootName(const Loot3& l) {
  switch (l.kind) {
    case LK_WEAPON: return WD[l.sub].name;
    case LK_AMMO: return TextFormat("%s x%d", AMMO_NAME[l.sub], l.amount);
    case LK_HEALTH: return TextFormat("MEDKIT +%d", l.amount);
    case LK_GRENADE: return TextFormat("GRENADE x%d", l.amount);
    case LK_FLARE: return "AIRSTRIKE FLARE";
    case LK_DRONE: return "FPV DRONE";
  }
  return "";
}

Color LootColor(const Loot3& l) {
  switch (l.kind) {
    case LK_WEAPON: return {60, 60, 66, 255};
    case LK_AMMO: return AMMO_COL[l.sub];
    case LK_HEALTH: return {240, 240, 240, 255};
    case LK_GRENADE: return C_OLIVE;
    case LK_FLARE: return {230, 40, 40, 255};
    case LK_DRONE: return {40, 40, 44, 255};
  }
  return WHITE;
}

// ---------------------------------------------------------------------------
// Entities
// ---------------------------------------------------------------------------
enum EKind { EK_ZOMBIE, EK_GOOMBA, EK_TANK, EK_HELI };
enum Cause { C_BULLET, C_KNIFE, C_BLAST };

struct Enemy3 {
  EKind kind;
  Vector3 pos{}, vel{};
  float yaw = 0, hp = 0, maxHp = 0;
  bool alive = true, onGround = false, wreck = false, crashing = false;
  float atkT = 1, animT = 0, flashT = 0, turret = 0, strafe = 1, stateT = 0, spin = 0, burstT = 0, rocketT = 4;
  int burst = 0;
};

struct Player3 {
  Vector3 pos{}, vel{};
  float yaw = 0, pitch = -0.15f, hp = MAX_HP, animT = 0;
  bool onGround = false;
  int weapon = W_PISTOL;
  bool owned[W_COUNT] = {true, true, false, false, false};
  int mag[W_COUNT] = {0, 12, 0, 0, 0};
  int reserve[A_COUNT] = {24, 0, 0, 0, 0};
  int grenades = 1, flares = 0, drones = 0;
  float fireCD = 0, reloadT = 0, slashT = 0, flashT = 0, hurtT = 0, recoilKick = 0, meleeCD = 0;
};

struct PlayerHeli {
  Vector3 pos{};  // center of the fuselage; skids are 1 m below
  Vector3 vel{};
  float yaw = 0, hp = PHELI_HP, rotor = 0, heat = 0, fireCD = 0, rocketCD = 0, tiltF = 0, tiltS = 0, flashT = 0;
  bool alive = true, overheated = false;
};

struct Drone3 {
  Vector3 pos{}, vel{};
  float yaw = 0, pitch = 0, battery = 0;
  int payload = 0;
};

struct Jet3 {
  bool active = false;
  Vector3 pos{}, dir{}, target{};
  int dropped = 0;
  float markerT = 0;
};

enum PKind { PK_GRENADE, PK_SHELL, PK_ROCKET, PK_EROCKET, PK_BOMB, PK_DBOMB };
struct Proj3 {
  PKind kind;
  Vector3 pos, vel;
  float t = 0;
  bool alive = true;
};

enum PtKind { PT_BLOOD, PT_SPARK, PT_FIRE, PT_SMOKE, PT_DEBRIS, PT_CASING };
struct Part3 {
  PtKind kind;
  Vector3 pos, vel;
  float life, maxLife, size;
  Color col;
};

struct Decal {
  Vector3 pos;
  float r, life;
  Color col;
};

struct Gib {
  EKind kind;
  int part;  // 1 = upper body, 2 = legs
  Vector3 pos, vel;
  float yaw, ang, angVel, t = 0, gush = 0;
  bool landed = false;
};

struct Tracer {
  Vector3 a, b;
  float t;
  Color col;
};

struct Blast {
  Vector3 pos;
  float t, r;
};

struct Wave3 {
  Vector3 pos;
  float t, dur, maxR, strength;
};

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
Player3 pl;
PlayerHeli ph;
bool inHeli = false;
Drone3 drone;
bool droneMode = false;
Jet3 jet;
std::vector<Enemy3> enemies;
std::vector<Loot3> loots;
std::vector<Proj3> projs;
std::vector<Part3> parts;
std::vector<Decal> decals;
std::vector<Gib> gibs;
std::vector<Tracer> tracers;
std::vector<Blast> blasts;
std::vector<Wave3> waves;
Camera3D cam{};
float camShake = 0, hitMarkT = 0, killMarkT = 0, toastT = 0, bannerT = 0;
std::string toast, banner;
int score = 0, wave = 0, toSpawn[4] = {0, 0, 0, 0};
float spawnT = 0, breakT = 0, globalT = 0;
bool scoped = false, aiming = false, paused = false, dead = false, quit = false;
float deadT = 0;
RenderTexture2D rt{};
Shader waveShader{};
bool waveOk = false;
int locWaves = -1, locCount = -1, locAspect = -1;

void Toast(const std::string& s) {
  toast = s;
  toastT = 2.0f;
}

void AddPart(PtKind k, Vector3 p, Vector3 v, float life, float size, Color c) {
  if (parts.size() > 2500) return;
  parts.push_back({k, p, v, life, life, size, c});
}

void Blood(Vector3 p, Vector3 dir, int n, float power = 1) {
  for (int i = 0; i < n; i++) {
    Vector3 v{dir.x * Rf(1, 6) * power + Rf(-2.5f, 2.5f), Rf(1, 6) * power, dir.z * Rf(1, 6) * power + Rf(-2.5f, 2.5f)};
    static const Color reds[3] = {{150, 0, 0, 255}, {190, 12, 10, 255}, {110, 0, 6, 255}};
    AddPart(PT_BLOOD, p, v, Rf(1.2f, 2.5f), Rf(0.05f, 0.12f), reds[GetRandomValue(0, 2)]);
  }
}

void Sparks(Vector3 p, int n) {
  for (int i = 0; i < n; i++)
    AddPart(PT_SPARK, p, {Rf(-5, 5), Rf(0, 5), Rf(-5, 5)}, Rf(0.1f, 0.3f), 0.04f, {255, 230, 120, 255});
}

void AddDecal(Vector3 p, float r, Color c, float life = 30) {
  if (decals.size() > 500) decals.erase(decals.begin());
  decals.push_back({p, r, life, c});
}

void AddShockwave(Vector3 p, float maxR, float strength, float dur) {
  if (waves.size() >= 4) waves.erase(waves.begin());
  waves.push_back({p, 0, dur, maxR, strength});
}

void Shake(float a) { camShake = std::max(camShake, a); }

void AddScore(int s) { score += s; }

Vector3 PlayerCenter() {
  if (inHeli) return ph.pos;
  return {pl.pos.x, pl.pos.y + 1.0f, pl.pos.z};
}

void HurtPlayer(float dmg) {
  if (dead) return;
  if (inHeli) {
    ph.hp -= dmg;
    ph.flashT = 0.12f;
    PlaySfx3(Sfx3::Clank);
    Shake(0.3f);
    return;
  }
  pl.hp -= dmg;
  pl.hurtT = 0.6f;
  Shake(0.25f);
  PlaySfx3(Sfx3::Hurt);
  if (pl.hp <= 0) {
    pl.hp = 0;
    dead = true;
    deadT = 0;
    Blood({pl.pos.x, pl.pos.y + 1, pl.pos.z}, {0, 0, 0}, 40);
    PlaySfx3(Sfx3::Squeal);
  }
}

// ---------------------------------------------------------------------------
// Ray casting
// ---------------------------------------------------------------------------
enum HitType { HT_NONE, HT_TERRAIN, HT_SOLID, HT_ENEMY, HT_CRATE };
struct Hit3 {
  bool hit = false;
  float dist = 1e9f;
  Vector3 point{};
  HitType type = HT_NONE;
  int index = -1;
};

BoundingBox EnemyBox(const Enemy3& e) {
  switch (e.kind) {
    case EK_ZOMBIE: return {{e.pos.x - 0.4f, e.pos.y, e.pos.z - 0.4f}, {e.pos.x + 0.4f, e.pos.y + 1.85f, e.pos.z + 0.4f}};
    case EK_GOOMBA: return {{e.pos.x - 0.55f, e.pos.y, e.pos.z - 0.55f}, {e.pos.x + 0.55f, e.pos.y + 1.15f, e.pos.z + 0.55f}};
    case EK_TANK: return {{e.pos.x - 1.7f, e.pos.y, e.pos.z - 1.7f}, {e.pos.x + 1.7f, e.pos.y + 2.0f, e.pos.z + 1.7f}};
    case EK_HELI: return {{e.pos.x - 2.2f, e.pos.y - 0.9f, e.pos.z - 2.2f}, {e.pos.x + 2.2f, e.pos.y + 1.0f, e.pos.z + 2.2f}};
  }
  return {};
}

BoundingBox CrateBox(const Crate3& c) {
  return {{c.pos.x - 0.5f, c.pos.y + c.dropY, c.pos.z - 0.5f}, {c.pos.x + 0.5f, c.pos.y + c.dropY + 1, c.pos.z + 0.5f}};
}

Hit3 CastRay(Ray ray, float maxDist, bool withEnemies) {
  Hit3 best;
  auto consider = [&](RayCollision rc, HitType t, int idx) {
    if (rc.hit && rc.distance > 0 && rc.distance < best.dist && rc.distance <= maxDist) {
      best.hit = true;
      best.dist = rc.distance;
      best.point = rc.point;
      best.type = t;
      best.index = idx;
    }
  };
  for (size_t i = 0; i < solids.size(); i++) consider(GetRayCollisionBox(ray, solids[i].bb), HT_SOLID, (int)i);
  for (size_t i = 0; i < crates.size(); i++)
    if (crates[i].alive) consider(GetRayCollisionBox(ray, CrateBox(crates[i])), HT_CRATE, (int)i);
  if (withEnemies)
    for (size_t i = 0; i < enemies.size(); i++)
      if (enemies[i].alive && !enemies[i].wreck) consider(GetRayCollisionBox(ray, EnemyBox(enemies[i])), HT_ENEMY, (int)i);
  // terrain: march, then refine
  float limit = std::min(maxDist, best.dist);
  for (float t = 0.25f; t < limit; t += 0.5f) {
    Vector3 q = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    if (q.y < TerrainH(q.x, q.z)) {
      float lo = t - 0.5f, hi = t;
      for (int k = 0; k < 8; k++) {
        float mid = (lo + hi) / 2;
        Vector3 m = Vector3Add(ray.position, Vector3Scale(ray.direction, mid));
        if (m.y < TerrainH(m.x, m.z)) hi = mid;
        else lo = mid;
      }
      best.hit = true;
      best.dist = hi;
      best.point = Vector3Add(ray.position, Vector3Scale(ray.direction, hi));
      best.type = HT_TERRAIN;
      best.index = -1;
      break;
    }
  }
  return best;
}

bool LineOfSight(Vector3 a, Vector3 b) {
  Vector3 d = Vector3Subtract(b, a);
  float len = Vector3Length(d);
  if (len < 0.01f) return true;
  Hit3 h = CastRay({a, Vector3Scale(d, 1 / len)}, len - 0.5f, false);
  return !h.hit;
}

// ---------------------------------------------------------------------------
// Loot
// ---------------------------------------------------------------------------
void SpawnLoot(LootKind k, int sub, int amount, Vector3 at) {
  Loot3 l{k, sub, amount, {at.x, at.y + 0.6f, at.z}, {Rf(-2.5f, 2.5f), Rf(4, 6), Rf(-2.5f, 2.5f)}};
  loots.push_back(l);
}

// Weighted roll that favours ammo for guns the player already has and weapons they lack.
void RollLoot(Vector3 at, bool rich) {
  int r = GetRandomValue(0, 99);
  bool anyMissing = !pl.owned[W_AK] || !pl.owned[W_SHOTGUN] || !pl.owned[W_SNIPER];
  if (rich && anyMissing && r < 35) {
    int w;
    do w = GetRandomValue(W_AK, W_SNIPER);
    while (pl.owned[w]);
    SpawnLoot(LK_WEAPON, w, 0, at);
  } else if (r < 55) {
    int a;
    int tries = 0;
    do a = GetRandomValue(0, A_308);
    while (++tries < 6 && a != A_9MM && !pl.owned[a == A_762 ? W_AK : a == A_12G ? W_SHOTGUN : W_SNIPER]);
    SpawnLoot(LK_AMMO, a, AMMO_BOX[a], at);
  } else if (r < 70) {
    SpawnLoot(LK_HEALTH, 0, GetRandomValue(0, 2) ? 25 : 50, at);
  } else if (r < 82) {
    SpawnLoot(LK_GRENADE, 0, GetRandomValue(1, 2), at);
  } else if (r < 89) {
    SpawnLoot(LK_FLARE, 0, 1, at);
  } else if (r < 95) {
    SpawnLoot(LK_DRONE, 0, 1, at);
  } else {
    SpawnLoot(LK_AMMO, A_ROCKET, AMMO_BOX[A_ROCKET], at);
  }
}

void OpenCrate(Crate3& c) {
  if (!c.alive) return;
  c.alive = false;
  PlaySfx3(Sfx3::Pickup);
  for (int i = 0; i < 10; i++)
    AddPart(PT_DEBRIS, {c.pos.x, c.pos.y + 0.5f, c.pos.z}, {Rf(-4, 4), Rf(2, 6), Rf(-4, 4)}, 2.0f, 0.15f, C_WOOD);
  int n = GetRandomValue(2, 3);
  for (int i = 0; i < n; i++) RollLoot(c.pos, i == 0);
}

void PickUp(Loot3& l) {
  switch (l.kind) {
    case LK_WEAPON:
      if (pl.owned[l.sub]) {  // duplicate gun becomes ammo
        AmmoId a = WD[l.sub].ammo;
        pl.reserve[a] = std::min(AMMO_CAP[a], pl.reserve[a] + WD[l.sub].mag);
      } else {
        pl.owned[l.sub] = true;
        pl.mag[l.sub] = WD[l.sub].mag;
        pl.weapon = l.sub;
        pl.reloadT = 0;
      }
      break;
    case LK_AMMO:
      if (pl.reserve[l.sub] >= AMMO_CAP[l.sub]) return;  // full: leave it
      pl.reserve[l.sub] = std::min(AMMO_CAP[l.sub], pl.reserve[l.sub] + l.amount);
      break;
    case LK_HEALTH:
      if (pl.hp >= MAX_HP) return;
      pl.hp = std::min(MAX_HP, pl.hp + l.amount);
      break;
    case LK_GRENADE: pl.grenades = std::min(9, pl.grenades + l.amount); break;
    case LK_FLARE: pl.flares = std::min(5, pl.flares + 1); break;
    case LK_DRONE: pl.drones = std::min(5, pl.drones + 1); break;
  }
  l.alive = false;
  Toast("PICKED UP " + LootName(l));
  PlaySfx3(l.kind == LK_HEALTH ? Sfx3::Coin : Sfx3::Pickup);
}

// ---------------------------------------------------------------------------
// Damage, explosions
// ---------------------------------------------------------------------------
void KillEnemy(Enemy3& e, Cause cause, Vector3 dir) {
  if (e.kind == EK_TANK) {
    e.wreck = true;
    e.stateT = 0;
    AddScore(1000);
    RollLoot(e.pos, true);
    RollLoot(e.pos, false);
    return;  // wreck explosion is triggered by caller via flag below
  }
  if (e.kind == EK_HELI) {
    e.crashing = true;
    e.vel = {dir.x * 4, 2, dir.z * 4};
    AddScore(1500);
    PlaySfx3(Sfx3::Clank);
    return;
  }
  e.alive = false;
  AddScore(e.kind == EK_ZOMBIE ? 100 : 150);
  Vector3 c{e.pos.x, e.pos.y + 1.0f, e.pos.z};
  if (cause == C_BULLET) {  // drops and topples over
    Gib g{e.kind, 0, e.pos, {dir.x * 2, 1.5f, dir.z * 2}, e.yaw, 0, 3.5f};
    gibs.push_back(g);
    Blood(c, dir, 20);
  } else {  // cut or blown in half
    float pw = cause == C_BLAST ? 2.0f : 1.0f;
    float upper = e.kind == EK_ZOMBIE ? 1.3f : 0.75f;
    gibs.push_back({e.kind, 1, {e.pos.x, e.pos.y + upper, e.pos.z}, {dir.x * 4 * pw, 5 * pw, dir.z * 4 * pw}, e.yaw, 0,
                    Rf(5, 9) * (GetRandomValue(0, 1) ? 1 : -1)});
    Gib legs{e.kind, 2, {e.pos.x, e.pos.y + 0.45f, e.pos.z}, {dir.x * pw, 1.5f * pw, dir.z * pw}, e.yaw, 0, 1.5f};
    legs.gush = 1.0f;
    gibs.push_back(legs);
    Blood(c, dir, cause == C_BLAST ? 70 : 50, 1.2f);
    PlaySfx3(cause == C_KNIFE ? Sfx3::Slice : Sfx3::Splat);
  }
  PlaySfx3(e.kind == EK_ZOMBIE ? Sfx3::Groan : Sfx3::Squeal);
  int dropChance = e.kind == EK_ZOMBIE ? 30 : 60;
  if (GetRandomValue(0, 99) < dropChance) RollLoot(e.pos, false);
}

void Explode3(Vector3 c, float radius, float dmg, bool byPlayer);

void DamageEnemy(Enemy3& e, float dmg, Cause cause, Vector3 dir, Vector3 at) {
  if (!e.alive || e.wreck || e.crashing) return;
  bool armored = e.kind == EK_TANK || e.kind == EK_HELI;
  if (armored && cause == C_BULLET) dmg *= 0.25f;  // small arms barely dent armor
  if (armored && cause == C_KNIFE) dmg = 10;
  e.hp -= dmg;
  e.flashT = 0.1f;
  if (armored) {
    Sparks(at, 6);
    PlaySfx3(Sfx3::Clank);
  } else {
    Blood(at, dir, cause == C_BLAST ? 12 : 8, 0.7f);
  }
  if (e.hp <= 0) {
    KillEnemy(e, cause, dir);
    if (e.kind == EK_TANK) Explode3({e.pos.x, e.pos.y + 1, e.pos.z}, 5, 40, false);
    killMarkT = 0.3f;
  }
}

void Explode3(Vector3 c, float radius, float dmg, bool byPlayer) {
  blasts.push_back({c, 0, radius});
  for (int i = 0; i < 45; i++) {
    Vector3 v{Rf(-1, 1), Rf(-0.3f, 1), Rf(-1, 1)};
    v = Vector3Scale(Vector3Normalize(v), Rf(2, radius * 3));
    AddPart(PT_FIRE, c, v, Rf(0.3f, 0.7f), Rf(0.25f, 0.6f), GetRandomValue(0, 1) ? C_FIRE : Color{255, 90, 20, 255});
  }
  for (int i = 0; i < 18; i++)
    AddPart(PT_SMOKE, Vector3Add(c, {Rf(-1, 1), Rf(0, 1), Rf(-1, 1)}), {Rf(-1, 1), Rf(1, 3), Rf(-1, 1)}, Rf(1.5f, 3),
            Rf(0.5f, 1.2f), {60, 60, 60, 200});
  for (int i = 0; i < 16; i++)
    AddPart(PT_DEBRIS, c, {Rf(-8, 8), Rf(4, 12), Rf(-8, 8)}, 3.0f, Rf(0.08f, 0.18f), {50, 45, 40, 255});
  float g = GroundAt(c.x, c.z, c.y);
  if (c.y - g < 2) AddDecal({c.x, g, c.z}, radius * 0.6f, {25, 22, 20, 200}, 60);
  AddShockwave(c, radius * 30, 10, 0.6f);
  float camDist = Vector3Distance(cam.position, c);
  Shake(std::clamp(1.2f - camDist / 60, 0.1f, 0.9f));
  PlaySfx3(Sfx3::Boom);

  for (auto& e : enemies) {
    if (!e.alive || e.wreck) continue;
    BoundingBox b = EnemyBox(e);
    Vector3 ec{(b.min.x + b.max.x) / 2, (b.min.y + b.max.y) / 2, (b.min.z + b.max.z) / 2};
    float size = (b.max.x - b.min.x) / 2;
    float d = Vector3Distance(c, ec);
    if (d > radius + size) continue;
    float fall = 1 - std::clamp((d - size) / radius, 0.f, 1.f);
    Vector3 dir = Vector3Normalize(Vector3Subtract(ec, c));
    DamageEnemy(e, dmg * (0.35f + 0.65f * fall), C_BLAST, dir, ec);
  }
  for (auto& cr : crates)
    if (cr.alive && Vector3Distance(c, cr.pos) < radius) OpenCrate(cr);
  for (auto& gb : gibs)
    if (Vector3Distance(c, gb.pos) < radius * 1.5f) {
      Vector3 d = Vector3Normalize(Vector3Subtract(gb.pos, c));
      gb.vel = Vector3Add(gb.vel, {d.x * 8, 6, d.z * 8});
      gb.landed = false;
      gb.angVel = Rf(-10, 10);
    }
  float pd = Vector3Distance(c, PlayerCenter());
  if (pd < radius) HurtPlayer(dmg * (0.3f + 0.7f * (1 - pd / radius)) * (byPlayer ? 0.5f : 1.0f));
}

// ---------------------------------------------------------------------------
// Waves
// ---------------------------------------------------------------------------
void StartWave() {
  wave++;
  toSpawn[EK_ZOMBIE] = 4 + 2 * wave;
  toSpawn[EK_GOOMBA] = 1 + wave;
  toSpawn[EK_TANK] = wave >= 2 ? wave / 2 : 0;
  toSpawn[EK_HELI] = wave >= 3 ? (wave - 1) / 2 : 0;
  spawnT = 1;
  banner = TextFormat("WAVE %d", wave);
  bannerT = 3;
  PlaySfx3(Sfx3::Radio);
}

void SpawnEnemy(EKind k) {
  Vector3 p{};
  for (int tries = 0; tries < 30; tries++) {
    float a = Rf(0, 2 * PI), r = Rf(55, 72);
    p = {cosf(a) * r, 0, sinf(a) * r};
    if (PointFree(p.x, p.z, 2) && Vector3Distance(p, pl.pos) > 35) break;
  }
  Enemy3 e;
  e.kind = k;
  e.pos = {p.x, TerrainH(p.x, p.z), p.z};
  e.yaw = atan2f(-p.x, -p.z);
  e.turret = e.yaw;
  switch (k) {
    case EK_ZOMBIE: e.hp = 60 + wave * 4; break;
    case EK_GOOMBA: e.hp = 40 + wave * 3; e.atkT = Rf(1, 2); break;
    case EK_TANK: e.hp = 420; e.atkT = 3; break;
    case EK_HELI: e.hp = 260; e.pos.y += 18; e.atkT = 2; break;
  }
  e.maxHp = e.hp;
  e.strafe = GetRandomValue(0, 1) ? 1.f : -1.f;
  enemies.push_back(e);
}

void SupplyDrop() {
  for (int i = 0; i < 3; i++) {
    for (int tries = 0; tries < 30; tries++) {
      float a = Rf(0, 2 * PI), r = Rf(10, 28);
      float x = pl.pos.x + cosf(a) * r, z = pl.pos.z + sinf(a) * r;
      if (!PointFree(x, z, 1.5f)) continue;
      Crate3 c{{x, TerrainH(x, z), z}};
      c.dropY = 40 + i * 6;
      crates.push_back(c);
      break;
    }
  }
  Toast("SUPPLY DROP INCOMING");
}

int EnemiesLeft() {
  int n = toSpawn[0] + toSpawn[1] + toSpawn[2] + toSpawn[3];
  for (const auto& e : enemies)
    if (e.alive && !e.wreck && !e.crashing) n++;
  return n;
}

void UpdateWaves(float dt) {
  if (spawnT > 0) spawnT -= dt;
  if (spawnT <= 0) {
    for (int k = 0; k < 4; k++) {
      if (toSpawn[k] > 0) {
        toSpawn[k]--;
        SpawnEnemy((EKind)k);
        spawnT = k >= EK_TANK ? 3.0f : 1.1f;
        break;
      }
    }
  }
  if (EnemiesLeft() == 0) {
    if (breakT <= 0) {
      breakT = WAVE_BREAK;
      banner = TextFormat("WAVE %d CLEARED", wave);
      bannerT = 3;
      AddScore(500 * wave);
      SupplyDrop();
    } else {
      breakT -= dt;
      if (breakT <= 0) StartWave();
    }
  }
}

// ---------------------------------------------------------------------------
// Player
// ---------------------------------------------------------------------------
Ray AimRay() {
  Vector3 d = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
  float skip = Vector3Distance(cam.position, PlayerCenter()) * (inHeli ? 0.9f : 1.0f);
  if (scoped) skip = 0.3f;
  return {Vector3Add(cam.position, Vector3Scale(d, skip)), d};
}

Vector3 MuzzlePos() {
  if (inHeli) return Vector3Add(ph.pos, Vector3Add(Vector3Scale(Fwd(ph.yaw), 2.6f), {0, -0.5f, 0}));
  Vector3 f = Fwd(pl.yaw), r = RightOf(pl.yaw);
  return Vector3Add(pl.pos, {f.x * 0.9f + r.x * 0.28f, 1.15f, f.z * 0.9f + r.z * 0.28f});
}

void StartReload() {
  const WeaponDef& w = WD[pl.weapon];
  if (w.ammo == A_NONE || pl.reloadT > 0 || pl.mag[pl.weapon] >= w.mag) return;
  if (pl.reserve[w.ammo] <= 0) {
    Toast(TextFormat("NO %s AMMO", AMMO_NAME[w.ammo]));
    return;
  }
  pl.reloadT = w.reload;
  scoped = false;
  PlaySfx3(Sfx3::Reload);
}

void FinishReload() {
  const WeaponDef& w = WD[pl.weapon];
  int need = w.mag - pl.mag[pl.weapon];
  int take = std::min(need, pl.reserve[w.ammo]);
  pl.mag[pl.weapon] += take;
  pl.reserve[w.ammo] -= take;
}

void KnifeSlash() {
  pl.slashT = 0.25f;
  pl.fireCD = WD[W_KNIFE].rate;
  PlaySfx3(Sfx3::Swish);
  Vector3 f = Fwd(pl.yaw);
  bool hit = false;
  for (auto& e : enemies) {
    if (!e.alive || e.wreck || e.crashing || e.kind == EK_HELI) continue;
    Vector3 to = Vector3Subtract(e.pos, pl.pos);
    to.y = 0;
    float d = Vector3Length(to);
    float reach = e.kind == EK_TANK ? 3.2f : 2.6f;
    if (d > reach || (d > 0.3f && Vector3DotProduct(Vector3Scale(to, 1 / d), f) < 0.35f)) continue;
    Vector3 at{e.pos.x, e.pos.y + 1, e.pos.z};
    DamageEnemy(e, WD[W_KNIFE].dmg, C_KNIFE, f, at);
    AddShockwave(at, 220, 7, 0.45f);
    hit = true;
  }
  for (auto& c : crates) {
    if (!c.alive || c.dropY > 0) continue;
    Vector3 to = Vector3Subtract(c.pos, pl.pos);
    to.y = 0;
    if (Vector3Length(to) < 2.0f) {
      OpenCrate(c);
      hit = true;
    }
  }
  if (hit) {
    hitMarkT = 0.2f;
    Shake(0.2f);
  }
}

void ApplyHitscan(Vector3 from, Ray aim, float spread, float range, float dmg, float tank, bool tracerAlways) {
  Vector3 d = Vector3Normalize(Vector3Add(aim.direction, {Rf(-spread, spread), Rf(-spread, spread), Rf(-spread, spread)}));
  Hit3 h = CastRay({aim.position, d}, range, true);
  Vector3 end = h.hit ? h.point : Vector3Add(aim.position, Vector3Scale(d, range));
  if (tracerAlways || GetRandomValue(0, 1)) tracers.push_back({from, end, 0.06f, {255, 235, 150, 255}});
  if (!h.hit) return;
  if (h.type == HT_ENEMY) {
    Enemy3& e = enemies[h.index];
    float dmgFinal = (e.kind == EK_TANK || e.kind == EK_HELI) ? tank : dmg;
    BoundingBox b = EnemyBox(e);
    bool head = (e.kind == EK_ZOMBIE || e.kind == EK_GOOMBA) && h.point.y > b.max.y - 0.4f;
    if (head) dmgFinal *= 2.0f;
    DamageEnemy(e, dmgFinal, C_BULLET, d, h.point);
    hitMarkT = 0.15f;
    if (head) Blood(h.point, d, 12, 1.2f);
  } else {
    Sparks(h.point, 4);
    AddPart(PT_SMOKE, h.point, {0, 0.6f, 0}, 0.6f, 0.15f, {140, 130, 110, 160});
    if (h.type == HT_TERRAIN) AddDecal({h.point.x, h.point.y, h.point.z}, 0.07f, {30, 26, 20, 220}, 20);
    if (GetRandomValue(0, 4) == 0) PlaySfx3(Sfx3::Ricochet);
  }
}

void FireWeapon(bool pressed) {
  const WeaponDef& w = WD[pl.weapon];
  if (pl.fireCD > 0) return;
  if (pl.weapon == W_KNIFE) {
    if (pressed) KnifeSlash();
    return;
  }
  if (!w.automatic && !pressed) return;
  if (pl.reloadT > 0) return;
  if (pl.mag[pl.weapon] <= 0) {
    if (pressed) PlaySfx3(Sfx3::Empty);
    StartReload();
    return;
  }
  pl.mag[pl.weapon]--;
  pl.fireCD = w.rate;
  pl.flashT = 0.05f;
  float spread = w.spread;
  if (scoped) spread = 0.0005f;
  else if (aiming) spread *= 0.5f;
  if (Vector3Length({pl.vel.x, 0, pl.vel.z}) > 1) spread *= 1.6f;
  Ray aim = AimRay();
  Vector3 muzzle = MuzzlePos();
  for (int i = 0; i < w.pellets; i++) ApplyHitscan(muzzle, aim, spread, w.range, w.dmg, w.dmg, true);
  pl.pitch += w.recoil * (scoped ? 0.6f : 1.0f);
  pl.yaw += Rf(-w.recoil, w.recoil) * 0.3f;
  pl.recoilKick = 1;
  AddPart(PT_CASING, muzzle, Vector3Add(Vector3Scale(RightOf(pl.yaw), 2), {0, 2.5f, 0}), 1.5f, 0.04f, {220, 180, 60, 255});
  PlaySfx3(w.sound);
  Shake(pl.weapon == W_SNIPER || pl.weapon == W_SHOTGUN ? 0.25f : 0.08f);
  if (pl.weapon == W_SNIPER) scoped = false;  // bolt cycle drops the scope
  if (pl.mag[pl.weapon] == 0) StartReload();
}

void ThrowGrenade() {
  if (pl.grenades <= 0) {
    Toast("NO GRENADES - FIND MORE IN CRATES");
    return;
  }
  pl.grenades--;
  Vector3 d = AimRay().direction;
  Vector3 from = Vector3Add(pl.pos, {0, 1.5f, 0});
  projs.push_back({PK_GRENADE, from, Vector3Add(Vector3Scale(d, 16), {pl.vel.x * 0.5f, 4, pl.vel.z * 0.5f})});
  PlaySfx3(Sfx3::Throw);
}

void CallAirstrike() {
  if (pl.flares <= 0) {
    Toast("NO AIRSTRIKE FLARES - FIND ONE IN CRATES");
    return;
  }
  if (jet.active) return;
  Hit3 h = CastRay(AimRay(), 160, true);
  if (!h.hit) {
    Toast("AIM AT THE GROUND TO MARK A TARGET");
    return;
  }
  pl.flares--;
  float a = Rf(0, 2 * PI);
  jet.active = true;
  jet.dir = {cosf(a), 0, sinf(a)};
  jet.target = h.point;
  jet.pos = Vector3Add(h.point, {-jet.dir.x * 160, 42, -jet.dir.z * 160});
  jet.dropped = 0;
  jet.markerT = 8;
  Toast("AIRSTRIKE MARKED");
  PlaySfx3(Sfx3::Radio);
  PlaySfx3(Sfx3::Jet);
}

void LaunchDrone() {
  if (pl.drones <= 0) {
    Toast("NO DRONES - FIND ONE IN CRATES");
    return;
  }
  pl.drones--;
  droneMode = true;
  scoped = false;
  drone = Drone3{};
  drone.pos = Vector3Add(pl.pos, {0, 2.2f, 0});
  drone.yaw = pl.yaw;
  drone.pitch = -0.3f;
  drone.battery = DRONE_BATTERY;
  drone.payload = 1;
  PlaySfx3(Sfx3::Switch);
}

void EndDrone(bool explode) {
  droneMode = false;
  StopSfx3(Sfx3::Drone);
  if (explode) Explode3(drone.pos, 5.5f, 150, true);
}

float NearestCrateDist(int* idx) {
  float best = 1e9f;
  for (size_t i = 0; i < crates.size(); i++) {
    if (!crates[i].alive || crates[i].dropY > 0) continue;
    float d = Vector3Distance(pl.pos, crates[i].pos);
    if (d < best) {
      best = d;
      *idx = (int)i;
    }
  }
  return best;
}

bool NearHeli() {
  return ph.alive && !inHeli && Vector3Distance(pl.pos, {ph.pos.x, ph.pos.y - 1, ph.pos.z}) < 4.5f;
}

void UpdatePlayer(float dt) {
  Vector2 md = GetMouseDelta();
  float sens = scoped ? 0.0006f : aiming ? 0.0018f : 0.003f;
  pl.yaw -= md.x * sens;
  pl.pitch = std::clamp(pl.pitch - md.y * sens, -1.2f, 1.0f);
  if (pl.recoilKick > 0) pl.recoilKick = std::max(0.f, pl.recoilKick - dt * 8);

  bool sprint = IsKeyDown(KEY_LEFT_SHIFT) && !aiming;
  Vector3 f = Fwd(pl.yaw), r = RightOf(pl.yaw);
  Vector3 want{0, 0, 0};
  if (IsKeyDown(KEY_W)) want = Vector3Add(want, f);
  if (IsKeyDown(KEY_S)) want = Vector3Subtract(want, f);
  if (IsKeyDown(KEY_D)) want = Vector3Add(want, r);
  if (IsKeyDown(KEY_A)) want = Vector3Subtract(want, r);
  float speed = sprint ? 9.5f : aiming ? 3.5f : 6.0f;
  if (Vector3Length(want) > 0) want = Vector3Scale(Vector3Normalize(want), speed);
  float accel = pl.onGround ? 14.f : 4.f;
  pl.vel.x += (want.x - pl.vel.x) * std::min(1.f, accel * dt);
  pl.vel.z += (want.z - pl.vel.z) * std::min(1.f, accel * dt);
  if (IsKeyPressed(KEY_SPACE) && pl.onGround) {
    pl.vel.y = 9;
    pl.onGround = false;
  }
  pl.vel.y -= GRAV * dt;
  pl.pos = Vector3Add(pl.pos, Vector3Scale(pl.vel, dt));
  float ground = CollideSolids(pl.pos, PLAYER_R, PLAYER_H, 0.5f);
  if (pl.pos.y <= ground) {
    pl.pos.y = ground;
    if (pl.vel.y < -18) HurtPlayer((-pl.vel.y - 18) * 3);  // hard landing
    pl.vel.y = 0;
    pl.onGround = true;
  } else {
    pl.onGround = pl.pos.y - ground < 0.05f;
  }
  pl.animT += Vector3Length({pl.vel.x, 0, pl.vel.z}) * dt;

  // weapons
  if (pl.fireCD > 0) pl.fireCD -= dt;
  if (pl.slashT > 0) pl.slashT -= dt;
  if (pl.flashT > 0) pl.flashT -= dt;
  if (pl.meleeCD > 0) pl.meleeCD -= dt;
  if (pl.reloadT > 0) {
    pl.reloadT -= dt;
    if (pl.reloadT <= 0) FinishReload();
  }
  for (int k = 0; k < W_COUNT; k++) {
    if (IsKeyPressed(KEY_ONE + k) && pl.owned[k] && pl.weapon != k) {
      pl.weapon = k;
      pl.reloadT = 0;
      scoped = false;
      PlaySfx3(Sfx3::Switch);
    }
  }
  float wheel = GetMouseWheelMove();
  if (wheel != 0) {
    int w = pl.weapon;
    for (int i = 0; i < W_COUNT; i++) {
      w = (w + (wheel < 0 ? 1 : W_COUNT - 1)) % W_COUNT;
      if (pl.owned[w]) break;
    }
    if (w != pl.weapon) {
      pl.weapon = w;
      pl.reloadT = 0;
      scoped = false;
      PlaySfx3(Sfx3::Switch);
    }
  }
  aiming = IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && pl.weapon != W_KNIFE && pl.reloadT <= 0;
  scoped = aiming && pl.weapon == W_SNIPER;
  if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) FireWeapon(IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
  if (IsKeyPressed(KEY_V) && pl.weapon != W_KNIFE && pl.meleeCD <= 0) {  // quick knife without switching
    pl.meleeCD = 0.6f;
    KnifeSlash();
  }
  if (IsKeyPressed(KEY_R)) StartReload();
  if (IsKeyPressed(KEY_G)) ThrowGrenade();
  if (IsKeyPressed(KEY_Q)) CallAirstrike();
  if (IsKeyPressed(KEY_E)) LaunchDrone();
  if (IsKeyPressed(KEY_F)) {
    int idx = -1;
    if (NearestCrateDist(&idx) < 2.2f) OpenCrate(crates[idx]);
  }
  if (IsKeyPressed(KEY_H) && NearHeli()) {
    inHeli = true;
    scoped = aiming = false;
    Toast("WASD FLY  SPACE/CTRL UP/DOWN  LMB GUN  RMB ROCKET  H EXIT");
  }
  // loot pickup
  for (auto& l : loots)
    if (l.alive && Vector3Distance({pl.pos.x, pl.pos.y + 0.5f, pl.pos.z}, l.pos) < 1.4f) PickUp(l);
  if (pl.pos.y < -10) HurtPlayer(999);
}

// ---------------------------------------------------------------------------
// Vehicles: player helicopter, drone, jet
// ---------------------------------------------------------------------------
void UpdatePlayerHeli(float dt) {
  if (!ph.alive) return;
  if (ph.flashT > 0) ph.flashT -= dt;
  if (ph.fireCD > 0) ph.fireCD -= dt;
  if (ph.rocketCD > 0) ph.rocketCD -= dt;
  ph.heat = std::max(0.f, ph.heat - 0.4f * dt);
  if (ph.overheated && ph.heat < 0.35f) ph.overheated = false;
  float ground = GroundAt(ph.pos.x, ph.pos.z, ph.pos.y);

  if (!inHeli) {
    ph.rotor = std::max(0.f, ph.rotor - dt * 0.4f);
    ph.vel = Vector3Scale(ph.vel, 0.95f);
    ph.vel.y -= 10 * dt;
    ph.pos = Vector3Add(ph.pos, Vector3Scale(ph.vel, dt));
    if (ph.pos.y - 1 < ground) {
      ph.pos.y = ground + 1;
      ph.vel = {0, 0, 0};
    }
    ph.tiltF = ph.tiltS = 0;
    return;
  }
  ph.rotor = std::min(1.f, ph.rotor + dt);
  if (!SfxPlaying3(Sfx3::Heli)) PlaySfx3(Sfx3::Heli);
  Vector2 md = GetMouseDelta();
  ph.yaw -= md.x * 0.0025f;
  pl.pitch = std::clamp(pl.pitch - md.y * 0.0025f, -1.0f, 0.6f);
  pl.yaw = ph.yaw;
  Vector3 f = Fwd(ph.yaw), r = RightOf(ph.yaw);
  float fw = (IsKeyDown(KEY_W) ? 1.f : 0.f) - (IsKeyDown(KEY_S) ? 1.f : 0.f);
  float st = (IsKeyDown(KEY_D) ? 1.f : 0.f) - (IsKeyDown(KEY_A) ? 1.f : 0.f);
  float up = (IsKeyDown(KEY_SPACE) ? 1.f : 0.f) - (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_C) ? 1.f : 0.f);
  Vector3 acc = Vector3Add(Vector3Scale(f, fw * 22), Vector3Scale(r, st * 16));
  acc.y = up * 14;
  ph.vel = Vector3Add(ph.vel, Vector3Scale(acc, dt));
  ph.vel = Vector3Scale(ph.vel, powf(0.35f, dt));  // air drag
  ph.pos = Vector3Add(ph.pos, Vector3Scale(ph.vel, dt));
  Vector3 feet{ph.pos.x, ph.pos.y - 1, ph.pos.z};
  float g2 = CollideSolids(feet, 2.0f, 2.0f, 0.6f);
  ph.pos.x = feet.x;
  ph.pos.z = feet.z;
  if (ph.pos.y - 1 < g2) {
    ph.pos.y = g2 + 1;
    if (ph.vel.y < -12) HurtPlayer(-ph.vel.y * 2);  // crash landing
    ph.vel.y = 0;
  }
  ph.pos.y = std::min(ph.pos.y, 70.f);
  ph.tiltF = fw * -0.18f + Vector3DotProduct(ph.vel, f) * -0.01f;
  ph.tiltS = st * 0.15f;

  Ray aim = AimRay();
  if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && ph.fireCD <= 0 && !ph.overheated) {
    ph.fireCD = 0.06f;
    ph.heat += 0.03f;
    if (ph.heat >= 1) {
      ph.overheated = true;
      Toast("MINIGUN OVERHEATED");
    }
    ApplyHitscan(MuzzlePos(), aim, 0.018f, 200, 16, 9, false);
    PlaySfx3(Sfx3::Shot);
    Shake(0.05f);
  }
  if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
    if (pl.reserve[A_ROCKET] <= 0) {
      Toast("NO ROCKETS - LOOT ROCKET PODS");
    } else if (ph.rocketCD <= 0) {
      pl.reserve[A_ROCKET]--;
      ph.rocketCD = 0.35f;
      Hit3 h = CastRay(aim, 250, true);
      Vector3 tgt = h.hit ? h.point : Vector3Add(aim.position, Vector3Scale(aim.direction, 250));
      Vector3 from = Vector3Add(ph.pos, Vector3Scale(r, (pl.reserve[A_ROCKET] % 2) ? 1.2f : -1.2f));
      Vector3 d = Vector3Normalize(Vector3Subtract(tgt, from));
      projs.push_back({PK_ROCKET, from, Vector3Scale(d, 45)});
      PlaySfx3(Sfx3::Rocket);
    }
  }
  pl.pos = {ph.pos.x, ph.pos.y - 1, ph.pos.z};
  for (auto& l : loots)  // winch up loot by flying low over it
    if (l.alive && Vector3Distance(ph.pos, l.pos) < 3) PickUp(l);
  if (IsKeyPressed(KEY_H)) {  // get out beside the helicopter
    inHeli = false;
    StopSfx3(Sfx3::Heli);
    Vector3 out = Vector3Add(ph.pos, Vector3Scale(r, 3));
    pl.pos = {out.x, GroundAt(out.x, out.z, ph.pos.y), out.z};
    pl.vel = {0, 0, 0};
    return;
  }
  if (ph.hp <= 0) {
    ph.alive = false;
    bool wasIn = inHeli;
    inHeli = false;
    StopSfx3(Sfx3::Heli);
    Explode3(ph.pos, 6, 40, false);
    Toast("HELICOPTER DESTROYED");
    if (wasIn) {
      pl.pos = {ph.pos.x, ph.pos.y - 1, ph.pos.z};
      pl.vel = {Rf(-4, 4), 6, Rf(-4, 4)};
      HurtPlayer(30);
    }
  }
}

void UpdateDrone(float dt) {
  Vector2 md = GetMouseDelta();
  drone.yaw -= md.x * 0.003f;
  drone.pitch = std::clamp(drone.pitch - md.y * 0.003f, -1.4f, 0.8f);
  drone.battery -= dt;
  Vector3 f = Fwd(drone.yaw), r = RightOf(drone.yaw);
  float fw = (IsKeyDown(KEY_W) ? 1.f : 0.f) - (IsKeyDown(KEY_S) ? 1.f : 0.f);
  float st = (IsKeyDown(KEY_D) ? 1.f : 0.f) - (IsKeyDown(KEY_A) ? 1.f : 0.f);
  float up = (IsKeyDown(KEY_SPACE) ? 1.f : 0.f) - (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_C) ? 1.f : 0.f);
  Vector3 acc = Vector3Add(Vector3Scale(f, fw * 30), Vector3Scale(r, st * 24));
  acc.y = up * 20 - 2;
  drone.vel = Vector3Scale(Vector3Add(drone.vel, Vector3Scale(acc, dt)), powf(0.3f, dt));
  Vector3 next = Vector3Add(drone.pos, Vector3Scale(drone.vel, dt));
  float g = TerrainH(next.x, next.z);
  if (InsideSolid(next)) {
    if (Vector3Length(drone.vel) > 14) {  // fast impact detonates
      EndDrone(true);
      return;
    }
    drone.vel = {0, 0, 0};
  } else {
    drone.pos = next;
  }
  if (drone.pos.y < g + 0.3f) {
    drone.pos.y = g + 0.3f;
    drone.vel.y = 0;
  }
  drone.pos.x = std::clamp(drone.pos.x, -ARENA + 1, ARENA - 1);
  drone.pos.z = std::clamp(drone.pos.z, -ARENA + 1, ARENA - 1);
  drone.pos.y = std::min(drone.pos.y, 60.f);
  if (!SfxPlaying3(Sfx3::Drone)) PlaySfx3(Sfx3::Drone);
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    if (drone.payload > 0) {
      drone.payload--;
      projs.push_back({PK_DBOMB, Vector3Add(drone.pos, {0, -0.3f, 0}), Vector3Scale(drone.vel, 0.6f)});
      PlaySfx3(Sfx3::Throw);
    } else {
      EndDrone(true);  // kamikaze
      return;
    }
  }
  if (IsKeyPressed(KEY_E)) {
    EndDrone(false);
    return;
  }
  if (drone.battery <= 0) {
    Toast("DRONE BATTERY DEAD");
    EndDrone(true);
  }
}

void UpdateJet(float dt) {
  if (jet.markerT > 0) {
    jet.markerT -= dt;
    if (GetRandomValue(0, 1))
      AddPart(PT_SMOKE, jet.target, {Rf(-0.3f, 0.3f), Rf(2, 4), Rf(-0.3f, 0.3f)}, 2.0f, 0.4f, {230, 40, 40, 200});
  }
  if (!jet.active) return;
  jet.pos = Vector3Add(jet.pos, Vector3Scale(jet.dir, 70 * dt));
  AddPart(PT_SMOKE, jet.pos, {0, 0, 0}, 1.2f, 0.4f, {240, 240, 240, 140});
  float along = Vector3DotProduct(Vector3Subtract(jet.pos, jet.target), jet.dir);
  while (jet.dropped < 5 && along >= -24 + jet.dropped * 8 - 12) {
    projs.push_back({PK_BOMB, jet.pos, Vector3Scale(jet.dir, 30)});
    PlaySfx3(Sfx3::Whistle);
    jet.dropped++;
  }
  if (along > 200) jet.active = false;
}

// ---------------------------------------------------------------------------
// Enemies
// ---------------------------------------------------------------------------
// Enemy small-arms fire: hit chance falls with range and the target's speed.
void EnemyShoot(Vector3 from, float dmg, float accuracy) {
  Vector3 target = PlayerCenter();
  if (!LineOfSight(from, target)) return;
  float dist = Vector3Distance(from, target);
  float moving = inHeli ? Vector3Length(ph.vel) : Vector3Length({pl.vel.x, 0, pl.vel.z});
  float chance = std::clamp(accuracy - dist / 60 - moving * 0.03f, 0.08f, 0.9f);
  Vector3 end = target;
  if (Rf(0, 1) < chance) {
    HurtPlayer(dmg);
    if (!inHeli) Blood(target, Vector3Normalize(Vector3Subtract(target, from)), 4, 0.5f);
  } else {
    end = Vector3Add(target, {Rf(-1.5f, 1.5f), Rf(-0.5f, 1.5f), Rf(-1.5f, 1.5f)});
  }
  tracers.push_back({from, end, 0.08f, {255, 110, 80, 255}});
  PlaySfx3(Sfx3::Pistol);
}

void MoveGround(Enemy3& e, Vector3 want, float dt, float r, float h) {
  e.vel.x += (want.x - e.vel.x) * std::min(1.f, 8 * dt);
  e.vel.z += (want.z - e.vel.z) * std::min(1.f, 8 * dt);
  e.vel.y -= GRAV * dt;
  Vector3 before = e.pos;
  e.pos = Vector3Add(e.pos, Vector3Scale(e.vel, dt));
  float g = CollideSolids(e.pos, r, h, 0.6f);
  if (e.pos.y <= g) {
    e.pos.y = g;
    e.vel.y = 0;
    e.onGround = true;
  }
  // stuck against something: hop or slide sideways
  float moved = Vector3Distance({before.x, 0, before.z}, {e.pos.x, 0, e.pos.z});
  if (Vector3Length(want) > 0.5f && moved < Vector3Length(want) * dt * 0.3f) {
    e.stateT += dt;
    if (e.stateT > 0.4f && e.onGround && e.kind != EK_TANK) {
      e.vel.y = 7;
      e.onGround = false;
    }
    if (e.stateT > 1.2f) {
      e.strafe = -e.strafe;
      e.stateT = 0;
    }
  } else {
    e.stateT = 0;
  }
}

void UpdateEnemy(Enemy3& e, float dt) {
  if (e.flashT > 0) e.flashT -= dt;
  Vector3 target = PlayerCenter();
  Vector3 to = Vector3Subtract(target, e.pos);
  float dist = Vector3Length({to.x, 0, to.z});
  float yawTo = atan2f(to.x, to.z);
  e.animT += dt;

  if (e.wreck) {  // burning tank hull
    e.stateT += dt;
    if (e.stateT < 15 && GetRandomValue(0, 2) == 0)
      AddPart(PT_SMOKE, Vector3Add(e.pos, {Rf(-1, 1), 2, Rf(-1, 1)}), {Rf(-0.5f, 0.5f), Rf(2, 3), Rf(-0.5f, 0.5f)}, 2.5f,
              Rf(0.5f, 1), {35, 35, 35, 200});
    if (e.stateT < 8 && GetRandomValue(0, 2) == 0)
      AddPart(PT_FIRE, Vector3Add(e.pos, {Rf(-1, 1), 1.6f, Rf(-1, 1)}), {0, Rf(1, 3), 0}, 0.5f, 0.35f, C_FIRE);
    return;
  }

  switch (e.kind) {
    case EK_ZOMBIE: {
      e.yaw = TurnTo(e.yaw, yawTo, 3 * dt);
      float sp = 2.4f + wave * 0.12f;
      Vector3 want = dist > 1.1f ? Vector3Scale(Vector3Add(Fwd(e.yaw), Vector3Scale(RightOf(e.yaw), e.stateT > 0.6f ? e.strafe : 0)), sp)
                                 : Vector3{0, 0, 0};
      MoveGround(e, want, dt, 0.4f, 1.8f);
      e.atkT -= dt;
      if (dist < 1.5f && fabsf(to.y) < 2 && e.atkT <= 0) {
        e.atkT = 1.0f;
        HurtPlayer(14);
        PlaySfx3(Sfx3::Groan);
      }
      if (GetRandomValue(0, 600) == 0 && dist < 30) PlaySfx3(Sfx3::Groan);
      break;
    }
    case EK_GOOMBA: {
      e.yaw = TurnTo(e.yaw, yawTo, 4 * dt);
      Vector3 f = Fwd(e.yaw), r = RightOf(e.yaw);
      Vector3 want{};
      if (dist > 15) want = Vector3Scale(f, 4.2f);
      else if (dist < 8) want = Vector3Scale(f, -3.0f);
      want = Vector3Add(want, Vector3Scale(r, e.strafe * 2.5f));
      if (GetRandomValue(0, 200) == 0) e.strafe = -e.strafe;
      MoveGround(e, want, dt, 0.5f, 1.1f);
      e.atkT -= dt;
      if (e.atkT <= 0 && dist < 45) {
        e.atkT = Rf(1.3f, 2.2f);
        EnemyShoot(Vector3Add(e.pos, {f.x * 0.6f, 0.75f, f.z * 0.6f}), 7, 0.75f);
      }
      break;
    }
    case EK_TANK: {
      Vector3 want{};
      if (dist > 24) {
        e.yaw = TurnTo(e.yaw, yawTo, 0.8f * dt);
        if (fabsf(AngleDiff(e.yaw, yawTo)) < 0.5f) want = Vector3Scale(Fwd(e.yaw), 3.2f);
      }
      MoveGround(e, want, dt, 1.6f, 2.0f);
      e.turret = TurnTo(e.turret, yawTo, 1.0f * dt);
      e.atkT -= dt;
      Vector3 muzzle = Vector3Add(e.pos, Vector3Add(Vector3Scale(Fwd(e.turret), 3.0f), {0, 1.6f, 0}));
      if (e.atkT <= 0 && dist < 80 && fabsf(AngleDiff(e.turret, yawTo)) < 0.08f && LineOfSight(muzzle, target)) {
        e.atkT = Rf(4, 5.5f);
        float flight = dist / 45;
        Vector3 d = Vector3Subtract(target, muzzle);
        Vector3 v{d.x / flight, d.y / flight + 0.5f * 6 * flight, d.z / flight};  // compensate light shell drop
        projs.push_back({PK_SHELL, muzzle, v});
        for (int i = 0; i < 12; i++)
          AddPart(PT_FIRE, muzzle, Vector3Add(Vector3Scale(Fwd(e.turret), Rf(2, 6)), {Rf(-1, 1), Rf(0, 1), Rf(-1, 1)}),
                  0.2f, 0.3f, C_FIRE);
        AddPart(PT_SMOKE, muzzle, {0, 1, 0}, 1.5f, 0.8f, {120, 120, 120, 180});
        PlaySfx3(Sfx3::Cannon);
        Shake(std::clamp(0.5f - dist / 100, 0.05f, 0.4f));
      }
      break;
    }
    case EK_HELI: {
      if (e.crashing) {
        e.vel.y -= 12 * dt;
        e.pos = Vector3Add(e.pos, Vector3Scale(e.vel, dt));
        e.spin += 6 * dt;
        AddPart(PT_SMOKE, e.pos, {0, 1, 0}, 2.0f, 0.7f, {40, 40, 40, 200});
        if (GetRandomValue(0, 1)) AddPart(PT_FIRE, e.pos, {Rf(-1, 1), 1, Rf(-1, 1)}, 0.4f, 0.4f, C_FIRE);
        if (e.pos.y - 1 < GroundAt(e.pos.x, e.pos.z, e.pos.y) || InsideSolid(e.pos)) {
          e.alive = false;
          Explode3(e.pos, 7, 60, false);
          RollLoot({e.pos.x, GroundAt(e.pos.x, e.pos.z, e.pos.y), e.pos.z}, true);
        }
        break;
      }
      // circle the target at a stand-off distance
      float orbitA = atan2f(e.pos.z - target.z, e.pos.x - target.x) + 0.25f * dt * e.strafe;
      Vector3 goal{target.x + cosf(orbitA) * 28, GroundAt(target.x, target.z) + 17, target.z + sinf(orbitA) * 28};
      Vector3 d = Vector3Subtract(goal, e.pos);
      e.vel = Vector3Add(e.vel, Vector3Scale(d, dt * 0.8f));
      e.vel = Vector3Scale(e.vel, powf(0.4f, dt));
      e.pos = Vector3Add(e.pos, Vector3Scale(e.vel, dt));
      e.pos.y = std::max(e.pos.y, GroundAt(e.pos.x, e.pos.z, e.pos.y + 50) + 6);
      e.yaw = TurnTo(e.yaw, yawTo, 1.5f * dt);
      e.atkT -= dt;
      if (e.atkT <= 0) {
        e.atkT = Rf(2.5f, 3.5f);
        e.burst = 6;
      }
      if (e.burst > 0) {
        e.burstT -= dt;
        if (e.burstT <= 0) {
          e.burstT = 0.1f;
          e.burst--;
          EnemyShoot(Vector3Add(e.pos, Vector3Scale(Fwd(e.yaw), 2.5f)), 6, 0.6f);
        }
      }
      e.rocketT -= dt;
      if (e.rocketT <= 0 && LineOfSight(e.pos, target)) {
        e.rocketT = Rf(6, 8);
        Vector3 rd = Vector3Normalize(Vector3Subtract(target, e.pos));
        projs.push_back({PK_EROCKET, Vector3Add(e.pos, Vector3Scale(rd, 3)), Vector3Scale(rd, 30)});
        PlaySfx3(Sfx3::Rocket);
      }
      break;
    }
  }
}

// ---------------------------------------------------------------------------
// Projectiles, particles, gibs
// ---------------------------------------------------------------------------
bool ProjHitsSomething(Vector3 p, bool checkPlayer) {
  if (p.y < TerrainH(p.x, p.z) || InsideSolid(p)) return true;
  for (const auto& e : enemies)
    if (e.alive && !e.wreck && CheckCollisionBoxSphere(EnemyBox(e), p, 0.3f)) return true;
  if (checkPlayer && Vector3Distance(p, PlayerCenter()) < (inHeli ? 2.5f : 1.0f)) return true;
  return false;
}

void UpdateProjs(float dt) {
  for (auto& pr : projs) {
    if (!pr.alive) continue;
    pr.t += dt;
    switch (pr.kind) {
      case PK_GRENADE: {
        pr.vel.y -= GRAV * dt;
        Vector3 next = Vector3Add(pr.pos, Vector3Scale(pr.vel, dt));
        if (InsideSolid(next)) {
          pr.vel.x *= -0.4f;
          pr.vel.z *= -0.4f;
          next = pr.pos;
        }
        float g = GroundAt(next.x, next.z, pr.pos.y);
        if (next.y < g + 0.12f) {
          next.y = g + 0.12f;
          if (pr.vel.y < -3) PlaySfx3(Sfx3::Throw);
          pr.vel.y = -pr.vel.y * 0.35f;
          pr.vel.x *= 0.6f;
          pr.vel.z *= 0.6f;
        }
        pr.pos = next;
        if (pr.t > 2.4f) {
          pr.alive = false;
          Explode3(pr.pos, 6.5f, 140, true);
        }
        break;
      }
      case PK_SHELL:
        pr.vel.y -= 6 * dt;
        pr.pos = Vector3Add(pr.pos, Vector3Scale(pr.vel, dt));
        if (GetRandomValue(0, 1)) AddPart(PT_SMOKE, pr.pos, {0, 0, 0}, 0.5f, 0.15f, {180, 180, 180, 140});
        if (pr.t > 0.08f && ProjHitsSomething(pr.pos, true)) {
          pr.alive = false;
          Explode3(pr.pos, 4.5f, 45, false);
        }
        break;
      case PK_ROCKET:
      case PK_EROCKET: {
        if (pr.kind == PK_ROCKET) {
          pr.vel = Vector3Scale(pr.vel, 1 + dt * 1.5f);
        } else {  // enemy rockets steer gently toward the player
          Vector3 want = Vector3Scale(Vector3Normalize(Vector3Subtract(PlayerCenter(), pr.pos)), Vector3Length(pr.vel));
          pr.vel = Vector3Lerp(pr.vel, want, dt * 0.8f);
        }
        if (Vector3Length(pr.vel) > 90) pr.vel = Vector3Scale(Vector3Normalize(pr.vel), 90);
        pr.pos = Vector3Add(pr.pos, Vector3Scale(pr.vel, dt));
        AddPart(PT_SMOKE, pr.pos, {Rf(-0.3f, 0.3f), Rf(0, 0.5f), Rf(-0.3f, 0.3f)}, 1.0f, 0.25f, {210, 210, 210, 150});
        AddPart(PT_FIRE, pr.pos, Vector3Scale(pr.vel, -0.05f), 0.08f, 0.2f, C_FIRE);
        bool hitPlayer = pr.kind == PK_EROCKET;
        if (pr.t > 0.1f && ProjHitsSomething(pr.pos, hitPlayer)) {
          pr.alive = false;
          Explode3(pr.pos, pr.kind == PK_ROCKET ? 5.5f : 4.5f, pr.kind == PK_ROCKET ? 130 : 40, pr.kind == PK_ROCKET);
        }
        if (pr.t > 8) pr.alive = false;
        break;
      }
      case PK_BOMB:
      case PK_DBOMB:
        pr.vel.y -= GRAV * dt;
        pr.pos = Vector3Add(pr.pos, Vector3Scale(pr.vel, dt));
        if (ProjHitsSomething(pr.pos, false)) {
          pr.alive = false;
          if (pr.kind == PK_BOMB) Explode3(pr.pos, 8, 220, true);
          else Explode3(pr.pos, 6, 160, true);
        }
        break;
    }
    if (pr.t > 15 || fabsf(pr.pos.x) > ARENA + 40 || fabsf(pr.pos.z) > ARENA + 40) pr.alive = false;
  }
  projs.erase(std::remove_if(projs.begin(), projs.end(), [](const Proj3& p) { return !p.alive; }), projs.end());
}

void UpdateParts(float dt) {
  for (auto& p : parts) {
    p.life -= dt;
    switch (p.kind) {
      case PT_BLOOD:
      case PT_DEBRIS:
      case PT_CASING: {
        p.vel.y -= GRAV * dt;
        p.pos = Vector3Add(p.pos, Vector3Scale(p.vel, dt));
        float g = GroundAt(p.pos.x, p.pos.z, p.pos.y + 0.3f);
        if (p.pos.y < g) {
          p.pos.y = g;
          if (p.kind == PT_BLOOD) {
            AddDecal(p.pos, p.size * Rf(1.5f, 3.0f), p.col, 25);
            p.life = 0;
          } else {
            p.vel = {p.vel.x * 0.5f, -p.vel.y * 0.3f, p.vel.z * 0.5f};
          }
        }
        break;
      }
      case PT_FIRE: p.vel = Vector3Scale(p.vel, powf(0.05f, dt)); p.vel.y += 3 * dt; p.pos = Vector3Add(p.pos, Vector3Scale(p.vel, dt)); break;
      case PT_SMOKE: p.vel = Vector3Scale(p.vel, powf(0.5f, dt)); p.pos = Vector3Add(p.pos, Vector3Scale(p.vel, dt)); break;
      case PT_SPARK: p.vel.y -= 10 * dt; p.pos = Vector3Add(p.pos, Vector3Scale(p.vel, dt)); break;
    }
  }
  parts.erase(std::remove_if(parts.begin(), parts.end(), [](const Part3& p) { return p.life <= 0; }), parts.end());
  for (auto& d : decals) d.life -= dt;
  decals.erase(std::remove_if(decals.begin(), decals.end(), [](const Decal& d) { return d.life <= 0; }), decals.end());

  for (auto& g : gibs) {
    g.t += dt;
    if (g.gush > 0) {
      g.gush -= dt;
      Blood(Vector3Add(g.pos, {0, 0.4f, 0}), {0, 0, 0}, 1, 0.8f);
    }
    if (g.landed) continue;
    if (g.part == 1 && g.t < 1) Blood(g.pos, {0, 0, 0}, 1, 0.3f);
    g.vel.y -= GRAV * dt;
    g.pos = Vector3Add(g.pos, Vector3Scale(g.vel, dt));
    g.ang += g.angVel * dt;
    float gy = GroundAt(g.pos.x, g.pos.z, g.pos.y + 0.5f) + 0.2f;
    if (g.part == 0) {  // whole body topples onto its back
      g.ang = std::min(g.ang, PI / 2);
      if (g.pos.y < gy - 0.2f) {
        g.pos.y = gy - 0.2f;
        g.vel = {0, 0, 0};
      }
      if (g.ang >= PI / 2 && g.vel.y == 0) g.landed = true;
      continue;
    }
    if (g.pos.y < gy) {
      g.pos.y = gy;
      if (g.vel.y < -4) {
        g.vel = {g.vel.x * 0.4f, -g.vel.y * 0.25f, g.vel.z * 0.4f};
        g.angVel *= 0.4f;
        PlaySfx3(Sfx3::Splat);
        Blood(g.pos, {0, 0, 0}, 6, 0.5f);
      } else {
        g.landed = true;
        g.ang = roundf(g.ang / (PI / 2)) * (PI / 2);
      }
    }
  }
  gibs.erase(std::remove_if(gibs.begin(), gibs.end(), [](const Gib& g) { return g.t > 20; }), gibs.end());
  for (auto& t : tracers) t.t -= dt;
  tracers.erase(std::remove_if(tracers.begin(), tracers.end(), [](const Tracer& t) { return t.t <= 0; }), tracers.end());
  for (auto& b : blasts) b.t += dt;
  blasts.erase(std::remove_if(blasts.begin(), blasts.end(), [](const Blast& b) { return b.t > 0.5f; }), blasts.end());
  for (auto& w : waves) w.t += dt;
  waves.erase(std::remove_if(waves.begin(), waves.end(), [](const Wave3& w) { return w.t >= w.dur; }), waves.end());
}

void UpdateLootAndCrates(float dt) {
  for (auto& l : loots) {
    l.t += dt;
    l.vel.y -= GRAV * dt;
    l.pos = Vector3Add(l.pos, Vector3Scale(l.vel, dt));
    float g = GroundAt(l.pos.x, l.pos.z, l.pos.y) + 0.35f;
    if (l.pos.y < g) {
      l.pos.y = g;
      l.vel = {0, 0, 0};
    }
    if (l.t > 90) l.alive = false;
  }
  loots.erase(std::remove_if(loots.begin(), loots.end(), [](const Loot3& l) { return !l.alive; }), loots.end());
  for (auto& c : crates) {
    if (c.dropY > 0) {  // parachute descent
      c.dropY = std::max(0.f, c.dropY - 5 * dt);
      if (c.dropY == 0) PlaySfx3(Sfx3::Clank);
    }
  }
  crates.erase(std::remove_if(crates.begin(), crates.end(), [](const Crate3& c) { return !c.alive; }), crates.end());
}

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------
void UpdateCamera3(float dt) {
  if (camShake > 0) camShake = std::max(0.f, camShake - dt * 1.8f);
  Vector3 shake{Rf(-1, 1) * camShake * 0.25f, Rf(-1, 1) * camShake * 0.25f, Rf(-1, 1) * camShake * 0.25f};
  if (droneMode) {
    cam.position = drone.pos;
    cam.target = Vector3Add(drone.pos, Dir(drone.yaw, drone.pitch));
    cam.fovy = 85;
    cam.position = Vector3Add(cam.position, Vector3Scale(shake, 0.3f));
    return;
  }
  if (scoped) {  // through the scope: from the eye, with breathing sway unless Shift holds breath
    float sway = IsKeyDown(KEY_LEFT_SHIFT) ? 0.0003f : 0.004f;
    float sy = sinf(globalT * 1.3f) * sway, sp = sinf(globalT * 2.1f) * sway;
    Vector3 eye = Vector3Add(pl.pos, {0, 1.55f, 0});
    cam.position = eye;
    cam.target = Vector3Add(eye, Dir(pl.yaw + sy, pl.pitch + sp));
    cam.fovy = 12;
    return;
  }
  float dist = inHeli ? 13.f : aiming ? 2.6f : 4.5f;
  float shoulder = inHeli ? 0 : 0.75f;
  float yaw = inHeli ? ph.yaw : pl.yaw;
  Vector3 base = inHeli ? Vector3Add(ph.pos, {0, 2.5f, 0}) : Vector3Add(pl.pos, {0, 1.6f, 0});
  Vector3 pivot = Vector3Add(base, Vector3Scale(RightOf(yaw), shoulder));
  Vector3 back = Vector3Scale(Dir(yaw, pl.pitch), -1);
  Hit3 h = CastRay({pivot, back}, dist, false);
  float d = h.hit ? std::max(0.6f, h.dist - 0.3f) : dist;
  cam.position = Vector3Add(Vector3Add(pivot, Vector3Scale(back, d)), shake);
  cam.target = Vector3Add(pivot, Dir(yaw, pl.pitch));
  cam.fovy = Lerp(cam.fovy, aiming ? 52.f : inHeli ? 75.f : 68.f, std::min(1.f, dt * 12));
}

// ---------------------------------------------------------------------------
// Drawing: models
// ---------------------------------------------------------------------------
// Filled horizontal disc (for shadows, blood and scorch marks).
void Disc(Vector3 c, float r, Color col, int seg = 14) {
  rlBegin(RL_TRIANGLES);
  rlColor4ub(col.r, col.g, col.b, col.a);
  for (int i = 0; i < seg; i++) {
    float a0 = 2 * PI * i / seg, a1 = 2 * PI * (i + 1) / seg;
    rlVertex3f(c.x, c.y, c.z);
    rlVertex3f(c.x + cosf(a1) * r, c.y, c.z + sinf(a1) * r);
    rlVertex3f(c.x + cosf(a0) * r, c.y, c.z + sinf(a0) * r);
  }
  rlEnd();
}

void DrawBlobShadow(Vector3 p, float r) {
  float g = GroundAt(p.x, p.z, p.y + 0.2f);
  Disc({p.x, g + 0.05f, p.z}, r, {0, 0, 0, 70});
}

void DrawGunModel(int w) {  // in hand space, pointing +Z
  switch (w) {
    case W_KNIFE:
      Box(0, 0, 0.12f, 0.05f, 0.05f, 0.14f, C_WOOD);
      Box(0, 0, 0.32f, 0.02f, 0.07f, 0.28f, {220, 220, 230, 255});
      break;
    case W_PISTOL:
      Box(0, 0.03f, 0.12f, 0.06f, 0.08f, 0.26f, C_GUN);
      Box(0, -0.06f, 0.04f, 0.05f, 0.12f, 0.06f, C_GUN);
      break;
    case W_AK:
      Box(0, 0.02f, -0.15f, 0.06f, 0.09f, 0.3f, C_WOOD);
      Box(0, 0.03f, 0.18f, 0.07f, 0.1f, 0.4f, C_GUN);
      Box(0, -0.1f, 0.22f, 0.05f, 0.18f, 0.08f, {120, 70, 20, 255});
      Box(0, 0.03f, 0.5f, 0.04f, 0.05f, 0.3f, C_GUN);
      break;
    case W_SHOTGUN:
      Box(0, 0.02f, -0.12f, 0.06f, 0.09f, 0.28f, C_WOOD);
      Box(0, 0.04f, 0.25f, 0.06f, 0.07f, 0.6f, C_GUN);
      Box(0, -0.03f, 0.3f, 0.06f, 0.05f, 0.3f, C_WOOD);
      break;
    case W_SNIPER:
      Box(0, 0.02f, -0.2f, 0.06f, 0.1f, 0.36f, {70, 80, 60, 255});
      Box(0, 0.03f, 0.25f, 0.06f, 0.08f, 0.55f, C_GUN);
      Box(0, 0.03f, 0.75f, 0.03f, 0.03f, 0.5f, C_GUN);
      Box(0, 0.12f, 0.15f, 0.05f, 0.05f, 0.3f, {30, 30, 34, 255});
      break;
  }
}

void DrawMario(float walk, int weapon, float slash, float pitch, bool flash) {
  float swing = sinf(walk * 3.0f) * 0.6f;
  rlPushMatrix();  // legs
  rlTranslatef(-0.14f, 0.62f, 0);
  rlRotatef(swing * RAD2DEG, 1, 0, 0);
  Box(0, -0.3f, 0, 0.2f, 0.6f, 0.24f, C_BLUE);
  Box(0, -0.58f, 0.05f, 0.22f, 0.1f, 0.32f, C_HAIR);
  rlPopMatrix();
  rlPushMatrix();
  rlTranslatef(0.14f, 0.62f, 0);
  rlRotatef(-swing * RAD2DEG, 1, 0, 0);
  Box(0, -0.3f, 0, 0.2f, 0.6f, 0.24f, C_BLUE);
  Box(0, -0.58f, 0.05f, 0.22f, 0.1f, 0.32f, C_HAIR);
  rlPopMatrix();
  Box(0, 0.92f, 0, 0.56f, 0.5f, 0.34f, C_BLUE);  // overalls
  Box(0, 1.12f, 0, 0.6f, 0.3f, 0.36f, C_RED);    // shirt
  Box(-0.12f, 1.0f, 0.18f, 0.06f, 0.06f, 0.02f, {252, 216, 0, 255});
  Box(0.12f, 1.0f, 0.18f, 0.06f, 0.06f, 0.02f, {252, 216, 0, 255});
  Box(0, 1.45f, 0.02f, 0.42f, 0.4f, 0.4f, C_SKIN);  // head
  Box(0, 1.37f, 0.23f, 0.3f, 0.07f, 0.06f, C_HAIR);  // mustache
  Box(0, 1.47f, 0.24f, 0.1f, 0.1f, 0.06f, C_SKIN);   // nose
  Box(-0.1f, 1.52f, 0.215f, 0.05f, 0.08f, 0.02f, {20, 20, 20, 255});
  Box(0.1f, 1.52f, 0.215f, 0.05f, 0.08f, 0.02f, {20, 20, 20, 255});
  Box(0, 1.7f, 0, 0.46f, 0.14f, 0.44f, C_RED);   // cap
  Box(0, 1.65f, 0.25f, 0.4f, 0.04f, 0.18f, C_RED);  // brim
  // arms hold the weapon forward; knife arm sweeps during a slash
  float armPitch = -pitch * RAD2DEG - 80;
  float armYaw = 0;
  if (weapon == W_KNIFE && slash > 0) {
    float k = 1 - slash / 0.25f;
    armYaw = 70 - 150 * k;
    armPitch = -80;
  }
  rlPushMatrix();
  rlTranslatef(0.34f, 1.22f, 0);
  rlRotatef(armYaw, 0, 1, 0);
  rlRotatef(armPitch, 1, 0, 0);
  Box(0, -0.25f, 0, 0.16f, 0.5f, 0.16f, C_RED);
  Box(0, -0.52f, 0, 0.16f, 0.1f, 0.16f, C_SKIN);
  rlPushMatrix();
  rlTranslatef(0, -0.55f, 0);
  rlRotatef(90, 1, 0, 0);
  DrawGunModel(weapon);
  if (flash && weapon != W_KNIFE) {
    float len = weapon == W_SNIPER ? 1.05f : weapon == W_SHOTGUN ? 0.6f : weapon == W_AK ? 0.68f : 0.3f;
    Box(0, 0.03f, len, 0.18f, 0.18f, 0.18f, {255, 220, 90, 230});
  }
  rlPopMatrix();
  rlPopMatrix();
  rlPushMatrix();
  rlTranslatef(-0.34f, 1.22f, 0);
  rlRotatef(weapon == W_KNIFE ? -20 : armPitch + 10, 1, 0, 0);
  rlRotatef(weapon == W_KNIFE ? 0 : -25, 0, 1, 0);
  Box(0, -0.25f, 0, 0.16f, 0.5f, 0.16f, C_RED);
  Box(0, -0.52f, 0, 0.16f, 0.1f, 0.16f, C_SKIN);
  rlPopMatrix();
}

// part: 0 whole, 1 upper body, 2 legs (models are built around feet at the origin)
void DrawZombie(int part, float walk, Color tint) {
  auto T = [&](Color c) {
    return Color{(unsigned char)(c.r * tint.r / 255), (unsigned char)(c.g * tint.g / 255), (unsigned char)(c.b * tint.b / 255), 255};
  };
  float swing = sinf(walk * 2.2f) * 0.5f;
  if (part != 1) {
    rlPushMatrix();
    rlTranslatef(-0.14f, 0.85f, 0);
    rlRotatef(swing * RAD2DEG, 1, 0, 0);
    Box(0, -0.42f, 0, 0.22f, 0.85f, 0.24f, T(C_ZPANTS));
    rlPopMatrix();
    rlPushMatrix();
    rlTranslatef(0.14f, 0.85f, 0);
    rlRotatef(-swing * RAD2DEG, 1, 0, 0);
    Box(0, -0.42f, 0, 0.22f, 0.85f, 0.24f, T(C_ZPANTS));
    rlPopMatrix();
    Box(0, 0.95f, 0, 0.5f, 0.2f, 0.3f, T(C_ZPANTS));
    if (part == 2) Box(0, 1.06f, 0, 0.44f, 0.03f, 0.26f, C_BLOOD);
  }
  if (part != 2) {
    if (part == 1) Box(0, 1.06f, 0, 0.46f, 0.03f, 0.28f, C_BLOOD);
    Box(0, 1.3f, 0, 0.56f, 0.6f, 0.32f, T(C_ZSHIRT));
    Box(-0.1f, 1.2f, 0.17f, 0.14f, 0.14f, 0.02f, C_BLOOD);
    Box(0, 1.8f, 0.04f, 0.4f, 0.4f, 0.4f, T(C_ZSKIN));
    Box(-0.1f, 1.84f, 0.245f, 0.08f, 0.06f, 0.02f, {230, 20, 20, 255});
    Box(0.1f, 1.84f, 0.245f, 0.08f, 0.06f, 0.02f, {230, 20, 20, 255});
    Box(0, 1.68f, 0.245f, 0.2f, 0.05f, 0.02f, {40, 20, 20, 255});
    for (float sx : {-0.36f, 0.36f}) {  // arms reaching forward
      rlPushMatrix();
      rlTranslatef(sx, 1.5f, 0);
      rlRotatef(-85 + swing * 15, 1, 0, 0);
      Box(0, -0.3f, 0, 0.15f, 0.6f, 0.15f, T(C_ZSKIN));
      rlPopMatrix();
    }
  }
}

void DrawGoomba(int part, float walk, Color tint, bool hasGun, float aimPitch) {
  auto T = [&](Color c) {
    return Color{(unsigned char)(c.r * tint.r / 255), (unsigned char)(c.g * tint.g / 255), (unsigned char)(c.b * tint.b / 255), 255};
  };
  float step = sinf(walk * 8) * 0.12f;
  if (part != 1) {
    Box(-0.25f, 0.1f, step, 0.32f, 0.2f, 0.42f, {30, 25, 20, 255});
    Box(0.25f, 0.1f, -step, 0.32f, 0.2f, 0.42f, {30, 25, 20, 255});
    Box(0, 0.38f, 0, 0.5f, 0.36f, 0.5f, T(C_TAN));
    if (part == 2) Box(0, 0.57f, 0, 0.46f, 0.03f, 0.46f, C_BLOOD);
  }
  if (part != 2) {
    if (part == 1) Box(0, 0.57f, 0, 0.9f, 0.03f, 0.9f, C_BLOOD);
    Box(0, 0.78f, 0, 1.05f, 0.42f, 1.0f, T(C_GBROWN));
    Box(0, 1.03f, 0, 0.8f, 0.14f, 0.8f, T(C_GBROWN));
    Box(-0.2f, 0.8f, 0.51f, 0.18f, 0.24f, 0.02f, WHITE);
    Box(0.2f, 0.8f, 0.51f, 0.18f, 0.24f, 0.02f, WHITE);
    Box(-0.18f, 0.78f, 0.525f, 0.07f, 0.13f, 0.02f, BLACK);
    Box(0.18f, 0.78f, 0.525f, 0.07f, 0.13f, 0.02f, BLACK);
    Box(-0.2f, 0.95f, 0.52f, 0.26f, 0.05f, 0.02f, BLACK);
    Box(0.2f, 0.95f, 0.52f, 0.26f, 0.05f, 0.02f, BLACK);
    if (hasGun) {
      rlPushMatrix();
      rlTranslatef(0.45f, 0.6f, 0.4f);
      rlRotatef(-aimPitch * RAD2DEG, 1, 0, 0);
      DrawGunModel(W_PISTOL);
      rlPopMatrix();
    }
  }
}

void DrawTank(const Enemy3& e) {
  Color hull = e.wreck ? Color{45, 38, 32, 255} : e.flashT > 0 ? Color{200, 120, 110, 255} : C_OLIVE;
  Color dark = e.wreck ? Color{25, 22, 20, 255} : C_DKOLIVE;
  rlPushMatrix();
  rlTranslatef(e.pos.x, e.pos.y, e.pos.z);
  rlRotatef(e.yaw * RAD2DEG, 0, 1, 0);
  Box(-1.15f, 0.4f, 0, 0.6f, 0.8f, 3.8f, {40, 40, 40, 255});
  Box(1.15f, 0.4f, 0, 0.6f, 0.8f, 3.8f, {40, 40, 40, 255});
  for (int i = 0; i < 5; i++) {
    float z = -1.5f + i * 0.75f + fmodf(e.animT * 2, 0.75f) * (e.wreck ? 0 : 1);
    Box(-1.46f, 0.4f, z, 0.03f, 0.5f, 0.1f, {70, 70, 70, 255});
    Box(1.46f, 0.4f, z, 0.03f, 0.5f, 0.1f, {70, 70, 70, 255});
  }
  Box(0, 0.95f, 0, 2.3f, 0.6f, 3.6f, hull);
  Box(0, 1.0f, 1.9f, 2.0f, 0.4f, 0.3f, dark);
  rlRotatef((e.turret - e.yaw) * RAD2DEG, 0, 1, 0);
  Box(0, 1.55f, -0.2f, 1.5f, 0.6f, 1.8f, hull);
  Box(0.3f, 1.92f, -0.5f, 0.5f, 0.15f, 0.5f, dark);
  Box(0, 1.6f, 1.8f, 0.18f, 0.18f, 2.2f, dark);
  rlPopMatrix();
}

void DrawHeliModel(Vector3 pos, float yaw, float tiltF, float tiltS, float rotor, Color body, Color tint, float spin) {
  auto T = [&](Color c) {
    return Color{(unsigned char)(c.r * tint.r / 255), (unsigned char)(c.g * tint.g / 255), (unsigned char)(c.b * tint.b / 255), 255};
  };
  rlPushMatrix();
  rlTranslatef(pos.x, pos.y, pos.z);
  rlRotatef((yaw + spin) * RAD2DEG, 0, 1, 0);
  rlRotatef(-tiltF * RAD2DEG, 1, 0, 0);
  rlRotatef(tiltS * RAD2DEG, 0, 0, 1);
  Box(0, 0, 0.3f, 1.6f, 1.4f, 3.2f, T(body));
  Box(0, 0.15f, 1.95f, 1.3f, 0.9f, 0.6f, {120, 190, 230, 255});  // canopy
  Box(0, 0.3f, -2.6f, 0.4f, 0.4f, 3.2f, T(body));                // tail boom
  Box(0, 0.9f, -4.0f, 0.12f, 1.2f, 0.6f, T(Shade(body, 0.8f)));  // fin
  Box(0, -0.9f, 0.3f, 0.1f, 0.1f, 0.1f, T(body));
  Box(-0.75f, -1.0f, 0.3f, 0.1f, 0.08f, 3.0f, C_GUN);  // skids
  Box(0.75f, -1.0f, 0.3f, 0.1f, 0.08f, 3.0f, C_GUN);
  Box(-0.75f, -0.75f, 0.3f, 0.08f, 0.5f, 0.08f, C_GUN);
  Box(0.75f, -0.75f, 0.3f, 0.08f, 0.5f, 0.08f, C_GUN);
  Box(0, -0.6f, 2.0f, 0.2f, 0.2f, 0.6f, C_GUN);   // chin gun
  Box(-1.0f, -0.2f, 0.4f, 0.35f, 0.35f, 1.0f, C_GUN);  // rocket pods
  Box(1.0f, -0.2f, 0.4f, 0.35f, 0.35f, 1.0f, C_GUN);
  Box(0, 0.85f, 0.3f, 0.25f, 0.3f, 0.25f, C_GUN);  // mast
  rlPushMatrix();
  rlTranslatef(0, 1.02f, 0.3f);
  rlRotatef(fmodf(globalT * 1400 * rotor, 360), 0, 1, 0);
  Color blade = rotor > 0.5f ? Color{40, 40, 40, 140} : Color{40, 40, 40, 255};
  Box(0, 0, 0, 9.0f, 0.04f, 0.3f, blade);
  Box(0, 0, 0, 0.3f, 0.04f, 9.0f, blade);
  rlPopMatrix();
  rlPushMatrix();
  rlTranslatef(0.15f, 0.9f, -4.0f);
  rlRotatef(fmodf(globalT * 2000 * rotor, 360), 1, 0, 0);
  Box(0, 0, 0, 0.03f, 1.4f, 0.12f, {40, 40, 40, 200});
  rlPopMatrix();
  rlPopMatrix();
}

void DrawTree(const Tree3& t) {
  rlPushMatrix();
  rlTranslatef(t.pos.x, t.pos.y, t.pos.z);
  rlScalef(t.s, t.s, t.s);
  Box(0, 1.1f, 0, 0.45f, 2.2f, 0.45f, C_WOOD);
  Box(0, 2.6f, 0, 2.4f, 1.4f, 2.4f, {50, 120, 45, 255});
  Box(0, 3.5f, 0, 1.6f, 1.0f, 1.6f, {60, 135, 50, 255});
  Box(0, 4.2f, 0, 0.8f, 0.6f, 0.8f, {70, 150, 60, 255});
  rlPopMatrix();
}

void DrawBuilding(const Solid3& s) {
  Vector3 c{(s.bb.min.x + s.bb.max.x) / 2, (s.bb.min.y + s.bb.max.y) / 2, (s.bb.min.z + s.bb.max.z) / 2};
  Vector3 sz{s.bb.max.x - s.bb.min.x, s.bb.max.y - s.bb.min.y, s.bb.max.z - s.bb.min.z};
  BoxV(c, sz, s.col);
  if (s.kind != SK_BUILDING) return;
  BoxV({c.x, s.bb.max.y + 0.15f, c.z}, {sz.x + 0.3f, 0.3f, sz.z + 0.3f}, Shade(s.col, 0.8f));  // roof lip
  Color win{40, 50, 70, 255};
  float base = s.bb.max.y - 1.6f;
  for (float y = base; y > s.bb.min.y + 1.5f; y -= 2.2f) {
    for (float x = s.bb.min.x + 1.2f; x < s.bb.max.x - 0.8f; x += 2.2f) {
      Box(x, y, s.bb.max.z + 0.02f, 0.9f, 1.0f, 0.04f, win);
      Box(x, y, s.bb.min.z - 0.02f, 0.9f, 1.0f, 0.04f, win);
    }
    for (float z = s.bb.min.z + 1.2f; z < s.bb.max.z - 0.8f; z += 2.2f) {
      Box(s.bb.max.x + 0.02f, y, z, 0.04f, 1.0f, 0.9f, win);
      Box(s.bb.min.x - 0.02f, y, z, 0.04f, 1.0f, 0.9f, win);
    }
  }
}

void DrawCrate(const Crate3& c) {
  float y = c.pos.y + c.dropY;
  Box(c.pos.x, y + 0.5f, c.pos.z, 1.0f, 1.0f, 1.0f, C_WOOD);
  Box(c.pos.x, y + 0.5f, c.pos.z, 1.02f, 0.12f, 1.02f, Shade(C_WOOD, 0.7f));
  Box(c.pos.x, y + 0.5f, c.pos.z, 0.12f, 1.02f, 1.02f, Shade(C_WOOD, 0.7f));
  Box(c.pos.x, y + 1.0f, c.pos.z, 0.5f, 0.02f, 0.5f, {240, 240, 240, 255});
  if (c.dropY > 0) {  // parachute
    DrawCylinder({c.pos.x, y + 3.5f, c.pos.z}, 0.0f, 2.2f, 1.0f, 12, {230, 230, 230, 255});
    DrawCylinderWires({c.pos.x, y + 3.5f, c.pos.z}, 0.0f, 2.2f, 1.0f, 12, {200, 40, 40, 255});
    for (float a = 0; a < 2 * PI; a += PI / 2)
      DrawLine3D({c.pos.x, y + 1, c.pos.z}, {c.pos.x + cosf(a) * 2.1f, y + 3.5f, c.pos.z + sinf(a) * 2.1f}, {60, 60, 60, 255});
  }
}

void DrawLoot(const Loot3& l) {
  float bob = sinf(l.t * 3) * 0.12f;
  rlPushMatrix();
  rlTranslatef(l.pos.x, l.pos.y + bob, l.pos.z);
  rlRotatef(l.t * 90, 0, 1, 0);
  Color c = LootColor(l);
  switch (l.kind) {
    case LK_WEAPON:
      rlScalef(1.8f, 1.8f, 1.8f);
      DrawGunModel(l.sub);
      break;
    case LK_HEALTH:
      Box(0, 0, 0, 0.6f, 0.4f, 0.4f, c);
      Box(0, 0, 0.21f, 0.35f, 0.1f, 0.02f, RED);
      Box(0, 0, 0.21f, 0.1f, 0.3f, 0.02f, RED);
      break;
    case LK_GRENADE:
      Box(0, 0, 0, 0.3f, 0.36f, 0.3f, c);
      Box(0, 0.22f, 0, 0.1f, 0.1f, 0.1f, C_GUN);
      break;
    case LK_DRONE:
      Box(0, 0, 0, 0.35f, 0.15f, 0.35f, c);
      Box(0, 0.05f, 0, 0.9f, 0.04f, 0.06f, c);
      Box(0, 0.05f, 0, 0.06f, 0.04f, 0.9f, c);
      break;
    default:
      Box(0, 0, 0, 0.55f, 0.35f, 0.35f, {70, 80, 50, 255});
      Box(0, 0.19f, 0, 0.56f, 0.06f, 0.36f, c);
      break;
  }
  rlPopMatrix();
  // glow pillar so loot is easy to spot
  DrawCylinder({l.pos.x, l.pos.y - 0.3f, l.pos.z}, 0.25f, 0.25f, 1.6f, 8, Alpha(c, 0.18f));
}

void DrawEnemies() {
  for (const auto& e : enemies) {
    if (!e.alive) continue;
    Color tint = e.flashT > 0 ? Color{255, 140, 140, 255} : WHITE;
    switch (e.kind) {
      case EK_ZOMBIE:
        DrawBlobShadow(e.pos, 0.45f);
        rlPushMatrix();
        rlTranslatef(e.pos.x, e.pos.y, e.pos.z);
        rlRotatef(e.yaw * RAD2DEG, 0, 1, 0);
        DrawZombie(0, e.animT * 1.5f, tint);
        rlPopMatrix();
        break;
      case EK_GOOMBA: {
        DrawBlobShadow(e.pos, 0.55f);
        Vector3 to = Vector3Subtract(PlayerCenter(), e.pos);
        float aim = atan2f(to.y - 0.6f, Vector3Length({to.x, 0, to.z}));
        rlPushMatrix();
        rlTranslatef(e.pos.x, e.pos.y, e.pos.z);
        rlRotatef(e.yaw * RAD2DEG, 0, 1, 0);
        DrawGoomba(0, e.animT, tint, true, aim);
        rlPopMatrix();
        break;
      }
      case EK_TANK:
        DrawBlobShadow(e.pos, 2.4f);
        DrawTank(e);
        break;
      case EK_HELI:
        DrawBlobShadow(e.pos, 3.0f);
        DrawHeliModel(e.pos, e.yaw, 0.15f, 0, 1, {70, 70, 60, 255}, tint, e.spin);
        break;
    }
  }
}

void DrawGibs() {
  for (const auto& g : gibs) {
    float pivotY = g.part == 1 ? (g.kind == EK_ZOMBIE ? 1.3f : 0.75f) : g.part == 2 ? 0.45f : 0;
    rlPushMatrix();
    rlTranslatef(g.pos.x, g.pos.y, g.pos.z);
    rlRotatef(g.yaw * RAD2DEG, 0, 1, 0);
    rlRotatef(-g.ang * RAD2DEG, 1, 0, 0);
    rlTranslatef(0, -pivotY, 0);
    if (g.kind == EK_ZOMBIE) DrawZombie(g.part, 0, {200, 200, 200, 255});
    else DrawGoomba(g.part, 0, {200, 200, 200, 255}, false, 0);
    rlPopMatrix();
  }
}

void DrawParts() {
  for (const auto& p : parts) {
    float k = p.life / p.maxLife;
    switch (p.kind) {
      case PT_FIRE: DrawCube(p.pos, p.size * (0.4f + k), p.size * (0.4f + k), p.size * (0.4f + k), Alpha(p.col, k)); break;
      case PT_SMOKE: {
        float s = p.size * (1.5f - 0.7f * k);
        DrawCube(p.pos, s, s, s, Alpha(p.col, k * 0.55f * p.col.a / 255.f));
        break;
      }
      default: DrawCube(p.pos, p.size, p.size, p.size, p.col); break;
    }
  }
  for (const auto& b : blasts) {
    float k = b.t / 0.5f;
    DrawSphere(b.pos, b.r * (0.3f + 0.7f * k), Alpha({255, 160, 40, 255}, 0.9f * (1 - k)));
    DrawSphere(b.pos, b.r * 0.45f * (1 - k), Alpha({255, 240, 160, 255}, 1 - k));
  }
  for (const auto& t : tracers) DrawLine3D(t.a, t.b, Alpha(t.col, std::min(1.f, t.t * 14)));
}

void DrawDecals() {
  for (const auto& d : decals)
    Disc({d.pos.x, d.pos.y + 0.06f, d.pos.z}, d.r, Alpha(d.col, std::min(1.f, d.life / 3) * d.col.a / 255.f), d.r > 1 ? 18 : 7);
}

void DrawProjs() {
  for (const auto& p : projs) {
    switch (p.kind) {
      case PK_GRENADE: DrawSphere(p.pos, 0.12f, C_OLIVE); break;
      case PK_SHELL: DrawSphere(p.pos, 0.15f, {60, 60, 60, 255}); break;
      case PK_ROCKET:
      case PK_EROCKET: {
        Vector3 d = Vector3Normalize(p.vel);
        DrawCylinderEx(Vector3Subtract(p.pos, Vector3Scale(d, 0.6f)), p.pos, 0.09f, 0.06f, 6,
                       p.kind == PK_ROCKET ? Color{200, 200, 200, 255} : Color{200, 80, 80, 255});
        break;
      }
      default: {
        Vector3 d = Vector3Normalize(p.vel);
        DrawCylinderEx(Vector3Subtract(p.pos, Vector3Scale(d, 0.5f)), Vector3Add(p.pos, Vector3Scale(d, 0.3f)), 0.18f, 0.12f,
                       8, {50, 55, 60, 255});
        break;
      }
    }
  }
}

void DrawJetModel() {
  if (!jet.active) return;
  float yaw = atan2f(jet.dir.x, jet.dir.z);
  rlPushMatrix();
  rlTranslatef(jet.pos.x, jet.pos.y, jet.pos.z);
  rlRotatef(yaw * RAD2DEG, 0, 1, 0);
  Box(0, 0, 0, 1.2f, 1.1f, 8, {150, 160, 170, 255});
  Box(0, 0, 0.5f, 9, 0.15f, 2.4f, {120, 130, 140, 255});
  Box(0, 1.0f, -3.2f, 0.15f, 1.8f, 1.4f, {120, 130, 140, 255});
  Box(0, 0, -3.4f, 4, 0.12f, 1.2f, {120, 130, 140, 255});
  Box(0, 0.45f, 2.5f, 0.8f, 0.4f, 1.6f, {80, 160, 220, 255});
  rlPopMatrix();
}

void DrawDroneModel() {
  if (!droneMode) return;
  rlPushMatrix();
  rlTranslatef(drone.pos.x, drone.pos.y, drone.pos.z);
  rlRotatef(drone.yaw * RAD2DEG, 0, 1, 0);
  Box(0, 0, 0, 0.3f, 0.12f, 0.4f, C_GUN);
  Box(0, 0, 0, 0.9f, 0.04f, 0.06f, C_GUN);
  Box(0, 0, 0, 0.06f, 0.04f, 0.9f, C_GUN);
  rlPopMatrix();
}

void DrawWorld3D() {
  rlDisableBackfaceCulling();
  DrawModel(terrain, {-ARENA, 0, -ARENA}, 1.0f, WHITE);
  DrawDecals();
  // helipad
  DrawCylinder({helipad.x, helipad.y - 0.2f, helipad.z}, 4.5f, 4.5f, 0.3f, 24, {90, 90, 95, 255});
  DrawCircle3D({helipad.x, helipad.y + 0.12f, helipad.z}, 3.6f, {1, 0, 0}, 90, {240, 240, 240, 255});
  rlPushMatrix();
  rlTranslatef(helipad.x, helipad.y + 0.14f, helipad.z);
  Box(-0.8f, 0, 0, 0.3f, 0.02f, 2.6f, {240, 240, 240, 255});
  Box(0.8f, 0, 0, 0.3f, 0.02f, 2.6f, {240, 240, 240, 255});
  Box(0, 0, 0, 1.6f, 0.02f, 0.3f, {240, 240, 240, 255});
  rlPopMatrix();
  for (const auto& s : solids)
    if (s.kind != SK_TRUNK) DrawBuilding(s);
  for (const auto& t : trees) DrawTree(t);
  for (const auto& c : crates) DrawCrate(c);
  for (const auto& l : loots) DrawLoot(l);
  DrawEnemies();
  DrawGibs();
  if (ph.alive) {
    DrawBlobShadow(ph.pos, 3.0f);
    Color tint = ph.flashT > 0 ? Color{255, 140, 140, 255} : WHITE;
    DrawHeliModel(ph.pos, ph.yaw, ph.tiltF, ph.tiltS, ph.rotor, {60, 90, 60, 255}, tint, 0);
  }
  if (!inHeli && !scoped && !dead) {
    DrawBlobShadow(pl.pos, 0.45f);
    rlPushMatrix();
    rlTranslatef(pl.pos.x, pl.pos.y, pl.pos.z);
    rlRotatef(pl.yaw * RAD2DEG, 0, 1, 0);
    DrawMario(pl.animT, pl.weapon, pl.slashT, pl.pitch, pl.flashT > 0);
    rlPopMatrix();
    if (pl.slashT > 0) {  // slash arc
      float k = 1 - pl.slashT / 0.25f;
      Vector3 c = Vector3Add(pl.pos, {0, 1.2f, 0});
      for (int i = 0; i < 10; i++) {
        float a0 = pl.yaw + 1.2f - k * 2.4f * i / 10.f, a1 = pl.yaw + 1.2f - k * 2.4f * (i + 1) / 10.f;
        DrawLine3D(Vector3Add(c, Vector3Scale(Fwd(a0), 1.4f)), Vector3Add(c, Vector3Scale(Fwd(a1), 1.4f)),
                   Alpha(WHITE, pl.slashT / 0.25f));
      }
    }
  }
  if (dead) {
    rlPushMatrix();
    rlTranslatef(pl.pos.x, pl.pos.y + 0.2f, pl.pos.z);
    rlRotatef(pl.yaw * RAD2DEG, 0, 1, 0);
    rlRotatef(std::min(deadT * 180, 90.f), 1, 0, 0);
    DrawMario(0, -1, 0, 0, false);
    rlPopMatrix();
  }
  DrawProjs();
  DrawJetModel();
  DrawDroneModel();
  DrawParts();
  rlEnableBackfaceCulling();
}

// ---------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------
void DrawTextCentered(const char* t, int y, int size, Color c) {
  DrawText(t, (GetScreenWidth() - MeasureText(t, size)) / 2, y, size, c);
}

void DrawMinimap() {
  const int S = 150, x0 = GetScreenWidth() - S - 14, y0 = 14;
  float s = S / (2 * ARENA);
  auto M = [&](float x, float z) { return Vector2{x0 + S / 2.f - x * s, y0 + S / 2.f - z * s}; };
  DrawRectangle(x0, y0, S, S, {20, 40, 20, 170});
  DrawRectangleLines(x0, y0, S, S, {200, 200, 200, 200});
  for (const auto& b : solids) {
    if (b.kind == SK_TRUNK) continue;
    Vector2 a = M(b.bb.max.x, b.bb.max.z), c = M(b.bb.min.x, b.bb.min.z);
    DrawRectangleV(a, {c.x - a.x, c.y - a.y}, {150, 150, 150, 220});
  }
  for (const auto& c : crates) DrawRectangleV(Vector2Subtract(M(c.pos.x, c.pos.z), {2, 2}), {4, 4}, {200, 140, 60, 255});
  for (const auto& l : loots) DrawCircleV(M(l.pos.x, l.pos.z), 2, YELLOW);
  if (ph.alive && !inHeli) DrawCircleV(M(ph.pos.x, ph.pos.z), 4, GREEN);
  for (const auto& e : enemies) {
    if (!e.alive || e.wreck) continue;
    Vector2 p = M(e.pos.x, e.pos.z);
    if (e.kind == EK_TANK) DrawRectangleV(Vector2Subtract(p, {3, 3}), {6, 6}, RED);
    else if (e.kind == EK_HELI) DrawTriangle({p.x, p.y - 4}, {p.x - 4, p.y + 3}, {p.x + 4, p.y + 3}, RED);
    else DrawCircleV(p, 2, RED);
  }
  Vector3 pp = inHeli ? ph.pos : pl.pos;
  float yaw = inHeli ? ph.yaw : pl.yaw;
  Vector2 c = M(pp.x, pp.z);
  Vector2 f{-sinf(yaw), -cosf(yaw)}, r{-f.y, f.x};
  DrawTriangle(Vector2Add(c, Vector2Scale(f, 7)), Vector2Add(c, Vector2Add(Vector2Scale(f, -4), Vector2Scale(r, -4))),
               Vector2Add(c, Vector2Add(Vector2Scale(f, -4), Vector2Scale(r, 4))), WHITE);
}

void DrawWorldLabels() {
  Vector3 camDir = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
  auto visible = [&](Vector3 p) { return Vector3DotProduct(Vector3Subtract(p, cam.position), camDir) > 0; };
  Vector3 me = inHeli ? ph.pos : pl.pos;
  for (const auto& l : loots) {
    if (Vector3Distance(me, l.pos) > 14 || !visible(l.pos)) continue;
    Vector2 s = GetWorldToScreen(Vector3Add(l.pos, {0, 0.8f, 0}), cam);
    std::string n = LootName(l);
    DrawText(n.c_str(), (int)s.x - MeasureText(n.c_str(), 16) / 2, (int)s.y, 16, LootColor(l).r + LootColor(l).g < 120 ? WHITE : LootColor(l));
  }
  for (const auto& e : enemies) {
    if (!e.alive || e.wreck || (e.kind != EK_TANK && e.kind != EK_HELI) || !visible(e.pos)) continue;
    if (Vector3Distance(me, e.pos) > 90) continue;
    Vector2 s = GetWorldToScreen(Vector3Add(e.pos, {0, e.kind == EK_TANK ? 3.f : 2.5f, 0}), cam);
    DrawRectangle((int)s.x - 30, (int)s.y, 60, 6, {60, 0, 0, 200});
    DrawRectangle((int)s.x - 30, (int)s.y, (int)(60 * std::max(0.f, e.hp) / e.maxHp), 6, RED);
  }
}

void DrawScopeOverlay() {
  int w = GetScreenWidth(), h = GetScreenHeight();
  Vector2 c{w / 2.f, h / 2.f};
  float R = h * 0.42f;
  DrawRing(c, R, (float)(w + h), 0, 360, 72, BLACK);
  DrawRing(c, R - 4, R, 0, 360, 72, {20, 20, 20, 255});
  DrawLineEx({c.x - R, c.y}, {c.x - 12, c.y}, 2, BLACK);
  DrawLineEx({c.x + 12, c.y}, {c.x + R, c.y}, 2, BLACK);
  DrawLineEx({c.x, c.y - R}, {c.x, c.y - 12}, 2, BLACK);
  DrawLineEx({c.x, c.y + 12}, {c.x, c.y + R}, 2, BLACK);
  for (int i = 1; i <= 4; i++) {
    DrawCircleV({c.x, c.y + i * 22.f}, 2.5f, BLACK);
    DrawCircleV({c.x + i * 22.f, c.y}, 2.5f, BLACK);
    DrawCircleV({c.x - i * 22.f, c.y}, 2.5f, BLACK);
  }
  DrawCircleV(c, 1.5f, RED);
  Hit3 hh = CastRay(AimRay(), 400, true);
  DrawText(hh.hit ? TextFormat("%dM", (int)hh.dist) : "---", (int)(c.x + R * 0.55f), (int)(c.y + 20), 20, {200, 30, 30, 255});
  if (!IsKeyDown(KEY_LEFT_SHIFT)) DrawText("HOLD SHIFT: STEADY", (int)c.x - 90, (int)(c.y + R * 0.75f), 18, {200, 200, 200, 200});
}

void DrawFpvOverlay() {
  int w = GetScreenWidth(), h = GetScreenHeight();
  float charge = std::max(0.f, drone.battery / DRONE_BATTERY);
  for (int y = 0; y < h; y += 3) DrawRectangle(0, y, w, 1, {0, 0, 0, 50});
  int noise = 150 + (int)((1 - charge) * 900);
  for (int i = 0; i < noise; i++) {
    unsigned char g = (unsigned char)GetRandomValue(120, 255);
    DrawRectangle(GetRandomValue(0, w), GetRandomValue(0, h), 2, 2, {g, g, g, 140});
  }
  DrawRectangleGradientV(0, 0, w, 80, {0, 0, 0, 150}, BLANK);
  DrawRectangleGradientV(0, h - 100, w, 100, BLANK, {0, 0, 0, 180});
  Vector2 c{w / 2.f, h / 2.f};
  DrawCircleLinesV(c, 24, C_HUD);
  DrawLineEx({c.x - 40, c.y}, {c.x - 14, c.y}, 2, C_HUD);
  DrawLineEx({c.x + 14, c.y}, {c.x + 40, c.y}, 2, C_HUD);
  DrawLineEx({c.x, c.y - 40}, {c.x, c.y - 14}, 2, C_HUD);
  // where a dropped bomb lands (straight down from the drone)
  Vector3 below{drone.pos.x, GroundAt(drone.pos.x, drone.pos.z, drone.pos.y), drone.pos.z};
  Vector3 camDir = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
  if (Vector3DotProduct(Vector3Subtract(below, cam.position), camDir) > 0) {
    Vector2 s = GetWorldToScreen(below, cam);
    DrawCircleLinesV(s, 12, {255, 80, 80, 230});
    DrawLineEx({s.x - 18, s.y}, {s.x + 18, s.y}, 2, {255, 80, 80, 230});
  }
  DrawText("FPV", 30, h - 80, 28, C_HUD);
  if (((int)(globalT * 2)) % 2 == 0) {
    DrawCircle(98, h - 66, 7, RED);
    DrawText("REC", 112, h - 78, 24, RED);
  }
  Color bat = charge < 0.3f ? Color{255, 80, 80, 255} : C_HUD;
  DrawRectangleLines(30, h - 40, 120, 18, bat);
  DrawRectangle(33, h - 37, (int)(114 * charge), 12, bat);
  DrawText(TextFormat("%d%%", (int)(charge * 100)), 160, h - 42, 22, bat);
  DrawText(TextFormat("ALT %dM", (int)(drone.pos.y - TerrainH(drone.pos.x, drone.pos.z))), 260, h - 80, 24, C_HUD);
  DrawText(TextFormat("SPD %d", (int)Vector3Length(drone.vel)), 420, h - 80, 24, C_HUD);
  DrawText(drone.payload > 0 ? "LMB DROP" : "LMB KAMIKAZE", 260, h - 42, 24, drone.payload > 0 ? C_HUD : Color{255, 80, 80, 255});
  DrawText("E RETURN", w - 160, h - 42, 24, C_HUD);
  if (charge < 0.25f && ((int)(globalT * 4)) % 2) DrawTextCentered("LOW BATTERY", h / 2 + 60, 30, {255, 80, 80, 255});
}

void DrawHud() {
  int w = GetScreenWidth(), h = GetScreenHeight();
  if (droneMode) {
    DrawFpvOverlay();
  } else if (scoped) {
    DrawScopeOverlay();
  } else {
    Vector2 c{w / 2.f, h / 2.f};
    float gap = 6 + (aiming ? 0 : 6) + pl.recoilKick * 8;
    Color ch = {255, 255, 255, 220};
    if (inHeli) {
      DrawCircleLinesV(c, 18, ch);
      DrawCircleV(c, 2, ch);
    } else if (pl.weapon == W_SHOTGUN) {
      DrawCircleLinesV(c, 22 + gap, ch);
    } else {
      DrawLineEx({c.x - gap - 10, c.y}, {c.x - gap, c.y}, 2, ch);
      DrawLineEx({c.x + gap, c.y}, {c.x + gap + 10, c.y}, 2, ch);
      DrawLineEx({c.x, c.y - gap - 10}, {c.x, c.y - gap}, 2, ch);
      DrawLineEx({c.x, c.y + gap}, {c.x, c.y + gap + 10}, 2, ch);
    }
    if (hitMarkT > 0) {
      Color hm = killMarkT > 0 ? Color{255, 60, 60, 255} : WHITE;
      DrawLineEx({c.x - 14, c.y - 14}, {c.x - 6, c.y - 6}, 3, hm);
      DrawLineEx({c.x + 14, c.y - 14}, {c.x + 6, c.y - 6}, 3, hm);
      DrawLineEx({c.x - 14, c.y + 14}, {c.x - 6, c.y + 6}, 3, hm);
      DrawLineEx({c.x + 14, c.y + 14}, {c.x + 6, c.y + 6}, 3, hm);
    }
  }
  if (pl.hurtT > 0) {
    unsigned char a = (unsigned char)(std::min(1.f, pl.hurtT) * 140);
    DrawRectangleGradientH(0, 0, w / 4, h, {200, 0, 0, a}, BLANK);
    DrawRectangleGradientH(w - w / 4, 0, w / 4, h, BLANK, {200, 0, 0, a});
  }
  if (pl.hp < 30 && !dead) DrawRectangleLinesEx({0, 0, (float)w, (float)h}, 8, {200, 0, 0, (unsigned char)(80 + 60 * sinf(globalT * 6))});

  DrawWorldLabels();
  DrawMinimap();

  // top: wave + score
  DrawText(TextFormat("WAVE %d", wave), 20, 16, 28, WHITE);
  DrawText(TextFormat("ENEMIES %d", EnemiesLeft()), 20, 48, 20, {255, 200, 200, 255});
  DrawText(TextFormat("SCORE %07d", score), 20, 72, 20, WHITE);
  if (breakT > 0 && EnemiesLeft() == 0) DrawText(TextFormat("NEXT WAVE IN %d", (int)ceilf(breakT)), 20, 96, 20, YELLOW);

  if (!droneMode) {  // while flying the drone the camera feed owns the bottom of the screen
    // bottom-left: health
    DrawRectangle(20, h - 60, 260, 26, {0, 0, 0, 150});
    DrawRectangle(23, h - 57, (int)(254 * pl.hp / MAX_HP), 20, pl.hp < 30 ? RED : Color{60, 200, 80, 255});
    DrawText(TextFormat("HP %d", (int)pl.hp), 28, h - 56, 20, WHITE);
    DrawText(TextFormat("G GRENADE x%d    Q AIRSTRIKE x%d    E DRONE x%d", pl.grenades, pl.flares, pl.drones), 20, h - 28, 18,
             {230, 230, 230, 255});

    // bottom-right: weapon + ammo
    if (inHeli) {
      DrawText("HELICOPTER", w - 300, h - 120, 24, YELLOW);
      DrawRectangle(w - 300, h - 90, 200, 14, {60, 0, 0, 255});
      DrawRectangle(w - 300, h - 90, (int)(200 * std::max(0.f, ph.hp) / PHELI_HP), 14, ph.hp < 90 ? RED : GREEN);
      DrawText(TextFormat("ROCKETS %d", pl.reserve[A_ROCKET]), w - 300, h - 70, 22, WHITE);
      DrawText("GUN", w - 300, h - 42, 20, ph.overheated ? RED : WHITE);
      DrawRectangle(w - 250, h - 38, 150, 12, {40, 40, 40, 255});
      DrawRectangle(w - 250, h - 38, (int)(150 * std::min(1.f, ph.heat)), 12, ph.overheated ? RED : ORANGE);
    } else {
      const WeaponDef& wd = WD[pl.weapon];
      DrawText(wd.name, w - 300, h - 110, 28, WHITE);
      if (wd.ammo != A_NONE) {
        Color mc = pl.mag[pl.weapon] == 0 ? RED : WHITE;
        DrawText(TextFormat("%d", pl.mag[pl.weapon]), w - 300, h - 76, 44, mc);
        DrawText(TextFormat("/ %d %s", pl.reserve[wd.ammo], AMMO_NAME[wd.ammo]), w - 230, h - 62, 22,
                 pl.reserve[wd.ammo] == 0 ? RED : Color{200, 200, 200, 255});
        if (pl.reloadT > 0) DrawTextCentered("RELOADING", h / 2 + 50, 22, YELLOW);
      }
      for (int k = 0; k < W_COUNT; k++) {  // weapon slots
        int x = w - 300 + k * 56, y = h - 140;
        Color sc = !pl.owned[k] ? Color{90, 90, 90, 160} : k == pl.weapon ? YELLOW : WHITE;
        DrawRectangleLines(x, y, 50, 22, sc);
        DrawText(TextFormat("%d", k + 1), x + 4, y + 3, 16, sc);
        DrawText(k == 0 ? "KNF" : k == 1 ? "PST" : k == 2 ? "AK" : k == 3 ? "SHG" : "SNP", x + 18, y + 4, 14, sc);
      }
    }

    // context hints
    int idx = -1;
    if (!inHeli && !droneMode && NearestCrateDist(&idx) < 2.2f) DrawTextCentered("F  OPEN CRATE", h / 2 + 90, 24, YELLOW);
    if (NearHeli()) DrawTextCentered("H  BOARD HELICOPTER", h / 2 + 120, 24, YELLOW);
  }
  if (toastT > 0) DrawTextCentered(toast.c_str(), h - 190, 22, YELLOW);
  if (bannerT > 0) DrawTextCentered(banner.c_str(), h / 4, 56, {255, 230, 120, (unsigned char)(std::min(1.f, bannerT) * 255)});

  if (dead) {
    DrawRectangle(0, 0, w, h, {80, 0, 0, (unsigned char)std::min(160.f, deadT * 120)});
    DrawTextCentered("YOU DIED", h / 2 - 60, 60, WHITE);
    DrawTextCentered(TextFormat("WAVE %d    SCORE %d", wave, score), h / 2 + 10, 28, WHITE);
    if (deadT > 1.5f) DrawTextCentered("ENTER: BACK TO TITLE", h / 2 + 60, 24, LIGHTGRAY);
  }
  if (paused) {
    DrawRectangle(0, 0, w, h, {0, 0, 0, 160});
    DrawTextCentered("PAUSED", h / 2 - 140, 50, WHITE);
    const char* lines[] = {"WASD MOVE   SHIFT SPRINT   SPACE JUMP   MOUSE LOOK",
                           "LMB FIRE   RMB AIM / SCOPE   R RELOAD   1-5 OR WHEEL WEAPONS   V QUICK KNIFE",
                           "F OPEN CRATE   G GRENADE   Q AIRSTRIKE (AIM)   E FPV DRONE   H HELICOPTER",
                           "M MUTE", "", "ESC  RESUME        ENTER  QUIT TO TITLE"};
    for (int i = 0; i < 6; i++) DrawTextCentered(lines[i], h / 2 - 60 + i * 32, 20, i == 5 ? YELLOW : LIGHTGRAY);
  }
}

// ---------------------------------------------------------------------------
// Shockwave post-process (same look as the 2D game)
// ---------------------------------------------------------------------------
const char* kWaveFs = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec4 waves[4];
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
    float width = 0.04;
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

bool UploadWaves() {
  if (!waveOk || waves.empty()) return false;
  float data[16] = {0};
  int n = 0;
  float W = (float)GetScreenWidth(), H = (float)GetScreenHeight();
  Vector3 camDir = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
  for (const auto& w : waves) {
    if (n >= 4) break;
    if (Vector3DotProduct(Vector3Subtract(w.pos, cam.position), camDir) <= 0) continue;
    Vector2 s = GetWorldToScreen(w.pos, cam);
    float k = w.t / w.dur;
    data[n * 4 + 0] = s.x / W;
    data[n * 4 + 1] = 1.0f - s.y / H;
    data[n * 4 + 2] = (w.maxR * (1 - (1 - k) * (1 - k))) / H;
    data[n * 4 + 3] = (w.strength * (1 - k) * (1 - k)) / H;
    n++;
  }
  if (n == 0) return false;
  float aspect = W / H;
  SetShaderValue(waveShader, locAspect, &aspect, SHADER_UNIFORM_FLOAT);
  SetShaderValueV(waveShader, locWaves, data, SHADER_UNIFORM_VEC4, 4);
  SetShaderValue(waveShader, locCount, &n, SHADER_UNIFORM_INT);
  return true;
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------
void InitArena() {
  pl = Player3{};
  ph = PlayerHeli{};
  inHeli = droneMode = false;
  jet = Jet3{};
  enemies.clear();
  loots.clear();
  projs.clear();
  parts.clear();
  decals.clear();
  gibs.clear();
  tracers.clear();
  blasts.clear();
  waves.clear();
  score = 0;
  wave = 0;
  breakT = 0;
  for (int& n : toSpawn) n = 0;
  dead = paused = quit = scoped = aiming = false;
  camShake = hitMarkT = killMarkT = toastT = bannerT = 0;
  BuildTerrain();
  BuildWorld();
  pl.pos = {0, TerrainH(0, -6), -6};
  pl.yaw = 0;
  ph.pos = {helipad.x, helipad.y + 1.0f, helipad.z};
  ph.yaw = PI;
  cam.up = {0, 1, 0};
  cam.fovy = 68;
  cam.projection = CAMERA_PERSPECTIVE;
  rt = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
  waveShader = LoadShaderFromMemory(nullptr, kWaveFs);
  waveOk = IsShaderValid(waveShader) && waveShader.id != rlGetShaderIdDefault();
  if (waveOk) {
    locWaves = GetShaderLocation(waveShader, "waves");
    locCount = GetShaderLocation(waveShader, "waveCount");
    locAspect = GetShaderLocation(waveShader, "aspect");
  }
  StartWave();
  Toast("LOOT CRATES (F) FOR WEAPONS AND AMMO");
}

void ShutdownArena() {
  StopSfx3(Sfx3::Heli);
  StopSfx3(Sfx3::Drone);
  UnloadRenderTexture(rt);
  if (waveOk) UnloadShader(waveShader);
  UnloadTexture(terrainTex);
  terrain.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = Texture2D{};
  UnloadModel(terrain);
}

void UpdateArena(float dt) {
  globalT += dt;
  if (toastT > 0) toastT -= dt;
  if (bannerT > 0) bannerT -= dt;
  if (hitMarkT > 0) hitMarkT -= dt;
  if (killMarkT > 0) killMarkT -= dt;
  if (pl.hurtT > 0) pl.hurtT -= dt;
  if (dead) {
    deadT += dt;
    UpdateParts(dt);
    UpdateCamera3(dt);
    return;
  }
  if (droneMode) UpdateDrone(dt);
  else if (!inHeli) UpdatePlayer(dt);
  UpdatePlayerHeli(dt);
  UpdateJet(dt);
  for (auto& e : enemies)
    if (e.alive) UpdateEnemy(e, dt);
  enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
                               [](const Enemy3& e) { return !e.alive || (e.wreck && e.stateT > 40); }),
                enemies.end());
  UpdateProjs(dt);
  UpdateParts(dt);
  UpdateLootAndCrates(dt);
  UpdateWaves(dt);
  UpdateCamera3(dt);
}

void RenderArena() {
  if (rt.texture.width != GetScreenWidth() || rt.texture.height != GetScreenHeight()) {
    UnloadRenderTexture(rt);
    rt = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
  }
  BeginTextureMode(rt);
  ClearBackground(SKY);
  BeginMode3D(cam);
  DrawWorld3D();
  EndMode3D();
  EndTextureMode();

  BeginDrawing();
  ClearBackground(BLACK);
  bool w = UploadWaves();
  if (w) BeginShaderMode(waveShader);
  DrawTextureRec(rt.texture, {0, 0, (float)rt.texture.width, -(float)rt.texture.height}, {0, 0}, WHITE);
  if (w) EndShaderMode();
  DrawHud();
  EndDrawing();
}

}  // namespace

int RunArena3D() {
  InitArena();
  DisableCursor();
  SetExitKey(KEY_NULL);
  while (!WindowShouldClose() && !quit) {
    float dt = std::min(GetFrameTime(), 1 / 30.f);
    if (IsKeyPressed(KEY_M)) ToggleMuteShared();
    if (IsKeyPressed(KEY_ESCAPE) && !dead) {
      paused = !paused;
      if (paused) {
        StopSfx3(Sfx3::Heli);
        StopSfx3(Sfx3::Drone);
        EnableCursor();
      } else {
        DisableCursor();
      }
    }
    if (paused && IsKeyPressed(KEY_ENTER)) quit = true;
    if (dead && deadT > 1.5f && IsKeyPressed(KEY_ENTER)) quit = true;
    if (!paused) UpdateArena(dt);
    RenderArena();
  }
  int finalScore = score;
  ShutdownArena();
  EnableCursor();
  SetExitKey(KEY_ESCAPE);
  return finalScore;
}

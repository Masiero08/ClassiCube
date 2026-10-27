#include "DayNight.h"
#include "Survival.h"
#include "Block.h"
#include "BlockID.h"
#include "Camera.h"
#include "Chat.h"
#include "Commands.h"
#include "Entity.h"
#include "EntityComponents.h"
#include "EntityRenderers.h"
#include "Event.h"
#include "ExtMath.h"
#include "Funcs.h"
#include "Game.h"
#include "Graphics.h"
#include "Lighting.h"
#include "Model.h"
#include "Platform.h"
#include "Server.h"
#include "String_.h"
#include "Utils.h"
#include "World.h"
#include "LateGame.h"

/* Day and night cycle with monsters at night, for survival mode: */
/*  - the sky darkens at dusk, and monsters appear around the player at night */
/*  - monsters chase and hurt the player, and can be hit back (swords help) */
/*  - when the sun rises they burn away or wander off */

#define DAY_SECONDS   600.0f
#define DUSK_SECONDS   60.0f
#define NIGHT_SECONDS 420.0f
#define DAWN_SECONDS   60.0f
#define CYCLE_SECONDS (DAY_SECONDS + DUSK_SECONDS + NIGHT_SECONDS + DAWN_SECONDS)

static float timeOfDay;
static int dayNightMode = DAYNIGHT_NORMAL;
static cc_bool cycleActive, envChanged;
static int sunStep = -1;
static RNGState daynight_rnd;

/* How bright the sky is, from 0 (night) to 1 (day) */
static float GetBrightness(void) {
	float t = timeOfDay;
	if (t < DAY_SECONDS) return 1.0f;
	t -= DAY_SECONDS;
	if (t < DUSK_SECONDS) return 1.0f - t / DUSK_SECONDS;
	t -= DUSK_SECONDS;
	if (t < NIGHT_SECONDS) return 0.0f;
	t -= NIGHT_SECONDS;
	return t / DAWN_SECONDS;
}

cc_bool DayNight_IsNight(void) {
	if (!cycleActive || dayNightMode == DAYNIGHT_END) return false;
	/* The Nether is always dark, so monsters always appear there */
	return dayNightMode == DAYNIGHT_NETHER || GetBrightness() < 0.25f;
}

void DayNight_SetMode(int mode) { dayNightMode = mode; }


/*########################################################################################################################*
*--------------------------------------------------------Sky colors-------------------------------------------------------*
*#########################################################################################################################*/
static PackedCol LerpCol(PackedCol night, PackedCol day, float t) {
	return PackedCol_Lerp(night, day, t);
}

static PackedCol daySky = ENV_DEFAULT_SKY_COLOR, dayFog = ENV_DEFAULT_FOG_COLOR, dayClouds = ENV_DEFAULT_CLOUDS_COLOR;

void DayNight_SetDayColors(PackedCol sky, PackedCol fog, PackedCol clouds) {
	daySky = sky; dayFog = fog; dayClouds = clouds;
}

static void SetSky(PackedCol sky, PackedCol fog, PackedCol clouds) {
	if (Env.SkyCol    != sky)    Env_SetSkyCol(sky);
	if (Env.FogCol    != fog)    Env_SetFogCol(fog);
	if (Env.CloudsCol != clouds) Env_SetCloudsCol(clouds);
}

/* Changing sun light rebuilds every chunk, which is slow on consoles, */
/*  so it only changes in a few steps (day, evening, night) */
static void SetSunStep(int step) {
	static const PackedCol suns[3] = {
		ENV_DEFAULT_SUN_COLOR,              PackedCol_Make(0xC8, 0xB4, 0xA0, 0xFF), PackedCol_Make(0x70, 0x70, 0x90, 0xFF)
	};
	static const PackedCol shadows[3] = {
		ENV_DEFAULT_SHADOW_COLOR,           PackedCol_Make(0x7A, 0x70, 0x66, 0xFF), PackedCol_Make(0x48, 0x48, 0x5C, 0xFF)
	};
	if (step == sunStep) return;
	sunStep = step;

	Env_SetSunCol(suns[step]);
	Env_SetShadowCol(shadows[step]);
}

static void UpdateSky(void) {
	float b = GetBrightness();
	/* Other dimensions keep the same sky all the time */
	if (dayNightMode != DAYNIGHT_NORMAL) {
		SetSky(daySky, dayFog, dayClouds);
		SetSunStep(dayNightMode == DAYNIGHT_NETHER ? 1 : 2);
		envChanged = true;
		return;
	}
	SetSky(LerpCol(PackedCol_Make(0x0A, 0x0F, 0x23, 0xFF), daySky,    b),
		   LerpCol(PackedCol_Make(0x10, 0x14, 0x20, 0xFF), dayFog,    b),
		   LerpCol(PackedCol_Make(0x30, 0x30, 0x40, 0xFF), dayClouds, b));

	SetSunStep(b > 0.66f ? 0 : (b > 0.33f ? 1 : 2));
	envChanged = true;
}

static void ResetSky(void) {
	if (!envChanged) return;
	envChanged = false;
	sunStep    = -1;

	SetSky(daySky, dayFog, dayClouds);
	Env_SetSunCol(ENV_DEFAULT_SUN_COLOR);
	Env_SetShadowCol(ENV_DEFAULT_SHADOW_COLOR);
}


/*########################################################################################################################*
*---------------------------------------------------------Monsters--------------------------------------------------------*
*#########################################################################################################################*/
#define MONSTERS_MAX     8
#define ANIMALS_MAX      6
#define SLOTS_MAX        (MONSTERS_MAX + ANIMALS_MAX)
#define MONSTER_ID_BASE  240 /* 200-224 are used by Criaturas, 225-239 by Minerios, 248 by the Ender Dragon */
#define ANIMAL_ID_BASE   249 /* farm animals use 249-254 */
#define DESPAWN_DIST     48.0f
#define CHASE_DIST       16.0f
#define ATTACK_REACH     4.0f

enum MonsterType { MONSTER_ZOMBIE, MONSTER_SKELETON, MONSTER_SPIDER, MONSTER_CREEPER, HOSTILE_TYPES,
	MONSTER_PIG = HOSTILE_TYPES, MONSTER_SHEEP, MONSTER_CHICKEN, MONSTER_TYPES };
#define IsAnimal(type) ((type) >= HOSTILE_TYPES)

struct MonsterDef {
	const char* model;
	int health, damage;
	float speed;
	cc_bool burns;      /* whether daylight hurts it */
	BlockID drop; int dropCount;
};

static const struct MonsterDef monster_defs[MONSTER_TYPES] = {
	{ "zombie",   12, 3, 2.2f, true,  BLOCK_BROWN_SHROOM, 1 },
	{ "skeleton", 10, 2, 2.6f, true,  BLOCK_COAL_ORE,     1 },
	{ "spider",    8, 2, 3.4f, false, BLOCK_ROPE,         2 },
	{ "creeper",  10, 0, 2.0f, false, BLOCK_TNT,          1 },
	/* Farm animals wander around in daylight and run away when hit */
	{ "pig",      10, 0, 2.0f, false, LITEM_RAW_MEAT,     2 },
	{ "sheep",     8, 0, 2.0f, false, BLOCK_WHITE,        2 },
	{ "chicken",   4, 0, 2.4f, false, LITEM_RAW_MEAT,     1 },
};

struct Monster {
	struct Entity base;
	cc_bool used;
	int type, health;
	float attackTimer, hurtTimer, thinkTimer, fuseTimer, burnTimer;
	Vec3 target;
};
static struct Monster monsters[SLOTS_MAX];
static float spawnTimer, animalTimer;

/* Monsters use the first MONSTERS_MAX slots, animals the rest */
static int SlotEntityID(int i) { return i < MONSTERS_MAX ? MONSTER_ID_BASE + i : ANIMAL_ID_BASE + (i - MONSTERS_MAX); }

static float Dist2XZ(Vec3 a, Vec3 b) {
	float dx = a.x - b.x, dz = a.z - b.z;
	return dx * dx + dz * dz;
}

static cc_bool IsSolid(BlockID b) {
	cc_uint8 col = Blocks.Collide[b];
	return col == COLLIDE_SOLID || col == COLLIDE_ICE || col == COLLIDE_SLIPPERY_ICE;
}

/* Finds where a monster can stand in column (x, z), searching downwards from yTop */
static cc_bool FindGround(int x, int z, int yTop, int yBottom, float* yOut) {
	BlockID b, above, above2;
	int y;
	if (x < 0 || z < 0 || x >= World.Width || z >= World.Length) return false;
	yTop    = min(yTop, World.MaxY - 2);
	yBottom = max(yBottom, 0);

	for (y = yTop; y >= yBottom; y--)
	{
		b      = World_GetBlock(x, y, z);
		above  = World_GetBlock(x, y + 1, z);
		above2 = World_GetBlock(x, y + 2, z);
		if (IsSolid(above) || IsSolid(above2)) continue;

		if (IsSolid(b)) { *yOut = y + Blocks.MaxBB[b].y; return true; }
		if (Blocks.IsLiquid[b]) return false;
	}
	return false;
}

static void Monster_Remove(struct Monster* m) {
	if (!m->used) return;
	Entities_Remove(SlotEntityID((int)(m - monsters)));
	m->used = false;
}

static void RemoveAll(void) {
	int i;
	for (i = 0; i < SLOTS_MAX; i++) Monster_Remove(&monsters[i]);
}

static void Monster_Walk(struct Monster* m, Vec3 target, float speed, float delta) {
	struct Entity* e = &m->base;
	Vec3 pos = e->next.pos;
	float dx = target.x - pos.x, dz = target.z - pos.z;
	float len = Math_SqrtF(dx * dx + dz * dz), step, newY;
	float nx, nz;

	if (len < 0.3f) return;
	step = min(speed * delta, len);
	nx   = pos.x + dx / len * step;
	nz   = pos.z + dz / len * step;
	e->next.yaw  = Math_Atan2f(dx, -dz) * MATH_RAD2DEG;
	e->next.rotY = e->next.yaw;

	if (!FindGround(Math_Floor(nx), Math_Floor(nz), Math_Floor(pos.y + 0.5f) + 1, Math_Floor(pos.y) - 6, &newY)) {
		m->thinkTimer = 0; return;
	}
	/* Can climb one block at a time (spiders climb walls) */
	if (newY - pos.y > (m->type == MONSTER_SPIDER ? 3.05f : 1.05f)) { m->thinkTimer = 0; return; }
	if (newY < pos.y) newY = max(newY, pos.y - 0.5f);

	pos.x = nx; pos.z = nz; pos.y = newY;
	e->next.pos = pos;
}

static void Explode(struct Monster* m) {
	struct Entity* player = &Entities.CurPlayer->Base;
	Vec3 pos = m->base.next.pos;
	int x, y, z, cx, cy, cz;
	float d2;
	BlockID b;

	cx = Math_Floor(pos.x); cy = Math_Floor(pos.y); cz = Math_Floor(pos.z);
	for (y = cy - 1; y <= cy + 2; y++)
		for (z = cz - 2; z <= cz + 2; z++)
			for (x = cx - 2; x <= cx + 2; x++)
			{
				if (!World_Contains(x, y, z)) continue;
				if ((x - cx) * (x - cx) + (y - cy) * (y - cy) + (z - cz) * (z - cz) > 5) continue;
				b = World_GetBlock(x, y, z);
				if (b == BLOCK_AIR || b == BLOCK_BEDROCK || Blocks.IsLiquid[b]) continue;
				Game_UpdateBlock(x, y, z, BLOCK_AIR);
			}

	d2 = Dist2XZ(pos, player->Position) + (pos.y - player->Position.y) * (pos.y - player->Position.y);
	if (d2 < 3.0f * 3.0f) Survival_Damage(d2 < 1.5f * 1.5f ? 12 : 7);
	Chat_AddOf(&(cc_string)String_FromConst("&cBOOM!"), MSG_TYPE_ANNOUNCEMENT);
	Monster_Remove(m);
}

static void Monster_Tick(struct Entity* e, float delta) {
	struct Monster* m = (struct Monster*)e;
	const struct MonsterDef* def = &monster_defs[m->type];
	struct Entity* player = &Entities.CurPlayer->Base;
	Vec3 pos;
	float d2, ang;
	IVec3 p;

	e->prev = e->next;
	m->attackTimer -= delta;
	m->hurtTimer   -= delta;
	m->thinkTimer  -= delta;
	pos = e->next.pos;
	d2  = Dist2XZ(pos, player->Position);

	if (d2 > DESPAWN_DIST * DESPAWN_DIST) { Monster_Remove(m); return; }

	if (IsAnimal(m->type)) {
		/* Runs away from the player for a while after being hit, otherwise wanders */
		if (m->fuseTimer > 0) {
			m->fuseTimer -= delta;
			m->target.x = pos.x + (pos.x - player->Position.x) * 2.0f;
			m->target.z = pos.z + (pos.z - player->Position.z) * 2.0f;
			Monster_Walk(m, m->target, def->speed * 1.6f, delta);
		} else {
			if (m->thinkTimer <= 0) {
				m->thinkTimer = 3.0f + Random_Float(&daynight_rnd) * 5.0f;
				ang = Random_Float(&daynight_rnd) * 2 * MATH_PI;
				m->target.x = pos.x + Math_CosF(ang) * 4.0f;
				m->target.z = pos.z + Math_SinF(ang) * 4.0f;
				/* stands still now and then, grazing */
				if (Random_Next(&daynight_rnd, 3) == 0) m->target = pos;
			}
			Monster_Walk(m, m->target, def->speed * 0.4f, delta);
		}
		AnimatedComp_Update(e, e->prev.pos, e->next.pos, delta);
		return;
	}

	/* Daylight burns zombies and skeletons, others wander off */
	if (!DayNight_IsNight() && dayNightMode == DAYNIGHT_NORMAL) {
		IVec3_Floor(&p, &pos); p.y++;
		if (def->burns && World_Contains(p.x, p.y, p.z) && Lighting.IsLit(p.x, p.y, p.z)) {
			m->burnTimer += delta;
			if (m->burnTimer >= 1.0f) { m->burnTimer = 0; m->health -= 2; m->hurtTimer = 0.3f; }
			if (m->health <= 0) { Monster_Remove(m); return; }
		} else if (d2 > CHASE_DIST * CHASE_DIST && Random_Next(&daynight_rnd, 600) == 0) {
			Monster_Remove(m); return;
		}
	}

	if (d2 < CHASE_DIST * CHASE_DIST && Math_AbsF(pos.y - player->Position.y) < 6) {
		if (d2 > 1.2f * 1.2f) Monster_Walk(m, player->Position, def->speed, delta);
		else e->next.yaw = e->next.rotY = Math_Atan2f(player->Position.x - pos.x, -(player->Position.z - pos.z)) * MATH_RAD2DEG;

		if (m->type == MONSTER_CREEPER) {
			/* Creepers hiss and blow up when they stay close */
			if (d2 < 2.5f * 2.5f) {
				m->fuseTimer += delta;
				if (m->fuseTimer >= 1.5f) { Explode(m); return; }
			} else {
				m->fuseTimer = 0;
			}
		} else if (d2 < 1.6f * 1.6f && Math_AbsF(pos.y - player->Position.y) < 1.8f && m->attackTimer <= 0) {
			m->attackTimer = 1.0f;
			Survival_Damage(def->damage);
		}
	} else {
		if (m->thinkTimer <= 0) {
			m->thinkTimer = 2.0f + Random_Float(&daynight_rnd) * 4.0f;
			ang = Random_Float(&daynight_rnd) * 2 * MATH_PI;
			m->target.x = pos.x + Math_CosF(ang) * 5.0f;
			m->target.z = pos.z + Math_SinF(ang) * 5.0f;
		}
		Monster_Walk(m, m->target, def->speed * 0.4f, delta);
	}
	AnimatedComp_Update(e, e->prev.pos, e->next.pos, delta);
}

static void Monster_Despawn(struct Entity* e) {
	if (e->Flags & ENTITY_FLAG_HAS_MODELVB) Gfx_DeleteScratchVb(&e->ModelVB);
}

static void Monster_SetLocation(struct Entity* e, struct LocationUpdate* update) {
	if (update->flags & LU_HAS_POS) {
		e->prev.pos = update->pos; e->next.pos = update->pos;
	}
}

static PackedCol Monster_GetCol(struct Entity* e) {
	struct Monster* m = (struct Monster*)e;
	Vec3 eyePos = Entity_GetEyePosition(e);
	IVec3 pos;
	PackedCol col;
	IVec3_Floor(&pos, &eyePos);
	col = Lighting.Color(pos.x, pos.y, pos.z);

	/* Flashes red when hurt, flickers white just before a creeper explodes */
	if (m->hurtTimer > 0) return PackedCol_Tint(col, PackedCol_Make(255, 80, 80, 255));
	if (m->fuseTimer > 0 && ((int)(m->fuseTimer * 8) & 1)) return PackedCol_Make(255, 255, 255, 255);
	return col;
}

static void Monster_RenderModel(struct Entity* e, float delta, float t) {
	Vec3_Lerp(&e->Position, &e->prev.pos, &e->next.pos, t);
	Entity_LerpAngles(e, t);
	AnimatedComp_GetCurrent(e, t);

	e->ShouldRender = Model_ShouldRender(e);
	if (e->ShouldRender) Model_Render(e->Model, e);
}

static cc_bool Monster_ShouldRenderName(struct Entity* e) { return false; }

static const struct EntityVTABLE monster_VTABLE = {
	Monster_Tick,        Monster_Despawn,         Monster_SetLocation, Monster_GetCol,
	Monster_RenderModel, Monster_ShouldRenderName
};

static void Monster_Create(int type, Vec3 pos) {
	cc_string model = String_FromReadonly(monster_defs[type].model);
	struct Monster* m;
	int i;

	int first = IsAnimal(type) ? MONSTERS_MAX : 0;
	int last  = IsAnimal(type) ? SLOTS_MAX    : MONSTERS_MAX;

	for (i = first; i < last; i++)
	{
		if (!monsters[i].used) break;
	}
	if (i == last) return;
	m = &monsters[i];

	Mem_Set(m, 0, sizeof(struct Monster));
	Entity_Init(&m->base);
	m->base.VTABLE = &monster_VTABLE;
	m->used   = true;
	m->type   = type;
	m->health = monster_defs[type].health;
	m->target = pos;

	m->base.Model = Model_Get(&model);
	Entity_UpdateModelBounds(&m->base);
	m->base.next.yaw  = m->base.prev.yaw  = (float)Random_Next(&daynight_rnd, 360);
	m->base.next.rotY = m->base.prev.rotY = m->base.next.yaw;
	m->base.prev.pos  = pos; m->base.next.pos = pos; m->base.Position = pos;

	Entities.List[SlotEntityID(i)] = &m->base;
}

static int CountMonsters(void) {
	int i, count = 0;
	for (i = 0; i < MONSTERS_MAX; i++) count += monsters[i].used;
	return count;
}

static int CountAnimals(void) {
	int i, count = 0;
	for (i = MONSTERS_MAX; i < SLOTS_MAX; i++) count += monsters[i].used;
	return count;
}

/* Tries to spawn a monster on the surface, 16 to 28 blocks away from the player */
static void TrySpawn(void) {
	struct Entity* player = &Entities.CurPlayer->Base;
	float ang  = Random_Float(&daynight_rnd) * 2 * MATH_PI;
	float dist = 16.0f + Random_Float(&daynight_rnd) * 12.0f;
	int x = Math_Floor(player->Position.x + Math_CosF(ang) * dist);
	int z = Math_Floor(player->Position.z + Math_SinF(ang) * dist);
	Vec3 pos;
	float y;

	if (!FindGround(x, z, World.MaxY - 2, 1, &y)) return;
	/* Only on open ground under the sky (not on roofs above the player's head) */
	if (dayNightMode == DAYNIGHT_NORMAL && !Lighting.IsLit(x, Math_Floor(y), z)) return;

	pos.x = x + 0.5f; pos.y = y; pos.z = z + 0.5f;
	/* Only zombies and skeletons live in the Nether */
	Monster_Create(dayNightMode == DAYNIGHT_NETHER ? Random_Next(&daynight_rnd, 2) : Random_Next(&daynight_rnd, HOSTILE_TYPES), pos);
}

/* Tries to spawn a farm animal on grass in daylight, 14 to 30 blocks away from the player */
static void TrySpawnAnimal(void) {
	struct Entity* player = &Entities.CurPlayer->Base;
	float ang  = Random_Float(&daynight_rnd) * 2 * MATH_PI;
	float dist = 14.0f + Random_Float(&daynight_rnd) * 16.0f;
	int x = Math_Floor(player->Position.x + Math_CosF(ang) * dist);
	int z = Math_Floor(player->Position.z + Math_SinF(ang) * dist);
	int type, i, count;
	Vec3 pos;
	float y;

	if (!FindGround(x, z, World.MaxY - 2, 1, &y)) return;
	if (World_GetBlock(x, Math_Floor(y) - 1, z) != BLOCK_GRASS) return;
	if (!Lighting.IsLit(x, Math_Floor(y), z)) return;

	/* Animals come in small groups of the same kind */
	type  = HOSTILE_TYPES + Random_Next(&daynight_rnd, MONSTER_TYPES - HOSTILE_TYPES);
	count = 1 + Random_Next(&daynight_rnd, 3);
	for (i = 0; i < count; i++)
	{
		pos.x = x + 0.5f + (i & 1); pos.y = y; pos.z = z + 0.5f + (i >> 1);
		Monster_Create(type, pos);
	}
}

cc_bool DayNight_HandleAttack(void) {
	struct Entity* player;
	struct Monster* best = NULL;
	Vec3 eye, dir, center, rel;
	float t, bestT = ATTACK_REACH, d2;
	int i, damage;

	if (!cycleActive) return false;
	player = &Entities.CurPlayer->Base;
	eye    = Entity_GetEyePosition(player);
	dir    = Vec3_GetDirVector(player->Yaw * MATH_DEG2RAD, player->Pitch * MATH_DEG2RAD + Camera.TiltPitch);

	/* Finds the closest monster (or animal) the player is looking at */
	for (i = 0; i < SLOTS_MAX; i++)
	{
		if (!monsters[i].used) continue;
		center    = monsters[i].base.Position;
		center.y += monsters[i].base.Size.y * 0.5f;
		Vec3_Sub(&rel, &center, &eye);

		t = rel.x * dir.x + rel.y * dir.y + rel.z * dir.z;
		if (t < 0 || t > bestT) continue;
		d2 = Vec3_LengthSquared(&rel) - t * t;
		if (d2 > 0.7f * 0.7f) continue;

		bestT = t; best = &monsters[i];
	}
	if (!best) return false;

	damage = Survival_AttackDamage();
	best->health   -= damage;
	best->hurtTimer = 0.3f;
	best->fuseTimer = IsAnimal(best->type) ? 4.0f : 0; /* animals run away for a while */

	if (best->health <= 0) {
		const struct MonsterDef* def = &monster_defs[best->type];
		Survival_AddBlocks(def->drop, def->dropCount);
		if (best->type == MONSTER_SHEEP) Survival_AddBlocks(LITEM_RAW_MEAT, 1);
		/* Skeletons in the Nether sometimes carry ender pearls (needed for the End) */
		if (dayNightMode == DAYNIGHT_NETHER && best->type == MONSTER_SKELETON && Random_Next(&daynight_rnd, 2) == 0)
			Survival_AddBlocks(LITEM_ENDER_PEARL, 1);
		Monster_Remove(best);
	} else {
		/* Knocked back a little */
		center = best->base.next.pos;
		center.x += dir.x * 1.5f; center.z += dir.z * 1.5f;
		Monster_Walk(best, center, 30.0f, 0.05f);
	}
	return true;
}


/*########################################################################################################################*
*---------------------------------------------------------Ticking---------------------------------------------------------*
*#########################################################################################################################*/
static float skyTimer;

static void UpdateActive(void) {
	cc_bool active = Survival_IsActive() && World.Loaded;
	if (active == cycleActive) return;
	cycleActive = active;

	if (!active) { RemoveAll(); ResetSky(); }
	else UpdateSky();
}

static cc_bool DayNight_Tick(struct ScheduledTask2* task) {
	float delta = task->interval;
	UpdateActive();
	if (!cycleActive || !Entities.CurPlayer) return true;

	timeOfDay += delta;
	if (timeOfDay >= CYCLE_SECONDS) timeOfDay -= CYCLE_SECONDS;

	skyTimer += delta;
	if (skyTimer >= 0.5f) { skyTimer = 0; UpdateSky(); }

	if (!DayNight_IsNight() && dayNightMode == DAYNIGHT_NORMAL) {
		animalTimer += delta;
		if (animalTimer >= 6.0f) {
			animalTimer = 0;
			if (CountAnimals() < ANIMALS_MAX - 2) TrySpawnAnimal();
		}
	}

	if (DayNight_IsNight()) {
		spawnTimer += delta;
		if (spawnTimer >= 4.0f) {
			spawnTimer = 0;
			if (CountMonsters() < MONSTERS_MAX) TrySpawn();
		}
	}
	return true;
}
static struct ScheduledTask2 daynight_task = { 0, 1.0f / 20, DayNight_Tick };

int  DayNight_GetTime(void)      { return (int)timeOfDay; }
int  DayNight_GetMode(void)      { return dayNightMode; }
void DayNight_SetTime(int value) { timeOfDay = (float)(value % (int)CYCLE_SECONDS); if (timeOfDay < 0) timeOfDay = 0; }

static void TimeCommand_Execute(const cc_string* args, int argsCount) {
	int count;
	if (argsCount && String_CaselessEqualsConst(&args[0], "noite")) {
		timeOfDay = DAY_SECONDS + DUSK_SECONDS;
	} else {
		timeOfDay = 0;
	}
	UpdateSky();

	count = CountMonsters();
	Chat_Add1(DayNight_IsNight() ? "&eAgora e noite &7(%i monstros por perto)" : "&eAgora e dia &7(%i monstros por perto)", &count);
}

static struct ChatCommand TimeCommand = {
	"Tempo", TimeCommand_Execute,
	COMMAND_FLAG_SINGLEPLAYER_ONLY,
	{
		"&a/client tempo [dia/noite]",
		"&eMuda a hora do dia no modo sobrevivencia.",
	}
};

static void OnInit(void) {
	Random_SeedFromCurrentTime(&daynight_rnd);
	Commands_Register(&TimeCommand);
	ScheduledTask2_Add(&daynight_task);
}

static void OnNewMap(void) {
	RemoveAll();
	timeOfDay   = 0;
	cycleActive = false;
	sunStep     = -1;
	envChanged  = false;
}

static void OnFree(void) { RemoveAll(); }

struct IGameComponent DayNight_Component = {
	OnInit,   /* Init  */
	OnFree,   /* Free  */
	NULL,     /* Reset */
	OnNewMap, /* OnNewMap */
	NULL      /* OnNewMapLoaded */
};

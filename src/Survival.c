#include "Survival.h"
#include "Block.h"
#include "BlockID.h"
#include "Chat.h"
#include "Commands.h"
#include "Entity.h"
#include "Event.h"
#include "ExtMath.h"
#include "Game.h"
#include "Inventory.h"
#include "Options.h"
#include "Server.h"
#include "String_.h"
#include "World.h"
#include "Funcs.h"
#include "Platform.h"

/* Basic survival mode for singleplayer, inspired by newer Minecraft versions: */
/*  - health (10 hearts), fall/lava/drowning damage, regeneration and dying */
/*  - broken blocks are collected, and placing a block uses one up */
/*  - flying, noclip, speed and respawning hacks are disabled */

#define MAX_HEALTH       20
#define SAFE_FALL_BLOCKS 3.0f
#define MAX_AIR_SECONDS  10.0f
#define REGEN_DELAY      4.0f
#define MAX_BLOCK_COUNT  999
#define OPT_SURVIVAL     "survival-mode"

cc_bool Survival_Enabled;
static cc_bool survival_active;
static int health;
static float fallStartY, airLeft, hurtCooldown, regenTimer;
static cc_bool wasFalling;
static cc_uint16 blockCounts[BLOCK_COUNT];
static RNGState survival_rnd;
static int lastHealth = -1, lastHeldCount = -1;
static BlockID lastHeld;


/*########################################################################################################################*
*----------------------------------------------------------HUD------------------------------------------------------------*
*#########################################################################################################################*/
static void UpdateHealthText(void) {
	cc_string str; char strBuffer[STRING_SIZE];
	int i;
	if (health == lastHealth) return;
	lastHealth = health;
	String_InitArray(str, strBuffer);

	/* '\x03' is the heart character in code page 437 */
	String_AppendConst(&str, "&c");
	for (i = 0; i < MAX_HEALTH / 2; i++)
	{
		if (i * 2 == health - 1) {
			String_AppendConst(&str, "&6\x03"); /* half heart */
		} else if (i * 2 >= health) {
			String_AppendConst(&str, "&8\x03");
		} else {
			String_Append(&str, '\x03');
		}
	}
	Chat_AddOf(&str, MSG_TYPE_BOTTOMRIGHT_2);
}

static void UpdateHeldText(void) {
	cc_string str; char strBuffer[STRING_SIZE];
	cc_string name;
	BlockID held = Inventory_SelectedBlock;
	int count    = blockCounts[held];

	if (held == lastHeld && count == lastHeldCount) return;
	lastHeld = held; lastHeldCount = count;
	String_InitArray(str, strBuffer);

	if (held != BLOCK_AIR) {
		name = Block_UNSAFE_GetName(held);
		String_Format2(&str, count ? "&f%s: &e%i" : "&7%s: &c%i", &name, &count);
	}
	Chat_AddOf(&str, MSG_TYPE_BOTTOMRIGHT_1);
}

static IVec3 breakPos;
static int breakHits;
static double lastHitTime;

static void UpdateBreakText(int hits, int hardness) {
	cc_string str; char strBuffer[STRING_SIZE];
	int i;
	String_InitArray(str, strBuffer);

	if (hits) {
		String_AppendConst(&str, "&7Quebrando &a");
		for (i = 0; i < hardness; i++)
		{
			if (i == hits) String_AppendConst(&str, "&8");
			String_Append(&str, '|');
		}
	}
	Chat_AddOf(&str, MSG_TYPE_BOTTOMRIGHT_3);
}

static void ClearHUD(void) {
	Chat_AddOf(&String_Empty, MSG_TYPE_BOTTOMRIGHT_1);
	Chat_AddOf(&String_Empty, MSG_TYPE_BOTTOMRIGHT_2);
	Chat_AddOf(&String_Empty, MSG_TYPE_BOTTOMRIGHT_3);
	lastHealth = -1; lastHeldCount = -1;
}


/*########################################################################################################################*
*--------------------------------------------------------Health-----------------------------------------------------------*
*#########################################################################################################################*/
static void ResetPlayerState(void) {
	struct LocalPlayer* p = (struct LocalPlayer*)Entities.CurPlayer;
	health       = MAX_HEALTH;
	airLeft      = MAX_AIR_SECONDS;
	hurtCooldown = 0;
	regenTimer   = 0;
	wasFalling   = false;
	fallStartY   = p ? p->Base.Position.y : 0;
}

static void ClearHotbar(void) {
	int i;
	for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR; i++)
	{
		Inventory_Set(i, BLOCK_AIR);
	}
	Event_RaiseVoid(&UserEvents.HeldBlockChanged);
}

static void Die(void) {
	struct LocalPlayer* p = (struct LocalPlayer*)Entities.CurPlayer;
	Chat_AddOf(&(cc_string)String_FromConst("&cVoce morreu!"), MSG_TYPE_ANNOUNCEMENT);
	Mem_Set(blockCounts, 0, sizeof(blockCounts));
	ClearHotbar();

	LocalPlayer_DoRespawn(p);
	ResetPlayerState();
}

static void Damage(int amount) {
	if (amount <= 0 || hurtCooldown > 0) return;
	health      -= amount;
	hurtCooldown = 0.5f;
	regenTimer   = 0;
	if (health <= 0) Die();
}

static cc_bool HeadInWater(struct Entity* e) {
	Vec3 eye = e->Position;
	IVec3 pos;
	BlockID block;

	eye.y += Entity_GetEyeHeight(e);
	IVec3_Floor(&pos, &eye);
	block = World_SafeGetBlock(pos.x, pos.y, pos.z);
	return Blocks.Collide[block] == COLLIDE_WATER;
}

static void UpdateFalling(struct Entity* e) {
	float y = e->Position.y;
	/* Water, ropes and ladders break falls */
	if (Entity_TouchesAnyWater(e) || Entity_TouchesAnyRope(e)) {
		wasFalling = false; return;
	}

	if (!e->OnGround) {
		if (!wasFalling || y > fallStartY) fallStartY = y;
		wasFalling = true;
	} else if (wasFalling) {
		wasFalling = false;
		Damage((int)(fallStartY - y - SAFE_FALL_BLOCKS));
	}
}

static cc_bool Survival_Tick(struct ScheduledTask2* task) {
	struct LocalPlayer* p = (struct LocalPlayer*)Entities.CurPlayer;
	struct Entity* e;
	float delta = task->interval;

	if (!survival_active || !World.Loaded || !p) return true;
	e = &p->Base;
	/* Stopped hitting the block being broken */
	if (breakHits && Game.Time - lastHitTime > 0.5) { breakHits = 0; UpdateBreakText(0, 0); }
	if (hurtCooldown > 0) hurtCooldown -= delta;

	UpdateFalling(e);
	if (Entity_TouchesAnyLava(e)) Damage(4);

	if (HeadInWater(e)) {
		airLeft -= delta;
		if (airLeft <= 0) { Damage(2); airLeft = 1.0f; }
	} else {
		airLeft = MAX_AIR_SECONDS;
	}

	if (health < MAX_HEALTH) {
		regenTimer += delta;
		if (regenTimer >= REGEN_DELAY) { health++; regenTimer = 0; }
	}

	UpdateHealthText();
	UpdateHeldText();
	return true;
}
static struct ScheduledTask2 survival_task = { 0, 1.0f / 20, Survival_Tick };


/*########################################################################################################################*
*--------------------------------------------------------Blocks-----------------------------------------------------------*
*#########################################################################################################################*/
/* Returns the block that is collected when the given block is broken */
static BlockID GetDrop(BlockID block) {
	switch (block)
	{
	case BLOCK_STONE:  return BLOCK_COBBLE;
	case BLOCK_GRASS:  return BLOCK_DIRT;
	case BLOCK_GLASS:  return BLOCK_AIR;
	case BLOCK_ICE:    return BLOCK_AIR;
	case BLOCK_FIRE:   return BLOCK_AIR;
	case BLOCK_LEAVES: 
		return Random_Next(&survival_rnd, 8) == 0 ? BLOCK_SAPLING : BLOCK_AIR;
	}
	return block;
}

/* Returns how many hits (0.25 seconds apart while held) it takes to break the given block */
static int GetHardness(BlockID block) {
	if (Blocks.Draw[block] == DRAW_SPRITE) return 1;

	switch (block)
	{
	case BLOCK_LEAVES: case BLOCK_GLASS: case BLOCK_SNOW: case BLOCK_TNT:
		return 1;
	case BLOCK_DIRT: case BLOCK_GRASS: case BLOCK_SAND: case BLOCK_GRAVEL: case BLOCK_SPONGE:
		return 2;
	case BLOCK_WOOD: case BLOCK_LOG: case BLOCK_BOOKSHELF: case BLOCK_CRATE:
		return 4;
	case BLOCK_GOLD_ORE: case BLOCK_IRON_ORE: case BLOCK_COAL_ORE:
	case BLOCK_GOLD: case BLOCK_IRON:
		return 8;
	case BLOCK_OBSIDIAN:
		return 24;
	}
	/* Cloth (wool) blocks */
	if (block >= BLOCK_RED && block <= BLOCK_WHITE) return 2;
	if (block >= BLOCK_LIGHT_PINK && block <= BLOCK_TURQUOISE) return 2;
	return 6;
}

cc_bool Survival_CanBreak(BlockID block, int x, int y, int z) {
	int hardness;
	if (!survival_active) return true;
	if (block == BLOCK_BEDROCK) return false;

	/* Continuing to hit the same block? */
	if (breakPos.x != x || breakPos.y != y || breakPos.z != z || Game.Time - lastHitTime > 0.5) {
		breakHits = 0;
		breakPos.x = x; breakPos.y = y; breakPos.z = z;
	}
	lastHitTime = Game.Time;
	hardness    = GetHardness(block);

	if (++breakHits < hardness) {
		UpdateBreakText(breakHits, hardness);
		return false;
	}
	breakHits = 0;
	UpdateBreakText(0, hardness);
	return true;
}

/* Puts a newly collected block into the first hotbar slot that has nothing left to place */
static void AddToHotbar(BlockID block) {
	int i;
	for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR; i++)
	{
		if (Inventory_Get(i) == block) return;
	}

	for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR; i++)
	{
		if (blockCounts[Inventory_Get(i)]) continue;
		Inventory_Set(i, block);
		Event_RaiseVoid(&UserEvents.HeldBlockChanged);
		return;
	}
}

void Survival_OnBlockBroken(BlockID block) {
	BlockID drop;
	if (!survival_active) return;

	drop = GetDrop(block);
	if (drop == BLOCK_AIR || blockCounts[drop] >= MAX_BLOCK_COUNT) return;
	blockCounts[drop]++;
	AddToHotbar(drop);
}

cc_bool Survival_CanPlace(BlockID block) {
	return !survival_active || blockCounts[block] > 0;
}

void Survival_OnBlockPlaced(BlockID block) {
	if (!survival_active || !blockCounts[block]) return;
	blockCounts[block]--;
}


/*########################################################################################################################*
*-------------------------------------------------------Activation--------------------------------------------------------*
*#########################################################################################################################*/
static void ApplyHacks(void) {
	struct LocalPlayer* p = (struct LocalPlayer*)Entities.CurPlayer;
	if (!p) return;

	String_Copy(&p->Hacks.HacksFlags, survival_active ? 
		&(cc_string)String_FromConst("-fly -noclip -speed -respawn") : &String_Empty);
	HacksComp_RecheckFlags(&p->Hacks);
}

static void UpdateActive(void) {
	cc_bool active = Survival_Enabled && Server.IsSinglePlayer;
	if (active == survival_active) return;
	survival_active = active;

	Mem_Set(blockCounts, 0, sizeof(blockCounts));
	ResetPlayerState();
	ApplyHacks();
	if (active) ClearHotbar();
	if (!active) ClearHUD();
}

static void SurvivalCommand_Execute(const cc_string* args, int argsCount) {
	Survival_Enabled = !Survival_Enabled;
	Options_SetBool(OPT_SURVIVAL, Survival_Enabled);
	UpdateActive();

	Chat_AddRaw(Survival_Enabled ? "&eModo sobrevivencia &aligado" : "&eModo sobrevivencia &cdesligado");
}

static struct ChatCommand SurvivalCommand = {
	"Survival", SurvivalCommand_Execute,
	COMMAND_FLAG_SINGLEPLAYER_ONLY,
	{
		"&a/client survival",
		"&eLiga ou desliga o modo sobrevivencia.",
		"&eNo modo sobrevivencia voce tem vida, toma dano,",
		"&e e so pode colocar os blocos que coletou.",
	}
};

static void OnInit(void) {
	Survival_Enabled = Options_GetBool(OPT_SURVIVAL, true);
	Random_SeedFromCurrentTime(&survival_rnd);
	Commands_Register(&SurvivalCommand);
	ScheduledTask2_Add(&survival_task);
}

static void OnNewMapLoaded(void) {
	/* Always restart with full health and an empty inventory in a new world */
	survival_active = false;
	UpdateActive();
}

struct IGameComponent Survival_Component = {
	OnInit,        /* Init  */
	NULL,          /* Free  */
	NULL,          /* Reset */
	NULL,          /* OnNewMap */
	OnNewMapLoaded /* OnNewMapLoaded */
};

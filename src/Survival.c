#include "Survival.h"
#include "Minerios.h"
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
/*  - hunger, which drains over time and with effort, and food made from flowers and mushrooms */
/*  - pickaxes, axes and shovels that break blocks faster and wear out */

#define MAX_HEALTH       20
#define SAFE_FALL_BLOCKS 3.0f
#define MAX_AIR_SECONDS  10.0f
#define REGEN_DELAY      4.0f
#define MAX_BLOCK_COUNT  999
#define OPT_SURVIVAL     "survival-mode"
#define MAX_FOOD         20
#define REGEN_MIN_FOOD   18
#define STARVE_DELAY     4.0f
#define FOOD_ICON        "\x0F"

/* How much hunger different actions cause (every 1.0 uses up half a food icon) */
#define EXHAUST_PER_SECOND (1.0f / 40.0f)
#define EXHAUST_PER_BLOCK  0.02f
#define EXHAUST_PER_BREAK  0.05f
#define EXHAUST_PER_HEAL   0.75f

cc_bool Survival_Enabled;
static cc_bool survival_active;
static int health;
static float fallStartY, airLeft, hurtCooldown, regenTimer;
static cc_bool wasFalling;
static cc_uint16 blockCounts[BLOCK_COUNT];
static RNGState survival_rnd;
static void Damage(int amount);
static int lastHealth = -1, lastHeldCount = -1, lastFood = -1;
static int food;
static float exhaustion, starveTimer;
static Vec3 lastPos;

enum ToolKind { TOOL_PICKAXE, TOOL_AXE, TOOL_SHOVEL, TOOL_SWORD, TOOL_KINDS };
#define MAX_TOOL_TIER 3
static const char* const toolNames[TOOL_KINDS]     = { "Picareta", "Machado", "Pa", "Espada" };
static const char* const tierNames[MAX_TOOL_TIER + 1] = { "", "madeira", "pedra", "ferro" };
static const int tierUses[MAX_TOOL_TIER + 1]       = { 0, 60, 132, 251 };
static cc_uint8 toolTier[TOOL_KINDS];
static int toolUses[TOOL_KINDS];
static BlockID lastHeld;


/*########################################################################################################################*
*----------------------------------------------------------HUD------------------------------------------------------------*
*#########################################################################################################################*/
static void UpdateHealthText(void) {
	cc_string str; char strBuffer[STRING_SIZE];
	int i;
	if (health == lastHealth && food == lastFood) return;
	lastHealth = health;
	lastFood   = food;
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

	String_AppendConst(&str, " ");
	for (i = 0; i < MAX_FOOD / 2; i++)
	{
		if (i * 2 == food - 1) {
			String_AppendConst(&str, "&e" FOOD_ICON); /* half food */
		} else if (i * 2 >= food) {
			String_AppendConst(&str, "&8" FOOD_ICON);
		} else {
			String_AppendConst(&str, "&6" FOOD_ICON);
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
	lastHealth = -1; lastHeldCount = -1; lastFood = -1;
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
	food         = MAX_FOOD;
	exhaustion   = 0;
	starveTimer  = 0;
	if (p) lastPos = p->Base.Position;
}

static void ResetTools(void) {
	Mem_Set(toolTier, 0, sizeof(toolTier));
	Mem_Set(toolUses, 0, sizeof(toolUses));
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
	ResetTools();

	LocalPlayer_DoRespawn(p);
	ResetPlayerState();
}

void Survival_Damage(int amount) { if (survival_active) Damage(amount); }

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

static void Exhaust(float amount) {
	exhaustion += amount;
	while (exhaustion >= 1.0f)
	{
		exhaustion -= 1.0f;
		if (food > 0) food--;
	}
}

static void UpdateHunger(struct Entity* e, float delta) {
	float dx = e->Position.x - lastPos.x;
	float dz = e->Position.z - lastPos.z;
	float dist = Math_SqrtF(dx * dx + dz * dz);
	lastPos = e->Position;

	/* Large jumps are teleports or respawns, not walking */
	if (dist < 2.0f) Exhaust(dist * EXHAUST_PER_BLOCK);
	Exhaust(delta * EXHAUST_PER_SECOND);

	if (food > 0) { starveTimer = 0; return; }
	/* Starving hurts, but (like normal difficulty) never kills */
	starveTimer += delta;
	if (starveTimer >= STARVE_DELAY) {
		starveTimer = 0;
		if (health > 1) Damage(1);
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

	UpdateHunger(e, delta);
	/* Only heal when well fed */
	if (health < MAX_HEALTH && food >= REGEN_MIN_FOOD) {
		regenTimer += delta;
		if (regenTimer >= REGEN_DELAY) { health++; regenTimer = 0; Exhaust(EXHAUST_PER_HEAL); }
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

/* Returns which kind of tool breaks the given block faster, or -1 if none does */
static int GetToolKind(BlockID block) {
	switch (block)
	{
	case BLOCK_STONE: case BLOCK_COBBLE: case BLOCK_GOLD_ORE: case BLOCK_IRON_ORE: case BLOCK_COAL_ORE:
	case BLOCK_GOLD: case BLOCK_IRON: case BLOCK_DOUBLE_SLAB: case BLOCK_SLAB: case BLOCK_BRICK:
	case BLOCK_MOSSY_ROCKS: case BLOCK_OBSIDIAN: case BLOCK_COBBLE_SLAB: case BLOCK_SANDSTONE:
	case BLOCK_ICE: case BLOCK_CERAMIC_TILE: case BLOCK_MAGMA: case BLOCK_PILLAR: case BLOCK_STONE_BRICK:
	case MBLOCK_RUBI_MINERIO: case MBLOCK_TITANIO_MINERIO: case MBLOCK_URANIO_MINERIO: case MBLOCK_OVO_MINERIO:
	case MBLOCK_RUBI: case MBLOCK_TITANIO: case MBLOCK_URANIO: case MBLOCK_PEDRA_CRISTAL:
		return TOOL_PICKAXE;
	case BLOCK_WOOD: case BLOCK_LOG: case BLOCK_BOOKSHELF: case BLOCK_CRATE:
		return TOOL_AXE;
	case BLOCK_DIRT: case BLOCK_GRASS: case BLOCK_SAND: case BLOCK_GRAVEL: case BLOCK_SNOW:
		return TOOL_SHOVEL;
	}
	return -1;
}

/* Returns the pickaxe tier needed for the given block to drop anything */
static int GetRequiredTier(BlockID block) {
	switch (block)
	{
	case BLOCK_GOLD_ORE: case BLOCK_IRON_ORE: case BLOCK_GOLD: case BLOCK_IRON:
	case MBLOCK_RUBI_MINERIO: case MBLOCK_RUBI:
		return 2;
	case MBLOCK_TITANIO_MINERIO: case MBLOCK_URANIO_MINERIO: case MBLOCK_TITANIO: case MBLOCK_URANIO:
		return 3;
	case BLOCK_OBSIDIAN:
		return 3;
	}
	return GetToolKind(block) == TOOL_PICKAXE ? 1 : 0;
}

static int GetToolTier(BlockID block) {
	int kind = GetToolKind(block);
	return kind >= 0 ? toolTier[kind] : 0;
}

/* Number of hits needed to break the given block with the player's current tools */
static int GetBreakHits(BlockID block) {
	int hardness = GetHardness(block);
	int tier     = GetToolTier(block);

	/* Stone-like blocks are slow to break by hand */
	if (tier < GetRequiredTier(block)) hardness *= 2;
	/* Better tools break blocks faster (wood 2x, stone 3x, iron 4x) */
	return max(1, (hardness + tier) / (tier + 1));
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
	hardness    = GetBreakHits(block);

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

static void WearToolKind(int kind) {
	cc_string name, tier;
	if (kind < 0 || !toolTier[kind]) return;
	if (--toolUses[kind] > 0) return;

	name = String_FromReadonly(toolNames[kind]);
	tier = String_FromReadonly(tierNames[toolTier[kind]]);
	Chat_Add2("&cSua %s de %s quebrou!", &name, &tier);
	toolTier[kind] = 0;
}

static void WearTool(BlockID block) { WearToolKind(GetToolKind(block)); }

int Survival_AttackDamage(void) {
	int tier;
	if (!survival_active) return 1;
	tier = toolTier[TOOL_SWORD];
	if (!tier) return 1;

	WearToolKind(TOOL_SWORD);
	return 3 + tier; /* wood 4, stone 5, iron 6 */
}

void Survival_AddBlocks(BlockID block, int count) {
	if (!survival_active || block == BLOCK_AIR) return;
	blockCounts[block] = min(blockCounts[block] + count, MAX_BLOCK_COUNT);
	AddToHotbar(block);
}

void Survival_OnBlockBroken(BlockID block) {
	BlockID drop;
	cc_bool harvested;
	if (!survival_active) return;

	Exhaust(EXHAUST_PER_BREAK);
	harvested = GetToolTier(block) >= GetRequiredTier(block);
	WearTool(block);
	if (!harvested) return;

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
*-------------------------------------------------------Crafting----------------------------------------------------------*
*#########################################################################################################################*/
struct Recipe {
	cc_uint8 count1; BlockID input1;
	cc_uint8 count2; BlockID input2; /* count2 is 0 when only one input is needed */
	cc_uint8 outCount; BlockID output;
};

static const struct Recipe recipes[] = {
	{ 1, BLOCK_LOG,       0, BLOCK_AIR,      4, BLOCK_WOOD         },
	{ 4, BLOCK_WOOD,      0, BLOCK_AIR,      1, BLOCK_CRATE        },
	{ 6, BLOCK_WOOD,      0, BLOCK_AIR,      1, BLOCK_BOOKSHELF    },
	{ 2, BLOCK_LEAVES,    0, BLOCK_AIR,      1, BLOCK_ROPE         },
	{ 1, BLOCK_SAND,      1, BLOCK_COAL_ORE, 4, BLOCK_GLASS        },
	{ 4, BLOCK_SAND,      0, BLOCK_AIR,      1, BLOCK_SANDSTONE    },
	{ 1, BLOCK_COBBLE,    1, BLOCK_COAL_ORE, 4, BLOCK_STONE        },
	{ 4, BLOCK_STONE,     0, BLOCK_AIR,      4, BLOCK_STONE_BRICK  },
	{ 3, BLOCK_STONE,     0, BLOCK_AIR,      6, BLOCK_SLAB         },
	{ 3, BLOCK_COBBLE,    0, BLOCK_AIR,      6, BLOCK_COBBLE_SLAB  },
	{ 4, BLOCK_COBBLE,    1, BLOCK_LEAVES,   4, BLOCK_MOSSY_ROCKS  },
	{ 4, BLOCK_DIRT,      1, BLOCK_COAL_ORE, 4, BLOCK_BRICK        },
	{ 4, BLOCK_IRON_ORE,  1, BLOCK_COAL_ORE, 1, BLOCK_IRON         },
	{ 4, BLOCK_GOLD_ORE,  1, BLOCK_COAL_ORE, 1, BLOCK_GOLD         },
	{ 4, BLOCK_SAND,      1, BLOCK_COAL_ORE, 1, BLOCK_TNT          },
	/* Minerios & Monstros (inspirado no OreSpawn) */
	{ 4, MBLOCK_RUBI_MINERIO,    0, BLOCK_AIR,              1, MBLOCK_RUBI            },
	{ 4, MBLOCK_TITANIO_MINERIO, 0, BLOCK_AIR,              1, MBLOCK_TITANIO         },
	{ 4, MBLOCK_URANIO_MINERIO,  0, BLOCK_AIR,              1, MBLOCK_URANIO          },
	{ 3, MBLOCK_TITANIO_MINERIO, 1, BLOCK_WOOD,             1, MITEM_ESPADA_TITANIO   },
	{ 2, MBLOCK_RUBI,            1, MITEM_ESPADA_TITANIO,   1, MITEM_ESPADA_SUPREMA   },
	{ 2, MBLOCK_URANIO,          1, MITEM_ESPADA_SUPREMA,   1, MITEM_BIG_BERTHA       },
	{ 3, MBLOCK_TITANIO,         0, BLOCK_AIR,              1, MITEM_ARMADURA_TITANIO },
	{ 1, MBLOCK_URANIO,          4, BLOCK_GOLD,             2, MBLOCK_PORTAL_UTOPIA   },
	{ 1, MBLOCK_RUBI,            4, BLOCK_GLASS,            2, MBLOCK_PORTAL_CRISTAL  },
};

struct FoodRecipe {
	const char* name; int value;
	cc_uint8 count1; BlockID input1;
	cc_uint8 count2; BlockID input2;
};

static const struct FoodRecipe foods[] = {
	{ "Sopa",              10, 1, BLOCK_BROWN_SHROOM, 1, BLOCK_RED_SHROOM },
	{ "Salada",             6, 1, BLOCK_DANDELION,    1, BLOCK_ROSE       },
	{ "Cogumelo",           3, 1, BLOCK_BROWN_SHROOM, 0, BLOCK_AIR        },
	{ "Cog. vermelho",      2, 1, BLOCK_RED_SHROOM,   0, BLOCK_AIR        },
};

struct ToolRecipe {
	cc_uint8 kind, tier;
	cc_uint8 count1; BlockID input1;
	cc_uint8 count2; BlockID input2;
};

static const struct ToolRecipe tools[] = {
	{ TOOL_PICKAXE, 1, 5, BLOCK_WOOD,   0, BLOCK_AIR  },
	{ TOOL_AXE,     1, 5, BLOCK_WOOD,   0, BLOCK_AIR  },
	{ TOOL_SHOVEL,  1, 3, BLOCK_WOOD,   0, BLOCK_AIR  },
	{ TOOL_SWORD,   1, 4, BLOCK_WOOD,   0, BLOCK_AIR  },
	{ TOOL_PICKAXE, 2, 3, BLOCK_COBBLE, 2, BLOCK_WOOD },
	{ TOOL_AXE,     2, 3, BLOCK_COBBLE, 2, BLOCK_WOOD },
	{ TOOL_SHOVEL,  2, 1, BLOCK_COBBLE, 2, BLOCK_WOOD },
	{ TOOL_SWORD,   2, 2, BLOCK_COBBLE, 1, BLOCK_WOOD },
	{ TOOL_PICKAXE, 3, 3, BLOCK_IRON,   2, BLOCK_WOOD },
	{ TOOL_AXE,     3, 3, BLOCK_IRON,   2, BLOCK_WOOD },
	{ TOOL_SHOVEL,  3, 1, BLOCK_IRON,   2, BLOCK_WOOD },
	{ TOOL_SWORD,   3, 2, BLOCK_IRON,   1, BLOCK_WOOD },
};

#define FOODS_COUNT Array_Elems(foods)
#define TOOLS_COUNT Array_Elems(tools)
int Survival_RecipesCount(void) { return FOODS_COUNT + TOOLS_COUNT + Array_Elems(recipes); }

static cc_bool HasInputs(int count1, BlockID input1, int count2, BlockID input2) {
	if (blockCounts[input1] < count1) return false;
	return !count2 || blockCounts[input2] >= count2;
}

static void UseInputs(int count1, BlockID input1, int count2, BlockID input2) {
	blockCounts[input1] -= count1;
	if (count2) blockCounts[input2] -= count2;
}

static cc_bool CanCraft(const struct Recipe* r) {
	if (!HasInputs(r->count1, r->input1, r->count2, r->input2)) return false;
	return blockCounts[r->output] + r->outCount <= MAX_BLOCK_COUNT;
}

/* Short Portuguese names, so recipes fit on the 3DS screen */
static cc_string GetShortName(BlockID block) {
	const char* name = NULL;
	switch (block)
	{
	case BLOCK_STONE:        name = "Pedra";      break;
	case BLOCK_DIRT:         name = "Terra";      break;
	case BLOCK_COBBLE:       name = "Pedregulho"; break;
	case BLOCK_WOOD:         name = "Tabua";      break;
	case BLOCK_SAND:         name = "Areia";      break;
	case BLOCK_GOLD_ORE:     name = "Min. ouro";  break;
	case BLOCK_IRON_ORE:     name = "Min. ferro"; break;
	case BLOCK_COAL_ORE:     name = "Carvao";     break;
	case BLOCK_LOG:          name = "Tronco";     break;
	case BLOCK_LEAVES:       name = "Folhas";     break;
	case BLOCK_GLASS:        name = "Vidro";      break;
	case BLOCK_DANDELION:    name = "Flor";       break;
	case BLOCK_ROSE:         name = "Rosa";       break;
	case BLOCK_BROWN_SHROOM: name = "Cogumelo";   break;
	case BLOCK_RED_SHROOM:   name = "Cog. verm."; break;
	case BLOCK_GOLD:         name = "Ouro";       break;
	case BLOCK_IRON:         name = "Ferro";      break;
	case BLOCK_SLAB:         name = "Laje";       break;
	case BLOCK_BRICK:        name = "Tijolo";     break;
	case BLOCK_BOOKSHELF:    name = "Estante";    break;
	case BLOCK_MOSSY_ROCKS:  name = "Musgosa";    break;
	case BLOCK_COBBLE_SLAB:  name = "Laje pedreg."; break;
	case BLOCK_ROPE:         name = "Corda";      break;
	case BLOCK_SANDSTONE:    name = "Arenito";    break;
	case BLOCK_CRATE:        name = "Caixote";    break;
	case BLOCK_STONE_BRICK:  name = "Tij. pedra"; break;
	}
	return name ? String_FromReadonly(name) : Block_UNSAFE_GetName(block);
}

/* Ingredients the player has enough of are shown in green, others in red */
static void AppendIngredient(cc_string* str, int count, BlockID block) {
	cc_string name = GetShortName(block);
	String_AppendConst(str, blockCounts[block] >= count ? "&a" : "&c");
	if (count > 1) String_Format1(str, "%i ", &count);
	String_AppendString(str, &name);
}

static void AppendIngredients(cc_string* str, int count1, BlockID input1, int count2, BlockID input2) {
	AppendIngredient(str, count1, input1);
	if (!count2) return;

	String_AppendConst(str, "&7, ");
	AppendIngredient(str, count2, input2);
}

static void DescribeFood(const struct FoodRecipe* f, cc_string* str) {
	cc_bool can = food < MAX_FOOD && HasInputs(f->count1, f->input1, f->count2, f->input2);
	String_Format3(str, "%c%c &6+%i&7: ", can ? "&f" : "&7", f->name, &f->value);
	AppendIngredients(str, f->count1, f->input1, f->count2, f->input2);
}

static void DescribeTool(const struct ToolRecipe* t, cc_string* str) {
	cc_bool owned = toolTier[t->kind] >= t->tier;
	cc_bool can   = !owned && HasInputs(t->count1, t->input1, t->count2, t->input2);
	String_Format3(str, "%c%c %c", can ? "&f" : "&7", toolNames[t->kind], tierNames[t->tier]);

	if (owned) {
		String_AppendConst(str, " &a(tem)");
	} else {
		String_AppendConst(str, "&7: ");
		AppendIngredients(str, t->count1, t->input1, t->count2, t->input2);
	}
}

void Survival_DescribeRecipe(int i, cc_string* str) {
	const struct Recipe* r;
	cc_string name;
	int outCount;

	if (i < FOODS_COUNT) { DescribeFood(&foods[i], str); return; }
	i -= FOODS_COUNT;
	if (i < TOOLS_COUNT) { DescribeTool(&tools[i], str); return; }
	i -= TOOLS_COUNT;

	r        = &recipes[i];
	name     = GetShortName(r->output);
	outCount = r->outCount;
	String_Format3(str, "%c%i %s&7: ", CanCraft(r) ? "&f" : "&7", &outCount, &name);
	AppendIngredients(str, r->count1, r->input1, r->count2, r->input2);
}

static cc_bool Eat(const struct FoodRecipe* f) {
	if (food >= MAX_FOOD || !HasInputs(f->count1, f->input1, f->count2, f->input2)) return false;

	UseInputs(f->count1, f->input1, f->count2, f->input2);
	food = min(food + f->value, MAX_FOOD);
	return true;
}

static cc_bool MakeTool(const struct ToolRecipe* t) {
	cc_string name, tier;
	if (toolTier[t->kind] >= t->tier || !HasInputs(t->count1, t->input1, t->count2, t->input2)) return false;

	UseInputs(t->count1, t->input1, t->count2, t->input2);
	toolTier[t->kind] = t->tier;
	toolUses[t->kind] = tierUses[t->tier];

	name = String_FromReadonly(toolNames[t->kind]);
	tier = String_FromReadonly(tierNames[t->tier]);
	Chat_Add2("&aVoce fez: %s de %s", &name, &tier);
	return true;
}

cc_bool Survival_Craft(int i) {
	const struct Recipe* r;
	if (!survival_active) return false;

	if (i < FOODS_COUNT) return Eat(&foods[i]);
	i -= FOODS_COUNT;
	if (i < TOOLS_COUNT) return MakeTool(&tools[i]);
	i -= TOOLS_COUNT;

	r = &recipes[i];
	if (!CanCraft(r)) return false;
	UseInputs(r->count1, r->input1, r->count2, r->input2);
	blockCounts[r->output] += r->outCount;
	AddToHotbar(r->output);
	return true;
}

cc_bool Survival_IsActive(void) { return survival_active; }


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
	ResetTools();
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

/*########################################################################################################################*
*------------------------------------------------------Saving/Loading-----------------------------------------------------*
*#########################################################################################################################*/
static cc_bool loadedPending, loadedHasTools;
static int loadedHealth, loadedFood = -1;
static cc_uint8 loadedTools[SURVIVAL_TOOLS_SIZE];
static cc_uint16 loadedCounts[BLOCK_COUNT];

int Survival_GetHealth(void) { return health; }
int Survival_GetFood(void)   { return food; }

void Survival_GetTools(cc_uint8* data) {
	int i;
	for (i = 0; i < TOOL_KINDS; i++)
	{
		data[i * 3 + 0] = toolTier[i];
		data[i * 3 + 1] = (cc_uint8)(toolUses[i] >> 8);
		data[i * 3 + 2] = (cc_uint8)(toolUses[i]);
	}
}

void Survival_LoadFood(int value) {
	loadedFood    = value;
	loadedPending = true;
}

void Survival_LoadTools(const cc_uint8* data, int len) {
	if (len < SURVIVAL_TOOLS_SIZE) return;
	Mem_Copy(loadedTools, data, SURVIVAL_TOOLS_SIZE);
	loadedHasTools = true;
	loadedPending  = true;
}

void Survival_GetCounts(cc_uint8* data) {
	int i;
	for (i = 0; i < BLOCK_COUNT; i++)
	{
		data[i * 2 + 0] = (cc_uint8)(blockCounts[i] >> 8);
		data[i * 2 + 1] = (cc_uint8)(blockCounts[i]);
	}
}

void Survival_LoadHealth(int value) {
	loadedHealth  = value;
	loadedPending = true;
}

void Survival_LoadCounts(const cc_uint8* data, int len) {
	int i, count;
	for (i = 0; i < BLOCK_COUNT && (i * 2 + 1) < len; i++)
	{
		count = (data[i * 2] << 8) | data[i * 2 + 1];
		loadedCounts[i] = min(count, MAX_BLOCK_COUNT);
	}
	loadedPending = true;
}

static void ApplyLoadedState(void) {
	int i;
	if (!loadedPending) return;
	loadedPending = false;
	if (!survival_active) return;

	if (loadedHealth > 0) health = min(loadedHealth, MAX_HEALTH);
	/* Worlds saved before hunger existed have no food value */
	if (loadedFood  >= 0) food   = min(loadedFood,   MAX_FOOD);

	for (i = 0; loadedHasTools && i < TOOL_KINDS; i++)
	{
		toolTier[i] = min(loadedTools[i * 3], MAX_TOOL_TIER);
		toolUses[i] = (loadedTools[i * 3 + 1] << 8) | loadedTools[i * 3 + 2];
		if (!toolUses[i]) toolTier[i] = 0;
	}
	Mem_Copy(blockCounts, loadedCounts, sizeof(blockCounts));
	blockCounts[BLOCK_AIR] = 0;

	for (i = BLOCK_AIR + 1; i < BLOCK_COUNT; i++)
	{
		if (blockCounts[i]) AddToHotbar((BlockID)i);
	}
}


static void OnInit(void) {
	Survival_Enabled = Options_GetBool(OPT_SURVIVAL, true);
	Random_SeedFromCurrentTime(&survival_rnd);
	Commands_Register(&SurvivalCommand);
	ScheduledTask2_Add(&survival_task);
}

static void OnNewMap(void) {
	/* Forget state from a previous world that was loaded but not applied */
	loadedPending  = false;
	loadedHasTools = false;
	loadedHealth   = 0;
	loadedFood     = -1;
	Mem_Set(loadedCounts, 0, sizeof(loadedCounts));
}

static void OnNewMapLoaded(void) {
	/* Restart with full health and an empty inventory, unless the world saved them */
	survival_active = false;
	/* Re-read the option so a mod selection menu can change it between worlds */
	Survival_Enabled = Options_GetBool(OPT_SURVIVAL, true);
	UpdateActive();
	ApplyLoadedState();

	/* Loading a world clears the HUD lines, so force them to be redrawn */
	lastHealth = -1; lastHeldCount = -1; lastFood = -1;
}

struct IGameComponent Survival_Component = {
	OnInit,        /* Init  */
	NULL,          /* Free  */
	NULL,          /* Reset */
	OnNewMap,      /* OnNewMap */
	OnNewMapLoaded /* OnNewMapLoaded */
};

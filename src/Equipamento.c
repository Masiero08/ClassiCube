#include "Equipamento.h"
#include "Survival.h"
#include "Minerios.h"
#include "LateGame.h"
#include "Paisagem.h"
#include "Block.h"
#include "BlockID.h"
#include "Bitmap.h"
#include "Chat.h"
#include "Entity.h"
#include "Event.h"
#include "ExtMath.h"
#include "Game.h"
#include "Inventory.h"
#include "Picking.h"
#include "Server.h"
#include "String_.h"
#include "TexturePack.h"
#include "World.h"
#include "Funcs.h"
#include "Utils.h"
#include "Platform.h"
#include "Lighting.h"
#include "Audio.h"
#include "Equipamento_tex.h"

/* Chests to store items, a furnace that smelts ores and cooks food over time while burning */
/*  fuel, metal bars, and four pieces of armour in five tiers, inspired by Minecraft. */
/* All textures are original art (see misc/equipamento/gen_textures.py), drawn into */
/*  unused tiles of the terrain atlas, so they work with any texture pack. */

/* First terrain atlas tile used (rows 13 and 14 are empty in the default texture pack) */
#define EQUIP_TILE_BASE 208
#define T(tex) (EQUIP_TILE_BASE + ETEX_##tex)
/* Top of the (unlit) furnace, from LateGame.c */
#define FURNACE_TOP_TILE (128 + 17)

struct EquipBlockDef {
	const char* name;
	cc_uint8 top, side, bottom;
	cc_uint8 draw, collide, sound, brightness;
};

static const struct EquipBlockDef equip_defs[EBLOCK_FIM - EBLOCK_BAU] = {
/*NAME                     TOP                  SIDE                 BOTTOM               DRAW_MODE    COLLIDE_MODE   SOUND        BRIGHTNESS */
{ "Bau",                   T(BAU_TOP),          T(BAU_SIDE),         T(BAU_TOP),          DRAW_OPAQUE, COLLIDE_SOLID, SOUND_WOOD,  0 },
{ "Fornalha acesa",        FURNACE_TOP_TILE,    T(FORNALHA_ACESA),   FURNACE_TOP_TILE,    DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE, FANCY_LIGHTING_MAX_LEVEL * 3 / 4 },
{ "Barra de ferro",        T(BARRA_FERRO),      T(BARRA_FERRO),      T(BARRA_FERRO),      DRAW_SPRITE, COLLIDE_NONE,  SOUND_METAL, 0 },
{ "Barra de ouro",         T(BARRA_OURO),       T(BARRA_OURO),       T(BARRA_OURO),       DRAW_SPRITE, COLLIDE_NONE,  SOUND_METAL, 0 },
{ "Barra de titanio",      T(BARRA_TITANIO),    T(BARRA_TITANIO),    T(BARRA_TITANIO),    DRAW_SPRITE, COLLIDE_NONE,  SOUND_METAL, 0 },
{ "Capacete de la",        T(CAPACETE_LA),      T(CAPACETE_LA),      T(CAPACETE_LA),      DRAW_SPRITE, COLLIDE_NONE,  SOUND_CLOTH, 0 },
{ "Peitoral de la",        T(PEITORAL_LA),      T(PEITORAL_LA),      T(PEITORAL_LA),      DRAW_SPRITE, COLLIDE_NONE,  SOUND_CLOTH, 0 },
{ "Calca de la",           T(CALCA_LA),         T(CALCA_LA),         T(CALCA_LA),         DRAW_SPRITE, COLLIDE_NONE,  SOUND_CLOTH, 0 },
{ "Botas de la",           T(BOTAS_LA),         T(BOTAS_LA),         T(BOTAS_LA),         DRAW_SPRITE, COLLIDE_NONE,  SOUND_CLOTH, 0 },
{ "Capacete de ouro",      T(CAPACETE_OURO),    T(CAPACETE_OURO),    T(CAPACETE_OURO),    DRAW_SPRITE, COLLIDE_NONE,  SOUND_METAL, 0 },
{ "Peitoral de ouro",      T(PEITORAL_OURO),    T(PEITORAL_OURO),    T(PEITORAL_OURO),    DRAW_SPRITE, COLLIDE_NONE,  SOUND_METAL, 0 },
{ "Calca de ouro",         T(CALCA_OURO),       T(CALCA_OURO),       T(CALCA_OURO),       DRAW_SPRITE, COLLIDE_NONE,  SOUND_METAL, 0 },
{ "Botas de ouro",         T(BOTAS_OURO),       T(BOTAS_OURO),       T(BOTAS_OURO),       DRAW_SPRITE, COLLIDE_NONE,  SOUND_METAL, 0 },
{ "Capacete de ferro",     T(CAPACETE_FERRO),   T(CAPACETE_FERRO),   T(CAPACETE_FERRO),   DRAW_SPRITE, COLLIDE_NONE,  SOUND_METAL, 0 },
{ "Peitoral de ferro",     T(PEITORAL_FERRO),   T(PEITORAL_FERRO),   T(PEITORAL_FERRO),   DRAW_SPRITE, COLLIDE_NONE,  SOUND_METAL, 0 },
{ "Calca de ferro",        T(CALCA_FERRO),      T(CALCA_FERRO),      T(CALCA_FERRO),      DRAW_SPRITE, COLLIDE_NONE,  SOUND_METAL, 0 },
{ "Botas de ferro",        T(BOTAS_FERRO),      T(BOTAS_FERRO),      T(BOTAS_FERRO),      DRAW_SPRITE, COLLIDE_NONE,  SOUND_METAL, 0 },
{ "Capacete de diamante",  T(CAPACETE_DIAMANTE),T(CAPACETE_DIAMANTE),T(CAPACETE_DIAMANTE),DRAW_SPRITE, COLLIDE_NONE,  SOUND_GLASS, 0 },
{ "Peitoral de diamante",  T(PEITORAL_DIAMANTE),T(PEITORAL_DIAMANTE),T(PEITORAL_DIAMANTE),DRAW_SPRITE, COLLIDE_NONE,  SOUND_GLASS, 0 },
{ "Calca de diamante",     T(CALCA_DIAMANTE),   T(CALCA_DIAMANTE),   T(CALCA_DIAMANTE),   DRAW_SPRITE, COLLIDE_NONE,  SOUND_GLASS, 0 },
{ "Botas de diamante",     T(BOTAS_DIAMANTE),   T(BOTAS_DIAMANTE),   T(BOTAS_DIAMANTE),   DRAW_SPRITE, COLLIDE_NONE,  SOUND_GLASS, 0 },
{ "Capacete de titanio",   T(CAPACETE_TITANIO), T(CAPACETE_TITANIO), T(CAPACETE_TITANIO), DRAW_SPRITE, COLLIDE_NONE,  SOUND_METAL, 0 },
{ "Calca de titanio",      T(CALCA_TITANIO),    T(CALCA_TITANIO),    T(CALCA_TITANIO),    DRAW_SPRITE, COLLIDE_NONE,  SOUND_METAL, 0 },
{ "Botas de titanio",      T(BOTAS_TITANIO),    T(BOTAS_TITANIO),    T(BOTAS_TITANIO),    DRAW_SPRITE, COLLIDE_NONE,  SOUND_METAL, 0 },
};


/*########################################################################################################################*
*-------------------------------------------------------Textures----------------------------------------------------------*
*#########################################################################################################################*/
/* Whether every pixel of the given atlas tile is the same colour (i.e. the tile is unused) */
static cc_bool IsTileBlank(struct Bitmap* bmp, int x, int y, int size) {
	BitmapCol first = Bitmap_GetPixel(bmp, x, y);
	int i, j;

	for (j = 0; j < size; j++)
	{
		BitmapCol* row = Bitmap_GetRow(bmp, y + j) + x;
		for (i = 0; i < size; i++)
		{
			if (row[i] != first) return false;
		}
	}
	return true;
}

static void DrawTile(struct Bitmap* bmp, int x, int y, int size, const cc_uint8* src) {
	int i, j, srcX, srcY;
	const cc_uint8* px;

	for (j = 0; j < size; j++)
	{
		BitmapCol* row = Bitmap_GetRow(bmp, y + j) + x;
		srcY = j * EQUIP_TEX_SIZE / size;

		for (i = 0; i < size; i++)
		{
			srcX   = i * EQUIP_TEX_SIZE / size;
			px     = src + (srcY * EQUIP_TEX_SIZE + srcX) * 4;
			row[i] = BitmapCol_Make(px[0], px[1], px[2], px[3]);
		}
	}
}

void Equip_PatchAtlas(struct Bitmap* bmp) {
	int size = bmp->width / ATLAS2D_TILES_PER_ROW;
	int rows = size ? bmp->height / size : 0;
	int i, tile, x, y;

	for (i = 0; i < EQUIP_TEX_COUNT; i++)
	{
		tile = EQUIP_TILE_BASE + i;
		x    = Atlas2D_TileX(tile) * size;
		y    = Atlas2D_TileY(tile);
		if (y >= rows) return;
		y   *= size;

		/* Don't overwrite textures that a texture pack provides */
		if (!IsTileBlank(bmp, x, y, size)) continue;
		DrawTile(bmp, x, y, size, equip_texData[i]);
	}
}


/*########################################################################################################################*
*--------------------------------------------------------Blocks-----------------------------------------------------------*
*#########################################################################################################################*/
static void DefineBlock(BlockID block, const struct EquipBlockDef* def) {
	cc_string name = String_FromReadonly(def->name);

	Block_ResetProps(block);
	Block_SetName(block, &name);

	Block_Tex(block, FACE_YMAX) = def->top;
	Block_SetSide(def->side, block);
	Block_Tex(block, FACE_YMIN) = def->bottom;

	Blocks.Collide[block]     = def->collide;
	Blocks.Draw[block]        = def->draw;
	Blocks.BlocksLight[block] = def->draw == DRAW_OPAQUE;
	Blocks.Brightness[block]  = def->brightness;
	Blocks.DigSounds[block]   = def->sound;
	Blocks.StepSounds[block]  = def->sound == SOUND_GLASS ? SOUND_STONE : def->sound;

	/* The chest is a little smaller than a full block, like in Minecraft */
	if (block == EBLOCK_BAU) {
		Blocks.MinBB[block].x = 1.0f / 16.0f; Blocks.MinBB[block].z = 1.0f / 16.0f;
		Blocks.MaxBB[block].x = 15.0f / 16.0f; Blocks.MaxBB[block].z = 15.0f / 16.0f;
		Blocks.MaxBB[block].y = 14.0f / 16.0f;
		Blocks.BlocksLight[block] = false;
	}
	Block_DefineCustom(block, true);
}

static void DefineBlocks(void) {
	BlockID block;
	int i;
	/* Multiplayer servers decide which blocks exist */
	if (!Server.IsSinglePlayer || !Game_AllowCustomBlocks) return;

	for (i = 0; i < Array_Elems(equip_defs); i++)
	{
		block = (BlockID)(EBLOCK_BAU + i);
		if (block > BLOCK_MAX_DEFINED) break;
		/* Saved worlds may already define the block (possibly differently) */
		if (Block_IsCustomDefined(block)) continue;
		DefineBlock(block, &equip_defs[i]);
	}
}


/*########################################################################################################################*
*--------------------------------------------------------Armour-----------------------------------------------------------*
*#########################################################################################################################*/
static const BlockID armorItems[ARMOR_TIERS][ARMOR_PIECES] = {
	{ BLOCK_AIR,               BLOCK_AIR,               BLOCK_AIR,            BLOCK_AIR            },
	{ EITEM_CAPACETE_LA,       EITEM_PEITORAL_LA,       EITEM_CALCA_LA,       EITEM_BOTAS_LA       },
	{ EITEM_CAPACETE_OURO,     EITEM_PEITORAL_OURO,     EITEM_CALCA_OURO,     EITEM_BOTAS_OURO     },
	{ EITEM_CAPACETE_FERRO,    EITEM_PEITORAL_FERRO,    EITEM_CALCA_FERRO,    EITEM_BOTAS_FERRO    },
	{ EITEM_CAPACETE_DIAMANTE, EITEM_PEITORAL_DIAMANTE, EITEM_CALCA_DIAMANTE, EITEM_BOTAS_DIAMANTE },
	{ EITEM_CAPACETE_TITANIO,  MITEM_ARMADURA_TITANIO,  EITEM_CALCA_TITANIO,  EITEM_BOTAS_TITANIO  },
};
/* Armour points of each piece (like Minecraft: each point blocks 4% of damage) */
static const cc_uint8 armorPoints[ARMOR_TIERS][ARMOR_PIECES] = {
	{ 0, 0, 0, 0 }, { 1, 3, 2, 1 }, { 2, 5, 3, 1 }, { 2, 6, 5, 2 }, { 3, 8, 6, 3 }, { 3, 9, 7, 3 },
};
/* How many hits each piece takes before it breaks */
static const cc_uint16 armorUses[ARMOR_TIERS] = { 0, 60, 80, 170, 360, 500 };
#define MAX_ARMOR_PERCENT 80

static BlockID worn[ARMOR_PIECES];
static int wornUses[ARMOR_PIECES];
static float damageCarry;

static void FindArmor(BlockID block, int* tier, int* piece) {
	int t, p;
	*tier = ARMOR_TIER_NONE; *piece = -1;
	if (block == BLOCK_AIR) return;

	for (t = 1; t < ARMOR_TIERS; t++)
		for (p = 0; p < ARMOR_PIECES; p++)
		{
			if (armorItems[t][p] != block) continue;
			*tier = t; *piece = p; return;
		}
}

int Equip_ArmorPiece(BlockID block) { int t, p; FindArmor(block, &t, &p); return p; }
int Equip_ArmorTier(BlockID block)  { int t, p; FindArmor(block, &t, &p); return t; }
BlockID Equip_ArmorItem(int tier, int piece) { return armorItems[tier][piece]; }
BlockID Equip_Worn(int piece) { return worn[piece]; }
int Equip_WornUses(int piece) { return wornUses[piece]; }
int Equip_MaxUses(BlockID block) { return armorUses[Equip_ArmorTier(block)]; }

cc_bool Equip_Wear(BlockID block) {
	cc_string name;
	int tier, piece;
	FindArmor(block, &tier, &piece);
	if (piece < 0 || Survival_GetCount(block) <= 0) return false;

	/* The piece worn before goes back into the player's items */
	if (worn[piece] != BLOCK_AIR) Equip_TakeOff(piece);
	Survival_TakeBlocks(block, 1);
	worn[piece]     = block;
	wornUses[piece] = armorUses[tier];

	name = Block_UNSAFE_GetName(block);
	Chat_Add1("&bVoce vestiu: %s", &name);
	return true;
}

cc_bool Equip_TakeOff(int piece) {
	BlockID block = worn[piece];
	if (block == BLOCK_AIR) return false;
	if (Survival_GetCount(block) >= SURVIVAL_MAX_COUNT) return false;

	/* NOTE: a worn out piece comes back as new, which is simpler than storing wear per item */
	Survival_AddBlocks(block, 1);
	worn[piece]     = BLOCK_AIR;
	wornUses[piece] = 0;
	return true;
}

int Equip_Defense(void) {
	int p, points = 0;
	for (p = 0; p < ARMOR_PIECES; p++)
	{
		if (worn[p] != BLOCK_AIR) points += armorPoints[Equip_ArmorTier(worn[p])][p];
	}
	return points;
}

int Equip_DefensePercent(void) { return min(Equip_Defense() * 4, MAX_ARMOR_PERCENT); }

static void WearArmor(void) {
	cc_string name;
	int p;
	for (p = 0; p < ARMOR_PIECES; p++)
	{
		if (worn[p] == BLOCK_AIR || --wornUses[p] > 0) continue;
		name = Block_UNSAFE_GetName(worn[p]);
		Chat_Add1("&cSeu %s quebrou!", &name);
		worn[p] = BLOCK_AIR;
	}
}

int Equip_ReduceDamage(int amount) {
	float dmg;
	int percent = Equip_DefensePercent();
	if (amount <= 0 || !percent) return amount;

	WearArmor();
	/* Keep the fractions, so that weak hits still hurt now and then */
	dmg = amount * (100 - percent) / 100.0f + damageCarry;
	amount      = (int)dmg;
	damageCarry = dmg - amount;
	return amount;
}

void Equip_ClearArmor(void) {
	Mem_Set(worn,     0, sizeof(worn));
	Mem_Set(wornUses, 0, sizeof(wornUses));
	damageCarry = 0;
}

void Equip_SaveArmor(cc_uint8* data) {
	int p;
	for (p = 0; p < ARMOR_PIECES; p++, data += 4)
	{
		Mem_WriteU16_BE(data,     worn[p]);
		Mem_WriteU16_BE(data + 2, wornUses[p]);
	}
}

void Equip_LoadArmor(const cc_uint8* data, int len) {
	int p;
	Equip_ClearArmor();
	for (p = 0; p < ARMOR_PIECES && len >= (p + 1) * 4; p++)
	{
		BlockID block = (BlockID)Mem_ReadU16_BE(data + p * 4);
		if (Equip_ArmorPiece(block) != p) continue;
		worn[p]     = block;
		wornUses[p] = Mem_ReadU16_BE(data + p * 4 + 2);
		if (!wornUses[p]) worn[p] = BLOCK_AIR;
	}
}


/*########################################################################################################################*
*-------------------------------------------------------Storage-----------------------------------------------------------*
*#########################################################################################################################*/
static struct EquipBau baus[EQUIP_MAX_BAUS];
static int bausCount;
static struct EquipFornalha fornalhas[EQUIP_MAX_FORNALHAS];
static int fornalhasCount;

int Equip_TakeFromSlot(struct EquipSlot* slot, int count) {
	int room;
	if (slot->block == BLOCK_AIR || !slot->count) return 0;

	room  = SURVIVAL_MAX_COUNT - Survival_GetCount(slot->block);
	count = min(count, min(room, slot->count));
	if (count <= 0) return 0;

	Survival_AddBlocks(slot->block, count);
	slot->count -= count;
	if (!slot->count) slot->block = BLOCK_AIR;
	return count;
}

int Equip_PutInSlot(struct EquipSlot* slot, BlockID block, int count) {
	if (slot->count && slot->block != block) return 0;
	count = min(count, min(Survival_GetCount(block), BAU_STACK - slot->count));
	if (count <= 0) return 0;

	Survival_TakeBlocks(block, count);
	slot->block  = block;
	slot->count += count;
	return count;
}

static void GiveBackSlots(struct EquipSlot* slots, int count) {
	int i;
	for (i = 0; i < count; i++)
	{
		Equip_TakeFromSlot(&slots[i], slots[i].count);
	}
}

static int FindBau(int x, int y, int z) {
	int i;
	for (i = 0; i < bausCount; i++)
	{
		if (baus[i].x == x && baus[i].y == y && baus[i].z == z) return i;
	}
	return -1;
}

static int FindFornalha(int x, int y, int z) {
	int i;
	for (i = 0; i < fornalhasCount; i++)
	{
		if (fornalhas[i].x == x && fornalhas[i].y == y && fornalhas[i].z == z) return i;
	}
	return -1;
}

static struct EquipBau* GetBau(int x, int y, int z) {
	struct EquipBau* b;
	int i = FindBau(x, y, z);
	if (i >= 0) return &baus[i];
	if (bausCount >= EQUIP_MAX_BAUS) return NULL;

	b = &baus[bausCount++];
	Mem_Set(b, 0, sizeof(*b));
	b->x = x; b->y = y; b->z = z;
	return b;
}

static struct EquipFornalha* GetFornalha(int x, int y, int z) {
	struct EquipFornalha* f;
	int i = FindFornalha(x, y, z);
	if (i >= 0) return &fornalhas[i];
	if (fornalhasCount >= EQUIP_MAX_FORNALHAS) return NULL;

	f = &fornalhas[fornalhasCount++];
	Mem_Set(f, 0, sizeof(*f));
	f->x = x; f->y = y; f->z = z;
	return f;
}

static cc_bool IsFurnace(BlockID b) { return b == LBLOCK_FURNACE || b == EBLOCK_FORNALHA_ACESA; }

/* When a chest or furnace is broken, the player gets back what was inside */
static void OnBlockChanged(void* obj, IVec3 coords, BlockID old, BlockID now) {
	int i;
	if (old == EBLOCK_BAU && now != EBLOCK_BAU && (i = FindBau(coords.x, coords.y, coords.z)) >= 0) {
		GiveBackSlots(baus[i].slots, BAU_SLOTS);
		baus[i] = baus[--bausCount];
	}
	if (IsFurnace(old) && !IsFurnace(now) && (i = FindFornalha(coords.x, coords.y, coords.z)) >= 0) {
		GiveBackSlots(fornalhas[i].slots, FORNALHA_SLOTS);
		fornalhas[i] = fornalhas[--fornalhasCount];
	}
}

/* Forgets chests and furnaces that were destroyed some other way (e.g. explosions) */
static void PruneStorage(void) {
	int i;
	for (i = bausCount - 1; i >= 0; i--)
	{
		struct EquipBau* b = &baus[i];
		if (World_Contains(b->x, b->y, b->z) && World_GetBlock(b->x, b->y, b->z) == EBLOCK_BAU) continue;
		baus[i] = baus[--bausCount];
	}
	for (i = fornalhasCount - 1; i >= 0; i--)
	{
		struct EquipFornalha* f = &fornalhas[i];
		if (World_Contains(f->x, f->y, f->z) && IsFurnace(World_GetBlock(f->x, f->y, f->z))) continue;
		fornalhas[i] = fornalhas[--fornalhasCount];
	}
}

static void WriteSlot(cc_uint8* data, const struct EquipSlot* slot) {
	Mem_WriteU16_BE(data,     slot->block);
	Mem_WriteU16_BE(data + 2, slot->count);
}

static void ReadSlot(const cc_uint8* data, struct EquipSlot* slot, int max) {
	slot->block = (BlockID)Mem_ReadU16_BE(data);
	slot->count = Mem_ReadU16_BE(data + 2);
	if (slot->block >= BLOCK_COUNT) slot->block = BLOCK_AIR;
	slot->count = min(slot->count, max);
	if (slot->block == BLOCK_AIR || !slot->count) { slot->block = BLOCK_AIR; slot->count = 0; }
}

int Equip_SaveChests(cc_uint8* data) {
	cc_uint8* cur = data;
	int i, j;
	PruneStorage();

	for (i = 0; i < bausCount; i++)
	{
		Mem_WriteU16_BE(cur, baus[i].x); Mem_WriteU16_BE(cur + 2, baus[i].y); Mem_WriteU16_BE(cur + 4, baus[i].z);
		cur += 6;
		for (j = 0; j < BAU_SLOTS; j++, cur += 4) WriteSlot(cur, &baus[i].slots[j]);
	}
	return (int)(cur - data);
}

void Equip_LoadChests(const cc_uint8* data, int len) {
	struct EquipBau* b;
	int j;
	bausCount = 0;

	for (; len >= EQUIP_BAU_SAVE_SIZE && bausCount < EQUIP_MAX_BAUS; len -= EQUIP_BAU_SAVE_SIZE)
	{
		b = &baus[bausCount++];
		b->x = Mem_ReadU16_BE(data); b->y = Mem_ReadU16_BE(data + 2); b->z = Mem_ReadU16_BE(data + 4);
		data += 6;
		for (j = 0; j < BAU_SLOTS; j++, data += 4) ReadSlot(data, &b->slots[j], BAU_STACK);
	}
}

int Equip_SaveFurnaces(cc_uint8* data) {
	cc_uint8* cur = data;
	struct EquipFornalha* f;
	int i, j;
	PruneStorage();

	for (i = 0; i < fornalhasCount; i++)
	{
		f = &fornalhas[i];
		Mem_WriteU16_BE(cur, f->x); Mem_WriteU16_BE(cur + 2, f->y); Mem_WriteU16_BE(cur + 4, f->z);
		cur += 6;
		for (j = 0; j < FORNALHA_SLOTS; j++, cur += 4) WriteSlot(cur, &f->slots[j]);
		/* Times are stored in tenths of a second */
		Mem_WriteU16_BE(cur,     (cc_uint16)(f->burnLeft * 10));
		Mem_WriteU16_BE(cur + 2, (cc_uint16)(f->burnMax  * 10));
		Mem_WriteU16_BE(cur + 4, (cc_uint16)(f->cook     * 10));
		cur += 6;
	}
	return (int)(cur - data);
}

void Equip_LoadFurnaces(const cc_uint8* data, int len) {
	struct EquipFornalha* f;
	int j;
	fornalhasCount = 0;

	for (; len >= EQUIP_FORNALHA_SAVE_SIZE && fornalhasCount < EQUIP_MAX_FORNALHAS; len -= EQUIP_FORNALHA_SAVE_SIZE)
	{
		f = &fornalhas[fornalhasCount++];
		f->x = Mem_ReadU16_BE(data); f->y = Mem_ReadU16_BE(data + 2); f->z = Mem_ReadU16_BE(data + 4);
		data += 6;
		for (j = 0; j < FORNALHA_SLOTS; j++, data += 4) ReadSlot(data, &f->slots[j], BAU_STACK);
		f->burnLeft = Mem_ReadU16_BE(data)     / 10.0f;
		f->burnMax  = Mem_ReadU16_BE(data + 2) / 10.0f;
		f->cook     = Mem_ReadU16_BE(data + 4) / 10.0f;
		data += 6;
	}
}


/*########################################################################################################################*
*-------------------------------------------------------Furnace-----------------------------------------------------------*
*#########################################################################################################################*/
static const struct { BlockID input, output; } smelting[] = {
	{ BLOCK_IRON_ORE,         EITEM_BARRA_FERRO    },
	{ BLOCK_GOLD_ORE,         EITEM_BARRA_OURO     },
	{ MBLOCK_TITANIO_MINERIO, EITEM_BARRA_TITANIO  },
	{ BLOCK_SAND,             BLOCK_GLASS          },
	{ BLOCK_COBBLE,           BLOCK_STONE          },
	{ LITEM_RAW_MEAT,         LITEM_COOKED_MEAT    },
	{ LBLOCK_NETHERRACK,      LBLOCK_NETHER_BRICKS },
	/* Charcoal: wood burnt in the furnace makes coal */
	{ BLOCK_LOG,              BLOCK_COAL_ORE       },
	{ PBLOCK_TRONCO_BETULA,   BLOCK_COAL_ORE       },
	{ PBLOCK_TRONCO_PINHEIRO, BLOCK_COAL_ORE       },
};

BlockID Equip_SmeltResult(BlockID block) {
	int i;
	for (i = 0; i < Array_Elems(smelting); i++)
	{
		if (smelting[i].input == block) return smelting[i].output;
	}
	return BLOCK_AIR;
}

float Equip_FuelTime(BlockID block) {
	switch (block)
	{
	case BLOCK_COAL_ORE: return FORNALHA_COOK_TIME * 8; /* 8 items */
	case BLOCK_LOG: case PBLOCK_TRONCO_BETULA: case PBLOCK_TRONCO_PINHEIRO:
	case BLOCK_WOOD: case BLOCK_CRATE: case BLOCK_BOOKSHELF:
		return FORNALHA_COOK_TIME * 1.5f;
	case LITEM_GRAVETO: case BLOCK_SAPLING:
		return FORNALHA_COOK_TIME * 0.5f;
	case BLOCK_LAVA: case BLOCK_STILL_LAVA:
		return FORNALHA_COOK_TIME * 20;
	}
	return 0;
}

/* Whether the furnace has something to smelt and room for the result */
static cc_bool CanSmelt(struct EquipFornalha* f) {
	struct EquipSlot* in  = &f->slots[FORNALHA_INPUT];
	struct EquipSlot* out = &f->slots[FORNALHA_OUTPUT];
	BlockID result;
	if (!in->count) return false;

	result = Equip_SmeltResult(in->block);
	if (result == BLOCK_AIR) return false;
	return !out->count || (out->block == result && out->count < BAU_STACK);
}

static void TickFornalha(struct EquipFornalha* f, float delta) {
	struct EquipSlot* fuel = &f->slots[FORNALHA_FUEL];
	struct EquipSlot* out  = &f->slots[FORNALHA_OUTPUT];
	cc_bool smelt = CanSmelt(f);
	cc_bool wasLit = f->burnLeft > 0;
	BlockID cur, want;

	/* Light the next piece of fuel only when there is something to smelt */
	if (f->burnLeft <= 0 && smelt && fuel->count) {
		f->burnMax  = Equip_FuelTime(fuel->block);
		f->burnLeft = f->burnMax;
		if (--fuel->count == 0) fuel->block = BLOCK_AIR;
	}

	if (f->burnLeft > 0) {
		f->burnLeft -= delta;
		if (smelt) {
			f->cook += delta;
			if (f->cook >= FORNALHA_COOK_TIME) {
				f->cook = 0;
				out->block = Equip_SmeltResult(f->slots[FORNALHA_INPUT].block);
				out->count++;
				if (--f->slots[FORNALHA_INPUT].count == 0) f->slots[FORNALHA_INPUT].block = BLOCK_AIR;
			}
		} else {
			f->cook = 0;
		}
		if (f->burnLeft < 0) f->burnLeft = 0;
	} else if (f->cook > 0) {
		/* Without fire, the progress slowly goes back */
		f->cook = max(0, f->cook - delta * 2);
	}

	/* Show a lit furnace while it burns */
	if (wasLit == (f->burnLeft > 0) || !World_Contains(f->x, f->y, f->z)) return;
	cur  = World_GetBlock(f->x, f->y, f->z);
	want = f->burnLeft > 0 ? EBLOCK_FORNALHA_ACESA : LBLOCK_FURNACE;
	if (IsFurnace(cur) && cur != want && Block_IsCustomDefined(want)) Game_UpdateBlock(f->x, f->y, f->z, want);
}

static cc_bool Equip_Tick(struct ScheduledTask2* task) {
	int i;
	if (!World.Loaded || !Survival_IsActive()) return true;
	for (i = 0; i < fornalhasCount; i++) TickFornalha(&fornalhas[i], task->interval);
	return true;
}
static struct ScheduledTask2 equip_task = { 0, 1.0f / 10, Equip_Tick };


/*########################################################################################################################*
*---------------------------------------------------------Using-----------------------------------------------------------*
*#########################################################################################################################*/
static cc_bool IsEquipItem(BlockID b) { return b >= EITEM_BARRA_FERRO && b < EBLOCK_FIM; }

cc_bool Equip_HandleUse(void) {
	struct EquipBau* bau;
	struct EquipFornalha* fornalha;
	IVec3 pos  = Game_SelectedPos.pos;
	BlockID held = Inventory_SelectedBlock;
	BlockID cur  = BLOCK_AIR;
	if (!Survival_IsActive()) return false;

	if (Game_SelectedPos.valid && World_Contains(pos.x, pos.y, pos.z)) {
		cur = World_GetBlock(pos.x, pos.y, pos.z);
	}

	if (cur == EBLOCK_BAU) {
		bau = GetBau(pos.x, pos.y, pos.z);
		if (bau) BauScreen_Show(bau);
		else Chat_AddRaw("&cJa existem baus demais neste mundo.");
		return true;
	}
	if (IsFurnace(cur)) {
		fornalha = GetFornalha(pos.x, pos.y, pos.z);
		if (fornalha) FornalhaScreen_Show(fornalha);
		else Chat_AddRaw("&cJa existem fornalhas demais neste mundo.");
		return true;
	}

	/* Armour in the hand is put on instead of being placed */
	if (Equip_ArmorPiece(held) >= 0) {
		if (!Equip_Wear(held)) Chat_AddRaw("&cVoce nao tem essa armadura.");
		return true;
	}
	/* Bars are items, they can't be placed in the world */
	return IsEquipItem(held);
}


/*########################################################################################################################*
*-------------------------------------------------------Component---------------------------------------------------------*
*#########################################################################################################################*/
static void OnInit(void) {
	ScheduledTask2_Add(&equip_task);
	Event_Register_(&UserEvents.BlockChanged, NULL, OnBlockChanged);
}

static void OnNewMap(void) {
	/* Chests and furnaces belong to a world; saved worlds load theirs after this */
	bausCount      = 0;
	fornalhasCount = 0;
	Equip_ClearArmor();
}

static void OnNewMapLoaded(void) {
	DefineBlocks();
}

struct IGameComponent Equipamento_Component = {
	OnInit,        /* Init  */
	NULL,          /* Free  */
	NULL,          /* Reset */
	OnNewMap,      /* OnNewMap */
	OnNewMapLoaded /* OnNewMapLoaded */
};


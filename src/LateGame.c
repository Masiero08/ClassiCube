#include "LateGame.h"
#include "Block.h"
#include "BlockID.h"
#include "Bitmap.h"
#include "Game.h"
#include "Generator.h"
#include "Server.h"
#include "TexturePack.h"
#include "Lighting.h"
#include "Audio.h"
#include "Funcs.h"
#include "String_.h"
#include "World.h"
#include "Utils.h"
#include "ExtMath.h"
#include "Platform.h"
#include "LateGame_tex.h"

/* Diamonds, the Nether and the End for singleplayer, inspired by newer Minecraft versions. */
/* All textures are original art (see misc/late_game/gen_textures.py), drawn into */
/*  unused tiles of the terrain atlas, so they work with any texture pack. */

/* First terrain atlas tile used (rows 8 and 9 are empty in the default texture pack) */
#define LATE_TILE_BASE 128
#define T(tex) (LATE_TILE_BASE + LTEX_##tex)

struct LateBlockDef {
	const char* name;
	cc_uint8 top, side, bottom;
	cc_uint8 draw, collide, sound, brightness;
};

static const struct LateBlockDef late_blockDefs[LBLOCK_END - LBLOCK_DIAMOND_ORE] = {
/*NAME                    TOP                 SIDE                BOTTOM              DRAW_MODE         COLLIDE_MODE   SOUND        BRIGHTNESS */
{ "Minerio de diamante",  T(DIAMOND_ORE),     T(DIAMOND_ORE),     T(DIAMOND_ORE),     DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Diamante",             T(DIAMOND),         T(DIAMOND),         T(DIAMOND),         DRAW_SPRITE,      COLLIDE_NONE,  SOUND_GLASS, 0 },
{ "Bloco de diamante",    T(DIAMOND_BLOCK),   T(DIAMOND_BLOCK),   T(DIAMOND_BLOCK),   DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_METAL, 0 },
{ "Netherrack",           T(NETHERRACK),      T(NETHERRACK),      T(NETHERRACK),      DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Tijolo do Nether",     T(NETHER_BRICKS),   T(NETHER_BRICKS),   T(NETHER_BRICKS),   DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Areia das almas",      T(SOUL_SAND),       T(SOUL_SAND),       T(SOUL_SAND),       DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_SAND,  0 },
{ "Pedra luminosa",       T(GLOWSTONE),       T(GLOWSTONE),       T(GLOWSTONE),       DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_GLASS, FANCY_LIGHTING_MAX_LEVEL },
{ "Minerio de quartzo",   T(QUARTZ_ORE),      T(QUARTZ_ORE),      T(QUARTZ_ORE),      DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Quartzo",              T(QUARTZ),          T(QUARTZ),          T(QUARTZ),          DRAW_SPRITE,      COLLIDE_NONE,  SOUND_GLASS, 0 },
{ "Portal do Nether",     T(NETHER_PORTAL),   T(NETHER_PORTAL),   T(NETHER_PORTAL),   DRAW_TRANSLUCENT, COLLIDE_NONE,  SOUND_GLASS, FANCY_LIGHTING_MAX_LEVEL },
{ "Pedra do Fim",         T(END_STONE),       T(END_STONE),       T(END_STONE),       DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Portal do Fim",        T(END_PORTAL),      T(END_PORTAL),      T(END_PORTAL),      DRAW_OPAQUE,      COLLIDE_NONE,  SOUND_GLASS, FANCY_LIGHTING_MAX_LEVEL },
{ "Ovo do dragao",        T(DRAGON_EGG),      T(DRAGON_EGG),      T(DRAGON_EGG),      DRAW_SPRITE,      COLLIDE_NONE,  SOUND_STONE, 0 },
{ "Perola do Fim",        T(ENDER_PEARL),     T(ENDER_PEARL),     T(ENDER_PEARL),     DRAW_SPRITE,      COLLIDE_NONE,  SOUND_GLASS, 0 },
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
		srcY = j * LATE_TEX_SIZE / size;

		for (i = 0; i < size; i++)
		{
			srcX   = i * LATE_TEX_SIZE / size;
			px     = src + (srcY * LATE_TEX_SIZE + srcX) * 4;
			row[i] = BitmapCol_Make(px[0], px[1], px[2], px[3]);
		}
	}
}

void LateGame_PatchAtlas(struct Bitmap* bmp) {
	int size = bmp->width / ATLAS2D_TILES_PER_ROW;
	int rows = size ? bmp->height / size : 0;
	int i, tile, x, y;

	for (i = 0; i < LATE_TEX_COUNT; i++)
	{
		tile = LATE_TILE_BASE + i;
		x    = Atlas2D_TileX(tile) * size;
		y    = Atlas2D_TileY(tile);
		if (y >= rows) return;
		y   *= size;

		/* Don't overwrite textures that a texture pack provides */
		if (!IsTileBlank(bmp, x, y, size)) continue;
		DrawTile(bmp, x, y, size, late_texData[i]);
	}
}


/*########################################################################################################################*
*--------------------------------------------------------Blocks-----------------------------------------------------------*
*#########################################################################################################################*/
static void DefineBlock(BlockID block, const struct LateBlockDef* def) {
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
	Block_DefineCustom(block, true);
}

static void DefineBlocks(void) {
	BlockID block;
	int i;
	/* Multiplayer servers decide which blocks exist */
	if (!Server.IsSinglePlayer || !Game_AllowCustomBlocks) return;

	for (i = 0; i < Array_Elems(late_blockDefs); i++)
	{
		block = (BlockID)(LBLOCK_DIAMOND_ORE + i);
		if (block > BLOCK_MAX_DEFINED) break;
		/* Saved worlds may already define the block (possibly differently) */
		if (Block_IsCustomDefined(block)) continue;
		DefineBlock(block, &late_blockDefs[i]);
	}
}


/*########################################################################################################################*
*-------------------------------------------------------Generation--------------------------------------------------------*
*#########################################################################################################################*/
static RNGState gen_rnd;
static BlockRaw GenGet(int x, int y, int z) { return Gen_Blocks[World_Pack(x, y, z)]; }
static void GenSet(int x, int y, int z, BlockRaw b) { Gen_Blocks[World_Pack(x, y, z)] = b; }

static cc_bool TouchesLava(int x, int y, int z) {
	static const int offsets[6][3] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
	BlockRaw b;
	int i;
	for (i = 0; i < 6; i++)
	{
		if (!World_Contains(x + offsets[i][0], y + offsets[i][1], z + offsets[i][2])) continue;
		b = GenGet(x + offsets[i][0], y + offsets[i][1], z + offsets[i][2]);
		if (b == BLOCK_LAVA || b == BLOCK_STILL_LAVA) return true;
	}
	return false;
}

/* Lava cools into obsidian where it touches stone (needed to build Nether portals) */
static void GenObsidian(void) {
	int x, y, z, maxY = World.Height / 2;
	Gen_CurrentState = "Esfriando a lava";

	for (y = 0; y < maxY; y++)
		for (z = 0; z < World.Length; z++)
			for (x = 0; x < World.Width; x++)
			{
				if (GenGet(x, y, z) != BLOCK_STONE || !TouchesLava(x, y, z)) continue;
				if (Random_Next(&gen_rnd, 2)) GenSet(x, y, z, BLOCK_OBSIDIAN);
			}
}

/* Diamonds are rare and only appear in small clumps near the bottom of the world */
static void GenDiamonds(void) {
	int clumps = World.Volume / 30000;
	int maxY   = max(4, World.Height / 6);
	int i, j, x, y, z, index;
	RNGState rnd;

	Gen_CurrentState = "Escondendo diamantes";
	Random_SeedFromCurrentTime(&rnd);

	for (i = 0; i < clumps; i++)
	{
		x = Random_Next(&rnd, World.Width);
		y = 1 + Random_Next(&rnd, maxY);
		z = Random_Next(&rnd, World.Length);

		for (j = 0; j < 1 + Random_Next(&rnd, 4); j++)
		{
			if (World_Contains(x, y, z)) {
				index = World_Pack(x, y, z);
				if (Gen_Blocks[index] == BLOCK_STONE) Gen_Blocks[index] = LBLOCK_DIAMOND_ORE;
			}
			/* Grow the clump in a random direction */
			switch (Random_Next(&rnd, 3))
			{
			case 0: x += Random_Next(&rnd, 2) ? 1 : -1; break;
			case 1: y += Random_Next(&rnd, 2) ? 1 : -1; break;
			case 2: z += Random_Next(&rnd, 2) ? 1 : -1; break;
			}
		}
	}
}


void LateGame_GenOverworld(void) {
	if (!Game_AllowCustomBlocks || LBLOCK_END > BLOCK_MAX_DEFINED) return;
	Random_SeedFromCurrentTime(&gen_rnd);
	GenObsidian();
	GenDiamonds();
}


/*########################################################################################################################*
*---------------------------------------------------------Nether----------------------------------------------------------*
*#########################################################################################################################*/
static BlockRaw ToNether(BlockRaw b) {
	switch (b)
	{
	case BLOCK_STONE: case BLOCK_DIRT: case BLOCK_GRASS: case BLOCK_COBBLE: case BLOCK_SAND: case BLOCK_GRAVEL:
	case BLOCK_GOLD_ORE: case BLOCK_IRON_ORE: case BLOCK_COAL_ORE: case BLOCK_OBSIDIAN: case LBLOCK_DIAMOND_ORE:
		return LBLOCK_NETHERRACK;
	case BLOCK_WATER: case BLOCK_STILL_WATER:
		return BLOCK_STILL_LAVA;
	case BLOCK_LOG: case BLOCK_LEAVES: case BLOCK_SAPLING: case BLOCK_DANDELION: case BLOCK_ROSE:
	case BLOCK_BROWN_SHROOM: case BLOCK_RED_SHROOM:
		return BLOCK_AIR;
	}
	return b;
}

/* Highest block in the column that is not air */
static int GenTop(int x, int z, int fromY) {
	int y;
	for (y = fromY; y > 0; y--)
	{
		if (GenGet(x, y, z) != BLOCK_AIR) return y;
	}
	return 0;
}

/* A long bridge of nether bricks with walls, lamps and ender pearls to find */
static void GenFortressBridge(int alongX, int start, int across, int y, int len) {
	int i, j, h, x, z, fy;
	for (i = 0; i < len; i++)
	{
		for (j = -2; j <= 2; j++)
		{
			x = alongX ? start + i : across + j;
			z = alongX ? across + j : start + i;
			if (!World_Contains(x, y + 3, z)) continue;

			GenSet(x, y, z, LBLOCK_NETHER_BRICKS);
			for (h = 1; h <= 3; h++) GenSet(x, y + h, z, BLOCK_AIR);
			/* Walls on both sides, with glowstone lamps */
			if (j == -2 || j == 2) GenSet(x, y + 1, z, (i % 8 == 4) ? LBLOCK_GLOWSTONE : LBLOCK_NETHER_BRICKS);

			/* Pillars down to the ground every 8 blocks */
			if (i % 8 == 0 && (j == -1 || j == 1)) {
				for (fy = y - 1; fy > 0 && GenGet(x, fy, z) != LBLOCK_NETHERRACK; fy--)
					GenSet(x, fy, z, LBLOCK_NETHER_BRICKS);
			}
		}
		/* Ender pearls lying on the bridge every now and then */
		x = alongX ? start + i : across;
		z = alongX ? across : start + i;
		if (i % 16 == 10 && World_Contains(x, y + 1, z)) GenSet(x, y + 1, z, LITEM_ENDER_PEARL);
	}
}

void LateGame_GenNether(void) {
	int x, y, z, i, ceil, top, cx, cz, floorY;
	int lavaY = World.Height / 2;
	float n;
	Gen_CurrentState = "Esquentando o Nether";
	Random_SeedFromCurrentTime(&gen_rnd);

	for (i = 0; i < World.Volume; i++) Gen_Blocks[i] = ToNether(Gen_Blocks[i]);

	/* A lower lava sea than the overworld oceans, so there is more ground to walk on */
	for (y = lavaY - 5; y < World.Height; y++)
		for (z = 0; z < World.Length; z++)
			for (x = 0; x < World.Width; x++)
			{
				if (GenGet(x, y, z) == BLOCK_STILL_LAVA) GenSet(x, y, z, BLOCK_AIR);
			}

	for (z = 0; z < World.Length; z++)
		for (x = 0; x < World.Width; x++)
		{
			/* A bumpy netherrack ceiling, with glowstone hanging from it */
			n    = Math_SinF(x * 0.21f) + Math_CosF(z * 0.17f) + Math_SinF((x + z) * 0.09f);
			ceil = World.MaxY - 6 - (int)(n * 2.5f) - Random_Next(&gen_rnd, 2);
			for (y = ceil; y <= World.MaxY; y++) GenSet(x, y, z, LBLOCK_NETHERRACK);
			GenSet(x, World.MaxY, z, BLOCK_BEDROCK);

			if (Random_Next(&gen_rnd, 90) == 0) {
				for (y = ceil - 1; y > ceil - 2 - Random_Next(&gen_rnd, 4) && y > 0; y--) GenSet(x, y, z, LBLOCK_GLOWSTONE);
			}

			/* Patches of soul sand on the ground */
			top = GenTop(x, z, ceil - 1);
			n   = Math_SinF(x * 0.13f + 1.7f) * Math_CosF(z * 0.11f + 0.3f);
			if (n > 0.55f && GenGet(x, top, z) == LBLOCK_NETHERRACK) GenSet(x, top, z, LBLOCK_SOUL_SAND);
		}

	/* Quartz ore in the netherrack */
	for (i = 0; i < World.Volume / 300; i++)
	{
		x = Random_Next(&gen_rnd, World.Width); y = Random_Next(&gen_rnd, World.Height); z = Random_Next(&gen_rnd, World.Length);
		if (GenGet(x, y, z) == LBLOCK_NETHERRACK) GenSet(x, y, z, LBLOCK_QUARTZ_ORE);
	}

	/* Fortress bridges crossing the lava sea */
	GenFortressBridge(true,  8, World.Length / 3,     lavaY + 6, World.Width  - 16);
	GenFortressBridge(false, 8, World.Width  * 2 / 3, lavaY + 9, World.Length - 16);

	/* An opening in the ceiling and a safe platform where the player arrives */
	cx = World.Width / 2; cz = World.Length / 2;
	floorY = max(lavaY + 1, GenTop(cx, cz, World.MaxY - 16));
	floorY = min(floorY, World.MaxY - 12);
	for (z = cz - 2; z <= cz + 2; z++)
		for (x = cx - 2; x <= cx + 2; x++)
		{
			if (!World_Contains(x, floorY, z)) continue;
			GenSet(x, floorY, z, LBLOCK_NETHERRACK);
			for (y = floorY + 1; y <= World.MaxY; y++) GenSet(x, y, z, BLOCK_AIR);
		}
}


/*########################################################################################################################*
*-----------------------------------------------------------End-----------------------------------------------------------*
*#########################################################################################################################*/
void LateGame_GenEnd(void) {
	int cx = World.Width / 2, cz = World.Length / 2, baseY = World.Height / 2;
	float radius = (float)(min(World.Width, World.Length) / 2 - 6);
	int x, y, z, i, h, px, pz, thick;
	float d, ang;
	Gen_CurrentState = "Abrindo o Fim";
	Random_SeedFromCurrentTime(&gen_rnd);
	if (radius > 44) radius = 44;

	Mem_Set(Gen_Blocks, BLOCK_AIR, World.Volume);
	/* A floating island of end stone */
	for (z = 0; z < World.Length; z++)
		for (x = 0; x < World.Width; x++)
		{
			d = Math_SqrtF((float)((x - cx) * (x - cx) + (z - cz) * (z - cz)));
			d += Math_SinF(x * 0.3f) + Math_CosF(z * 0.27f);
			if (d >= radius) continue;

			thick = 2 + (int)((1.0f - d / radius) * 14.0f);
			for (y = baseY - thick; y <= baseY; y++)
			{
				if (y >= 0) GenSet(x, y, z, LBLOCK_END_STONE);
			}
		}

	/* Tall obsidian pillars in a ring */
	for (i = 0; i < 8; i++)
	{
		ang = i * 2 * MATH_PI / 8;
		px  = cx + (int)(Math_CosF(ang) * radius * 0.6f);
		pz  = cz + (int)(Math_SinF(ang) * radius * 0.6f);
		h   = 10 + Random_Next(&gen_rnd, 12);
		for (y = baseY + 1; y <= baseY + h && y < World.MaxY; y++)
			for (z = pz - 1; z <= pz + 1; z++)
				for (x = px - 1; x <= px + 1; x++)
				{
					if (World_Contains(x, y, z)) GenSet(x, y, z, BLOCK_OBSIDIAN);
				}
	}
}


/*########################################################################################################################*
*--------------------------------------------------------Component--------------------------------------------------------*
*#########################################################################################################################*/
static void OnNewMapLoaded(void) { DefineBlocks(); }

struct IGameComponent LateGame_Component = {
	NULL,          /* Init  */
	NULL,          /* Free  */
	NULL,          /* Reset */
	NULL,          /* OnNewMap */
	OnNewMapLoaded /* OnNewMapLoaded */
};

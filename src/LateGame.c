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
#include "Entity.h"
#include "EntityComponents.h"
#include "Model.h"
#include "Graphics.h"
#include "Stream.h"
#include "Chat.h"
#include "Camera.h"
#include "Survival.h"
#include "DayNight.h"
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
{ "Carne crua",           T(RAW_MEAT),        T(RAW_MEAT),        T(RAW_MEAT),        DRAW_SPRITE,      COLLIDE_NONE,  SOUND_CLOTH, 0 },
{ "Carne assada",         T(COOKED_MEAT),     T(COOKED_MEAT),     T(COOKED_MEAT),     DRAW_SPRITE,      COLLIDE_NONE,  SOUND_CLOTH, 0 },
{ "Fornalha",             T(FURNACE_TOP),     T(FURNACE_SIDE),    T(FURNACE_TOP),     DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Cama",                 T(BED_TOP),         T(BED_SIDE),        4,                  DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_CLOTH, 0 },
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

	/* The bed is a low block you can sleep on (see Survival_HandleUse) */
	if (block == LBLOCK_BED) {
		Blocks.MaxBB[block].y     = 9.0f / 16.0f;
		Blocks.BlocksLight[block] = false;
	}
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
*------------------------------------------------------Ender Dragon-------------------------------------------------------*
*#########################################################################################################################*/
/* A boss that circles the End island and swoops at the player. When defeated it leaves */
/*  an exit portal back to the overworld and a dragon egg on a bedrock pillar. */
#define DRAGON_ID      248
#define DRAGON_HEALTH  100
#define DRAGON_SCALE   3.0f
#define DRAGON_PARTS   10
#define DRAGON_REACH   7.0f

static struct ModelVertex dragon_vertices[DRAGON_PARTS * MODEL_BOX_VERTICES];
static struct ModelPart dragon_body, dragon_neck, dragon_head, dragon_tail, dragon_wingL, dragon_wingR, dragon_legs[4];
static struct ModelTex dragon_tex = { "dragao_fim.png", SKIN_64x64 };
static struct Model dragon_model;
static cc_bool dragon_registered;

static void MakeBox(struct ModelPart* part, int u, int v, int x1, int y1, int z1, int x2, int y2, int z2, float rx, float ry, float rz) {
	struct BoxDesc desc;
	desc.texX  = u; desc.texY = v;
	desc.sizeX = x2 - x1; desc.sizeY = y2 - y1; desc.sizeZ = z2 - z1;
	desc.x1 = x1 / 16.0f; desc.y1 = y1 / 16.0f; desc.z1 = z1 / 16.0f;
	desc.x2 = x2 / 16.0f; desc.y2 = y2 / 16.0f; desc.z2 = z2 / 16.0f;
	desc.rotX = rx / 16.0f; desc.rotY = ry / 16.0f; desc.rotZ = rz / 16.0f;
	BoxDesc_BuildBox(part, &desc);
}

/* Model faces -Z (head at the front), see the skin layout in misc/late_game/gen_textures.py */
static void Dragon_MakeParts(void) {
	MakeBox(&dragon_body,  0, 13,  -4,  8,  -8,  4, 14,  10, 0, 11,   0);
	MakeBox(&dragon_neck,  0, 52,  -2, 10, -14,  2, 14,  -8, 0, 12,  -8);
	MakeBox(&dragon_head, 32, 37,  -3,  9, -22,  3, 15, -14, 0, 12, -14);
	MakeBox(&dragon_tail,  0, 37,  -2,  9,  10,  2, 12,  22, 0, 10,  10);
	MakeBox(&dragon_wingL, 0,  0,   4, 13,  -6, 24, 14,   6,  4, 13.5f, 0);
	MakeBox(&dragon_wingR, 0,  0, -24, 13,  -6, -4, 14,   6, -4, 13.5f, 0);
	MakeBox(&dragon_legs[0], 20, 52, -4, 2, -7, -2, 8, -5, -3, 8, -6);
	MakeBox(&dragon_legs[1], 20, 52,  2, 2, -7,  4, 8, -5,  3, 8, -6);
	MakeBox(&dragon_legs[2], 20, 52, -4, 2,  5, -2, 8,  7, -3, 8,  6);
	MakeBox(&dragon_legs[3], 20, 52,  2, 2,  5,  4, 8,  7,  3, 8,  6);
}

static void Dragon_CheckTexture(void) {
	static const cc_string name = String_FromConst("dragao_fim.png");
	struct Stream mem;
	if (dragon_tex.texID) return;

	Stream_ReadonlyMemory(&mem, (void*)late_dragonPng, sizeof(late_dragonPng));
	Game_UpdateTexture(&dragon_tex.texID, &mem, &name, &dragon_tex.skinType, NULL);
}

static void Dragon_Draw(struct Entity* e) {
	float flap = Math_SinF((float)Game.Time * 3.0f) * 0.7f;
	float sway = Math_SinF((float)Game.Time * 1.5f) * 0.25f;
	int i;

	Dragon_CheckTexture();
	Model_ApplyTexture(e);
	Model_LockVB(e, DRAGON_PARTS * MODEL_BOX_VERTICES);

	Model_DrawPart(&dragon_body);
	Model_DrawPart(&dragon_neck);
	Model_DrawRotate(sway * 0.4f, 0, 0, &dragon_head, false);
	Model_DrawRotate(0, sway, 0, &dragon_tail, false);
	Model_DrawRotate(0, 0,  flap, &dragon_wingL, false);
	Model_DrawRotate(0, 0, -flap, &dragon_wingR, false);
	for (i = 0; i < 4; i++) Model_DrawRotate(0.6f, 0, 0, &dragon_legs[i], false);

	Model_UnlockVB();
	Gfx_DrawVb_IndexedTris(DRAGON_PARTS * MODEL_BOX_VERTICES);
}

static float Dragon_GetNameY(struct Entity* e) { return 16 / 16.0f; }
static float Dragon_GetEyeY(struct Entity* e)  { return 12 / 16.0f; }
static void  Dragon_GetSize(struct Entity* e)  { Vec3_Set(e->Size, 16 / 16.0f, 14 / 16.0f, 16 / 16.0f); }
static void  Dragon_GetBounds(struct Entity* e) {
	Vec3_Set(e->ModelAABB.Min, -24 / 16.0f,  2 / 16.0f, -22 / 16.0f);
	Vec3_Set(e->ModelAABB.Max,  24 / 16.0f, 15 / 16.0f,  22 / 16.0f);
}

static void Dragon_RegisterModel(void) {
	if (dragon_registered) return;
	dragon_registered = true;
	Model_RegisterTexture(&dragon_tex);

	dragon_model.name       = "dragao_fim";
	dragon_model.vertices   = dragon_vertices;
	dragon_model.defaultTex = &dragon_tex;
	dragon_model.MakeParts  = Dragon_MakeParts;
	dragon_model.Draw       = Dragon_Draw;
	dragon_model.GetNameY   = Dragon_GetNameY;
	dragon_model.GetEyeY    = Dragon_GetEyeY;
	dragon_model.GetCollisionSize = Dragon_GetSize;
	dragon_model.GetPickingBounds = Dragon_GetBounds;

	Model_Init(&dragon_model);
	dragon_model.usesSkin    = false;
	dragon_model.maxVertices = DRAGON_PARTS * MODEL_BOX_VERTICES;
	Model_Register(&dragon_model);
}

struct Dragon {
	struct Entity base;
	cc_bool used, swooping;
	int health, lastShownHealth;
	float angle, swoopTimer, attackTimer, hurtTimer;
};
static struct Dragon dragon;
static RNGState dragon_rnd;

static void EndCenter(int* cx, int* cy, int* cz) {
	*cx = World.Width / 2; *cy = World.Height / 2; *cz = World.Length / 2;
}

/* The exit portal's bedrock pillar marks a dragon that was already defeated */
static cc_bool Dragon_WasDefeated(void) {
	int cx, cy, cz;
	EndCenter(&cx, &cy, &cz);
	return World_Contains(cx, cy + 1, cz) && World_GetBlock(cx, cy + 1, cz) == BLOCK_BEDROCK;
}

static void Dragon_ShowHealth(void) {
	cc_string str; char strBuffer[STRING_SIZE];
	int i, filled;
	if (dragon.lastShownHealth == dragon.health) return;
	dragon.lastShownHealth = dragon.health;

	if (!dragon.used) { Chat_AddOf(&String_Empty, MSG_TYPE_EXTRASTATUS_1); return; }
	String_InitArray(str, strBuffer);
	String_AppendConst(&str, "&5Ender Dragon &d");
	filled = (dragon.health * 20 + DRAGON_HEALTH - 1) / DRAGON_HEALTH;
	for (i = 0; i < 20; i++)
	{
		if (i == filled) String_AppendConst(&str, "&8");
		String_Append(&str, '|');
	}
	Chat_AddOf(&str, MSG_TYPE_EXTRASTATUS_1);
}

static void Dragon_Remove(void) {
	if (!dragon.used) return;
	Entities_Remove(DRAGON_ID);
	dragon.used = false;
	Dragon_ShowHealth();
}

/* Exit portal on bedrock in the middle of the island, with the egg on a pillar */
static void Dragon_BuildExit(void) {
	int cx, cy, cz, x, y, z, d2;
	EndCenter(&cx, &cy, &cz);

	for (z = cz - 3; z <= cz + 3; z++)
		for (x = cx - 3; x <= cx + 3; x++)
		{
			d2 = (x - cx) * (x - cx) + (z - cz) * (z - cz);
			if (d2 > 10 || !World_Contains(x, cy + 1, z)) continue;
			Game_UpdateBlock(x, cy, z, BLOCK_BEDROCK);
			for (y = cy + 1; y <= cy + 6; y++) Game_UpdateBlock(x, y, z, BLOCK_AIR);
			Game_UpdateBlock(x, cy + 1, z, d2 > 4 ? BLOCK_BEDROCK : LBLOCK_END_PORTAL);
		}

	for (y = cy + 1; y <= cy + 4; y++) Game_UpdateBlock(cx, y, cz, BLOCK_BEDROCK);
	Game_UpdateBlock(cx, cy + 5, cz, LBLOCK_DRAGON_EGG);
}

static void Dragon_MoveTowards(Vec3 target, float speed, float delta) {
	struct Entity* e = &dragon.base;
	Vec3 pos = e->next.pos, dir;
	float len, step;

	Vec3_Sub(&dir, &target, &pos);
	len = Math_SqrtF(Vec3_LengthSquared(&dir));
	if (len < 0.01f) return;
	step = min(speed * delta, len);

	pos.x += dir.x / len * step;
	pos.y += dir.y / len * step;
	pos.z += dir.z / len * step;
	e->next.pos  = pos;
	e->next.yaw  = Math_Atan2f(dir.x, -dir.z) * MATH_RAD2DEG;
	e->next.rotY = e->next.yaw;
}

static void Dragon_Tick(struct Entity* e, float delta) {
	struct Entity* player = &Entities.CurPlayer->Base;
	Vec3 target, rel;
	int cx, cy, cz;
	float dist;

	e->prev = e->next;
	EndCenter(&cx, &cy, &cz);
	if (dragon.hurtTimer   > 0) dragon.hurtTimer   -= delta;
	if (dragon.attackTimer > 0) dragon.attackTimer -= delta;
	dragon.swoopTimer -= delta;

	Vec3_Sub(&rel, &player->Position, &e->next.pos);
	dist = Math_SqrtF(Vec3_LengthSquared(&rel));

	if (dragon.swooping) {
		/* Dives at the player, bites, then climbs back up */
		target = player->Position; target.y += 1.0f;
		Dragon_MoveTowards(target, 13.0f, delta);

		if (dist < 3.5f && dragon.attackTimer <= 0) {
			Survival_Damage(5);
			dragon.attackTimer = 1.5f;
			player->Velocity.x += rel.x / (dist + 0.1f) * 1.2f;
			player->Velocity.z += rel.z / (dist + 0.1f) * 1.2f;
			player->Velocity.y  = 0.6f;
			dragon.swooping = false;
		}
		if (dragon.swoopTimer < 4.0f) dragon.swooping = false;
	} else {
		/* Circles around the pillars */
		dragon.angle += delta * 0.3f;
		target.x = cx + Math_CosF(dragon.angle) * 22.0f;
		target.z = cz + Math_SinF(dragon.angle) * 22.0f;
		target.y = cy + 16.0f + Math_SinF(dragon.angle * 3.0f) * 4.0f;
		Dragon_MoveTowards(target, 9.0f, delta);

		if (dragon.swoopTimer <= 0) {
			dragon.swoopTimer = 9.0f + Random_Float(&dragon_rnd) * 5.0f;
			if (dist < 60.0f) dragon.swooping = true;
		}
	}
}

static void Dragon_Despawn(struct Entity* e) {
	if (e->Flags & ENTITY_FLAG_HAS_MODELVB) Gfx_DeleteScratchVb(&e->ModelVB);
}

static void Dragon_SetLocation(struct Entity* e, struct LocationUpdate* update) {
	if (update->flags & LU_HAS_POS) {
		e->prev.pos = update->pos; e->next.pos = update->pos;
	}
}

static PackedCol Dragon_GetCol(struct Entity* e) {
	if (dragon.hurtTimer > 0) return PackedCol_Make(255, 90, 90, 255);
	return PackedCol_Make(230, 230, 230, 255);
}

static void Dragon_RenderModel(struct Entity* e, float delta, float t) {
	Vec3_Lerp(&e->Position, &e->prev.pos, &e->next.pos, t);
	Entity_LerpAngles(e, t);
	e->ShouldRender = Model_ShouldRender(e);
	if (e->ShouldRender) Model_Render(e->Model, e);
}

static cc_bool Dragon_ShouldRenderName(struct Entity* e) { return false; }

static const struct EntityVTABLE dragon_VTABLE = {
	Dragon_Tick,        Dragon_Despawn,         Dragon_SetLocation, Dragon_GetCol,
	Dragon_RenderModel, Dragon_ShouldRenderName
};

static void Dragon_Spawn(void) {
	struct Entity* e = &dragon.base;
	int cx, cy, cz;
	Vec3 pos;
	if (dragon.used || Entities.List[DRAGON_ID]) return;
	Dragon_RegisterModel();
	EndCenter(&cx, &cy, &cz);

	Mem_Set(&dragon, 0, sizeof(dragon));
	Entity_Init(e);
	e->VTABLE = &dragon_VTABLE;
	e->Model  = Model_Get(&(cc_string)String_FromConst("dragao_fim"));
	Vec3_Set(e->ModelScale, DRAGON_SCALE, DRAGON_SCALE, DRAGON_SCALE);
	Entity_UpdateModelBounds(e);

	pos.x = cx + 22.0f; pos.y = cy + 16.0f; pos.z = (float)cz;
	e->prev.pos = pos; e->next.pos = pos; e->Position = pos;

	dragon.used        = true;
	dragon.health      = DRAGON_HEALTH;
	dragon.swoopTimer  = 8.0f;
	dragon.lastShownHealth = -1;
	Entities.List[DRAGON_ID] = e;
	Dragon_ShowHealth();
	Chat_AddOf(&(cc_string)String_FromConst("&5O Ender Dragon acordou!"), MSG_TYPE_ANNOUNCEMENT);
}

cc_bool LateGame_HandleAttack(void) {
	struct Entity* player;
	Vec3 eye, dir, center, rel;
	float t, d2;

	if (!dragon.used || !Survival_IsActive()) return false;
	player = &Entities.CurPlayer->Base;
	eye    = Entity_GetEyePosition(player);
	dir    = Vec3_GetDirVector(player->Yaw * MATH_DEG2RAD, player->Pitch * MATH_DEG2RAD + Camera.TiltPitch);

	center    = dragon.base.Position;
	center.y += 11 / 16.0f * DRAGON_SCALE;
	Vec3_Sub(&rel, &center, &eye);
	t = rel.x * dir.x + rel.y * dir.y + rel.z * dir.z;
	if (t < 0 || t > DRAGON_REACH) return false;
	d2 = Vec3_LengthSquared(&rel) - t * t;
	if (d2 > 2.5f * 2.5f) return false;

	dragon.health   -= Survival_AttackDamage();
	dragon.hurtTimer = 0.3f;
	Dragon_ShowHealth();
	if (dragon.health > 0) return true;

	dragon.health = 0;
	Dragon_Remove();
	Dragon_BuildExit();
	Chat_AddOf(&(cc_string)String_FromConst("&5O Ender Dragon foi derrotado!"), MSG_TYPE_ANNOUNCEMENT);
	Chat_AddRaw("&dUm portal de volta apareceu no centro da ilha.");
	return true;
}

static cc_bool Dragon_TaskTick(struct ScheduledTask2* task) {
	cc_bool inEnd = World.Loaded && Survival_IsActive() && DayNight_GetMode() == DAYNIGHT_END;
	if (!inEnd) { Dragon_Remove(); return true; }
	if (!dragon.used && !Dragon_WasDefeated()) Dragon_Spawn();
	return true;
}
static struct ScheduledTask2 dragon_task = { 0, 1.0f, Dragon_TaskTick };


/*########################################################################################################################*
*--------------------------------------------------------Component--------------------------------------------------------*
*#########################################################################################################################*/
static void OnInit(void) {
	Random_SeedFromCurrentTime(&dragon_rnd);
	ScheduledTask2_Add(&dragon_task);
}

static void OnNewMap(void) { Dragon_Remove(); }
static void OnNewMapLoaded(void) { DefineBlocks(); }

struct IGameComponent LateGame_Component = {
	OnInit,        /* Init  */
	NULL,          /* Free  */
	NULL,          /* Reset */
	OnNewMap,      /* OnNewMap */
	OnNewMapLoaded /* OnNewMapLoaded */
};

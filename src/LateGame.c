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
/* Diamonds are rare and only appear in small clumps near the bottom of the world */
void LateGame_GenDiamonds(void) {
	int clumps = World.Volume / 30000;
	int maxY   = max(4, World.Height / 6);
	int i, j, x, y, z, index;
	RNGState rnd;

	if (!Game_AllowCustomBlocks || LBLOCK_DIAMOND_ORE > BLOCK_MAX_DEFINED) return;
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

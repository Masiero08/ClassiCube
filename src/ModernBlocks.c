#include "ModernBlocks.h"
#include "Block.h"
#include "Bitmap.h"
#include "Game.h"
#include "Server.h"
#include "TexturePack.h"
#include "Lighting.h"
#include "Audio.h"
#include "Funcs.h"
#include "String_.h"
#include "ModernBlocks_tex.h"

/* Adds a set of blocks inspired by newer Minecraft versions to singleplayer worlds. */
/* Their textures are original art (see misc/modern_blocks/gen_textures.py), drawn */
/*  into unused tiles of the terrain atlas, so they work with any texture pack. */

/* First terrain atlas tile used by modern block textures (tiles 96 onwards are unused by default packs) */
#define MODERN_TILE_BASE 96
#define MODERN_BLOCK_BASE (BLOCK_MAX_CPE + 1)
#define T(tex) (MODERN_TILE_BASE + MTEX_##tex)

struct ModernBlockDef {
	const char* name;
	cc_uint8 top, side, bottom;
	cc_uint8 draw, collide, sound, brightness;
};

static const struct ModernBlockDef modern_blockDefs[] = {
/*NAME                    TOP                     SIDE                    BOTTOM                  DRAW_MODE    COLLIDE_MODE   SOUND        BRIGHTNESS */
{ "Deepslate",            T(DEEPSLATE_TOP),       T(DEEPSLATE_SIDE),      T(DEEPSLATE_TOP),       DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Cobbled deepslate",    T(COBBLED_DEEPSLATE),   T(COBBLED_DEEPSLATE),   T(COBBLED_DEEPSLATE),   DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Deepslate bricks",     T(DEEPSLATE_BRICKS),    T(DEEPSLATE_BRICKS),    T(DEEPSLATE_BRICKS),    DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Deepslate tiles",      T(DEEPSLATE_TILES),     T(DEEPSLATE_TILES),     T(DEEPSLATE_TILES),     DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Tuff",                 T(TUFF),                T(TUFF),                T(TUFF),                DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Calcite",              T(CALCITE),             T(CALCITE),             T(CALCITE),             DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Granite",              T(GRANITE),             T(GRANITE),             T(GRANITE),             DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Diorite",              T(DIORITE),             T(DIORITE),             T(DIORITE),             DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Andesite",             T(ANDESITE),            T(ANDESITE),            T(ANDESITE),            DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Copper block",         T(COPPER),              T(COPPER),              T(COPPER),              DRAW_OPAQUE, COLLIDE_SOLID, SOUND_METAL, 0 },
{ "Cut copper",           T(CUT_COPPER),          T(CUT_COPPER),          T(CUT_COPPER),          DRAW_OPAQUE, COLLIDE_SOLID, SOUND_METAL, 0 },
{ "Oxidized copper",      T(OXIDIZED_COPPER),     T(OXIDIZED_COPPER),     T(OXIDIZED_COPPER),     DRAW_OPAQUE, COLLIDE_SOLID, SOUND_METAL, 0 },
{ "Amethyst block",       T(AMETHYST),            T(AMETHYST),            T(AMETHYST),            DRAW_OPAQUE, COLLIDE_SOLID, SOUND_GLASS, 0 },
{ "Blackstone",           T(BLACKSTONE),          T(BLACKSTONE),          T(BLACKSTONE),          DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Blackstone bricks",    T(BLACKSTONE_BRICKS),   T(BLACKSTONE_BRICKS),   T(BLACKSTONE_BRICKS),   DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Mud bricks",           T(MUD_BRICKS),          T(MUD_BRICKS),          T(MUD_BRICKS),          DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Cherry planks",        T(CHERRY_PLANKS),       T(CHERRY_PLANKS),       T(CHERRY_PLANKS),       DRAW_OPAQUE, COLLIDE_SOLID, SOUND_WOOD,  0 },
{ "Cherry log",           T(CHERRY_LOG_TOP),      T(CHERRY_LOG_SIDE),     T(CHERRY_LOG_TOP),      DRAW_OPAQUE, COLLIDE_SOLID, SOUND_WOOD,  0 },
{ "Mangrove planks",      T(MANGROVE_PLANKS),     T(MANGROVE_PLANKS),     T(MANGROVE_PLANKS),     DRAW_OPAQUE, COLLIDE_SOLID, SOUND_WOOD,  0 },
{ "Bamboo planks",        T(BAMBOO_PLANKS),       T(BAMBOO_PLANKS),       T(BAMBOO_PLANKS),       DRAW_OPAQUE, COLLIDE_SOLID, SOUND_WOOD,  0 },
{ "Moss block",           T(MOSS),                T(MOSS),                T(MOSS),                DRAW_OPAQUE, COLLIDE_SOLID, SOUND_GRASS, 0 },
{ "Prismarine",           T(PRISMARINE),          T(PRISMARINE),          T(PRISMARINE),          DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Sea lantern",          T(SEA_LANTERN),         T(SEA_LANTERN),         T(SEA_LANTERN),         DRAW_OPAQUE, COLLIDE_SOLID, SOUND_GLASS, FANCY_LIGHTING_MAX_LEVEL },
{ "Shroomlight",          T(SHROOMLIGHT),         T(SHROOMLIGHT),         T(SHROOMLIGHT),         DRAW_OPAQUE, COLLIDE_SOLID, SOUND_GRASS, FANCY_LIGHTING_MAX_LEVEL },
{ "Terracotta",           T(TERRACOTTA),          T(TERRACOTTA),          T(TERRACOTTA),          DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Packed ice",           T(PACKED_ICE),          T(PACKED_ICE),          T(PACKED_ICE),          DRAW_OPAQUE, COLLIDE_ICE,   SOUND_STONE, 0 },
{ "Basalt",               T(BASALT_TOP),          T(BASALT_SIDE),         T(BASALT_TOP),          DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Lantern",              T(LANTERN),             T(LANTERN),             T(LANTERN),             DRAW_SPRITE, COLLIDE_NONE,  SOUND_METAL, FANCY_LIGHTING_MAX_LEVEL },
};
#define MODERN_BLOCKS_COUNT Array_Elems(modern_blockDefs)


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
		srcY = j * MODERN_TEX_SIZE / size;

		for (i = 0; i < size; i++)
		{
			srcX   = i * MODERN_TEX_SIZE / size;
			px     = src + (srcY * MODERN_TEX_SIZE + srcX) * 4;
			row[i] = BitmapCol_Make(px[0], px[1], px[2], px[3]);
		}
	}
}

void ModernBlocks_PatchAtlas(struct Bitmap* bmp) {
	int size = bmp->width / ATLAS2D_TILES_PER_ROW;
	int rows = size ? bmp->height / size : 0;
	int i, tile, x, y;

	for (i = 0; i < MODERN_TEX_COUNT; i++)
	{
		tile = MODERN_TILE_BASE + i;
		x    = Atlas2D_TileX(tile) * size;
		y    = Atlas2D_TileY(tile);
		if (y >= rows) return;
		y   *= size;

		/* Don't overwrite textures that a texture pack provides */
		if (!IsTileBlank(bmp, x, y, size)) continue;
		DrawTile(bmp, x, y, size, modern_texData[i]);
	}
}


/*########################################################################################################################*
*--------------------------------------------------------Blocks-----------------------------------------------------------*
*#########################################################################################################################*/
static void DefineBlock(BlockID block, const struct ModernBlockDef* def) {
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

	if (def->draw == DRAW_SPRITE) {
		Vec3_Set(Blocks.MinBB[block], 5.0f/16.0f, 0,           5.0f/16.0f);
		Vec3_Set(Blocks.MaxBB[block], 11.0f/16.0f, 14.0f/16.0f, 11.0f/16.0f);
	}
	Block_DefineCustom(block, true);
}

static void OnNewMapLoaded(void) {
	BlockID block;
	int i;
	/* Multiplayer servers decide which blocks exist */
	if (!Server.IsSinglePlayer || !Game_AllowCustomBlocks) return;

	for (i = 0; i < MODERN_BLOCKS_COUNT; i++)
	{
		block = (BlockID)(MODERN_BLOCK_BASE + i);
		if (block > BLOCK_MAX_DEFINED) break;
		/* Saved worlds may already define the block (possibly differently) */
		if (Block_IsCustomDefined(block)) continue;
		DefineBlock(block, &modern_blockDefs[i]);
	}
}

struct IGameComponent ModernBlocks_Component = {
	NULL,          /* Init  */
	NULL,          /* Free  */
	NULL,          /* Reset */
	NULL,          /* OnNewMap */
	OnNewMapLoaded /* OnNewMapLoaded */
};

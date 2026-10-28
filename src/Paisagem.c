#include "Paisagem.h"
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
#include "ExtMath.h"
#include "Minerios.h"
#include "Paisagem_tex.h"

/* Paisagem: makes newly generated singleplayer worlds feel alive. */
/* All textures are original art (see misc/paisagem/gen_textures.py), drawn into */
/*  unused tiles of the terrain atlas, so they work with any texture pack. */

#define PAISAGEM_TILE_BASE 176
#define T(tex) (PAISAGEM_TILE_BASE + PTEX_##tex)

struct PaisagemBlockDef {
	const char* name;
	cc_uint8 top, side, bottom;
	cc_uint8 draw, collide, sound;
};

static const struct PaisagemBlockDef paisagem_blockDefs[PBLOCK_FIM - PBLOCK_GRAMA_ALTA] = {
/*NAME                     TOP                 SIDE                 BOTTOM              DRAW_MODE               COLLIDE_MODE   SOUND */
{ "Grama alta",            T(TALL_GRASS),      T(TALL_GRASS),       T(TALL_GRASS),      DRAW_SPRITE,            COLLIDE_NONE,  SOUND_GRASS },
{ "Samambaia",             T(FERN),            T(FERN),             T(FERN),            DRAW_SPRITE,            COLLIDE_NONE,  SOUND_GRASS },
{ "Tulipa",                T(TULIP),           T(TULIP),            T(TULIP),           DRAW_SPRITE,            COLLIDE_NONE,  SOUND_GRASS },
{ "Flor azul",             T(BLUE_FLOWER),     T(BLUE_FLOWER),      T(BLUE_FLOWER),     DRAW_SPRITE,            COLLIDE_NONE,  SOUND_GRASS },
{ "Margaridas",            T(DAISIES),         T(DAISIES),          T(DAISIES),         DRAW_SPRITE,            COLLIDE_NONE,  SOUND_GRASS },
{ "Tronco de betula",      T(BIRCH_TOP),       T(BIRCH_SIDE),       T(BIRCH_TOP),       DRAW_OPAQUE,            COLLIDE_SOLID, SOUND_WOOD  },
{ "Folhas de betula",      T(BIRCH_LEAVES),    T(BIRCH_LEAVES),     T(BIRCH_LEAVES),    DRAW_TRANSPARENT_THICK, COLLIDE_SOLID, SOUND_GRASS },
{ "Arbusto de frutinhas",  T(BERRY_BUSH),      T(BERRY_BUSH),       T(BERRY_BUSH),      DRAW_SPRITE,            COLLIDE_NONE,  SOUND_GRASS },
{ "Frutinhas",             T(BERRIES),         T(BERRIES),          T(BERRIES),         DRAW_SPRITE,            COLLIDE_NONE,  SOUND_GRASS },
{ "Cana-de-acucar",        T(SUGAR_CANE),      T(SUGAR_CANE),       T(SUGAR_CANE),      DRAW_SPRITE,            COLLIDE_NONE,  SOUND_GRASS },
{ "Abobora",               T(PUMPKIN_TOP),     T(PUMPKIN_SIDE),     T(PUMPKIN_SIDE),    DRAW_OPAQUE,            COLLIDE_SOLID, SOUND_WOOD  },
{ "Folhas de cerejeira",   T(CHERRY_LEAVES),   T(CHERRY_LEAVES),    T(CHERRY_LEAVES),   DRAW_TRANSPARENT_THICK, COLLIDE_SOLID, SOUND_GRASS },
{ "Folhas de pinheiro",    T(PINE_LEAVES),     T(PINE_LEAVES),      T(PINE_LEAVES),     DRAW_TRANSPARENT_THICK, COLLIDE_SOLID, SOUND_GRASS },
{ "Tronco de pinheiro",    T(BIRCH_TOP),       T(PINE_LOG_SIDE),    T(BIRCH_TOP),       DRAW_OPAQUE,            COLLIDE_SOLID, SOUND_WOOD  },
};


/*########################################################################################################################*
*-------------------------------------------------------Textures----------------------------------------------------------*
*#########################################################################################################################*/
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
		srcY = j * PAISAGEM_TEX_SIZE / size;

		for (i = 0; i < size; i++)
		{
			srcX   = i * PAISAGEM_TEX_SIZE / size;
			px     = src + (srcY * PAISAGEM_TEX_SIZE + srcX) * 4;
			row[i] = BitmapCol_Make(px[0], px[1], px[2], px[3]);
		}
	}
}

void Paisagem_PatchAtlas(struct Bitmap* bmp) {
	int size = bmp->width / ATLAS2D_TILES_PER_ROW;
	int rows = size ? bmp->height / size : 0;
	int i, tile, x, y;

	for (i = 0; i < PAISAGEM_TEX_COUNT; i++)
	{
		tile = PAISAGEM_TILE_BASE + i;
		x    = Atlas2D_TileX(tile) * size;
		y    = Atlas2D_TileY(tile);
		if (y >= rows) return;
		y   *= size;

		/* Don't overwrite textures that a texture pack provides */
		if (!IsTileBlank(bmp, x, y, size)) continue;
		DrawTile(bmp, x, y, size, paisagem_texData[i]);
	}
}


/*########################################################################################################################*
*--------------------------------------------------------Blocks-----------------------------------------------------------*
*#########################################################################################################################*/
static void DefineBlock(BlockID block, const struct PaisagemBlockDef* def) {
	cc_string name = String_FromReadonly(def->name);

	Block_ResetProps(block);
	Block_SetName(block, &name);

	Block_Tex(block, FACE_YMAX) = def->top;
	Block_SetSide(def->side, block);
	Block_Tex(block, FACE_YMIN) = def->bottom;

	Blocks.Collide[block]     = def->collide;
	Blocks.Draw[block]        = def->draw;
	Blocks.BlocksLight[block] = def->draw == DRAW_OPAQUE || def->draw == DRAW_TRANSPARENT_THICK;
	Blocks.DigSounds[block]   = def->sound;
	Blocks.StepSounds[block]  = def->sound;

	if (def->draw == DRAW_SPRITE) {
		Vec3_Set(Blocks.MinBB[block], 3.0f/16.0f, 0,           3.0f/16.0f);
		Vec3_Set(Blocks.MaxBB[block], 13.0f/16.0f, 13.0f/16.0f, 13.0f/16.0f);
	}
	Block_DefineCustom(block, true);
}

static void DefineBlocks(void) {
	BlockID block;
	int i;
	/* Multiplayer servers decide which blocks exist */
	if (!Server.IsSinglePlayer || !Game_AllowCustomBlocks) return;

	for (i = 0; i < Array_Elems(paisagem_blockDefs); i++)
	{
		block = (BlockID)(PBLOCK_GRAMA_ALTA + i);
		if (block > BLOCK_MAX_DEFINED) break;
		/* Saved worlds may already define the block (possibly differently) */
		if (Block_IsCustomDefined(block)) continue;
		DefineBlock(block, &paisagem_blockDefs[i]);
	}
}


/*########################################################################################################################*
*-------------------------------------------------------Generation--------------------------------------------------------*
*#########################################################################################################################*/
static RNGState gen_rnd;
static int gen_seed;
static BlockRaw GenGet(int x, int y, int z) { return Gen_Blocks[World_Pack(x, y, z)]; }
static void GenSet(int x, int y, int z, BlockRaw b) { Gen_Blocks[World_Pack(x, y, z)] = b; }

/* Only replaces air, so trees never cut into each other or the terrain */
static void GenLeaf(int x, int y, int z, BlockRaw b) {
	if (!World_Contains(x, y, z)) return;
	if (GenGet(x, y, z) == BLOCK_AIR) GenSet(x, y, z, b);
}

/* Smooth value noise in 0..1, varying over roughly 'scale' blocks */
static float Hash2D(int x, int z, int salt) {
	cc_uint32 h = (cc_uint32)(x * 374761393 + z * 668265263 + (gen_seed + salt) * 982451653);
	h = (h ^ (h >> 13)) * 1274126177u;
	h ^= h >> 16;
	return (h & 0xFFFF) / 65535.0f;
}

static float SmoothNoise(int x, int z, int scale, int salt) {
	int cx = x / scale, cz = z / scale;
	float fx = (float)(x % scale) / scale, fz = (float)(z % scale) / scale;
	float a = Hash2D(cx, cz, salt),     b = Hash2D(cx + 1, cz, salt);
	float c = Hash2D(cx, cz + 1, salt), d = Hash2D(cx + 1, cz + 1, salt);

	fx = fx * fx * (3 - 2 * fx);
	fz = fz * fz * (3 - 2 * fz);
	return (a + (b - a) * fx) * (1 - fz) + (c + (d - c) * fx) * fz;
}

/* Highest non-air block in the column, or -1 */
static int SurfaceY(int x, int z) {
	int y;
	for (y = World.MaxY; y >= 0; y--)
	{
		if (GenGet(x, y, z) != BLOCK_AIR) return y;
	}
	return -1;
}

static cc_bool TrunkFits(int x, int y, int z, int height) {
	int i;
	if (y + height + 2 > World.MaxY) return false;
	for (i = 0; i < height + 1; i++)
	{
		if (GenGet(x, y + i, z) != BLOCK_AIR) return false;
	}
	return true;
}

/* Keeps trees from growing right next to each other */
static cc_bool NearTree(int x, int y, int z) {
	int dx, dz, dy;
	BlockRaw b;
	for (dy = 0; dy <= 3; dy++)
		for (dz = -2; dz <= 2; dz++)
			for (dx = -2; dx <= 2; dx++)
			{
				if (!World_Contains(x + dx, y + dy, z + dz)) continue;
				b = GenGet(x + dx, y + dy, z + dz);
				if (b == BLOCK_LOG || b == PBLOCK_TRONCO_BETULA || b == PBLOCK_TRONCO_PINHEIRO || b == BLOCK_LEAVES ||
					b == PBLOCK_FOLHAS_BETULA || b == PBLOCK_FOLHAS_CEREJEIRA || b == PBLOCK_FOLHAS_PINHEIRO) return true;
			}
	return false;
}

static void Trunk(int x, int y, int z, int height, BlockRaw log) {
	int i;
	for (i = 0; i < height; i++) GenSet(x, y + i, z, log);
}

static void LeafDisc(int x, int y, int z, int radius, BlockRaw leaf, cc_bool roundCorners) {
	int dx, dz;
	for (dz = -radius; dz <= radius; dz++)
		for (dx = -radius; dx <= radius; dx++)
		{
			if (roundCorners && radius > 0 && Math_AbsI(dx) == radius && Math_AbsI(dz) == radius) {
				if (Random_Next(&gen_rnd, 3)) continue;
			}
			GenLeaf(x + dx, y, z + dz, leaf);
		}
}

static void GrowBirch(int x, int y, int z) {
	int h = 5 + Random_Next(&gen_rnd, 3);
	if (!TrunkFits(x, y, z, h)) return;

	LeafDisc(x, y + h - 3, z, 2, PBLOCK_FOLHAS_BETULA, true);
	LeafDisc(x, y + h - 2, z, 2, PBLOCK_FOLHAS_BETULA, true);
	LeafDisc(x, y + h - 1, z, 1, PBLOCK_FOLHAS_BETULA, false);
	LeafDisc(x, y + h,     z, 1, PBLOCK_FOLHAS_BETULA, true);
	Trunk(x, y, z, h, PBLOCK_TRONCO_BETULA);
}

static void GrowCherry(int x, int y, int z) {
	int h = 4 + Random_Next(&gen_rnd, 2);
	if (!TrunkFits(x, y, z, h + 1)) return;

	LeafDisc(x, y + h - 1, z, 3, PBLOCK_FOLHAS_CEREJEIRA, true);
	LeafDisc(x, y + h,     z, 3, PBLOCK_FOLHAS_CEREJEIRA, true);
	LeafDisc(x, y + h + 1, z, 2, PBLOCK_FOLHAS_CEREJEIRA, true);
	Trunk(x, y, z, h + 1, BLOCK_LOG);
}

static void GrowPine(int x, int y, int z) {
	int h = 7 + Random_Next(&gen_rnd, 3);
	int i, r;
	if (!TrunkFits(x, y, z, h)) return;

	/* Cone of alternating wide and narrow layers */
	for (i = 2; i <= h; i++)
	{
		r = (h - i) / 2;
		if (r > 2) r = 2;
		if ((i & 1) && r > 0) r--;
		LeafDisc(x, y + i, z, r, PBLOCK_FOLHAS_PINHEIRO, false);
	}
	GenLeaf(x, y + h + 1, z, PBLOCK_FOLHAS_PINHEIRO);
	Trunk(x, y, z, h, PBLOCK_TRONCO_PINHEIRO);
}

static void GrowBigOak(int x, int y, int z) {
	int h = 6 + Random_Next(&gen_rnd, 2);
	int dx, dy, dz;
	if (!TrunkFits(x, y, z, h)) return;

	for (dy = -2; dy <= 2; dy++)
		for (dz = -3; dz <= 3; dz++)
			for (dx = -3; dx <= 3; dx++)
			{
				if (dx * dx + dz * dz + dy * dy * 2 > 10 + Random_Next(&gen_rnd, 3)) continue;
				GenLeaf(x + dx, y + h - 1 + dy, z + dz, BLOCK_LEAVES);
			}
	Trunk(x, y, z, h, BLOCK_LOG);
}

static void GrowOak(int x, int y, int z) {
	IVec3 coords[TREE_MAX_COUNT];
	BlockRaw blocks[TREE_MAX_COUNT];
	int h = 4 + Random_Next(&gen_rnd, 3), count, i;

	Tree_Blocks = Gen_Blocks;
	Tree_Rnd    = &gen_rnd;
	if (!TreeGen_CanGrow(x, y, z, h)) return;

	count = TreeGen_Grow(x, y, z, h, coords, blocks);
	for (i = 0; i < count; i++)
	{
		GenSet(coords[i].x, coords[i].y, coords[i].z, blocks[i]);
	}
}

/* A low bush: a log stump with a few leaves around it */
static void GrowBush(int x, int y, int z) {
	if (!TrunkFits(x, y, z, 1)) return;
	GenSet(x, y, z, BLOCK_LOG);
	LeafDisc(x, y, z, 1, BLOCK_LEAVES, true);
	GenLeaf(x, y + 1, z, BLOCK_LEAVES);
}

static cc_bool NextToWater(int x, int y, int z) {
	static const int offsets[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
	BlockRaw b;
	int i;
	for (i = 0; i < 4; i++)
	{
		if (!World_ContainsXZ(x + offsets[i][0], z + offsets[i][1])) continue;
		b = GenGet(x + offsets[i][0], y, z + offsets[i][1]);
		if (b == BLOCK_WATER || b == BLOCK_STILL_WATER) return true;
	}
	return false;
}

static BlockRaw PickFlower(int x, int z) {
	static const BlockRaw flowers[5] = { BLOCK_DANDELION, BLOCK_ROSE, PBLOCK_TULIPA, PBLOCK_FLOR_AZUL, PBLOCK_MARGARIDAS };
	/* Each patch of flowers has mostly one kind */
	int kind = (int)(SmoothNoise(x, z, 9, 5) * 5.0f * 1.3f) % 5;
	return flowers[kind];
}

static void DecorateColumn(int x, int z, int waterLevel) {
	int y = SurfaceY(x, z);
	float forest, kind, flowers, r;
	BlockRaw ground;

	if (y < 1 || y + 2 > World.MaxY) return;
	ground = GenGet(x, y, z);

	/* Sugar cane on banks right next to water */
	if ((ground == BLOCK_GRASS || ground == BLOCK_SAND || ground == BLOCK_DIRT) && y == waterLevel && NextToWater(x, y, z)) {
		if (Random_Next(&gen_rnd, 5) == 0) {
			int i, h = 1 + Random_Next(&gen_rnd, 3);
			for (i = 1; i <= h; i++) GenSet(x, y + i, z, PBLOCK_CANA);
		}
		return;
	}
	if (ground != BLOCK_GRASS) return;

	/* Snowy peaks */
	if (y >= waterLevel + 13) {
		if (Random_Next(&gen_rnd, 30) == 0 && !NearTree(x, y + 1, z)) { GrowPine(x, y + 1, z); return; }
		GenSet(x, y + 1, z, BLOCK_SNOW);
		return;
	}

	forest  = SmoothNoise(x, z, 22, 1);
	kind    = SmoothNoise(x, z, 37, 2);
	flowers = SmoothNoise(x, z, 11, 3);
	r       = Random_Float(&gen_rnd);

	/* Trees, denser in forests */
	if (r < forest * forest * 0.09f && !NearTree(x, y + 1, z)) {
		if (y >= waterLevel + 9)  { GrowPine(x, y + 1, z); return; }
		if (kind < 0.30f)         { GrowBirch(x, y + 1, z); return; }
		if (kind > 0.78f)         { GrowCherry(x, y + 1, z); return; }
		if (Random_Next(&gen_rnd, 6) == 0) { GrowBigOak(x, y + 1, z); return; }
		GrowOak(x, y + 1, z);
		return;
	}
	r = Random_Float(&gen_rnd);

	if (GenGet(x, y + 1, z) != BLOCK_AIR) return;
	if (r < 0.004f && forest > 0.4f && !NearTree(x, y + 1, z)) { GrowBush(x, y + 1, z); return; }
	if (r < 0.007f)            { GenSet(x, y + 1, z, PBLOCK_ARBUSTO); return; }
	if (r < 0.0085f)           { GenSet(x, y + 1, z, PBLOCK_ABOBORA); return; }
	if (flowers > 0.72f && r < 0.30f) { GenSet(x, y + 1, z, PickFlower(x, z)); return; }

	/* Tall grass everywhere, ferns in the shade of forests */
	if (r < 0.18f + forest * 0.30f) {
		GenSet(x, y + 1, z, (forest > 0.55f && Random_Next(&gen_rnd, 3) == 0) ? PBLOCK_SAMAMBAIA : PBLOCK_GRAMA_ALTA);
	}
}

void Paisagem_GenOverworld(void) {
	int x, z, waterLevel = World.Height / 2;
	if (!Game_AllowCustomBlocks || PBLOCK_FIM > BLOCK_MAX_DEFINED) return;
	/* Only the normal world, not the Nether, the End or the other dimensions */
	if (Minerios_DimensaoGerando() > 0) return;

	Gen_CurrentState = "Deixando o mundo mais vivo";
	Random_SeedFromCurrentTime(&gen_rnd);
	gen_seed = Random_Next(&gen_rnd, 0x7FFFFFFF);

	for (z = 0; z < World.Length; z++)
	{
		Gen_CurrentProgress = (float)z / World.Length;
		for (x = 0; x < World.Width; x++)
		{
			DecorateColumn(x, z, waterLevel);
		}
	}
}


/*########################################################################################################################*
*-------------------------------------------------------Component---------------------------------------------------------*
*#########################################################################################################################*/
static void OnNewMapLoaded(void) { DefineBlocks(); }

struct IGameComponent Paisagem_Component = {
	NULL,          /* Init  */
	NULL,          /* Free  */
	NULL,          /* Reset */
	NULL,          /* OnNewMap */
	OnNewMapLoaded /* OnNewMapLoaded */
};

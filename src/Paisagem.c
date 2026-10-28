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
#include "LateGame.h"
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
{ "Emblema das Plantas",   T(EMBLEM_PLANTA),   T(EMBLEM_PLANTA),    T(EMBLEM_PLANTA),   DRAW_OPAQUE,            COLLIDE_SOLID, SOUND_STONE },
{ "Emblema das Aguas",     T(EMBLEM_AGUA),     T(EMBLEM_AGUA),      T(EMBLEM_AGUA),     DRAW_OPAQUE,            COLLIDE_SOLID, SOUND_STONE },
{ "Emblema das Pedras",    T(EMBLEM_PEDRA),    T(EMBLEM_PEDRA),     T(EMBLEM_PEDRA),    DRAW_OPAQUE,            COLLIDE_SOLID, SOUND_STONE },
{ "Emblema dos Raios",     T(EMBLEM_ELETRICO), T(EMBLEM_ELETRICO),  T(EMBLEM_ELETRICO), DRAW_OPAQUE,            COLLIDE_SOLID, SOUND_STONE },
{ "Emblema dos Ventos",    T(EMBLEM_VENTO),    T(EMBLEM_VENTO),     T(EMBLEM_VENTO),    DRAW_OPAQUE,            COLLIDE_SOLID, SOUND_STONE },
{ "Emblema do Fogo",       T(EMBLEM_FOGO),     T(EMBLEM_FOGO),      T(EMBLEM_FOGO),     DRAW_OPAQUE,            COLLIDE_SOLID, SOUND_STONE },
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
	/* Gym emblems glow a little and can't be broken */
	if (block >= PBLOCK_EMBLEMA_PLANTA && block <= PBLOCK_EMBLEMA_FOGO) {
		Blocks.Brightness[block] = FANCY_LIGHTING_MAX_LEVEL / 2;
		Blocks.CanDelete[block]  = false;
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
static cc_bool NearStructure(int x, int z, int margin);

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
		if (Random_Next(&gen_rnd, 30) == 0 && !NearTree(x, y + 1, z) && !NearStructure(x, z, 5)) { GrowPine(x, y + 1, z); return; }
		GenSet(x, y + 1, z, BLOCK_SNOW);
		return;
	}

	forest  = SmoothNoise(x, z, 22, 1);
	kind    = SmoothNoise(x, z, 37, 2);
	flowers = SmoothNoise(x, z, 11, 3);
	r       = Random_Float(&gen_rnd);

	/* Trees, denser in forests */
	if (r < forest * forest * 0.09f && !NearTree(x, y + 1, z) && !NearStructure(x, z, 5)) {
		if (y >= waterLevel + 9)  { GrowPine(x, y + 1, z); return; }
		if (kind < 0.30f)         { GrowBirch(x, y + 1, z); return; }
		if (kind > 0.78f)         { GrowCherry(x, y + 1, z); return; }
		if (Random_Next(&gen_rnd, 6) == 0) { GrowBigOak(x, y + 1, z); return; }
		GrowOak(x, y + 1, z);
		return;
	}
	r = Random_Float(&gen_rnd);

	if (GenGet(x, y + 1, z) != BLOCK_AIR) return;
	if (r < 0.004f && forest > 0.4f && !NearTree(x, y + 1, z) && !NearStructure(x, z, 3)) { GrowBush(x, y + 1, z); return; }
	if (r < 0.007f)            { GenSet(x, y + 1, z, PBLOCK_ARBUSTO); return; }
	if (r < 0.0085f)           { GenSet(x, y + 1, z, PBLOCK_ABOBORA); return; }
	if (flowers > 0.72f && r < 0.30f) { GenSet(x, y + 1, z, PickFlower(x, z)); return; }

	/* Tall grass everywhere, ferns in the shade of forests */
	if (r < 0.18f + forest * 0.30f) {
		GenSet(x, y + 1, z, (forest > 0.55f && Random_Next(&gen_rnd, 3) == 0) ? PBLOCK_SAMAMBAIA : PBLOCK_GRAMA_ALTA);
	}
}

/*########################################################################################################################*
*-------------------------------------------------------Underground-------------------------------------------------------*
*#########################################################################################################################*/
static cc_bool IsFluid(BlockRaw b) {
	return b == BLOCK_WATER || b == BLOCK_STILL_WATER || b == BLOCK_LAVA || b == BLOCK_STILL_LAVA;
}

/* Turns a block into air, unless that would let water or lava flow into the hole */
static void CarveAir(int x, int y, int z) {
	static const int offsets[5][3] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,0,1}, {0,0,-1} };
	BlockRaw b;
	int i;
	if (y < 2 || !World_Contains(x, y, z)) return;
	b = GenGet(x, y, z);
	if (b == BLOCK_AIR || b == BLOCK_BEDROCK || IsFluid(b)) return;

	for (i = 0; i < 5; i++)
	{
		if (!World_Contains(x + offsets[i][0], y + offsets[i][1], z + offsets[i][2])) continue;
		if (IsFluid(GenGet(x + offsets[i][0], y + offsets[i][1], z + offsets[i][2]))) return;
	}
	GenSet(x, y, z, BLOCK_AIR);
}

/* Big open caverns deep underground, some with a lava pool on the floor */
static void GenCaverns(int waterLevel) {
	int count = World.Volume / 140000 + 2;
	int i, x, y, z, cx, cy, cz, rx, ry, rz;
	float dx, dy, dz;

	for (i = 0; i < count; i++)
	{
		rx = 5 + Random_Next(&gen_rnd, 5);
		rz = 5 + Random_Next(&gen_rnd, 5);
		ry = 3 + Random_Next(&gen_rnd, 2);
		cx = Random_Next(&gen_rnd, World.Width);
		cz = Random_Next(&gen_rnd, World.Length);
		cy = 6 + ry + Random_Next(&gen_rnd, max(1, waterLevel - 14 - ry));

		for (y = cy - ry; y <= cy + ry; y++)
			for (z = cz - rz; z <= cz + rz; z++)
				for (x = cx - rx; x <= cx + rx; x++)
				{
					dx = (float)(x - cx) / rx; dy = (float)(y - cy) / ry; dz = (float)(z - cz) / rz;
					/* bumpy walls */
					if (dx * dx + dy * dy + dz * dz > 1.0f - Random_Float(&gen_rnd) * 0.15f) continue;
					CarveAir(x, y, z);
				}

		/* A small lava pool in some of the caverns */
		if (Random_Next(&gen_rnd, 3) == 0) {
			y = cy - ry + 1;
			for (z = cz - 2; z <= cz + 2; z++)
				for (x = cx - 2; x <= cx + 2; x++)
				{
					if (!World_Contains(x, y, z) || GenGet(x, y, z) != BLOCK_AIR) continue;
					if (GenGet(x, y - 1, z) == BLOCK_AIR) continue;
					GenSet(x, y, z, BLOCK_STILL_LAVA);
				}
		}
	}
}

/* Ravines: long, deep cracks that open caves to the surface */
static void GenRavines(void) {
	int count = 1 + World.Width * World.Length / 12000;
	int i, j, len, x, y, z, top, w, dx, dz;
	float px, pz, ang;

	for (i = 0; i < count; i++)
	{
		px  = (float)Random_Next(&gen_rnd, World.Width);
		pz  = (float)Random_Next(&gen_rnd, World.Length);
		ang = Random_Float(&gen_rnd) * 2 * MATH_PI;
		len = 30 + Random_Next(&gen_rnd, 40);

		for (j = 0; j < len; j++)
		{
			px  += Math_CosF(ang); pz += Math_SinF(ang);
			ang += (Random_Float(&gen_rnd) - 0.5f) * 0.4f;
			x = (int)px; z = (int)pz;
			if (!World_ContainsXZ(x, z)) break;

			/* narrow at the ends, widest in the middle */
			w   = 1 + (int)(2.5f * Math_SinF(j * MATH_PI / len));
			top = SurfaceY(x, z);
			for (dz = -w; dz <= w; dz++)
				for (dx = -w; dx <= w; dx++)
					for (y = 5 + Random_Next(&gen_rnd, 3); y <= top; y++)
					{
						CarveAir(x + dx, y, z + dz);
					}
		}
	}
}

/* Ores and other details on the walls of caves, so exploring them is worth it */
static void DecorateCaveWalls(int waterLevel) {
	int x, y, z, r;
	BlockRaw b;

	for (y = 2; y < waterLevel - 2; y++)
		for (z = 1; z < World.Length - 1; z++)
			for (x = 1; x < World.Width - 1; x++)
			{
				if (GenGet(x, y, z) != BLOCK_STONE) continue;
				/* Only stone that touches the air of a cave */
				if (GenGet(x + 1, y, z) != BLOCK_AIR && GenGet(x - 1, y, z) != BLOCK_AIR &&
					GenGet(x, y, z + 1) != BLOCK_AIR && GenGet(x, y, z - 1) != BLOCK_AIR &&
					GenGet(x, y + 1, z) != BLOCK_AIR && GenGet(x, y - 1, z) != BLOCK_AIR) continue;

				r = Random_Next(&gen_rnd, 1000);
				b = BLOCK_STONE;
				if      (r < 30)  b = BLOCK_COAL_ORE;
				else if (r < 48)  b = BLOCK_IRON_ORE;
				else if (r < 54)  b = BLOCK_GOLD_ORE;
				else if (r < 57 && y < 14) b = LBLOCK_DIAMOND_ORE;
				else if (r < 61 && Minerios_Enabled) b = MBLOCK_RUBI_MINERIO;
				else if (r < 63 && Minerios_Enabled) b = MBLOCK_TITANIO_MINERIO;
				else if (r < 64 && Minerios_Enabled && y < 16) b = MBLOCK_URANIO_MINERIO;
				else if (r < 90)  b = BLOCK_MOSSY_ROCKS;
				else if (r < 104) b = BLOCK_GRAVEL;
				/* Glowing stone on cave ceilings lights up the way */
				else if (r < 108 && GenGet(x, y - 1, z) == BLOCK_AIR) b = LBLOCK_GLOWSTONE;
				GenSet(x, y, z, b);
			}
}


/*########################################################################################################################*
*-------------------------------------------------------Structures--------------------------------------------------------*
*#########################################################################################################################*/
#define MAX_STRUCTURES 16
static int structX[MAX_STRUCTURES], structZ[MAX_STRUCTURES], structSize[MAX_STRUCTURES], structCount;

/* Whether the column is on or near a structure (so no trees grow in front of a door) */
static cc_bool NearStructure(int x, int z, int margin) {
	int i;
	for (i = 0; i < structCount; i++)
	{
		if (x >= structX[i] - margin && x < structX[i] + structSize[i] + margin &&
			z >= structZ[i] - margin && z < structZ[i] + structSize[i] + margin) return true;
	}
	return false;
}

/* Highest block in the column that is real ground (not a plant, log or leaves), or -1 */
static int GroundY(int x, int z) {
	int y;
	BlockRaw b;
	for (y = World.MaxY; y >= 0; y--)
	{
		b = GenGet(x, y, z);
		if (b == BLOCK_AIR || b == BLOCK_LOG || b == BLOCK_LEAVES || Blocks.Draw[b] == DRAW_SPRITE) continue;
		return y;
	}
	return -1;
}

/* Finds a dry, fairly flat spot of the given size far from other structures. Returns floor y or -1 */
static int FindSite(int size, int* outX, int* outZ, int waterLevel) {
	int tries, i, x, z, dx, dz, y, h, minH, maxH;
	cc_bool ok;

	for (tries = 0; tries < 250; tries++)
	{
		x = 4 + Random_Next(&gen_rnd, World.Width  - size - 8);
		z = 4 + Random_Next(&gen_rnd, World.Length - size - 8);
		ok = true;

		for (i = 0; i < structCount; i++)
		{
			if (Math_AbsI(structX[i] - x) < 22 && Math_AbsI(structZ[i] - z) < 22) { ok = false; break; }
		}
		if (!ok) continue;

		minH = World.Height; maxH = 0;
		for (dz = 0; dz < size && ok; dz += 2)
			for (dx = 0; dx < size && ok; dx += 2)
			{
				h = GroundY(x + dx, z + dz);
				if (h <= waterLevel || (GenGet(x + dx, h, z + dz) != BLOCK_GRASS && GenGet(x + dx, h, z + dz) != BLOCK_DIRT)) ok = false;
				minH = min(minH, h); maxH = max(maxH, h);
			}
		if (!ok || maxH - minH > 4) continue;

		y = minH + 1;
		if (y + 10 > World.MaxY) continue;
		*outX = x; *outZ = z;
		if (structCount < MAX_STRUCTURES) {
			structX[structCount] = x; structZ[structCount] = z; structSize[structCount] = size; structCount++;
		}
		return y;
	}
	return -1;
}

/* Flattens the ground under a structure and clears the space above it */
static void PrepareSite(int x0, int y, int z0, int size, int height) {
	int x, z, yy;
	for (z = z0; z < z0 + size; z++)
		for (x = x0; x < x0 + size; x++)
		{
			for (yy = y - 1; yy >= 1; yy--)
			{
				BlockRaw b = GenGet(x, yy, z);
				if (b != BLOCK_AIR && !IsFluid(b) && Blocks.Draw[b] != DRAW_SPRITE) break;
				GenSet(x, yy, z, BLOCK_DIRT);
			}
			GenSet(x, y - 1, z, BLOCK_GRASS);
			for (yy = y; yy < y + height && yy <= World.MaxY; yy++) GenSet(x, yy, z, BLOCK_AIR);
		}

	/* Clear trees right around it, so the building can be seen */
	for (z = z0 - 3; z < z0 + size + 3; z++)
		for (x = x0 - 3; x < x0 + size + 3; x++)
			for (yy = y - 2; yy < y + height + 6 && yy <= World.MaxY; yy++)
			{
				if (!World_Contains(x, yy, z)) continue;
				if (GenGet(x, yy, z) == BLOCK_LOG || GenGet(x, yy, z) == BLOCK_LEAVES) GenSet(x, yy, z, BLOCK_AIR);
			}
}

struct GymStyle { BlockRaw floor, wall, corner, roof, window, deco; };
/* Same order as mestres_def: plants, water, rocks, lightning, wind, fire */
static const struct GymStyle gymStyles[6] = {
	{ BLOCK_WOOD,        BLOCK_WOOD,        BLOCK_LOG,         BLOCK_LEAVES,      BLOCK_GLASS, BLOCK_ROSE        },
	{ BLOCK_CYAN,        BLOCK_BLUE,        BLOCK_STONE_BRICK, BLOCK_DEEP_BLUE,   BLOCK_GLASS, BLOCK_STILL_WATER },
	{ BLOCK_COBBLE,      BLOCK_STONE_BRICK, BLOCK_COBBLE,      BLOCK_COBBLE_SLAB, BLOCK_GLASS, BLOCK_GRAVEL      },
	{ BLOCK_GOLD,        BLOCK_YELLOW,      BLOCK_IRON,        BLOCK_IRON,        BLOCK_GLASS, BLOCK_IRON        },
	{ BLOCK_WHITE,       BLOCK_WHITE,       BLOCK_PILLAR,      BLOCK_AQUA,        BLOCK_GLASS, BLOCK_GLASS       },
	{ BLOCK_BRICK,       BLOCK_RED,         BLOCK_BRICK,       BLOCK_MAGMA,       BLOCK_GLASS, BLOCK_STILL_LAVA  },
};

/* A small arena house for one Mestre de Bioma: walls, windows, roof, door and the emblem */
static void BuildGym(int idx, int waterLevel) {
	const struct GymStyle* st = &gymStyles[idx];
	int size = 9, wallH = 4, x0, z0, y, x, z, yy;
	cc_bool edgeX, edgeZ;

	y = FindSite(size, &x0, &z0, waterLevel);
	if (y < 0) return;
	PrepareSite(x0, y, z0, size, wallH + 6);

	for (z = 0; z < size; z++)
		for (x = 0; x < size; x++)
		{
			edgeX = x == 0 || x == size - 1;
			edgeZ = z == 0 || z == size - 1;
			GenSet(x0 + x, y - 1, z0 + z, st->floor);

			for (yy = 0; yy < wallH; yy++)
			{
				if (!edgeX && !edgeZ) continue;
				if (edgeX && edgeZ) { GenSet(x0 + x, y + yy, z0 + z, st->corner); continue; }
				/* windows in the middle of the walls */
				if (yy == 2 && ((edgeX && (z == 2 || z == 6)) || (edgeZ && (x == 2 || x == 6)))) {
					GenSet(x0 + x, y + yy, z0 + z, st->window); continue;
				}
				GenSet(x0 + x, y + yy, z0 + z, st->wall);
			}
		}

	/* Stepped roof */
	for (yy = 0; yy < 3; yy++)
		for (z = yy; z < size - yy; z++)
			for (x = yy; x < size - yy; x++)
			{
				if (yy < 2 && x > yy && x < size - 1 - yy && z > yy && z < size - 1 - yy) continue;
				GenSet(x0 + x, y + wallH + yy, z0 + z, st->roof);
			}
	GenSet(x0 + 4, y + wallH + 3, z0 + 4, st->roof);

	/* Door (2 high) facing -Z and a path out front */
	GenSet(x0 + 4, y,     z0, BLOCK_AIR);
	GenSet(x0 + 4, y + 1, z0, BLOCK_AIR);
	if (z0 > 0) GenSet(x0 + 4, y - 1, z0 - 1, BLOCK_GRAVEL);

	/* Emblem at the back on a pedestal, with decorations on both sides */
	GenSet(x0 + 4, y,     z0 + size - 2, st->corner);
	GenSet(x0 + 4, y + 1, z0 + size - 2, (BlockRaw)(PBLOCK_EMBLEMA_PLANTA + idx));
	if (Blocks.Draw[st->deco] == DRAW_SPRITE || st->deco == BLOCK_STILL_WATER || st->deco == BLOCK_STILL_LAVA) {
		/* sunken into the floor so liquids can't spread */
		GenSet(x0 + 2, y - 1, z0 + size - 2, st->deco == BLOCK_ROSE ? BLOCK_GRASS : st->deco);
		GenSet(x0 + 6, y - 1, z0 + size - 2, st->deco == BLOCK_ROSE ? BLOCK_GRASS : st->deco);
		if (st->deco == BLOCK_ROSE) {
			GenSet(x0 + 2, y, z0 + size - 2, BLOCK_ROSE);
			GenSet(x0 + 6, y, z0 + size - 2, PBLOCK_TULIPA);
		}
	} else {
		GenSet(x0 + 2, y, z0 + size - 2, st->deco);
		GenSet(x0 + 6, y, z0 + size - 2, st->deco);
	}
	/* Lights inside */
	GenSet(x0 + 1, y + wallH - 1, z0 + 1, LBLOCK_GLOWSTONE);
	GenSet(x0 + size - 2, y + wallH - 1, z0 + size - 2, LBLOCK_GLOWSTONE);
}

/* Ruins with the altar of the first boss of Minerios & Monstros (place the altar to call it) */
static void BuildAltarRuin(int waterLevel) {
	int size = 7, x0, z0, y, x, z, h;
	y = FindSite(size, &x0, &z0, waterLevel);
	if (y < 0) return;
	PrepareSite(x0, y, z0, size, 6);

	for (z = 0; z < size; z++)
		for (x = 0; x < size; x++)
		{
			GenSet(x0 + x, y - 1, z0 + z, Random_Next(&gen_rnd, 3) ? BLOCK_STONE_BRICK : BLOCK_MOSSY_ROCKS);
		}
	/* Broken pillars in the corners */
	for (z = 0; z < size; z += size - 1)
		for (x = 0; x < size; x += size - 1)
			for (h = 0; h < 1 + Random_Next(&gen_rnd, 4); h++)
			{
				GenSet(x0 + x, y + h, z0 + z, h ? BLOCK_MOSSY_ROCKS : BLOCK_PILLAR);
			}

	GenSet(x0 + 3, y, z0 + 3, BLOCK_OBSIDIAN);
	GenSet(x0 + 3, y + 1, z0 + 3, MBLOCK_ALTAR_RAINHA);
	GenSet(x0 + 1, y, z0 + 3, MBLOCK_SORTE);
	GenSet(x0 + 5, y, z0 + 3, MBLOCK_RUBI_MINERIO);
}

/* A dragon nest: a ring of stones around a dragon egg (it hatches into a dragon) */
static void BuildDragonNest(int waterLevel) {
	int size = 7, x0, z0, y, x, z;
	float d;
	y = FindSite(size, &x0, &z0, waterLevel);
	if (y < 0) return;
	PrepareSite(x0, y, z0, size, 4);

	for (z = 0; z < size; z++)
		for (x = 0; x < size; x++)
		{
			d = Math_SqrtF((float)((x - 3) * (x - 3) + (z - 3) * (z - 3)));
			if (d > 3.4f) continue;
			GenSet(x0 + x, y - 1, z0 + z, d < 2.2f ? BLOCK_GRAVEL : BLOCK_COBBLE);
			if (d >= 2.4f && Random_Next(&gen_rnd, 3)) GenSet(x0 + x, y, z0 + z, BLOCK_OBSIDIAN);
		}
	GenSet(x0 + 3, y, z0 + 3, MBLOCK_OVO_DRAGAO);
	GenSet(x0 + 2, y, z0 + 3, BLOCK_LOG);
	GenSet(x0 + 4, y, z0 + 4, BLOCK_LOG);
}

/* A watchtower with ores to collect and a lucky block on top */
static void BuildOreTower(int waterLevel) {
	int size = 5, x0, z0, y, x, z, h, height = 8;
	cc_bool edge;
	y = FindSite(size, &x0, &z0, waterLevel);
	if (y < 0) return;
	PrepareSite(x0, y, z0, size, height + 3);

	for (h = 0; h < height; h++)
		for (z = 0; z < size; z++)
			for (x = 0; x < size; x++)
			{
				edge = x == 0 || z == 0 || x == size - 1 || z == size - 1;
				if (!edge) continue;
				if (h < 2 && x == 2 && z == 0) continue; /* door */
				GenSet(x0 + x, y + h, z0 + z, (h == 3 || h == 6) && (x == 2 || z == 2) ? BLOCK_GLASS : BLOCK_COBBLE);
			}
	for (z = 0; z < size; z++)
		for (x = 0; x < size; x++)
		{
			GenSet(x0 + x, y + height, z0 + z, BLOCK_WOOD);
			if (x == 0 || z == 0 || x == size - 1 || z == size - 1) {
				if ((x + z) % 2 == 0) GenSet(x0 + x, y + height + 1, z0 + z, BLOCK_COBBLE);
			}
		}
	/* Ores to mine on the ground floor */
	GenSet(x0 + 1, y, z0 + 3, BLOCK_IRON_ORE);
	GenSet(x0 + 3, y, z0 + 3, BLOCK_GOLD_ORE);
	GenSet(x0 + 1, y, z0 + 1, BLOCK_COAL_ORE);
	if (Minerios_Enabled) {
		GenSet(x0 + 3, y, z0 + 1, MBLOCK_TITANIO_MINERIO);
		GenSet(x0 + 2, y + height + 1, z0 + 2, MBLOCK_SORTE);
	} else {
		GenSet(x0 + 3, y, z0 + 1, BLOCK_IRON_ORE);
	}
}

static void GenStructures(int waterLevel) {
	int i;
	structCount = 0;
	Gen_CurrentState = "Construindo ginasios e ruinas";

	for (i = 0; i < 6; i++) BuildGym(i, waterLevel);
	BuildOreTower(waterLevel);
	if (!Minerios_Enabled) return;

	BuildAltarRuin(waterLevel);
	BuildDragonNest(waterLevel);
	BuildOreTower(waterLevel);
}

int Paisagem_AcharGinasio(int x, int y, int z, int radius, IVec3* pos) {
	int xx, yy, zz;
	BlockID b;

	for (yy = y - 8; yy <= y + 8; yy++)
		for (zz = z - radius; zz <= z + radius; zz++)
			for (xx = x - radius; xx <= x + radius; xx++)
			{
				if (!World_Contains(xx, yy, zz)) continue;
				b = World_GetBlock(xx, yy, zz);
				if (b < PBLOCK_EMBLEMA_PLANTA || b > PBLOCK_EMBLEMA_FOGO) continue;
				pos->x = xx; pos->y = yy; pos->z = zz;
				return b - PBLOCK_EMBLEMA_PLANTA;
			}
	return -1;
}

void Paisagem_GenOverworld(void) {
	int x, z, waterLevel = World.Height / 2;
	if (!Game_AllowCustomBlocks || PBLOCK_FIM > BLOCK_MAX_DEFINED) return;
	/* Only the normal world, not the Nether, the End or the other dimensions */
	if (Minerios_DimensaoGerando() > 0) return;

	Gen_CurrentState = "Deixando o mundo mais vivo";
	Random_SeedFromCurrentTime(&gen_rnd);
	gen_seed = Random_Next(&gen_rnd, 0x7FFFFFFF);

	Gen_CurrentState = "Abrindo cavernas";
	GenCaverns(waterLevel);
	GenRavines();
	DecorateCaveWalls(waterLevel);
	GenStructures(waterLevel);

	Gen_CurrentState = "Deixando o mundo mais vivo";

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

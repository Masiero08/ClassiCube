#include "Minerios.h"
#include "Block.h"
#include "BlockID.h"
#include "Bitmap.h"
#include "Game.h"
#include "Server.h"
#include "TexturePack.h"
#include "Lighting.h"
#include "Audio.h"
#include "Funcs.h"
#include "String_.h"
#include "Entity.h"
#include "EntityComponents.h"
#include "EntityRenderers.h"
#include "Model.h"
#include "World.h"
#include "Chat.h"
#include "Event.h"
#include "ExtMath.h"
#include "Commands.h"
#include "Graphics.h"
#include "Utils.h"
#include "Options.h"
#include "Platform.h"
#include "Picking.h"
#include "Inventory.h"
#include "Generator.h"
#include "Formats.h"
#include "Deflate.h"
#include "Stream.h"
#include "Survival.h"
#include "Errors.h"
#include "Logger.h"
#include "Minerios_tex.h"

/* Minerios & Monstros: um mod pequeno inspirado no OreSpawn.
   Os nomes e as texturas sao originais (misc/minerios/gen_textures.py).
   - Rubi, titanio e uranio: minerio (aparece em mundos novos) + bloco
   - Minerio de ovo: ao quebrar, choca uma criatura; as raras sao gigantes
   - Espadas (titanio, suprema, Big Bertha) e armadura de titanio
   - Mosquitos: enxames que voam em volta do jogador e picam
   - Dimensoes Utopia e Cristal, com portais de ida e volta
*/
cc_bool Minerios_Enabled = true;
static RNGState rnd;
static cc_bool Ativo(void) { return Minerios_Enabled && Server.IsSinglePlayer && World.Loaded; }


/*########################################################################################################################*
*--------------------------------------------------------Blocos-----------------------------------------------------------*
*#########################################################################################################################*/
/* Tiles livres do atlas (vazios no pacote padrao e dentro das 8 linhas do atlas reserva). */
/* 96-126 ficam com os blocos modernos. */
static const cc_uint8 minerios_tiles[MINERIOS_TEX_COUNT] = {
	88, 89, 90, 91, 92, 93, 94, 87, 95, 127, 27, 43, 44, 45, 46, 59, 60, 61, 62
};
#define T(tex) MTX_##tex

struct MinerioDef {
	const char* name;
	cc_uint8 top, side, bottom;
	cc_uint8 draw, collide, sound, brightness;
};

static const struct MinerioDef minerio_defs[MBLOCK_FIM - MBLOCK_RUBI_MINERIO] = {
/*NAME                     TOP                     SIDE                     BOTTOM                  DRAW              COLLIDE        SOUND        BRIGHTNESS */
{ "Minerio de rubi",       T(ORE_RUBY),            T(ORE_RUBY),             T(ORE_RUBY),            DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Minerio de titanio",    T(ORE_TITANIUM),        T(ORE_TITANIUM),         T(ORE_TITANIUM),        DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Minerio de uranio",     T(ORE_URANIUM),         T(ORE_URANIUM),          T(ORE_URANIUM),         DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_STONE, FANCY_LIGHTING_MAX_LEVEL / 2 },
{ "Minerio de ovo",        T(ORE_EGG),             T(ORE_EGG),              T(ORE_EGG),             DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Bloco de rubi",         T(BLOCK_RUBY),          T(BLOCK_RUBY),           T(BLOCK_RUBY),          DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_GLASS, 0 },
{ "Bloco de titanio",      T(BLOCK_TITANIUM),      T(BLOCK_TITANIUM),       T(BLOCK_TITANIUM),      DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_METAL, 0 },
{ "Bloco de uranio",       T(BLOCK_URANIUM),       T(BLOCK_URANIUM),        T(BLOCK_URANIUM),       DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_METAL, FANCY_LIGHTING_MAX_LEVEL },
{ "Espada de titanio",     T(SWORD_TITANIUM),      T(SWORD_TITANIUM),       T(SWORD_TITANIUM),      DRAW_SPRITE,      COLLIDE_NONE,  SOUND_METAL, 0 },
{ "Espada suprema",        T(SWORD_SUPREME),       T(SWORD_SUPREME),        T(SWORD_SUPREME),       DRAW_SPRITE,      COLLIDE_NONE,  SOUND_METAL, 0 },
{ "Big Bertha",            T(BIG_BERTHA),          T(BIG_BERTHA),           T(BIG_BERTHA),          DRAW_SPRITE,      COLLIDE_NONE,  SOUND_METAL, 0 },
{ "Armadura de titanio",   T(ARMOR_TITANIUM),      T(ARMOR_TITANIUM),       T(ARMOR_TITANIUM),      DRAW_SPRITE,      COLLIDE_NONE,  SOUND_METAL, 0 },
{ "Portal da Utopia",      T(PORTAL_UTOPIA),       T(PORTAL_UTOPIA),        T(PORTAL_UTOPIA),       DRAW_TRANSLUCENT, COLLIDE_NONE,  SOUND_GLASS, FANCY_LIGHTING_MAX_LEVEL },
{ "Portal de Cristal",     T(PORTAL_CRYSTAL),      T(PORTAL_CRYSTAL),       T(PORTAL_CRYSTAL),      DRAW_TRANSLUCENT, COLLIDE_NONE,  SOUND_GLASS, FANCY_LIGHTING_MAX_LEVEL },
{ "Pedra de cristal",      T(CRYSTAL_STONE),       T(CRYSTAL_STONE),        T(CRYSTAL_STONE),       DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_STONE, 0 },
{ "Grama de cristal",      T(CRYSTAL_GRASS_TOP),   T(CRYSTAL_GRASS_SIDE),   T(CRYSTAL_STONE),       DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_GRASS, 0 },
{ "Tronco de cristal",     T(CRYSTAL_LOG),         T(CRYSTAL_LOG),          T(CRYSTAL_LOG),         DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_WOOD,  0 },
{ "Folhas de cristal",     T(CRYSTAL_LEAVES),      T(CRYSTAL_LEAVES),       T(CRYSTAL_LEAVES),      DRAW_TRANSPARENT, COLLIDE_SOLID, SOUND_GRASS, 0 },
{ "Cristal",               T(CRYSTAL_CLUSTER),     T(CRYSTAL_CLUSTER),      T(CRYSTAL_CLUSTER),     DRAW_SPRITE,      COLLIDE_NONE,  SOUND_GLASS, FANCY_LIGHTING_MAX_LEVEL },
};
#define MINERIOS_COUNT Array_Elems(minerio_defs)
#define Def(block) (&minerio_defs[(block) - MBLOCK_RUBI_MINERIO])

static cc_bool EhItem(BlockID b) { return b >= MITEM_ESPADA_TITANIO && b <= MITEM_ARMADURA_TITANIO; }

const struct MinerioVeio Minerios_Veios[] = {
	{ 0.30f, "Carving ruby ore",     MBLOCK_RUBI_MINERIO    },
	{ 0.35f, "Carving titanium ore", MBLOCK_TITANIO_MINERIO },
	{ 0.25f, "Carving uranium ore",  MBLOCK_URANIO_MINERIO  },
	{ 0.30f, "Carving egg ore",      MBLOCK_OVO_MINERIO     },
};
const int Minerios_VeiosCount = Array_Elems(Minerios_Veios);

static void DefineBlock(BlockID block, const struct MinerioDef* def) {
	cc_string name = String_FromReadonly(def->name);

	Block_ResetProps(block);
	Block_SetName(block, &name);
	Block_Tex(block, FACE_YMAX) = minerios_tiles[def->top];
	Block_SetSide(minerios_tiles[def->side], block);
	Block_Tex(block, FACE_YMIN) = minerios_tiles[def->bottom];

	Blocks.Collide[block]     = def->collide;
	Blocks.Draw[block]        = def->draw;
	Blocks.BlocksLight[block] = def->draw == DRAW_OPAQUE;
	Blocks.Brightness[block]  = def->brightness;
	Blocks.DigSounds[block]   = def->sound;
	Blocks.StepSounds[block]  = def->sound == SOUND_GLASS ? SOUND_STONE : def->sound;

	if (def->draw == DRAW_SPRITE) {
		Vec3_Set(Blocks.MinBB[block], 2.0f/16.0f,  0,           2.0f/16.0f);
		Vec3_Set(Blocks.MaxBB[block], 14.0f/16.0f, 14.0f/16.0f, 14.0f/16.0f);
	}
	Block_DefineCustom(block, true);
}


/*########################################################################################################################*
*-------------------------------------------------------Texturas----------------------------------------------------------*
*#########################################################################################################################*/
/* Se todos os pixels do tile tem a mesma cor (ou seja, o tile nao e usado) */
static cc_bool TileVazio(struct Bitmap* bmp, int x, int y, int size) {
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

/* Desenha a textura no tile; nos minerios as partes transparentes mostram a pedra (tile 1) */
static void DesenharTile(struct Bitmap* bmp, int x, int y, int size, const cc_uint8* src, cc_bool sobrePedra) {
	int baseX = Atlas2D_TileX(1) * size;
	int baseY = Atlas2D_TileY(1) * size;
	int i, j, srcX, srcY;
	const cc_uint8* px;

	for (j = 0; j < size; j++)
	{
		BitmapCol* row  = Bitmap_GetRow(bmp, y + j) + x;
		BitmapCol* base = Bitmap_GetRow(bmp, baseY + j) + baseX;
		srcY = j * MINERIOS_TEX_SIZE / size;

		for (i = 0; i < size; i++)
		{
			srcX = i * MINERIOS_TEX_SIZE / size;
			px   = src + (srcY * MINERIOS_TEX_SIZE + srcX) * 4;

			if (px[3] || !sobrePedra) {
				row[i] = BitmapCol_Make(px[0], px[1], px[2], px[3]);
			} else {
				row[i] = base[i];
			}
		}
	}
}

void Minerios_PatchAtlas(struct Bitmap* bmp) {
	int size = bmp->width / ATLAS2D_TILES_PER_ROW;
	int rows = size ? bmp->height / size : 0;
	int i, tile, x, y;

	for (i = 0; i < MINERIOS_TEX_COUNT; i++)
	{
		tile = minerios_tiles[i];
		x    = Atlas2D_TileX(tile) * size;
		y    = Atlas2D_TileY(tile);
		if (y >= rows) continue;
		y   *= size;

		/* Nao sobrescreve texturas que o pacote de texturas ja tem */
		if (!TileVazio(bmp, x, y, size)) continue;
		DesenharTile(bmp, x, y, size, minerios_texData[i], minerios_texSobrePedra[i]);
	}
}


/*########################################################################################################################*
*---------------------------------------------------Modelo do mosquito----------------------------------------------------*
*#########################################################################################################################*/
static struct ModelVertex mosquito_vertices[MOSQUITO_PARTES * MODEL_BOX_VERTICES];
static struct ModelPart   mosquito_modelPartes[MOSQUITO_PARTES];
static struct ModelTex    mosquito_tex = { "mosquito.png", SKIN_64x32 };
static struct Model       mosquito_model;

static void Mosquito_MakeParts(void) {
	struct BoxDesc desc;
	int i;

	for (i = 0; i < MOSQUITO_PARTES; i++)
	{
		const cc_int8* p = mosquito_partes[i];
		desc.texX  = p[6]; desc.texY  = p[7];
		desc.sizeX = p[3] - p[0]; desc.sizeY = p[4] - p[1]; desc.sizeZ = p[5] - p[2];
		desc.x1 = p[0] / 16.0f; desc.y1 = p[1] / 16.0f; desc.z1 = p[2] / 16.0f;
		desc.x2 = p[3] / 16.0f; desc.y2 = p[4] / 16.0f; desc.z2 = p[5] / 16.0f;
		if (p[8] >= 3) {
			/* pernas giram em volta de onde encostam no corpo */
			desc.rotX = (p[0] + p[3]) / 32.0f;
			desc.rotY = p[4] / 16.0f;
			desc.rotZ = (p[2] + p[5]) / 32.0f;
		} else {
			/* asas giram em volta do ponto onde encostam no torax */
			desc.rotX = (p[8] == 1 ? 2 : p[8] == 2 ? -2 : 0) / 16.0f;
			desc.rotY = 9 / 16.0f;
			desc.rotZ = 0;
		}
		BoxDesc_BuildBox(&mosquito_modelPartes[i], &desc);
	}
}

static void Mosquito_CheckTexture(void) {
	static const cc_string name = String_FromConst("mosquito.png");
	struct Stream mem;
	if (mosquito_tex.texID) return;

	Stream_ReadonlyMemory(&mem, (void*)mosquito_png, sizeof(mosquito_png));
	Game_UpdateTexture(&mosquito_tex.texID, &mem, &name, &mosquito_tex.skinType, NULL);
}

static void Mosquito_Draw(struct Entity* e) {
	/* bate as asas bem rapido */
	float asa = Math_SinF((float)Game.Time * 45.0f) * 0.7f;
	int i;

	Mosquito_CheckTexture();
	Model_ApplyTexture(e);
	Model_LockVB(e, MOSQUITO_PARTES * MODEL_BOX_VERTICES);

	for (i = 0; i < MOSQUITO_PARTES; i++)
	{
		switch (mosquito_partes[i][8])
		{
		case 1:  Model_DrawRotate(0, 0, -asa, &mosquito_modelPartes[i], false); break;
		case 2:  Model_DrawRotate(0, 0,  asa, &mosquito_modelPartes[i], false); break;
		/* pernas abertas para os lados, como um mosquito pousando */
		case 3:  Model_DrawRotate(0, 0,  0.75f, &mosquito_modelPartes[i], false); break;
		case 4:  Model_DrawRotate(0, 0, -0.75f, &mosquito_modelPartes[i], false); break;
		default: Model_DrawPart(&mosquito_modelPartes[i]); break;
		}
	}
	Model_UnlockVB();
	Gfx_DrawVb_IndexedTris(MOSQUITO_PARTES * MODEL_BOX_VERTICES);
}

static float Mosquito_GetNameY(struct Entity* e) { return 12 / 16.0f; }
static float Mosquito_GetEyeY(struct Entity* e)  { return 8 / 16.0f; }
static void  Mosquito_GetSize(struct Entity* e)  { Vec3_Set(e->Size, 8 / 16.0f, 10 / 16.0f, 8 / 16.0f); }
static void  Mosquito_GetBounds(struct Entity* e) {
	/* um pouco maior que o desenho, para ser possivel acertar o bichinho */
	Vec3_Set(e->ModelAABB.Min, -6 / 16.0f, 0, -9 / 16.0f);
	Vec3_Set(e->ModelAABB.Max,  6 / 16.0f, 11 / 16.0f, 8 / 16.0f);
}

static void Mosquito_Register(void) {
	Model_RegisterTexture(&mosquito_tex);

	mosquito_model.name       = "mosquito";
	mosquito_model.vertices   = mosquito_vertices;
	mosquito_model.defaultTex = &mosquito_tex;
	mosquito_model.MakeParts  = Mosquito_MakeParts;
	mosquito_model.Draw       = Mosquito_Draw;
	mosquito_model.GetNameY   = Mosquito_GetNameY;
	mosquito_model.GetEyeY    = Mosquito_GetEyeY;
	mosquito_model.GetCollisionSize = Mosquito_GetSize;
	mosquito_model.GetPickingBounds = Mosquito_GetBounds;

	Model_Init(&mosquito_model);
	mosquito_model.usesSkin    = false;
	mosquito_model.maxVertices = MOSQUITO_PARTES * MODEL_BOX_VERTICES;
	Model_Register(&mosquito_model);
}


/*########################################################################################################################*
*-------------------------------------------------------Monstros----------------------------------------------------------*
*#########################################################################################################################*/
#define MONSTROS_MAX     16
#define MONSTRO_ID_BASE  232 /* 200-223 ficam com o mod Criaturas do Bioma */
#define DIST_SUMIR       (48.0f * 48.0f)

struct MonstroDef {
	const char* nome;
	const char* modelo;
	float escala, vel;
	cc_bool bravo;  /* persegue e ataca o jogador */
	cc_bool voa;
	int chance;     /* chance de sair do minerio de ovo */
	int vidaMax, dano;
	BlockID drop;   /* o que deixa ao ser derrotado (no modo sobrevivencia) */
	int dropCount;
};

enum { M_GALINHA, M_LEITAO, M_CARNEIRO, M_ARANHA, M_ARANHA_GIGANTE, M_ZUMBI_TITAN, M_CREEPER_COLOSSO, M_GALINHA_MONSTRO, M_MOSQUITO };

static const struct MonstroDef monstro_defs[] = {
/*NOME                         MODELO      ESCALA VEL   BRAVO  VOA    CHANCE VIDA DANO DROP                    QTD */
{ "&fGalinha chocada",         "chicken",  1.0f,  2.0f, false, false, 20,    4,   0,   BLOCK_AIR,              0 },
{ "&dLeitaozinho",             "pig",      1.0f,  2.0f, false, false, 15,    6,   0,   BLOCK_AIR,              0 },
{ "&fCarneirinho",             "sheep",    1.0f,  2.0f, false, false, 15,    6,   0,   BLOCK_AIR,              0 },
{ "&7Aranha da caverna",       "spider",   1.0f,  3.5f, true,  false, 15,    8,   2,   BLOCK_AIR,              0 },
{ "&cAranha Gigante",          "spider",   2.6f,  3.0f, true,  false, 12,    40,  3,   MBLOCK_RUBI_MINERIO,    3 },
{ "&2Zumbi Titan",             "zombie",   2.4f,  2.4f, true,  false, 10,    50,  4,   MBLOCK_TITANIO_MINERIO, 4 },
{ "&aCreeper Colosso",         "creeper",  3.0f,  2.2f, true,  false,  8,    45,  5,   MBLOCK_URANIO_MINERIO,  3 },
{ "&6Galinha Monstro",         "chicken",  4.5f,  3.2f, true,  false,  5,    60,  4,   MBLOCK_RUBI,            1 },
{ "&8Mosquito",                "mosquito", 0.8f,  4.5f, true,  true,   0,    2,   1,   BLOCK_AIR,              0 },
};
#define MONSTRO_DEFS_COUNT Array_Elems(monstro_defs)

struct Monstro {
	struct Entity base;
	cc_bool usado;
	int tipo, vida;
	float tempo, pensarTimer, ataqueTimer, hurtTimer, angulo;
	Vec3 destino;
};
static struct Monstro monstros[MONSTROS_MAX];
static cc_bool armadura; /* jogador esta usando a armadura de titanio */

static float Dist2XZ(Vec3 a, Vec3 b) {
	float dx = a.x - b.x, dz = a.z - b.z;
	return dx * dx + dz * dz;
}

static float Dist2(Vec3 a, Vec3 b) {
	float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
	return dx * dx + dy * dy + dz * dz;
}

static cc_bool Solido(BlockID b) {
	cc_uint8 col = Blocks.Collide[b];
	return col == COLLIDE_SOLID || col == COLLIDE_ICE || col == COLLIDE_SLIPPERY_ICE;
}

/* Acha onde o monstro fica em pe na coluna (x, z), entre yTopo e yFim */
static cc_bool AcharChao(int x, int z, int yTopo, int yFim, float* yOut) {
	BlockID b;
	int y;
	if (x < 0 || z < 0 || x >= World.Width || z >= World.Length) return false;

	yTopo = min(yTopo, World.MaxY);
	yFim  = max(yFim, 0);
	for (y = yTopo; y >= yFim; y--)
	{
		b = World_GetBlock(x, y, z);
		if (y + 1 <= World.MaxY && Solido(World_GetBlock(x, y + 1, z))) continue;
		if (Solido(b)) { *yOut = y + Blocks.MaxBB[b].y; return true; }
		if (Blocks.IsLiquid[b]) return false;
	}
	return false;
}

static void Monstro_Remover(struct Monstro* m) {
	if (!m->usado) return;
	Entities_Remove(MONSTRO_ID_BASE + (int)(m - monstros));
	m->usado = false;
}

static void RemoverTodos(void) {
	int i;
	for (i = 0; i < MONSTROS_MAX; i++) Monstro_Remover(&monstros[i]);
}

static int ContarTipo(int tipo) {
	int i, n = 0;
	for (i = 0; i < MONSTROS_MAX; i++)
	{
		if (monstros[i].usado && monstros[i].tipo == tipo) n++;
	}
	return n;
}

static void Monstro_AtualizarNome(struct Monstro* m) {
	const struct MonstroDef* def = &monstro_defs[m->tipo];
	cc_string str; char strBuffer[STRING_SIZE];
	String_InitArray(str, strBuffer);

	String_AppendConst(&str, def->nome);
	/* mostra a vida depois de levar o primeiro golpe */
	if (m->vida < def->vidaMax) {
		String_Format2(&str, " %c%i", m->vida * 3 <= def->vidaMax ? "&c" : "&a", &m->vida);
	}
	Entity_SetName(&m->base, &str);
}

/* Machuca o jogador (so tira vida no modo sobrevivencia) */
static void DanoJogador(int dano) {
	if (dano <= 0 || !Survival_IsActive()) return;
	/* a armadura de titanio corta o dano pela metade */
	if (armadura) dano = max(1, dano / 2);
	Survival_Damage(dano);
}

static void Monstro_Andar(struct Monstro* m, Vec3 alvo, float vel, float delta) {
	struct Entity* e = &m->base;
	Vec3 pos = e->next.pos;
	float dx = alvo.x - pos.x, dz = alvo.z - pos.z;
	float len = Math_SqrtF(dx * dx + dz * dz), passo, novoY;
	float nx, nz;

	if (len < 0.3f) return;
	passo = min(vel * delta, len);
	nx = pos.x + dx / len * passo;
	nz = pos.z + dz / len * passo;
	e->next.yaw  = Math_Atan2f(dx, -dz) * MATH_RAD2DEG;
	e->next.rotY = e->next.yaw;

	if (!AcharChao(Math_Floor(nx), Math_Floor(nz), Math_Floor(pos.y + 0.5f) + 2, Math_Floor(pos.y) - 4, &novoY)) {
		m->pensarTimer = 0; return;
	}
	if (novoY - pos.y > 1.05f) { m->pensarTimer = 0; return; }

	/* sobe degraus na hora, desce caindo aos poucos */
	if (novoY < pos.y) novoY = max(novoY, pos.y - 0.5f);
	pos.x = nx; pos.z = nz; pos.y = novoY;
	e->next.pos = pos;
}

/* Voa direto ate o alvo (mosquitos), sem atravessar blocos solidos */
static void Monstro_Voar(struct Monstro* m, Vec3 alvo, float vel, float delta) {
	struct Entity* e = &m->base;
	Vec3 pos = e->next.pos, novo;
	float dx = alvo.x - pos.x, dy = alvo.y - pos.y, dz = alvo.z - pos.z;
	float len = Math_SqrtF(dx * dx + dy * dy + dz * dz), passo;

	if (len < 0.1f) return;
	passo = min(vel * delta, len);
	novo.x = pos.x + dx / len * passo;
	novo.y = pos.y + dy / len * passo;
	novo.z = pos.z + dz / len * passo;
	e->next.yaw  = Math_Atan2f(dx, -dz) * MATH_RAD2DEG;
	e->next.rotY = e->next.yaw;

	if (!World_Contains(Math_Floor(novo.x), Math_Floor(novo.y), Math_Floor(novo.z))) return;
	if (Solido(World_GetBlock(Math_Floor(novo.x), Math_Floor(novo.y), Math_Floor(novo.z)))) {
		/* bateu: tenta subir */
		novo = pos; novo.y += passo;
		if (!World_Contains(Math_Floor(novo.x), Math_Floor(novo.y), Math_Floor(novo.z))) return;
		if (Solido(World_GetBlock(Math_Floor(novo.x), Math_Floor(novo.y), Math_Floor(novo.z)))) return;
	}
	e->next.pos = novo;
}

/* Empurra o jogador para longe (e para cima), mais forte quanto maior o monstro */
static void Monstro_Atacar(struct Monstro* m, struct Entity* jogador) {
	const struct MonstroDef* def = &monstro_defs[m->tipo];
	float dx = jogador->Position.x - m->base.next.pos.x;
	float dz = jogador->Position.z - m->base.next.pos.z;
	float len = Math_SqrtF(dx * dx + dz * dz) + 0.01f;
	float forca = 0.5f + def->escala * 0.25f;

	if (!def->voa) {
		jogador->Velocity.x = dx / len * forca;
		jogador->Velocity.z = dz / len * forca;
		jogador->Velocity.y = 0.35f + def->escala * 0.05f;
	}
	DanoJogador(def->dano);
	m->ataqueTimer = def->voa ? 2.0f : 1.5f;
}

static void Monstro_TickMosquito(struct Monstro* m, struct Entity* jogador, float delta) {
	const struct MonstroDef* def = &monstro_defs[m->tipo];
	Vec3 alvo = jogador->Position;

	/* zumbe em circulos em volta da cabeca do jogador e da rasantes para picar */
	m->angulo += delta * 2.5f;
	if (m->ataqueTimer > 0.6f) {
		alvo.x += Math_CosF(m->angulo) * 1.8f;
		alvo.z += Math_SinF(m->angulo) * 1.8f;
		alvo.y += 1.6f + Math_SinF(m->angulo * 3.0f) * 0.4f;
	} else {
		alvo.y += 1.2f;
	}
	Monstro_Voar(m, alvo, def->vel, delta);

	if (m->ataqueTimer <= 0 && Dist2(m->base.next.pos, alvo) < 0.8f * 0.8f) {
		Monstro_Atacar(m, jogador);
	}
}

static void Monstro_Tick(struct Entity* e, float delta) {
	struct Monstro* m = (struct Monstro*)e;
	const struct MonstroDef* def = &monstro_defs[m->tipo];
	struct Entity* jogador = &Entities.CurPlayer->Base;
	Vec3 pos = e->next.pos;
	float d2, alcance, ang;

	e->prev = e->next;
	m->tempo       -= delta;
	m->pensarTimer -= delta;
	m->ataqueTimer -= delta;
	m->hurtTimer   -= delta;

	d2 = Dist2XZ(pos, jogador->Position);
	if (m->tempo <= 0 || d2 > DIST_SUMIR) { Monstro_Remover(m); return; }

	if (def->voa) {
		Monstro_TickMosquito(m, jogador, delta);
	} else if (def->bravo && d2 < 16 * 16 && Math_AbsF(jogador->Position.y - pos.y) < 6) {
		/* persegue o jogador */
		alcance = 0.9f + def->escala * 0.6f;
		if (d2 > alcance * alcance) {
			Monstro_Andar(m, jogador->Position, def->vel, delta);
		} else if (m->ataqueTimer <= 0) {
			Monstro_Atacar(m, jogador);
		}
	} else {
		/* passeia sem rumo */
		if (m->pensarTimer <= 0) {
			m->pensarTimer = 2.0f + Random_Float(&rnd) * 4.0f;
			ang = Random_Float(&rnd) * 2 * MATH_PI;
			m->destino.x = pos.x + Math_CosF(ang) * 5.0f;
			m->destino.z = pos.z + Math_SinF(ang) * 5.0f;
			if (Random_Next(&rnd, 3) == 0) m->destino = pos;
		}
		Monstro_Andar(m, m->destino, def->vel * 0.4f, delta);
	}
	AnimatedComp_Update(e, e->prev.pos, e->next.pos, delta);
}

static void Monstro_Despawn(struct Entity* e) {
	EntityNames_Delete(e);
	if (e->Flags & ENTITY_FLAG_HAS_MODELVB) Gfx_DeleteScratchVb(&e->ModelVB);
}

static void Monstro_SetLocation(struct Entity* e, struct LocationUpdate* update) {
	if (update->flags & LU_HAS_POS) {
		e->prev.pos = update->pos; e->next.pos = update->pos;
	}
}

static PackedCol Monstro_GetCol(struct Entity* e) {
	struct Monstro* m = (struct Monstro*)e;
	Vec3 eyePos = Entity_GetEyePosition(e);
	IVec3 pos; PackedCol col;
	IVec3_Floor(&pos, &eyePos);
	col = Lighting.Color(pos.x, pos.y, pos.z);

	/* pisca vermelho ao levar dano */
	if (m->hurtTimer > 0) col = PackedCol_Tint(col, PackedCol_Make(255, 80, 80, 255));
	return col;
}

static void Monstro_RenderModel(struct Entity* e, float delta, float t) {
	Vec3_Lerp(&e->Position, &e->prev.pos, &e->next.pos, t);
	Entity_LerpAngles(e, t);
	AnimatedComp_GetCurrent(e, t);

	e->ShouldRender = Model_ShouldRender(e);
	if (e->ShouldRender) Model_Render(e->Model, e);
}

static cc_bool Monstro_ShouldRenderName(struct Entity* e) {
	if (!e->ShouldRender) return false;
	return Model_RenderDistance(e) <= 32.0f * 32.0f;
}

static const struct EntityVTABLE monstro_VTABLE = {
	Monstro_Tick,        Monstro_Despawn,         Monstro_SetLocation, Monstro_GetCol,
	Monstro_RenderModel, Monstro_ShouldRenderName
};

static int SortearTipo(void) {
	int i, total = 0, r;
	for (i = 0; i < MONSTRO_DEFS_COUNT; i++) total += monstro_defs[i].chance;

	r = Random_Next(&rnd, total);
	for (i = 0; i < MONSTRO_DEFS_COUNT; i++)
	{
		if (r < monstro_defs[i].chance) return i;
		r -= monstro_defs[i].chance;
	}
	return 0;
}

static struct Monstro* Monstro_Criar(int tipo, Vec3 pos) {
	const struct MonstroDef* def = &monstro_defs[tipo];
	cc_string modelo = String_FromReadonly(def->modelo);
	struct Monstro* m;
	int i;

	for (i = 0; i < MONSTROS_MAX; i++)
	{
		if (!monstros[i].usado) break;
	}
	/* sem vaga: o mais antigo volta para a terra */
	if (i == MONSTROS_MAX) {
		i = 0;
		Monstro_Remover(&monstros[0]);
	}
	m = &monstros[i];

	Mem_Set(m, 0, sizeof(struct Monstro));
	Entity_Init(&m->base);
	m->base.VTABLE = &monstro_VTABLE;
	m->usado   = true;
	m->tipo    = tipo;
	m->vida    = def->vidaMax;
	m->tempo   = def->bravo ? 90.0f : 300.0f;
	m->destino = pos;
	m->angulo  = Random_Float(&rnd) * 2 * MATH_PI;
	m->ataqueTimer = 1.0f; /* da um tempinho para o jogador correr */

	m->base.Model = Model_Get(&modelo);
	Vec3_Set(m->base.ModelScale, def->escala, def->escala, def->escala);
	Entity_UpdateModelBounds(&m->base);
	Monstro_AtualizarNome(m);

	m->base.next.yaw  = m->base.prev.yaw  = (float)Random_Next(&rnd, 360);
	m->base.next.rotY = m->base.prev.rotY = m->base.next.yaw;
	m->base.prev.pos  = pos; m->base.next.pos = pos; m->base.Position = pos;

	Entities.List[MONSTRO_ID_BASE + i] = &m->base;
	return m;
}

/* Choca um ovo na posicao (centro do bloco) */
static void ChocarOvo(int x, int y, int z, int tipo) {
	const struct MonstroDef* def;
	float chaoY;
	Vec3 pos;
	if (tipo < 0) tipo = SortearTipo();
	def = &monstro_defs[tipo];

	if (!AcharChao(x, z, y, y - 8, &chaoY)) chaoY = (float)y;
	Vec3_Set(pos, x + 0.5f, chaoY, z + 0.5f);
	Monstro_Criar(tipo, pos);

	if (def->bravo && def->escala > 2.0f) {
		Chat_Add1("&cCuidado! &eDo ovo saiu: %c", def->nome);
	} else {
		Chat_Add1("&eDo ovo saiu: %c", def->nome);
	}
}

/* Um enxame de mosquitos aparece perto do jogador */
static void SoltarMosquitos(int quantos) {
	struct Entity* jogador = &Entities.CurPlayer->Base;
	float ang = Random_Float(&rnd) * 2 * MATH_PI;
	Vec3 pos;
	int i;

	for (i = 0; i < quantos; i++)
	{
		pos.x = jogador->Position.x + Math_CosF(ang) * 10.0f + Random_Float(&rnd) * 2.0f;
		pos.z = jogador->Position.z + Math_SinF(ang) * 10.0f + Random_Float(&rnd) * 2.0f;
		pos.y = jogador->Position.y + 3.0f + Random_Float(&rnd);
		if (!World_Contains(Math_Floor(pos.x), Math_Floor(pos.y), Math_Floor(pos.z))) continue;
		if (Solido(World_GetBlock(Math_Floor(pos.x), Math_Floor(pos.y), Math_Floor(pos.z)))) pos = jogador->Position, pos.y += 3.0f;
		Monstro_Criar(M_MOSQUITO, pos);
	}
	Chat_AddRaw("&8Bzzzz... &7um enxame de mosquitos chegou!");
}

static void OnBlockChanged(void* obj, IVec3 coords, BlockID old, BlockID now) {
	if (!Minerios_Enabled) return;
	if (!Server.IsSinglePlayer || old != MBLOCK_OVO_MINERIO || now != BLOCK_AIR) return;
	if (!Block_IsCustomDefined(MBLOCK_OVO_MINERIO)) return;
	ChocarOvo(coords.x, coords.y, coords.z, -1);
}


/*########################################################################################################################*
*---------------------------------------------------------Combate---------------------------------------------------------*
*#########################################################################################################################*/
static int DanoDoItem(BlockID held) {
	switch (held)
	{
	case MITEM_ESPADA_TITANIO: return 7;
	case MITEM_ESPADA_SUPREMA: return 14;
	case MITEM_BIG_BERTHA:     return 30;
	}
	return 2; /* na mao (ou com um bloco) */
}

static void Monstro_Derrotado(struct Monstro* m) {
	const struct MonstroDef* def = &monstro_defs[m->tipo];
	int i;
	Chat_Add1("&e%c &efoi derrotado(a)!", def->nome);

	if (def->drop != BLOCK_AIR) {
		for (i = 0; i < def->dropCount; i++) Survival_OnBlockBroken(def->drop);
		if (Survival_IsActive()) {
			cc_string nome = Block_UNSAFE_GetName(def->drop);
			Chat_Add2("&aVoce ganhou %i %s", &def->dropCount, &nome);
		}
	}
	Monstro_Remover(m);
}

static void Monstro_Dano(struct Monstro* m, int dano, Vec3 origem, float empurrao) {
	float dx = m->base.next.pos.x - origem.x, dz = m->base.next.pos.z - origem.z;
	float len = Math_SqrtF(dx * dx + dz * dz) + 0.01f;
	const struct MonstroDef* def = &monstro_defs[m->tipo];

	m->vida     -= dano;
	m->hurtTimer = 0.3f;
	if (m->vida <= 0) { Monstro_Derrotado(m); return; }

	/* empurrao para tras (gigantes quase nao se mexem) */
	empurrao /= def->escala;
	m->base.next.pos.x += dx / len * empurrao;
	m->base.next.pos.z += dz / len * empurrao;
	if (!def->voa) {
		float y;
		if (AcharChao(Math_Floor(m->base.next.pos.x), Math_Floor(m->base.next.pos.z),
				Math_Floor(m->base.next.pos.y) + 1, Math_Floor(m->base.next.pos.y) - 3, &y)) {
			m->base.next.pos.y = y;
		} else {
			m->base.next.pos.x -= dx / len * empurrao;
			m->base.next.pos.z -= dz / len * empurrao;
		}
	}
	m->base.prev.pos = m->base.next.pos;
	Monstro_AtualizarNome(m);
}

static struct Monstro* MonstroNaMira(float alcance) {
	struct LocalPlayer* p = Entities.CurPlayer;
	Vec3 eye = Entity_GetEyePosition(&p->Base);
	Vec3 dir = Vec3_GetDirVector(p->Base.Yaw * MATH_DEG2RAD, p->Base.Pitch * MATH_DEG2RAD);
	struct Monstro* melhor = NULL;
	float t0, t1, melhorT = alcance, blocoT;
	int i;

	/* nao atravessa paredes: compara com o bloco mirado */
	if (Game_SelectedPos.valid) {
		blocoT = Math_SqrtF(Dist2(eye, Game_SelectedPos.intersect));
		if (blocoT < melhorT) melhorT = blocoT;
	}

	for (i = 0; i < MONSTROS_MAX; i++)
	{
		if (!monstros[i].usado) continue;
		if (!Intersection_RayIntersectsRotatedBox(eye, dir, &monstros[i].base, &t0, &t1)) continue;
		if (t0 < 0 || t0 > melhorT) continue;
		melhorT = t0; melhor = &monstros[i];
	}
	return melhor;
}

/* A Big Bertha acerta todos os monstros num arco na frente do jogador */
static cc_bool GolpeBigBertha(void) {
	struct Entity* jogador = &Entities.CurPlayer->Base;
	Vec3 dir = Vec3_GetDirVector(jogador->Yaw * MATH_DEG2RAD, 0);
	int i, acertos = 0;
	float dx, dz, len, alcance;

	for (i = 0; i < MONSTROS_MAX; i++)
	{
		struct Monstro* m = &monstros[i];
		if (!m->usado) continue;
		dx  = m->base.next.pos.x - jogador->Position.x;
		dz  = m->base.next.pos.z - jogador->Position.z;
		len = Math_SqrtF(dx * dx + dz * dz) + 0.01f;
		alcance = 4.5f + monstro_defs[m->tipo].escala * 0.5f;
		if (len > alcance || Math_AbsF(m->base.next.pos.y - jogador->Position.y) > 4) continue;
		/* so o que esta na frente (mais ou menos 70 graus para cada lado) */
		if ((dx * dir.x + dz * dir.z) / len < 0.35f) continue;

		Monstro_Dano(m, DanoDoItem(MITEM_BIG_BERTHA), jogador->Position, 2.5f);
		acertos++;
	}
	return acertos > 0;
}

cc_bool Minerios_HandleClick(cc_bool place) {
	struct Entity* jogador;
	struct Monstro* m;
	BlockID held = Inventory_SelectedBlock;
	if (!Ativo()) return false;

	if (place) {
		if (!EhItem(held)) return false;
		/* itens nao sao colocados no mundo */
		if (held == MITEM_ARMADURA_TITANIO) {
			if (armadura) {
				Chat_AddRaw("&7Voce ja esta usando a armadura de titanio.");
			} else {
				armadura = true;
				Chat_AddRaw("&bVoce vestiu a armadura de titanio! &7(leva metade do dano)");
			}
		}
		return true;
	}

	jogador = &Entities.CurPlayer->Base;
	if (held == MITEM_BIG_BERTHA && GolpeBigBertha()) return true;

	m = MonstroNaMira(Entities.CurPlayer->ReachDistance + (held == MITEM_BIG_BERTHA ? 1.5f : 0.5f));
	if (!m) return false;
	Monstro_Dano(m, DanoDoItem(held), jogador->Position, 0.8f);
	return true;
}


/*########################################################################################################################*
*-------------------------------------------------------Dimensoes---------------------------------------------------------*
*#########################################################################################################################*/
enum { DIM_NORMAL, DIM_UTOPIA, DIM_CRISTAL, DIM_COUNT };
static const char* const dim_nomes[DIM_COUNT]    = { "Mundo normal", "Utopia", "Dimensao de Cristal" };
static const char* const dim_arquivos[DIM_COUNT] = { "maps/orespawn_normal.cw", "maps/orespawn_utopia.cw", "maps/orespawn_cristal.cw" };

static int  dimAtual, dimDestino = -1, dimGerando = -1;
static int  viagemPendente = -1;
static cc_bool viajando, colocarPortal;
static float portalCooldown;
static Vec3  posSegura; /* ultima posicao do jogador fora de um portal */

/* Vida e blocos do modo sobrevivencia, levados de uma dimensao para outra */
static cc_bool survLevar;
static int     survVida;
static cc_uint8 survBlocos[BLOCK_COUNT * 2];

static cc_result SalvarMundo(const char* arquivo) {
	static const cc_string dir = String_FromConst("maps");
	cc_string path = String_FromReadonly(arquivo);
	struct GZipState* state;
	struct Stream stream, compStream;
	cc_filepath raw;
	cc_result res;

	Platform_EncodePath(&raw, &dir);
	Directory_Create2(&raw);

	state = (struct GZipState*)Mem_TryAlloc(1, sizeof(struct GZipState));
	if (!state) return ERR_OUT_OF_MEMORY;

	Platform_EncodePath(&raw, &path);
	res = Stream_CreatePath(&stream, &raw);
	if (!res) {
		GZip_MakeStream(&compStream, state, &stream);
		res = Cw_Save(&compStream);
		if (!res) res = compStream.Close(&compStream);
		stream.Close(&stream);
	}
	Mem_Free(state);
	if (res) Logger_SysWarn2(res, "salvando", &path);
	return res;
}

static cc_bool ArquivoExiste(const char* arquivo) {
	cc_string path = String_FromReadonly(arquivo);
	cc_filepath raw;
	Platform_EncodePath(&raw, &path);
	return File_Exists(&raw);
}

static void Viajar(int destino) {
	struct LocalPlayer* p = Entities.CurPlayer;
	cc_string path;
	if (destino == dimAtual || viajando) return;

	/* o jogador volta para perto do portal (e nao dentro dele) */
	p->Spawn = posSegura;
	if (SalvarMundo(dim_arquivos[dimAtual])) {
		Chat_AddRaw("&cNao foi possivel salvar o mundo, a viagem foi cancelada.");
		return;
	}

	survLevar = Survival_IsActive();
	if (survLevar) {
		survVida = Survival_GetHealth();
		Survival_GetCounts(survBlocos);
	}

	viajando   = true;
	dimDestino = destino;
	if (ArquivoExiste(dim_arquivos[destino])) {
		path = String_FromReadonly(dim_arquivos[destino]);
		Map_LoadFrom(&path);
	} else {
		/* primeira visita: gera a dimensao do mesmo tamanho do mundo atual */
		dimGerando = destino;
		Gen_Start(&NotchyGen, Random_Next(&rnd, 0x7FFFFFFF), World.Width, World.Height, World.Length);
	}
}

static int DimensaoDoPortal(BlockID b) {
	if (b == MBLOCK_PORTAL_UTOPIA)  return DIM_UTOPIA;
	if (b == MBLOCK_PORTAL_CRISTAL) return DIM_CRISTAL;
	return -1;
}

static void ChecarPortal(void) {
	struct Entity* e = &Entities.CurPlayer->Base;
	int x = Math_Floor(e->Position.x), y = Math_Floor(e->Position.y), z = Math_Floor(e->Position.z);
	int dim = -1;

	if (World_Contains(x, y, z))     dim = DimensaoDoPortal(World_GetBlock(x, y, z));
	if (dim < 0 && World_Contains(x, y + 1, z)) dim = DimensaoDoPortal(World_GetBlock(x, y + 1, z));

	if (dim < 0) { posSegura = e->Position; return; }
	if (portalCooldown > 0) return;
	/* entrar no portal da dimensao onde voce esta leva de volta ao mundo normal */
	viagemPendente = dim == dimAtual ? DIM_NORMAL : dim;
}

/* Se o jogador chegou dentro da agua, procura terra firme por perto */
static void AcharTerraFirme(void) {
	struct Entity* e = &Entities.CurPlayer->Base;
	struct LocationUpdate u;
	int cx = Math_Floor(e->Position.x), cz = Math_Floor(e->Position.z);
	int r, x, z, y;
	BlockID b;

	b = World_SafeGetBlock(cx, Math_Floor(e->Position.y), cz);
	if (!Blocks.IsLiquid[b]) return;

	for (r = 1; r < 48; r++)
	{
		for (x = cx - r; x <= cx + r; x++)
		{
			for (z = cz - r; z <= cz + r; z += (x == cx - r || x == cx + r) ? 1 : 2 * r)
			{
				if (!World_Contains(x, 0, z)) continue;
				for (y = World.MaxY; y > 0 && World_GetBlock(x, y, z) == BLOCK_AIR; y--) { }
				b = World_GetBlock(x, y, z);
				if (Blocks.IsLiquid[b] || !Solido(b) || y + 2 > World.MaxY) continue;

				u.flags = LU_HAS_POS;
				Vec3_Set(u.pos, x + 0.5f, y + 1.0f, z + 0.5f);
				e->VTABLE->SetLocation(e, &u);
				return;
			}
		}
	}
}

/* Coloca um portal de volta perto de onde o jogador chegou */
static void ColocarPortalVolta(void) {
	struct Entity* e = &Entities.CurPlayer->Base;
	Vec3 dir = Vec3_GetDirVector(e->Yaw * MATH_DEG2RAD, 0);
	int x = Math_Floor(e->Position.x + dir.x * 3.0f);
	int z = Math_Floor(e->Position.z + dir.z * 3.0f);
	int y = Math_Floor(e->Position.y);
	BlockID portal = dimAtual == DIM_UTOPIA ? MBLOCK_PORTAL_UTOPIA : MBLOCK_PORTAL_CRISTAL;

	if (!World_Contains(x, y, z) || !World_Contains(x, y + 1, z)) return;
	if (y > 0) Game_UpdateBlock(x, y - 1, z, dimAtual == DIM_UTOPIA ? MBLOCK_RUBI : MBLOCK_PEDRA_CRISTAL);
	Game_UpdateBlock(x, y,     z, portal);
	Game_UpdateBlock(x, y + 1, z, portal);
	if (y + 2 <= World.MaxY) Game_UpdateBlock(x, y + 2, z, dimAtual == DIM_UTOPIA ? MBLOCK_RUBI : MBLOCK_PEDRA_CRISTAL);
}

static void AplicarCeu(int dim) {
	if (dim == DIM_UTOPIA) {
		Env_SetSkyCol(PackedCol_Make(90, 230, 215, 255));
		Env_SetFogCol(PackedCol_Make(205, 255, 235, 255));
		Env_SetCloudsCol(PackedCol_Make(255, 230, 150, 255));
		Env_SetSunCol(PackedCol_Make(255, 250, 225, 255));
	} else if (dim == DIM_CRISTAL) {
		Env_SetSkyCol(PackedCol_Make(185, 120, 240, 255));
		Env_SetFogCol(PackedCol_Make(235, 180, 250, 255));
		Env_SetCloudsCol(PackedCol_Make(255, 205, 245, 255));
		Env_SetSunCol(PackedCol_Make(255, 235, 255, 255));
	}
}

/* --- Geracao --- */
static BlockRaw GenGet(int x, int y, int z) { return Gen_Blocks[World_Pack(x, y, z)]; }
static void GenSet(int x, int y, int z, BlockRaw b) { Gen_Blocks[World_Pack(x, y, z)] = b; }

static int GenTopo(int x, int z) {
	int y;
	for (y = World.MaxY; y > 0; y--)
	{
		if (GenGet(x, y, z) != BLOCK_AIR) return y;
	}
	return 0;
}

static void GenArvores(int quantas) {
	IVec3 coords[TREE_MAX_COUNT];
	BlockRaw blocks[TREE_MAX_COUNT];
	int i, j, x, y, z, altura, count;

	Tree_Blocks = Gen_Blocks;
	Tree_Rnd    = &rnd;
	for (i = 0; i < quantas; i++)
	{
		x = Random_Next(&rnd, World.Width);
		z = Random_Next(&rnd, World.Length);
		y = GenTopo(x, z);
		if (GenGet(x, y, z) != BLOCK_GRASS) continue;

		altura = 5 + Random_Next(&rnd, 4);
		if (!TreeGen_CanGrow(x, y + 1, z, altura)) continue;
		count = TreeGen_Grow(x, y + 1, z, altura, coords, blocks);
		for (j = 0; j < count; j++) GenSet(coords[j].x, coords[j].y, coords[j].z, blocks[j]);
	}
}

/* Espalha pedacinhos de minerio na pedra */
static void GenMinerios(BlockRaw bloco, int quantos) {
	int i, x, y, z;
	for (i = 0; i < quantos; i++)
	{
		x = Random_Next(&rnd, World.Width);
		y = Random_Next(&rnd, max(1, World.Height - 8));
		z = Random_Next(&rnd, World.Length);
		if (GenGet(x, y, z) == BLOCK_STONE) GenSet(x, y, z, bloco);
		if (x + 1 < World.Width && GenGet(x + 1, y, z) == BLOCK_STONE) GenSet(x + 1, y, z, bloco);
	}
}

static void GerarUtopia(void) {
	int area = World.Width * World.Length;
	int x, z, y;
	Gen_CurrentState = "Plantando a Utopia";

	/* muito mais arvores e flores, e minerios raros de sobra */
	GenArvores(area / 60);
	for (x = 0; x < World.Width; x++)
	{
		for (z = 0; z < World.Length; z++)
		{
			y = GenTopo(x, z);
			if (GenGet(x, y, z) != BLOCK_GRASS || y + 1 > World.MaxY) continue;
			if (Random_Next(&rnd, 7) == 0) GenSet(x, y + 1, z, Random_Next(&rnd, 2) ? BLOCK_DANDELION : BLOCK_ROSE);
		}
	}
	GenMinerios(MBLOCK_RUBI_MINERIO,    World.Volume / 900);
	GenMinerios(MBLOCK_TITANIO_MINERIO, World.Volume / 900);
	GenMinerios(MBLOCK_URANIO_MINERIO,  World.Volume / 1400);
	GenMinerios(MBLOCK_OVO_MINERIO,     World.Volume / 1400);
}

static void GerarCristal(void) {
	int i, x, y, z;
	BlockRaw b;
	Gen_CurrentState = "Crescendo cristais";

	GenArvores(World.Width * World.Length / 150);
	for (i = 0; i < World.Volume; i++)
	{
		b = Gen_Blocks[i];
		switch (b)
		{
		case BLOCK_STONE: case BLOCK_COBBLE: case BLOCK_DIRT: case BLOCK_GRAVEL:
			Gen_Blocks[i] = MBLOCK_PEDRA_CRISTAL; break;
		case BLOCK_GRASS:  Gen_Blocks[i] = MBLOCK_GRAMA_CRISTAL;  break;
		case BLOCK_LOG:    Gen_Blocks[i] = MBLOCK_TRONCO_CRISTAL; break;
		case BLOCK_LEAVES: Gen_Blocks[i] = MBLOCK_FOLHAS_CRISTAL; break;
		case BLOCK_DANDELION: case BLOCK_ROSE: case BLOCK_BROWN_SHROOM: case BLOCK_RED_SHROOM:
			Gen_Blocks[i] = MBLOCK_CRISTAL; break;
		}
	}

	/* aglomerados de cristal brilhando pelo chao */
	for (x = 0; x < World.Width; x++)
	{
		for (z = 0; z < World.Length; z++)
		{
			y = GenTopo(x, z);
			if (GenGet(x, y, z) != MBLOCK_GRAMA_CRISTAL || y + 1 > World.MaxY) continue;
			if (Random_Next(&rnd, 14) == 0) GenSet(x, y + 1, z, MBLOCK_CRISTAL);
		}
	}
}

void Minerios_PosGeracao(void) {
	if (dimGerando == DIM_UTOPIA)  GerarUtopia();
	if (dimGerando == DIM_CRISTAL) GerarCristal();
}


/*########################################################################################################################*
*--------------------------------------------------------Comando----------------------------------------------------------*
*#########################################################################################################################*/
static void MineriosCommand_Execute(const cc_string* args, int argsCount) {
	struct Entity* jogador = &Entities.CurPlayer->Base;
	float yaw = jogador->Yaw * MATH_DEG2RAD;
	cc_string nome;
	int i, id, x, z, tipo = -1, count = 1;

	if (!Minerios_Enabled) { Chat_AddRaw("&cO mod Minerios & Monstros esta desligado."); return; }

	if (argsCount && String_CaselessEqualsConst(&args[0], "limpar")) {
		RemoverTodos();
		Chat_AddRaw("&eMonstros removidos.");
		return;
	}

	if (argsCount && String_CaselessEqualsConst(&args[0], "mosquitos")) {
		SoltarMosquitos(4);
		return;
	}

	if (argsCount && String_CaselessEqualsConst(&args[0], "ir")) {
		int dim = -1;
		if (argsCount > 1 && String_CaselessEqualsConst(&args[1], "normal"))  dim = DIM_NORMAL;
		if (argsCount > 1 && String_CaselessEqualsConst(&args[1], "utopia"))  dim = DIM_UTOPIA;
		if (argsCount > 1 && String_CaselessEqualsConst(&args[1], "cristal")) dim = DIM_CRISTAL;
		if (dim < 0) { Chat_AddRaw("&e/client minerios ir [normal|utopia|cristal]"); return; }
		viagemPendente = dim;
		return;
	}

	if (argsCount && String_CaselessEqualsConst(&args[0], "ovo")) {
		if (argsCount > 1 && String_CaselessEqualsConst(&args[1], "todos")) {
			count = M_MOSQUITO;
		} else if (argsCount > 1) {
			Convert_ParseInt(&args[1], &tipo);
			if (tipo < 1 || tipo > M_MOSQUITO) { Chat_AddRaw("&cNumero de monstro invalido."); return; }
			tipo--;
		}

		for (i = 0; i < count; i++)
		{
			float ang  = yaw + (count > 1 ? (i - (count - 1) * 0.5f) * 0.22f : 0.0f);
			float dist = count > 1 ? 14.0f : 6.0f;
			x = Math_Floor(jogador->Position.x + Math_SinF(ang) * dist);
			z = Math_Floor(jogador->Position.z - Math_CosF(ang) * dist);
			ChocarOvo(x, Math_Floor(jogador->Position.y) + 6, z, count > 1 ? i : tipo);
		}
		return;
	}

	Chat_AddRaw("&eMinerios & Monstros - blocos e itens (ID):");
	for (i = 0; i < MINERIOS_COUNT; i += 2)
	{
		id = MBLOCK_RUBI_MINERIO + i;
		if (i + 1 < MINERIOS_COUNT) {
			int id2 = id + 1;
			Chat_Add4("&f  %i &7%c  &f%i &7%c", &id, minerio_defs[i].name, &id2, minerio_defs[i + 1].name);
		} else {
			Chat_Add2("&f  %i &7%c", &id, minerio_defs[i].name);
		}
	}
	Chat_AddRaw("&eMonstros do minerio de ovo:");
	for (i = 0; i < M_MOSQUITO; i++)
	{
		id = i + 1;
		nome = String_FromReadonly(monstro_defs[i].nome);
		Chat_Add3("&f  %i &7- %s%c", &id, &nome, monstro_defs[i].bravo ? " &7(bravo)" : "");
	}
}

static struct ChatCommand MineriosCommand = {
	"Minerios", MineriosCommand_Execute,
	COMMAND_FLAG_SINGLEPLAYER_ONLY,
	{
		"&a/client minerios &e- lista os blocos, itens e monstros do mod",
		"&a/client minerios ovo [numero|todos] &e- choca um ovo na sua frente",
		"&a/client minerios mosquitos &e- chama um enxame de mosquitos",
		"&a/client minerios ir [normal|utopia|cristal] &e- viaja para a dimensao",
		"&a/client minerios limpar &e- remove os monstros",
	}
};


/*########################################################################################################################*
*-------------------------------------------------------Componente--------------------------------------------------------*
*#########################################################################################################################*/
static float mosquitoTimer = 30.0f;

static void Minerios_Tick(struct ScheduledTask* task) {
	float delta = (float)task->interval;
	int dest;
	portalCooldown -= delta;
	if (!Ativo() || viajando) return;

	if (colocarPortal) { colocarPortal = false; AcharTerraFirme(); ColocarPortalVolta(); return; }

	ChecarPortal();
	if (viagemPendente >= 0) {
		dest = viagemPendente;
		viagemPendente = -1;
		Viajar(dest);
		return;
	}

	/* de vez em quando aparece um enxame de mosquitos (mais na Utopia, nenhum no Cristal) */
	if (dimAtual == DIM_CRISTAL) return;
	mosquitoTimer -= delta;
	if (mosquitoTimer > 0) return;
	mosquitoTimer = (dimAtual == DIM_UTOPIA ? 40.0f : 80.0f) + Random_Float(&rnd) * 60.0f;
	if (ContarTipo(M_MOSQUITO) < 4) SoltarMosquitos(3 + Random_Next(&rnd, 2));
}

static void OnNewMap(void) {
	RemoverTodos();
	/* um mundo carregado pelo menu volta a ser o "mundo normal" */
	dimAtual = viajando ? dimDestino : DIM_NORMAL;
	if (!viajando) armadura = false;
}

static void OnNewMapLoaded(void) {
	BlockID block;
	int i;
	/* No multijogador o servidor decide quais blocos existem */
	Minerios_Enabled = Options_GetBool(MINERIOS_OPTION, true);
	if (!Minerios_Enabled || !Server.IsSinglePlayer || !Game_AllowCustomBlocks) { viajando = false; return; }

	for (i = 0; i < MINERIOS_COUNT; i++)
	{
		block = (BlockID)(MBLOCK_RUBI_MINERIO + i);
		if (block > BLOCK_MAX_DEFINED) break;
		/* Mundos salvos podem ja definir o bloco (talvez diferente) */
		if (Block_IsCustomDefined(block)) continue;
		DefineBlock(block, &minerio_defs[i]);
	}
	if (!viajando) return;

	/* precisa vir antes do componente de sobrevivencia aplicar o estado do mundo */
	if (survLevar) {
		Survival_LoadHealth(survVida);
		Survival_LoadCounts(survBlocos, sizeof(survBlocos));
	}
	if (dimGerando >= 0) {
		AplicarCeu(dimGerando);
		colocarPortal = dimAtual != DIM_NORMAL;
		dimGerando    = -1;
	}
	viajando       = false;
	portalCooldown = 3.0f;
	Chat_Add1("&dVoce chegou em: &f%c", dim_nomes[dimAtual]);
}

static void Minerios_Init(void) {
	Minerios_Enabled = Options_GetBool(MINERIOS_OPTION, true);
	Random_SeedFromCurrentTime(&rnd);
	Mosquito_Register();
	Commands_Register(&MineriosCommand);
	Event_Register_(&UserEvents.BlockChanged, NULL, OnBlockChanged);
	ScheduledTask_Add(GAME_DEF_TICKS, Minerios_Tick);
}

static void Minerios_Free(void) { RemoverTodos(); }

struct IGameComponent Minerios_Component = {
	Minerios_Init,  /* Init  */
	Minerios_Free,  /* Free  */
	RemoverTodos,   /* Reset */
	OnNewMap,       /* OnNewMap */
	OnNewMapLoaded  /* OnNewMapLoaded */
};

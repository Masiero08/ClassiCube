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
#include "DayNight.h"
#include "Options.h"
#include "Platform.h"
#include "Picking.h"
#include "Inventory.h"
#include "Generator.h"
#include "Formats.h"
#include "Deflate.h"
#include "Stream.h"
#include "Survival.h"
#include "Particle.h"
#include "DayNight.h"
#include "Errors.h"
#include "Logger.h"
#include "Minerios_tex.h"
#include "LateGame.h"

/* Minerios & Monstros: um mod pequeno inspirado no OreSpawn.
   Os nomes e as texturas sao originais (misc/minerios/gen_textures.py).
   - Rubi, titanio e uranio: minerio (aparece em mundos novos) + bloco
   - Minerio de ovo: ao quebrar, choca uma criatura; as raras sao gigantes
   - Espadas (titanio, suprema, Big Bertha) e armadura de titanio
   - Mosquitos: enxames que voam em volta do jogador e picam; as vezes vem a Rainha
     dos Mosquitos, o primeiro chefao
   - Chefoes em ordem (Rainha, Golem de Titanio, Aranha de Cristal, Rei Creeper): cada um
     deixa um trofeu que faz o altar do proximo; o ultimo trofeu faz a Espada Devastadora
   - Dragao: nasce do ovo de dragao, e domado com rubis e da para montar e voar
   - Skins proprias para todos os monstros (misc/minerios/gen_textures.py)
   - Bloco da sorte: ao quebrar, sai uma surpresa boa (minerios, espadas, Big Bertha)
     ou ruim (mosquitos, monstro gigante, explosao, gaiola de vidro)
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
	88, 89, 90, 91, 92, 93, 94, 87, 95, 127, 27, 43, 44, 45, 46, 59, 60, 61, 62, 63,
	/* chefoes e dragao: alem das 8 linhas do atlas reserva (o pacote padrao tem 16) */
	160, 161, 162, 163, 164, 165, 166, 167, 168, 169
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
{ "Bloco da sorte",        T(LUCKY_BLOCK),         T(LUCKY_BLOCK),          T(LUCKY_BLOCK),         DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_WOOD,  FANCY_LIGHTING_MAX_LEVEL / 3 },
{ "Ferrao da Rainha",      T(TROFEU_FERRAO),       T(TROFEU_FERRAO),        T(TROFEU_FERRAO),       DRAW_SPRITE,      COLLIDE_NONE,  SOUND_METAL, 0 },
{ "Nucleo de titanio",     T(TROFEU_NUCLEO),       T(TROFEU_NUCLEO),        T(TROFEU_NUCLEO),       DRAW_SPRITE,      COLLIDE_NONE,  SOUND_METAL, 0 },
{ "Olho de cristal",       T(TROFEU_OLHO),         T(TROFEU_OLHO),          T(TROFEU_OLHO),         DRAW_SPRITE,      COLLIDE_NONE,  SOUND_GLASS, 0 },
{ "Coracao de uranio",     T(TROFEU_CORACAO),      T(TROFEU_CORACAO),       T(TROFEU_CORACAO),      DRAW_SPRITE,      COLLIDE_NONE,  SOUND_GLASS, 0 },
{ "Altar da Rainha",       T(ALTAR_RAINHA),        T(ALTAR_RAINHA),         T(ALTAR_RAINHA),        DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_STONE, FANCY_LIGHTING_MAX_LEVEL / 3 },
{ "Altar do Golem",        T(ALTAR_GOLEM),         T(ALTAR_GOLEM),          T(ALTAR_GOLEM),         DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_STONE, FANCY_LIGHTING_MAX_LEVEL / 3 },
{ "Altar da Aranha",       T(ALTAR_ARANHA),        T(ALTAR_ARANHA),         T(ALTAR_ARANHA),        DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_STONE, FANCY_LIGHTING_MAX_LEVEL / 3 },
{ "Altar do Rei Creeper",  T(ALTAR_CREEPER),       T(ALTAR_CREEPER),        T(ALTAR_CREEPER),       DRAW_OPAQUE,      COLLIDE_SOLID, SOUND_STONE, FANCY_LIGHTING_MAX_LEVEL / 3 },
{ "Espada Devastadora",    T(ESPADA_DEVASTADORA),  T(ESPADA_DEVASTADORA),   T(ESPADA_DEVASTADORA),  DRAW_SPRITE,      COLLIDE_NONE,  SOUND_METAL, FANCY_LIGHTING_MAX_LEVEL / 2 },
{ "Ovo de dragao",         T(OVO_DRAGAO),          T(OVO_DRAGAO),           T(OVO_DRAGAO),          DRAW_SPRITE,      COLLIDE_NONE,  SOUND_STONE, 0 },
};
#define MINERIOS_COUNT Array_Elems(minerio_defs)
#define Def(block) (&minerio_defs[(block) - MBLOCK_RUBI_MINERIO])

static cc_bool EhItem(BlockID b) {
	return (b >= MITEM_ESPADA_TITANIO && b <= MITEM_ARMADURA_TITANIO) ||
		(b >= MITEM_FERRAO && b <= MITEM_CORACAO) || b == MITEM_DEVASTADORA;
}
static cc_bool EhAltar(BlockID b) { return b >= MBLOCK_ALTAR_RAINHA && b <= MBLOCK_ALTAR_CREEPER; }

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
*----------------------------------------------------Modelos proprios-----------------------------------------------------*
*#########################################################################################################################*/
/* Modelos feitos de caixas (tabelas geradas em Minerios_tex.h) */
struct ModeloProprio {
	const char* nome;
	const cc_int8 (*partes)[12];
	int count;
	const cc_uint8* png; int pngLen;
	float asaVel, asaAbertura;
	const cc_int8* caixa; /* caixa em volta do modelo, para acertar e para o nome */
	struct ModelVertex* vertices;
	struct ModelPart* modelPartes;
	struct ModelTex tex;
	struct Model model;
};

#define MODELO_VERTICES(nome, NOME) \
	static struct ModelVertex nome##_vertices[NOME##_PARTES * MODEL_BOX_VERTICES]; \
	static struct ModelPart   nome##_modelPartes[NOME##_PARTES];
MODELO_VERTICES(mosquito,       MOSQUITO)
MODELO_VERTICES(dragao,         DRAGAO)
MODELO_VERTICES(rainha,         RAINHA)
MODELO_VERTICES(golem,          GOLEM)
MODELO_VERTICES(aranha_cristal, ARANHA_CRISTAL)
MODELO_VERTICES(rei_creeper,    REI_CREEPER)
MODELO_VERTICES(dragao_fogo,    DRAGAO_FOGO)
MODELO_VERTICES(dragao_gelo,    DRAGAO_GELO)

#define MODELO(nome, NOME, modelo, asaVel, asaAbertura) \
	{ modelo, nome##_partes, NOME##_PARTES, nome##_png, sizeof(nome##_png), asaVel, asaAbertura, nome##_caixa, \
		nome##_vertices, nome##_modelPartes, { modelo ".png", SKIN_64x64 } }

static struct ModeloProprio modelos[] = {
	MODELO(mosquito,       MOSQUITO,       "mosquito",         45.0f, 0.7f),
	MODELO(dragao,         DRAGAO,         "dragao_esmeralda",  6.0f, 0.6f),
	MODELO(rainha,         RAINHA,         "rainha_mosquito",  30.0f, 0.6f),
	MODELO(golem,          GOLEM,          "golem_titanio",     0.0f, 0.0f),
	MODELO(aranha_cristal, ARANHA_CRISTAL, "aranha_cristal",    0.0f, 0.0f),
	MODELO(rei_creeper,    REI_CREEPER,    "rei_creeper",       0.0f, 0.0f),
	MODELO(dragao_fogo,    DRAGAO_FOGO,    "dragao_fogo",       4.5f, 0.7f),
	MODELO(dragao_gelo,    DRAGAO_GELO,    "dragao_gelo",       8.0f, 0.4f),
};

static struct ModeloProprio* ModeloDe(struct Model* model) {
	int i;
	for (i = 0; i < Array_Elems(modelos); i++)
	{
		if (model == &modelos[i].model) return &modelos[i];
	}
	return &modelos[0];
}
#define ModeloAtivo() ModeloDe(Models.Active)

static void ModeloProprio_MakeParts(void) {
	struct ModeloProprio* mp = ModeloAtivo();
	struct BoxDesc desc;
	int i;

	for (i = 0; i < mp->count; i++)
	{
		const cc_int8* p = mp->partes[i];
		desc.texX  = p[6]; desc.texY  = p[7];
		desc.sizeX = p[3] - p[0]; desc.sizeY = p[4] - p[1]; desc.sizeZ = p[5] - p[2];
		desc.x1 = p[0] / 16.0f; desc.y1 = p[1] / 16.0f; desc.z1 = p[2] / 16.0f;
		desc.x2 = p[3] / 16.0f; desc.y2 = p[4] / 16.0f; desc.z2 = p[5] / 16.0f;
		/* ponto em volta do qual a parte gira (asas no corpo, pernas no quadril) */
		desc.rotX = p[9] / 32.0f; desc.rotY = p[10] / 32.0f; desc.rotZ = p[11] / 32.0f;
		BoxDesc_BuildBox(&mp->modelPartes[i], &desc);
	}
}

static void ModeloProprio_CheckTexture(struct ModeloProprio* mp) {
	cc_string name;
	struct Stream mem;
	if (mp->tex.texID) return;

	name = String_FromReadonly(mp->tex.name);
	Stream_ReadonlyMemory(&mem, (void*)mp->png, mp->pngLen);
	Game_UpdateTexture(&mp->tex.texID, &mem, &name, &mp->tex.skinType, NULL);
}

static void ModeloProprio_Draw(struct Entity* e) {
	struct ModeloProprio* mp = ModeloAtivo();
	/* bate as asas (o mosquito bem rapido, o dragao devagar) */
	float asa  = Math_SinF((float)Game.Time * mp->asaVel) * mp->asaAbertura;
	float rabo = Math_SinF((float)Game.Time * 2.0f) * 0.3f;
	int i, verts = mp->count * MODEL_BOX_VERTICES;

	ModeloProprio_CheckTexture(mp);
	Model_ApplyTexture(e);
	Model_LockVB(e, verts);

	for (i = 0; i < mp->count; i++)
	{
		struct ModelPart* part = &mp->modelPartes[i];
		switch (mp->partes[i][8])
		{
		case 1:  Model_DrawRotate(0, 0, -asa, part, false); break;
		case 2:  Model_DrawRotate(0, 0,  asa, part, false); break;
		/* pernas abertas para os lados, como um mosquito pousando */
		case 3:  Model_DrawRotate(0, 0,  0.75f, part, false); break;
		case 4:  Model_DrawRotate(0, 0, -0.75f, part, false); break;
		/* pernas andando */
		case 5:  Model_DrawRotate(e->Anim.LeftLegX,  0, 0, part, false); break;
		case 6:  Model_DrawRotate(e->Anim.RightLegX, 0, 0, part, false); break;
		/* rabo balancando (7 e 14 em lados opostos, como uma cobra) */
		case 7:  Model_DrawRotate(0,  rabo, 0, part, false); break;
		case 14: Model_DrawRotate(0, -rabo, 0, part, false); break;
		/* bracos balancando ao contrario das pernas */
		case 8:  Model_DrawRotate(e->Anim.RightLegX * 0.8f, 0, 0, part, false); break;
		case 9:  Model_DrawRotate(e->Anim.LeftLegX  * 0.8f, 0, 0, part, false); break;
		/* pernas de aranha: inclinadas para baixo e mexendo para frente e para tras */
		case 10: Model_DrawRotate(0,  e->Anim.LeftLegX  * 0.5f, -0.55f, part, false); break;
		case 11: Model_DrawRotate(0,  e->Anim.RightLegX * 0.5f,  0.55f, part, false); break;
		default: Model_DrawPart(part); break;
		}
	}
	Model_UnlockVB();
	Gfx_DrawVb_IndexedTris(verts);
}

static float ModeloProprio_GetNameY(struct Entity* e) { return (ModeloDe(e->Model)->caixa[4] + 2) / 16.0f; }
static float ModeloProprio_GetEyeY(struct Entity* e)  { return ModeloDe(e->Model)->caixa[4] * 0.7f / 16.0f; }
static void  ModeloProprio_GetSize(struct Entity* e) {
	const cc_int8* c = ModeloDe(e->Model)->caixa;
	float largura = min(c[3] - c[0], c[5] - c[2]) / 16.0f;
	Vec3_Set(e->Size, largura, (c[4] - c[1]) / 16.0f, largura);
}
static void  ModeloProprio_GetBounds(struct Entity* e) {
	/* um pouco maior que o desenho, para ser facil acertar */
	const cc_int8* c = ModeloDe(e->Model)->caixa;
	Vec3_Set(e->ModelAABB.Min, (c[0] - 1) / 16.0f, c[1] / 16.0f, (c[2] - 1) / 16.0f);
	Vec3_Set(e->ModelAABB.Max, (c[3] + 1) / 16.0f, (c[4] + 1) / 16.0f, (c[5] + 1) / 16.0f);
}

static void Modelos_Register(void) {
	struct ModeloProprio* mp;
	struct Model* m;
	int i;

	for (i = 0; i < Array_Elems(modelos); i++)
	{
		mp = &modelos[i];
		m  = &mp->model;
		Model_RegisterTexture(&mp->tex);

		m->name       = mp->nome;
		m->vertices   = mp->vertices;
		m->defaultTex = &mp->tex;
		m->MakeParts  = ModeloProprio_MakeParts;
		m->Draw       = ModeloProprio_Draw;
		m->GetNameY   = ModeloProprio_GetNameY;
		m->GetEyeY    = ModeloProprio_GetEyeY;
		m->GetCollisionSize = ModeloProprio_GetSize;
		m->GetPickingBounds = ModeloProprio_GetBounds;

		Model_Init(m);
		m->usesSkin    = false;
		m->maxVertices = mp->count * MODEL_BOX_VERTICES;
		Model_Register(m);
	}
}


/*########################################################################################################################*
*---------------------------------------------------Skins dos monstros----------------------------------------------------*
*#########################################################################################################################*/
/* Skins proprias para os modelos padrao (galinha, porco, aranha...), para os monstros nao */
/* ficarem iguais aos do jogo normal. Ficam registradas como texturas de modelo, entao sao */
/* recriadas sozinhas se o contexto grafico for perdido. */
static struct ModelTex skins_tex[MINERIOS_SKINS];

static GfxResourceID SkinTex(int i, cc_uint8* skinType) {
	struct ModelTex* tex = &skins_tex[i];
	cc_string name;
	struct Stream mem;

	if (!tex->texID) {
		name = String_FromReadonly(tex->name);
		Stream_ReadonlyMemory(&mem, (void*)minerios_skins[i].data, minerios_skins[i].len);
		Game_UpdateTexture(&tex->texID, &mem, &name, &tex->skinType, NULL);
	}
	*skinType = tex->skinType;
	return tex->texID;
}

static void Skins_Register(void) {
	static const char* nomes[MINERIOS_SKINS] = {
		"m_galinha.png", "m_leitao.png", "m_carneiro.png", "m_aranha.png", "m_aranha_gigante.png",
		"m_zumbi_titan.png", "m_creeper_colosso.png", "m_galinha_monstro.png"
	};
	int i;
	for (i = 0; i < MINERIOS_SKINS; i++)
	{
		skins_tex[i].name = nomes[i];
		Model_RegisterTexture(&skins_tex[i]);
	}
}


/*########################################################################################################################*
*-------------------------------------------------------Monstros----------------------------------------------------------*
*#########################################################################################################################*/
#define MONSTROS_MAX     14
#define MONSTRO_ID_BASE  226 /* 200-225 ficam com Criaturas do Bioma, 240-247 com os monstros da noite */
#define DIST_SUMIR       (48.0f * 48.0f)
#define DIST_SUMIR_CHEFE (96.0f * 96.0f)

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
	int skin;       /* skin propria (SKIN_*) ou -1 para a textura do modelo */
	int chefe;      /* ordem do chefao (1 a 4), 0 se nao for chefao */
};

enum {
	M_GALINHA, M_LEITAO, M_CARNEIRO, M_ARANHA, M_ARANHA_GIGANTE, M_ZUMBI_TITAN, M_CREEPER_COLOSSO, M_GALINHA_MONSTRO,
	M_MOSQUITO, M_RAINHA, M_GOLEM, M_ARANHA_CRISTAL, M_REI_CREEPER, M_DRAGAO, M_DRAGAO_FOGO, M_DRAGAO_GELO
};
#define EhDragao(tipo) ((tipo) >= M_DRAGAO && (tipo) <= M_DRAGAO_GELO)
#define DRAGOES 3
#define CHEFOES 4

static const struct MonstroDef monstro_defs[] = {
/*NOME                         MODELO         ESCALA VEL   BRAVO  VOA    CHANCE VIDA DANO DROP                    QTD SKIN                  CHEFE */
{ "&fGalinha chocada",         "chicken",     1.0f,  2.0f, false, false, 20,    4,   0,   BLOCK_AIR,              0,  SKIN_GALINHA,         0 },
{ "&dLeitaozinho",             "pig",         1.0f,  2.0f, false, false, 15,    6,   0,   BLOCK_AIR,              0,  SKIN_LEITAO,          0 },
{ "&bCarneirinho",             "sheep_nofur", 1.0f,  2.0f, false, false, 15,    6,   0,   BLOCK_AIR,              0,  SKIN_CARNEIRO,        0 },
{ "&7Aranha da caverna",       "spider",      1.0f,  3.5f, true,  false, 15,    8,   2,   BLOCK_AIR,              0,  SKIN_ARANHA,          0 },
{ "&cAranha Gigante",          "spider",      2.6f,  3.0f, true,  false, 12,    40,  3,   MBLOCK_RUBI_MINERIO,    3,  SKIN_ARANHA_GIGANTE,  0 },
{ "&2Zumbi Titan",             "zombie",      2.4f,  2.4f, true,  false, 10,    50,  4,   MBLOCK_TITANIO_MINERIO, 4,  SKIN_ZUMBI_TITAN,     0 },
{ "&9Creeper Colosso",         "creeper",     3.0f,  2.2f, true,  false,  8,    45,  5,   MBLOCK_URANIO_MINERIO,  3,  SKIN_CREEPER_COLOSSO, 0 },
{ "&6Galinha Monstro",         "chicken",     4.5f,  3.2f, true,  false,  5,    60,  4,   MBLOCK_RUBI,            1,  SKIN_GALINHA_MONSTRO, 0 },
{ "&8Mosquito",                "mosquito",    0.8f,  4.5f, true,  true,   0,    2,   1,   BLOCK_AIR,              0,  -1,                   0 },
{ "&5Rainha dos Mosquitos",    "rainha_mosquito", 1.7f, 3.2f, true, true, 0,    80,  4,   MITEM_FERRAO,           1,  -1,                   1 },
{ "&bGolem de Titanio",        "golem_titanio", 2.0f, 2.3f, true,  false,  0,    180, 6,   MITEM_NUCLEO,           1,  -1,                   2 },
{ "&dAranha de Cristal",       "aranha_cristal",1.8f, 3.6f, true,  false,  0,    300, 7,   MITEM_OLHO,             1,  -1,                   3 },
{ "&aRei Creeper",             "rei_creeper",   2.0f, 2.4f, true,  false,  0,    450, 9,   MITEM_CORACAO,          1,  -1,                   4 },
/* dragoes: o dano e o quanto eles batem nos monstros quando domados */
{ "&2Dragao Esmeralda",        "dragao_esmeralda", 1.5f, 6.0f, false, true, 0,  60,  8,   BLOCK_AIR,              0,  -1,                   0 },
{ "&cDragao de Fogo",          "dragao_fogo",   1.6f, 7.0f, false, true,   0,    70,  12,  BLOCK_AIR,              0,  -1,                   0 },
{ "&bDragao de Gelo",          "dragao_gelo",   1.6f, 8.0f, false, true,   0,    80,  6,   BLOCK_AIR,              0,  -1,                   0 },
};
#define MONSTRO_DEFS_COUNT Array_Elems(monstro_defs)

struct Monstro {
	struct Entity base;
	cc_bool usado, domado;
	int tipo, vida, comida;
	float tempo, pensarTimer, ataqueTimer, hurtTimer, angulo, especialTimer, correndo, congelado;
	Vec3 destino;
};
static struct Monstro monstros[MONSTROS_MAX];
static cc_bool armadura; /* jogador esta usando a armadura de titanio */
static cc_bool montado;  /* jogador esta montado no dragao */
static int dragaoPendente; /* trazer o dragao domado (1 a 3, o tipo) para o mundo novo */
#define OPT_DRAGAO "minerios-dragao"
#define RUBIS_PARA_DOMAR 3

static cc_bool Protegido(struct Monstro* m) {
	return monstro_defs[m->tipo].chefe || EhDragao(m->tipo);
}

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

static void Desmontar(void);
static void Monstro_Remover(struct Monstro* m) {
	if (!m->usado) return;
	if (m->domado && montado) Desmontar();
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

	if (m->domado) String_AppendConst(&str, "&aSeu ");
	String_AppendConst(&str, m->domado ? def->nome + 2 : def->nome); /* sem a cor */
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
	e->next.yaw  = Math_Atan2f(-dz, dx) * MATH_RAD2DEG; /* Math_Atan2f(x, y) = atan2(y, x) */
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
	e->next.yaw  = Math_Atan2f(-dz, dx) * MATH_RAD2DEG; /* Math_Atan2f(x, y) = atan2(y, x) */
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

static struct Monstro* Monstro_Criar(int tipo, Vec3 pos);
static void Monstro_Dano(struct Monstro* m, int dano, Vec3 origem, float empurrao);

static void Monstro_TickMosquito(struct Monstro* m, struct Entity* jogador, float delta) {
	const struct MonstroDef* def = &monstro_defs[m->tipo];
	Vec3 alvo = jogador->Position;

	cc_bool rainha = m->tipo == M_RAINHA;
	float raio = rainha ? 5.0f : 1.8f, alcance = rainha ? 1.6f : 0.8f;

	/* zumbe em circulos em volta da cabeca do jogador e da rasantes para picar */
	/* (a rainha voa mais longe e mais alto, e de vez em quando chama os filhos) */
	m->angulo += delta * (rainha ? 0.9f : 2.5f);
	if (m->ataqueTimer > (rainha ? 1.2f : 0.6f)) {
		alvo.x += Math_CosF(m->angulo) * raio;
		alvo.z += Math_SinF(m->angulo) * raio;
		alvo.y += (rainha ? 3.5f : 1.6f) + Math_SinF(m->angulo * 3.0f) * 0.4f;
	} else {
		alvo.y += rainha ? 0.2f : 1.2f;
	}
	Monstro_Voar(m, alvo, def->vel, delta);

	if (m->ataqueTimer <= 0 && Dist2(m->base.next.pos, alvo) < alcance * alcance) {
		Monstro_Atacar(m, jogador);
		if (rainha) m->ataqueTimer = 4.0f;
	}

	if (rainha && m->pensarTimer <= 0) {
		m->pensarTimer = 9.0f;
		if (ContarTipo(M_MOSQUITO) < 4) {
			Monstro_Criar(M_MOSQUITO, m->base.next.pos);
			Monstro_Criar(M_MOSQUITO, m->base.next.pos);
			Chat_AddRaw("&5A Rainha chamou mais mosquitos!");
		}
	}
}

static void Explodir(int x, int y, int z, int raio) {
	int dx, dy, dz;
	BlockID b;

	for (dy = -raio; dy <= raio; dy++)
		for (dz = -raio; dz <= raio; dz++)
			for (dx = -raio; dx <= raio; dx++)
			{
				if (dx * dx + dy * dy + dz * dz > raio * raio + 1) continue;
				if (!World_Contains(x + dx, y + dy, z + dz)) continue;
				b = World_GetBlock(x + dx, y + dy, z + dz);
				if (b == BLOCK_AIR || b == BLOCK_BEDROCK || EhAltar(b)) continue;
				Game_UpdateBlock(x + dx, y + dy, z + dz, BLOCK_AIR);
			}
}

/* Os ataques especiais de cada chefao */
static void Chefe_Especial(struct Monstro* m, struct Entity* jogador, float d2) {
	Vec3 pos = m->base.next.pos;
	if (m->especialTimer > 0) return;

	switch (m->tipo)
	{
	case M_GOLEM:
		/* pisao: joga o jogador para cima */
		m->especialTimer = 6.0f;
		if (d2 > 7 * 7) return;
		jogador->Velocity.y = 0.9f;
		DanoJogador(5);
		Chat_AddRaw("&bO Golem de Titanio pisou forte no chao!");
		break;
	case M_ARANHA_CRISTAL:
		/* da um bote: corre muito rapido por um tempinho */
		m->especialTimer = 7.0f;
		m->correndo      = 1.5f;
		Chat_AddRaw("&dA Aranha de Cristal deu um bote!");
		break;
	case M_REI_CREEPER:
		/* explode tudo em volta sem morrer */
		m->especialTimer = 8.0f;
		if (d2 > 10 * 10) return;
		Explodir(Math_Floor(pos.x), Math_Floor(pos.y), Math_Floor(pos.z), 3);
		if (d2 < 5 * 5) DanoJogador(8);
		Chat_AddRaw("&aO Rei Creeper explodiu de raiva!");
		break;
	}
}

/* O dragao: selvagem fica voando em volta de onde nasceu; domado segue o jogador, */
/* briga com os monstros bravos e pode ser montado */
static void Monstro_TickDragao(struct Monstro* m, struct Entity* jogador, float delta) {
	const struct MonstroDef* def = &monstro_defs[m->tipo];
	struct Monstro* alvoM = NULL;
	float melhor = 8.0f * 8.0f, d;
	Vec3 alvo;
	int i;

	if (m->domado && montado) {
		/* o jogador voa e o dragao vai junto, logo embaixo */
		alvo = jogador->Position;
		alvo.y -= def->escala * 0.8f;
		m->base.next.pos   = alvo;
		m->base.next.yaw   = jogador->Yaw;
		m->base.next.rotY  = jogador->Yaw;
		return;
	}

	m->angulo += delta * 0.7f;
	if (m->domado) {
		/* segue o jogador, um pouco atras e acima */
		alvo = jogador->Position;
		if (Dist2(alvo, m->base.next.pos) > 24 * 24) {
			alvo.y += 3.0f;
			m->base.next.pos = alvo; m->base.prev.pos = alvo;
			return;
		}
		alvo.x += Math_CosF(m->angulo) * 3.0f;
		alvo.z += Math_SinF(m->angulo) * 3.0f;
		alvo.y += 2.5f;
	} else {
		alvo = m->destino;
		alvo.x += Math_CosF(m->angulo) * 5.0f;
		alvo.z += Math_SinF(m->angulo) * 5.0f;
		alvo.y += 4.0f;
	}
	Monstro_Voar(m, alvo, def->vel * (m->domado ? 1.0f : 0.5f), delta);

	/* o dragao domado ataca o monstro bravo mais perto */
	if (!m->domado || m->ataqueTimer > 0) return;
	for (i = 0; i < MONSTROS_MAX; i++)
	{
		struct Monstro* o = &monstros[i];
		if (!o->usado || o == m || !monstro_defs[o->tipo].bravo) continue;
		d = Dist2(o->base.next.pos, m->base.next.pos);
		if (d < melhor) { melhor = d; alvoM = o; }
	}
	if (!alvoM) return;
	m->ataqueTimer = 1.2f;
	Monstro_Dano(alvoM, def->dano, m->base.next.pos, 1.0f);
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

	m->especialTimer -= delta;
	m->correndo      -= delta;
	m->congelado     -= delta;
	if (m->domado) m->tempo = 600.0f; /* o dragao domado nunca vai embora */

	d2 = Dist2XZ(pos, jogador->Position);
	if (m->tempo <= 0 || (!m->domado && d2 > (def->chefe ? DIST_SUMIR_CHEFE : DIST_SUMIR))) { Monstro_Remover(m); return; }

	if (EhDragao(m->tipo)) {
		Monstro_TickDragao(m, jogador, delta);
	} else if (def->voa) {
		Monstro_TickMosquito(m, jogador, delta);
	} else if (def->bravo && d2 < (def->chefe ? 40 * 40 : 16 * 16) && Math_AbsF(jogador->Position.y - pos.y) < (def->chefe ? 12 : 6)) {
		/* persegue o jogador */
		alcance = 0.9f + def->escala * 0.6f;
		if (def->chefe) Chefe_Especial(m, jogador, d2);
		if (d2 > alcance * alcance) {
			Monstro_Andar(m, jogador->Position, def->vel * (m->correndo > 0 ? 2.2f : 1.0f) * (m->congelado > 0 ? 0.3f : 1.0f), delta);
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

	/* congelado pelo sopro do dragao de gelo */
	if (m->congelado > 0) col = PackedCol_Tint(col, PackedCol_Make(140, 200, 255, 255));
	/* a rainha tem a carapaca roxa */
	if (m->tipo == M_RAINHA) col = PackedCol_Tint(col, PackedCol_Make(200, 120, 255, 255));
	/* pisca vermelho ao levar dano */
	if (m->hurtTimer > 0) col = PackedCol_Tint(col, PackedCol_Make(255, 80, 80, 255));
	return col;
}

static void Monstro_RenderModel(struct Entity* e, float delta, float t) {
	struct Monstro* m = (struct Monstro*)e;
	int skin = monstro_defs[m->tipo].skin;

	/* usa a skin propria do monstro no lugar da textura padrao do modelo */
	if (skin >= 0) {
		e->TextureId    = SkinTex(skin, &e->SkinType);
		e->NonHumanSkin = e->TextureId != 0;
	}
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
	/* sem vaga: o mais antigo volta para a terra (chefoes e o dragao ficam) */
	if (i == MONSTROS_MAX) {
		for (i = 0; i < MONSTROS_MAX && Protegido(&monstros[i]); i++) { }
		if (i == MONSTROS_MAX) return NULL;
		Monstro_Remover(&monstros[i]);
	}
	m = &monstros[i];

	Mem_Set(m, 0, sizeof(struct Monstro));
	Entity_Init(&m->base);
	m->base.VTABLE = &monstro_VTABLE;
	m->usado   = true;
	m->tipo    = tipo;
	m->vida    = def->vidaMax;
	m->tempo   = def->chefe || EhDragao(tipo) ? 600.0f : def->bravo ? 90.0f : 300.0f;
	m->especialTimer = 4.0f;
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


/*########################################################################################################################*
*----------------------------------------------------Bloco da sorte-------------------------------------------------------*
*#########################################################################################################################*/
/* Ganha um bloco ou item: vai para o inventario na sobrevivencia, ou aparece no lugar do bloco da sorte */
static void SorteDar(int x, int y, int z, BlockID bloco, int quantos) {
	cc_string nome = Block_UNSAFE_GetName(bloco);

	if (Survival_IsActive()) {
		Survival_AddBlocks(bloco, quantos);
	} else {
		Game_UpdateBlock(x, y, z, bloco);
	}
	Chat_Add2("&aSorte! &fVoce ganhou %i %s", &quantos, &nome);
}

static void SorteExplosao(int x, int y, int z) {
	int dx, dy, dz;
	BlockID b;

	for (dy = -2; dy <= 2; dy++)
		for (dz = -2; dz <= 2; dz++)
			for (dx = -2; dx <= 2; dx++)
			{
				if (dx * dx + dy * dy + dz * dz > 5) continue;
				if (!World_Contains(x + dx, y + dy, z + dz)) continue;
				b = World_GetBlock(x + dx, y + dy, z + dz);
				if (b == BLOCK_BEDROCK || b == BLOCK_AIR) continue;
				Game_UpdateBlock(x + dx, y + dy, z + dz, BLOCK_AIR);
			}
	DanoJogador(6);
	Chat_AddRaw("&cAzar! &fO bloco da sorte explodiu!");
}

/* Uma gaiola de vidro em volta do jogador (da para quebrar e sair) */
static void SorteGaiola(void) {
	struct Entity* jogador = &Entities.CurPlayer->Base;
	int px = Math_Floor(jogador->Position.x);
	int py = Math_Floor(jogador->Position.y);
	int pz = Math_Floor(jogador->Position.z);
	int dx, dy, dz;

	for (dy = -1; dy <= 2; dy++)
		for (dz = -1; dz <= 1; dz++)
			for (dx = -1; dx <= 1; dx++)
			{
				if (dx == 0 && dz == 0 && dy >= 0 && dy <= 1) continue;
				if (!World_Contains(px + dx, py + dy, pz + dz)) continue;
				if (World_GetBlock(px + dx, py + dy, pz + dz) != BLOCK_AIR) continue;
				Game_UpdateBlock(px + dx, py + dy, pz + dz, BLOCK_GLASS);
			}
	Chat_AddRaw("&cAzar! &fVoce ficou preso numa gaiola de vidro!");
}

/* Uma torre de ouro sobe do chao */
static void SorteTorre(int x, int y, int z) {
	int i;
	for (i = 0; i < 6; i++)
	{
		if (!World_Contains(x, y + i, z) || World_GetBlock(x, y + i, z) != BLOCK_AIR) break;
		Game_UpdateBlock(x, y + i, z, BLOCK_GOLD);
	}
	Chat_AddRaw("&aSorte! &fUma torre de ouro!");
}

/* Um anel de flores em volta do bloco */
static void SorteJardim(int x, int y, int z) {
	int dx, dz;
	for (dz = -2; dz <= 2; dz++)
		for (dx = -2; dx <= 2; dx++)
		{
			if (!World_Contains(x + dx, y, z + dz) || World_GetBlock(x + dx, y, z + dz) != BLOCK_AIR) continue;
			if (!y || !Solido(World_GetBlock(x + dx, y - 1, z + dz))) continue;
			Game_UpdateBlock(x + dx, y, z + dz, (dx + dz) & 1 ? BLOCK_ROSE : BLOCK_DANDELION);
		}
	Chat_AddRaw("&aSorte! &fFlores brotaram em volta!");
}

enum {
	S_RUBI, S_TITANIO, S_URANIO, S_ESPADA_TITANIO, S_ARMADURA, S_ESPADA_SUPREMA, S_BIG_BERTHA,
	S_TORRE, S_FILHOTES, S_JARDIM, S_OVOS, S_DRAGAO,
	S_MOSQUITOS, S_GIGANTE, S_EXPLOSAO, S_GAIOLA, S_SORTE_COUNT
};
/* Chance de cada surpresa (de um total de 100) */
static const cc_uint8 sorte_chances[S_SORTE_COUNT] = {
	10, 10, 7, 7, 5, 3, 1,
	7, 8, 6, 7, 2,
	9, 6, 7, 5
};

/* Abre um bloco da sorte quebrado em (x, y, z) */
static void AbrirSorte(int x, int y, int z) {
	int i, r = Random_Next(&rnd, 100);
	Vec3 pos;

	for (i = 0; i < S_SORTE_COUNT - 1; i++)
	{
		if (r < sorte_chances[i]) break;
		r -= sorte_chances[i];
	}

	switch (i)
	{
	case S_RUBI:            SorteDar(x, y, z, MBLOCK_RUBI_MINERIO,    5); break;
	case S_TITANIO:         SorteDar(x, y, z, MBLOCK_TITANIO_MINERIO, 4); break;
	case S_URANIO:          SorteDar(x, y, z, MBLOCK_URANIO_MINERIO,  3); break;
	case S_ESPADA_TITANIO:  SorteDar(x, y, z, MITEM_ESPADA_TITANIO,   1); break;
	case S_ARMADURA:        SorteDar(x, y, z, MITEM_ARMADURA_TITANIO, 1); break;
	case S_ESPADA_SUPREMA:  SorteDar(x, y, z, MITEM_ESPADA_SUPREMA,   1); break;
	case S_BIG_BERTHA:
		Chat_AddRaw("&6SUPER SORTE!!! &fA lendaria Big Bertha!");
		SorteDar(x, y, z, MITEM_BIG_BERTHA, 1); break;
	case S_TORRE:   SorteTorre(x, y, z);  break;
	case S_JARDIM:  SorteJardim(x, y, z); break;
	case S_FILHOTES:
		Chat_AddRaw("&aSorte! &fFilhotes apareceram!");
		for (r = 0; r < 3; r++)
		{
			Vec3_Set(pos, x + 0.5f + (r - 1) * 1.2f, (float)y, z + 0.5f);
			Monstro_Criar(Random_Next(&rnd, 3), pos); /* galinha, leitao ou carneiro */
		}
		break;
	case S_OVOS:
		SorteDar(x, y, z, MBLOCK_OVO_MINERIO, 3); break;
	case S_DRAGAO:
		Chat_AddRaw("&6SUPER SORTE!!! &fUm ovo de dragao! Quebre para chocar.");
		Game_UpdateBlock(x, y, z, MBLOCK_OVO_DRAGAO); break;
	case S_MOSQUITOS:
		Chat_AddRaw("&cAzar! &fO bloco estava cheio de mosquitos!");
		SoltarMosquitos(5); break;
	case S_GIGANTE:
		ChocarOvo(x, y, z, M_ARANHA_GIGANTE + Random_Next(&rnd, 4)); break;
	case S_EXPLOSAO: SorteExplosao(x, y, z); break;
	default:         SorteGaiola(); break;
	}
}

/* A Rainha dos Mosquitos chega voando de longe */
static void SoltarRainha(void) {
	struct Entity* jogador = &Entities.CurPlayer->Base;
	float ang = Random_Float(&rnd) * 2 * MATH_PI;
	struct Monstro* m;
	Vec3 pos;

	pos.x = jogador->Position.x + Math_CosF(ang) * 14.0f;
	pos.z = jogador->Position.z + Math_SinF(ang) * 14.0f;
	pos.y = jogador->Position.y + 6.0f;
	if (!World_Contains(Math_Floor(pos.x), Math_Floor(pos.y), Math_Floor(pos.z))) pos = jogador->Position, pos.y += 6.0f;

	m = Monstro_Criar(M_RAINHA, pos);
	if (!m) return;
	m->pensarTimer = 6.0f;
	Chat_AddRaw("&5BZZZZZZ! &dA Rainha dos Mosquitos apareceu! &7(chefao 1 de 4)");
}

/* Chama o chefao do altar, um pouco atras do altar (visto pelo jogador) */
static void ChamarChefe(int tipo, int x, int y, int z) {
	struct Entity* jogador = &Entities.CurPlayer->Base;
	const struct MonstroDef* def = &monstro_defs[tipo];
	float dx = x + 0.5f - jogador->Position.x, dz = z + 0.5f - jogador->Position.z;
	float len = Math_SqrtF(dx * dx + dz * dz) + 0.01f, chaoY;
	struct Monstro* m;
	Vec3 pos;

	if (tipo == M_RAINHA) { SoltarRainha(); return; }
	if (ContarTipo(tipo)) { Chat_Add1("&e%c &eja esta por aqui!", def->nome); return; }

	pos.x = x + 0.5f + dx / len * 6.0f;
	pos.z = z + 0.5f + dz / len * 6.0f;
	if (!AcharChao(Math_Floor(pos.x), Math_Floor(pos.z), y + 8, y - 8, &chaoY)) chaoY = (float)y;
	pos.y = chaoY;

	m = Monstro_Criar(tipo, pos);
	if (!m) return;
	Chat_Add2("&cO chao treme... %c &capareceu! &7(chefao %i de 4)", def->nome, &def->chefe);
}

/* Choca um dragao (tipo < 0: sorteia entre Esmeralda, Fogo e Gelo) */
static void ChocarDragao(int x, int y, int z, int tipo) {
	struct Monstro* m;
	Vec3 pos;
	Vec3_Set(pos, x + 0.5f, (float)y, z + 0.5f);
	if (tipo < 0) tipo = M_DRAGAO + Random_Next(&rnd, DRAGOES);

	m = Monstro_Criar(tipo, pos);
	if (!m) return;
	Chat_Add1("&eNasceu um %c&e! De 3 rubis para ele (segure rubi e aperte colocar) para domar.", monstro_defs[tipo].nome);
}

static void OnBlockChanged(void* obj, IVec3 coords, BlockID old, BlockID now) {
	if (!Minerios_Enabled || !Server.IsSinglePlayer) return;

	/* colocar um altar chama o chefao (o altar some) */
	if (old == BLOCK_AIR && EhAltar(now) && Block_IsCustomDefined(now)) {
		Game_UpdateBlock(coords.x, coords.y, coords.z, BLOCK_AIR);
		ChamarChefe(M_RAINHA + (now - MBLOCK_ALTAR_RAINHA), coords.x, coords.y, coords.z);
		return;
	}

	if (now != BLOCK_AIR || !Block_IsCustomDefined(old)) return;
	switch (old)
	{
	case MBLOCK_SORTE:      AbrirSorte(coords.x, coords.y, coords.z); break;
	case MBLOCK_OVO_MINERIO: ChocarOvo(coords.x, coords.y, coords.z, -1); break;
	case MBLOCK_OVO_DRAGAO:  ChocarDragao(coords.x, coords.y, coords.z, -1); break;
	}
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
	case MITEM_DEVASTADORA:    return 50;
	}
	/* na mao (ou com um bloco): usa a espada de ferramenta da sobrevivencia, se tiver */
	return 1 + Survival_AttackDamage();
}

static void Monstro_Derrotado(struct Monstro* m) {
	const struct MonstroDef* def = &monstro_defs[m->tipo];
	int i;
	Chat_Add1("&e%c &efoi derrotado(a)!", def->nome);
	if (def->chefe == CHEFOES) {
		Chat_AddRaw("&6Voce venceu todos os chefoes! &eJunte o Coracao de uranio com a Big Bertha");
		Chat_AddRaw("&epara fazer a &6Espada Devastadora&e.");
	} else if (def->chefe) {
		Chat_AddRaw("&6Chefao vencido! &eUse o trofeu para fazer o altar do proximo chefao.");
	}

	if (def->drop != BLOCK_AIR) {
		Survival_AddBlocks(def->drop, def->dropCount);
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
	if (m->domado) return;

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
/* Golpe que acerta varios monstros: a Big Bertha acerta na frente, a Devastadora em volta */
static cc_bool GolpeEmArea(BlockID held) {
	struct Entity* jogador = &Entities.CurPlayer->Base;
	Vec3 dir = Vec3_GetDirVector(jogador->Yaw * MATH_DEG2RAD, 0);
	cc_bool devastadora = held == MITEM_DEVASTADORA;
	int i, acertos = 0;
	float dx, dz, len, alcance;

	for (i = 0; i < MONSTROS_MAX; i++)
	{
		struct Monstro* m = &monstros[i];
		if (!m->usado || EhDragao(m->tipo)) continue;
		dx  = m->base.next.pos.x - jogador->Position.x;
		dz  = m->base.next.pos.z - jogador->Position.z;
		len = Math_SqrtF(dx * dx + dz * dz) + 0.01f;
		alcance = (devastadora ? 6.5f : 4.5f) + monstro_defs[m->tipo].escala * 0.5f;
		if (len > alcance || Math_AbsF(m->base.next.pos.y - jogador->Position.y) > (devastadora ? 6 : 4)) continue;
		/* a Big Bertha so acerta o que esta na frente (mais ou menos 70 graus para cada lado) */
		if (!devastadora && (dx * dir.x + dz * dir.z) / len < 0.35f) continue;

		Monstro_Dano(m, DanoDoItem(held), jogador->Position, devastadora ? 3.5f : 2.5f);
		acertos++;
	}
	return acertos > 0;
}

/*########################################################################################################################*
*---------------------------------------------------------Dragao----------------------------------------------------------*
*#########################################################################################################################*/
static void Montar(void) {
	struct LocalPlayer* p = Entities.CurPlayer;
	montado = true;
	HacksComp_SetFlying(&p->Hacks, true);
	Chat_AddRaw("&2Voce montou no dragao! &ePule para subir, agache para descer.");
	Chat_AddRaw("&eAperte colocar para descer do dragao.");
}

/* Desce do dragao: o jogador vai para o chao embaixo dele, sem dano de queda */
static void Desmontar(void) {
	struct LocalPlayer* p = Entities.CurPlayer;
	struct Entity* e = &p->Base;
	struct LocationUpdate u;
	float chaoY;
	montado = false;

	if (AcharChao(Math_Floor(e->Position.x), Math_Floor(e->Position.z), Math_Floor(e->Position.y), 0, &chaoY)) {
		u.flags = LU_HAS_POS;
		Vec3_Set(u.pos, e->Position.x, chaoY, e->Position.z);
		e->VTABLE->SetLocation(e, &u);
	}
	Vec3_Set(e->Velocity, 0, 0, 0);
	HacksComp_SetFlying(&p->Hacks, false);
}

static struct Monstro* DragaoDomado(void) {
	int i;
	for (i = 0; i < MONSTROS_MAX; i++)
	{
		if (monstros[i].usado && monstros[i].domado) return &monstros[i];
	}
	return NULL;
}

static void DragaoClique(struct Monstro* m, BlockID held) {
	int total = RUBIS_PARA_DOMAR;
	if (m->domado) {
		if (DragaoDomado() == m) Montar();
		return;
	}
	if (held != MBLOCK_RUBI_MINERIO && held != MBLOCK_RUBI) {
		Chat_AddRaw("&2O dragao quer rubis! &eSegure um minerio de rubi e aperte colocar nele.");
		return;
	}
	if (Survival_IsActive()) {
		if (!Survival_CanPlace(held)) { Chat_AddRaw("&cVoce nao tem rubis."); return; }
		Survival_OnBlockPlaced(held); /* gasta um rubi */
	}

	m->comida++;
	if (m->comida < RUBIS_PARA_DOMAR) {
		Chat_Add2("&2O dragao comeu o rubi! &e(%i de %i)", &m->comida, &total);
		return;
	}
	if (DragaoDomado()) {
		Chat_AddRaw("&eVoce ja tem um dragao. Este ficou seu amigo, mas vai voltar para o ninho.");
		Monstro_Remover(m);
		return;
	}
	m->domado = true;
	Monstro_AtualizarNome(m);
	Options_SetInt(OPT_DRAGAO, m->tipo - M_DRAGAO + 1); /* lembra qual dragao e o seu */
	Chat_AddRaw("&aO dragao agora e seu! &eEle te segue e briga com os monstros. Aperte colocar nele para montar.");
}

static float soproTimer;

/* Montado, o botao de quebrar faz o dragao soprar na direcao que o jogador olha: */
/* fogo (mais dano), gelo (deixa os monstros lentos) ou o sopro verde do Esmeralda */
static void SoproDragao(void) {
	struct Entity* jogador = &Entities.CurPlayer->Base;
	struct Monstro* dragao = DragaoDomado();
	const struct MonstroDef* def;
	Vec3 eye, dir, pos, d;
	BlockID efeito;
	IVec3 bloco;
	float len;
	int i, dano;

	if (!dragao || soproTimer > 0) return;
	def = &monstro_defs[dragao->tipo];
	soproTimer = 1.0f;

	eye = Entity_GetEyePosition(jogador);
	dir = Vec3_GetDirVector(jogador->Yaw * MATH_DEG2RAD, jogador->Pitch * MATH_DEG2RAD);
	efeito = dragao->tipo == M_DRAGAO_FOGO ? BLOCK_LAVA : dragao->tipo == M_DRAGAO_GELO ? BLOCK_ICE : BLOCK_LEAVES;
	dano   = dragao->tipo == M_DRAGAO_FOGO ? def->dano * 2 : def->dano;

	/* rastro de particulas */
	for (i = 2; i <= 10; i += 2)
	{
		Vec3_Mul1(&pos, &dir, (float)i);
		Vec3_AddBy(&pos, &eye);
		IVec3_Floor(&bloco, &pos);
		Particles_BreakBlockEffect(bloco, efeito, BLOCK_AIR);
	}

	for (i = 0; i < MONSTROS_MAX; i++)
	{
		struct Monstro* m = &monstros[i];
		if (!m->usado || m->domado || EhDragao(m->tipo)) continue;
		Vec3_Sub(&d, &m->base.next.pos, &eye);
		d.y += 0.5f;
		len = Math_SqrtF(Vec3_LengthSquared(&d)) + 0.01f;
		if (len > 12.0f || (d.x * dir.x + d.y * dir.y + d.z * dir.z) / len < 0.85f) continue;

		Monstro_Dano(m, dano, eye, 1.5f);
		if (dragao->tipo == M_DRAGAO_GELO && m->usado) m->congelado = 4.0f;
	}
}

cc_bool Minerios_HandleClick(cc_bool place) {
	struct Entity* jogador;
	struct Monstro* m;
	BlockID held = Inventory_SelectedBlock;
	if (!Ativo()) return false;

	if (place) {
		/* montado: colocar desce do dragao */
		if (montado) { Desmontar(); return true; }

		m = MonstroNaMira(Entities.CurPlayer->ReachDistance + 1.0f);
		if (m && EhDragao(m->tipo)) { DragaoClique(m, held); return true; }
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

	if (montado) { SoproDragao(); return true; }
	jogador = &Entities.CurPlayer->Base;
	if ((held == MITEM_BIG_BERTHA || held == MITEM_DEVASTADORA) && GolpeEmArea(held)) return true;

	m = MonstroNaMira(Entities.CurPlayer->ReachDistance + (held == MITEM_BIG_BERTHA ? 1.5f : 0.5f));
	if (!m) return false;
	if (m->domado) return true; /* nao bate no proprio dragao */
	Monstro_Dano(m, DanoDoItem(held), jogador->Position, 0.8f);
	return true;
}


/*########################################################################################################################*
*-------------------------------------------------------Dimensoes---------------------------------------------------------*
*#########################################################################################################################*/
/* O Nether e o Fim (LateGame.c) usam o mesmo sistema, mas funcionam no modo sobrevivencia */
/*  mesmo com o mod desligado */
enum { DIM_NORMAL, DIM_UTOPIA, DIM_CRISTAL, DIM_NETHER, DIM_FIM, DIM_COUNT };
static const char* const dim_nomes[DIM_COUNT]    = { "Mundo normal", "Utopia", "Dimensao de Cristal", "Nether", "O Fim" };
static const char* const dim_arquivos[DIM_COUNT] = { 
	"maps/orespawn_normal.cw", "maps/orespawn_utopia.cw", "maps/orespawn_cristal.cw", "maps/orespawn_nether.cw", "maps/orespawn_fim.cw"
};

/* Se as dimensoes funcionam (com o mod ou no modo sobrevivencia) */
static cc_bool DimensoesLigadas(void) {
	return (Minerios_Enabled || Options_GetBool("survival-mode", true)) && Server.IsSinglePlayer && World.Loaded;
}

static int  dimAtual, dimDestino = -1, dimGerando = -1;
static int  viagemPendente = -1;
static cc_bool viajando, colocarPortal;
static float portalCooldown;
static Vec3  posSegura; /* ultima posicao do jogador fora de um portal */

/* Vida e blocos do modo sobrevivencia, levados de uma dimensao para outra */
static cc_bool survLevar;
static int     survVida, survComida;
static cc_uint8 survFerramentas[SURVIVAL_TOOLS_SIZE];
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
		survVida   = Survival_GetHealth();
		survComida = Survival_GetFood();
		Survival_GetCounts(survBlocos);
		Survival_GetTools(survFerramentas);
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
	if (b == LBLOCK_NETHER_PORTAL)  return DIM_NETHER;
	if (b == LBLOCK_END_PORTAL)     return DIM_FIM;
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
	BlockID portal, moldura;

	switch (dimAtual)
	{
	case DIM_UTOPIA:  portal = MBLOCK_PORTAL_UTOPIA;  moldura = MBLOCK_RUBI;          break;
	case DIM_CRISTAL: portal = MBLOCK_PORTAL_CRISTAL; moldura = MBLOCK_PEDRA_CRISTAL; break;
	case DIM_NETHER:  portal = LBLOCK_NETHER_PORTAL;  moldura = BLOCK_OBSIDIAN;       break;
	default:          portal = LBLOCK_END_PORTAL;     moldura = BLOCK_OBSIDIAN;       break;
	}

	if (!World_Contains(x, y, z) || !World_Contains(x, y + 1, z)) return;
	/* abre espaco para o portal (no Nether ele pode cair dentro da pedra) */
	if (y + 2 <= World.MaxY) Game_UpdateBlock(x, y + 2, z, moldura);
	if (y > 0) Game_UpdateBlock(x, y - 1, z, moldura);
	Game_UpdateBlock(x, y,     z, portal);
	Game_UpdateBlock(x, y + 1, z, portal);
}

static void AplicarCeu(int dim, cc_bool mudarAgora) {
	PackedCol sky    = ENV_DEFAULT_SKY_COLOR;
	PackedCol fog    = ENV_DEFAULT_FOG_COLOR;
	PackedCol clouds = ENV_DEFAULT_CLOUDS_COLOR;

	if (dim == DIM_UTOPIA) {
		sky    = PackedCol_Make( 90, 230, 215, 255);
		fog    = PackedCol_Make(205, 255, 235, 255);
		clouds = PackedCol_Make(255, 230, 150, 255);
	} else if (dim == DIM_CRISTAL) {
		sky    = PackedCol_Make(185, 120, 240, 255);
		fog    = PackedCol_Make(235, 180, 250, 255);
		clouds = PackedCol_Make(255, 205, 245, 255);
	} else if (dim == DIM_NETHER) {
		sky    = PackedCol_Make( 70,  15,  10, 255);
		fog    = PackedCol_Make(110,  30,  20, 255);
		clouds = PackedCol_Make( 70,  15,  10, 255);
	} else if (dim == DIM_FIM) {
		sky    = PackedCol_Make( 20,  10,  30, 255);
		fog    = PackedCol_Make( 35,  25,  45, 255);
		clouds = PackedCol_Make( 20,  10,  30, 255);
	}
	DayNight_SetMode(dim == DIM_NETHER ? DAYNIGHT_NETHER : (dim == DIM_FIM ? DAYNIGHT_END : DAYNIGHT_NORMAL));
	/* o ciclo de dia e noite clareia e escurece a partir dessas cores */
	DayNight_SetDayColors(sky, fog, clouds);
	if (!mudarAgora) return;
	Env_SetSkyCol(sky);
	Env_SetFogCol(fog);
	Env_SetCloudsCol(clouds);
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

/* Espalha o bloco pelo chao (blocos da sorte, ovos de dragao) */
static void GenSorte(BlockRaw bloco, int quantos) {
	int i, x, y, z;
	BlockRaw chao;
	for (i = 0; i < quantos; i++)
	{
		x = Random_Next(&rnd, World.Width);
		z = Random_Next(&rnd, World.Length);
		y = GenTopo(x, z);
		chao = GenGet(x, y, z);
		if (y + 1 > World.MaxY) continue;
		if (chao != BLOCK_GRASS && chao != BLOCK_SAND && chao != MBLOCK_GRAMA_CRISTAL) continue;
		GenSet(x, y + 1, z, bloco);
	}
}

int Minerios_DimensaoGerando(void) { return dimGerando; }

void Minerios_PosGeracao(void) {
	int area = World.Width * World.Length;
	if (!Game_AllowCustomBlocks) return;
	if (dimGerando == DIM_NETHER) { LateGame_GenNether(); return; }
	if (dimGerando == DIM_FIM)    { LateGame_GenEnd();    return; }
	if (!Minerios_Enabled) return;

	if (dimGerando == DIM_UTOPIA)  GerarUtopia();
	if (dimGerando == DIM_CRISTAL) GerarCristal();
	GenSorte(MBLOCK_SORTE, dimGerando == DIM_UTOPIA ? area / 500 : area / 2500);
	/* ovos de dragao: alguns na dimensao de Cristal, raros na Utopia */
	if (dimGerando == DIM_CRISTAL) GenSorte(MBLOCK_OVO_DRAGAO, max(3, area / 4000));
	if (dimGerando == DIM_UTOPIA)  GenSorte(MBLOCK_OVO_DRAGAO, area / 16000);
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

	if (argsCount && String_CaselessEqualsConst(&args[0], "rainha")) {
		SoltarRainha();
		return;
	}

	if (argsCount && String_CaselessEqualsConst(&args[0], "chefe")) {
		if (argsCount > 1) Convert_ParseInt(&args[1], &tipo);
		if (tipo < 1 || tipo > CHEFOES) { Chat_AddRaw("&e/client minerios chefe [1-4]"); return; }
		x = Math_Floor(jogador->Position.x + Math_SinF(yaw) * 4.0f);
		z = Math_Floor(jogador->Position.z - Math_CosF(yaw) * 4.0f);
		ChamarChefe(M_RAINHA + tipo - 1, x, Math_Floor(jogador->Position.y), z);
		return;
	}

	if (argsCount && String_CaselessEqualsConst(&args[0], "dragao")) {
		x = Math_Floor(jogador->Position.x + Math_SinF(yaw) * 4.0f);
		z = Math_Floor(jogador->Position.z - Math_CosF(yaw) * 4.0f);
		if (argsCount > 1) Convert_ParseInt(&args[1], &tipo);
		ChocarDragao(x, Math_Floor(jogador->Position.y), z, tipo >= 1 && tipo <= DRAGOES ? M_DRAGAO + tipo - 1 : -1);
		return;
	}

	if (argsCount && String_CaselessEqualsConst(&args[0], "sorte")) {
		x = Math_Floor(jogador->Position.x + Math_SinF(yaw) * 2.5f);
		z = Math_Floor(jogador->Position.z - Math_CosF(yaw) * 2.5f);
		i = Math_Floor(jogador->Position.y);
		if (!World_Contains(x, i, z)) return;
		Game_UpdateBlock(x, i, z, MBLOCK_SORTE);
		Chat_AddRaw("&eUm bloco da sorte apareceu na sua frente. Quebre para abrir!");
		return;
	}

	if (argsCount && String_CaselessEqualsConst(&args[0], "ir")) {
		int dim = -1;
		if (argsCount > 1 && String_CaselessEqualsConst(&args[1], "normal"))  dim = DIM_NORMAL;
		if (argsCount > 1 && String_CaselessEqualsConst(&args[1], "utopia"))  dim = DIM_UTOPIA;
		if (argsCount > 1 && String_CaselessEqualsConst(&args[1], "cristal")) dim = DIM_CRISTAL;
		if (argsCount > 1 && String_CaselessEqualsConst(&args[1], "nether"))  dim = DIM_NETHER;
		if (argsCount > 1 && String_CaselessEqualsConst(&args[1], "fim"))     dim = DIM_FIM;
		if (dim < 0) { Chat_AddRaw("&e/client minerios ir [normal|utopia|cristal|nether|fim]"); return; }
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
		"&a/client minerios ovo [numero|todos] | dragao [1-3] &e- choca um ovo na sua frente",
		"&a/client minerios chefe [1-4] | mosquitos | limpar &e- chama um chefao, mosquitos ou tira os monstros",
		"&a/client minerios sorte &e- coloca um bloco da sorte na sua frente",
		"&a/client minerios ir [normal|utopia|cristal|nether|fim] &e- viaja para a dimensao",
	}
};


/*########################################################################################################################*
*-------------------------------------------------------Componente--------------------------------------------------------*
*#########################################################################################################################*/
static float mosquitoTimer = 180.0f;

/* Tem agua a ate 8 blocos do jogador? */
static cc_bool PertoDaAgua(void) {
	Vec3 pos = Entities.CurPlayer->Base.Position;
	int px = (int)pos.x, py = (int)pos.y, pz = (int)pos.z;
	int x, y, z;
	BlockID b;

	for (y = py - 4; y <= py + 2; y++) {
		for (z = pz - 8; z <= pz + 8; z++) {
			for (x = px - 8; x <= px + 8; x++) {
				if (!World_Contains(x, y, z)) continue;
				b = World_GetBlock(x, y, z);
				if (b == BLOCK_WATER || b == BLOCK_STILL_WATER) return true;
			}
		}
	}
	return false;
}

/* Barra de vida do chefao mais perto, no canto de cima da tela */
static cc_bool barraVisivel;
static void AtualizarBarraChefe(void) {
	struct Entity* jogador = &Entities.CurPlayer->Base;
	struct Monstro* chefe = NULL;
	float d, melhor = 48.0f * 48.0f;
	cc_string str; char strBuffer[STRING_SIZE];
	int i, cheio;

	for (i = 0; i < MONSTROS_MAX; i++)
	{
		struct Monstro* m = &monstros[i];
		if (!m->usado || !monstro_defs[m->tipo].chefe) continue;
		d = Dist2(m->base.next.pos, jogador->Position);
		if (d < melhor) { melhor = d; chefe = m; }
	}

	if (!chefe) {
		if (barraVisivel) Chat_AddOf(&String_Empty, MSG_TYPE_EXTRASTATUS_2);
		barraVisivel = false; return;
	}
	String_InitArray(str, strBuffer);
	String_AppendConst(&str, monstro_defs[chefe->tipo].nome);
	String_AppendConst(&str, " &c");
	cheio = (max(0, chefe->vida) * 20 + monstro_defs[chefe->tipo].vidaMax - 1) / monstro_defs[chefe->tipo].vidaMax;
	for (i = 0; i < 20; i++)
	{
		if (i == cheio) String_AppendConst(&str, "&8");
		String_Append(&str, '|');
	}
	Chat_AddOf(&str, MSG_TYPE_EXTRASTATUS_2);
	barraVisivel = true;
}

static void Minerios_Tick(struct ScheduledTask* task) {
	float delta = (float)task->interval;
	int dest;
	portalCooldown -= delta;
	soproTimer     -= delta;
	if (Ativo()) AtualizarBarraChefe();
	if (!DimensoesLigadas() || viajando) return;

	if (colocarPortal) { colocarPortal = false; AcharTerraFirme(); ColocarPortalVolta(); return; }

	/* o dragao domado vem junto para cada mundo */
	if (dragaoPendente) {
		struct Monstro* m;
		Vec3 pos = Entities.CurPlayer->Base.Position;
		pos.x += 2.0f; pos.y += 3.0f;
		m = Monstro_Criar(M_DRAGAO + dragaoPendente - 1, pos);
		dragaoPendente = 0;
		if (m) { m->domado = true; Monstro_AtualizarNome(m); }
	}
	/* enquanto montado, continua voando (a sobrevivencia nao deixa voar sozinho) */
	if (montado) HacksComp_SetFlying(&Entities.CurPlayer->Hacks, true);

	ChecarPortal();
	if (viagemPendente >= 0) {
		dest = viagemPendente;
		viagemPendente = -1;
		Viajar(dest);
		return;
	}

	/* Enxames de mosquitos sao raros: so a noite ou perto da agua, uma vez a cada
	   poucos minutos (um pouco mais na Utopia; nenhum no Cristal, Nether e Fim) */
	if (!Ativo() || dimAtual == DIM_CRISTAL || dimAtual == DIM_NETHER || dimAtual == DIM_FIM) return;
	mosquitoTimer -= delta;
	if (mosquitoTimer > 0) return;
	mosquitoTimer = (dimAtual == DIM_UTOPIA ? 150.0f : 240.0f) + Random_Float(&rnd) * 180.0f;
	if (!DayNight_IsNight() && !PertoDaAgua()) return;
	if (Random_Next(&rnd, 2)) return;
	if (ContarTipo(M_MOSQUITO) == 0) SoltarMosquitos(2 + Random_Next(&rnd, 2));
	/* as vezes a rainha vem junto (mais na Utopia) */
	if (!ContarTipo(M_RAINHA) && Random_Next(&rnd, dimAtual == DIM_UTOPIA ? 8 : 16) == 0) SoltarRainha();
}

static void OnNewMap(void) {
	if (montado) Desmontar();
	RemoverTodos();
	mosquitoTimer = 180.0f;
	/* um mundo carregado pelo menu volta a ser o "mundo normal" */
	dimAtual = viajando ? dimDestino : DIM_NORMAL;
	if (!viajando) armadura = false;
}

static void OnNewMapLoaded(void) {
	BlockID block;
	int i;
	/* No multijogador o servidor decide quais blocos existem */
	Minerios_Enabled = Options_GetBool(MINERIOS_OPTION, true);
	if (!(Minerios_Enabled || Options_GetBool("survival-mode", true)) || !Server.IsSinglePlayer || !Game_AllowCustomBlocks) {
		AplicarCeu(DIM_NORMAL, false);
		viajando = false; return;
	}
	AplicarCeu(dimAtual, false);

	for (i = 0; i < MINERIOS_COUNT && Minerios_Enabled; i++)
	{
		block = (BlockID)(MBLOCK_RUBI_MINERIO + i);
		if (block > BLOCK_MAX_DEFINED) break;
		/* Mundos salvos podem ja definir o bloco (talvez diferente) */
		if (Block_IsCustomDefined(block)) continue;
		DefineBlock(block, &minerio_defs[i]);
	}
	dragaoPendente = Options_GetInt(OPT_DRAGAO, 0, DRAGOES, 0);
	if (!viajando) return;

	/* precisa vir antes do componente de sobrevivencia aplicar o estado do mundo */
	if (survLevar) {
		Survival_LoadHealth(survVida);
		Survival_LoadFood(survComida);
		Survival_LoadCounts(survBlocos, sizeof(survBlocos));
		Survival_LoadTools(survFerramentas, sizeof(survFerramentas));
	}
	if (dimGerando >= 0) {
		AplicarCeu(dimAtual, dimAtual != DIM_NORMAL);
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
	Modelos_Register();
	Skins_Register();
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

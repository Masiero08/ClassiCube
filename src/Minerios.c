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
#include "Minerios_tex.h"

/* Minerios & Monstros: um mod pequeno inspirado no OreSpawn.
   Os nomes e as texturas sao originais (misc/minerios/gen_textures.py).
   - Rubi, titanio e uranio: minerio (aparece em mundos novos) + bloco
   - Minerio de ovo: ao quebrar, choca uma criatura; as raras sao gigantes
     e perseguem o jogador, empurrando-o para longe.
*/

/*########################################################################################################################*
*--------------------------------------------------------Blocos-----------------------------------------------------------*
*#########################################################################################################################*/
/* Primeiro tile do atlas usado: 88-94 ficam vazios no pacote padrao, os de 96 em diante */
/* ficam com os blocos modernos, e o atlas reserva (16x8 tiles) so vai ate o 127 */
#define MINERIOS_TILE_BASE  88
/* Primeiro ID de bloco usado (66-93 ficam com os blocos modernos) */
#define MINERIOS_BLOCK_BASE 100
#define T(tex) (MINERIOS_TILE_BASE + MTX_##tex)

#define BLOCK_RUBI_MIN     (MINERIOS_BLOCK_BASE + 0)
#define BLOCK_TITANIO_MIN  (MINERIOS_BLOCK_BASE + 1)
#define BLOCK_URANIO_MIN   (MINERIOS_BLOCK_BASE + 2)
#define BLOCK_OVO_MIN      (MINERIOS_BLOCK_BASE + 3)

struct MinerioDef {
	const char* name;
	cc_uint8 tex, sound, brightness;
};

static const struct MinerioDef minerio_defs[] = {
/*NAME                     TEXTURE               SOUND        BRIGHTNESS */
{ "Minerio de rubi",       T(ORE_RUBY),          SOUND_STONE, 0 },
{ "Minerio de titanio",    T(ORE_TITANIUM),      SOUND_STONE, 0 },
{ "Minerio de uranio",     T(ORE_URANIUM),       SOUND_STONE, FANCY_LIGHTING_MAX_LEVEL / 2 },
{ "Minerio de ovo",        T(ORE_EGG),           SOUND_STONE, 0 },
{ "Bloco de rubi",         T(BLOCK_RUBY),        SOUND_GLASS, 0 },
{ "Bloco de titanio",      T(BLOCK_TITANIUM),    SOUND_METAL, 0 },
{ "Bloco de uranio",       T(BLOCK_URANIUM),     SOUND_METAL, FANCY_LIGHTING_MAX_LEVEL },
};
#define MINERIOS_COUNT Array_Elems(minerio_defs)

const struct MinerioVeio Minerios_Veios[] = {
	{ 0.30f, "Carving ruby ore",     BLOCK_RUBI_MIN    },
	{ 0.35f, "Carving titanium ore", BLOCK_TITANIO_MIN },
	{ 0.25f, "Carving uranium ore",  BLOCK_URANIO_MIN  },
	{ 0.30f, "Carving egg ore",      BLOCK_OVO_MIN     },
};
const int Minerios_VeiosCount = Array_Elems(Minerios_Veios);

cc_bool Minerios_Enabled = true;

static void DefineBlock(BlockID block, const struct MinerioDef* def) {
	cc_string name = String_FromReadonly(def->name);

	Block_ResetProps(block);
	Block_SetName(block, &name);
	Block_SetSide(def->tex, block);
	Block_Tex(block, FACE_YMAX) = def->tex;
	Block_Tex(block, FACE_YMIN) = def->tex;

	Blocks.Collide[block]     = COLLIDE_SOLID;
	Blocks.Draw[block]        = DRAW_OPAQUE;
	Blocks.BlocksLight[block] = true;
	Blocks.Brightness[block]  = def->brightness;
	Blocks.DigSounds[block]   = def->sound;
	Blocks.StepSounds[block]  = def->sound == SOUND_GLASS ? SOUND_STONE : def->sound;
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

/* Desenha a textura no tile; partes transparentes mostram o tile 'baseTile' (pedra) */
static void DesenharTile(struct Bitmap* bmp, int x, int y, int size, const cc_uint8* src, int baseTile) {
	int baseX = Atlas2D_TileX(baseTile) * size;
	int baseY = Atlas2D_TileY(baseTile) * size;
	int i, j, srcX, srcY;
	const cc_uint8* px;

	for (j = 0; j < size; j++)
	{
		BitmapCol* row  = Bitmap_GetRow(bmp, y + j) + x;
		BitmapCol* base = Bitmap_GetRow(bmp, baseY + j) + baseX;
		srcY = j * MINERIOS_TEX_SIZE / size;

		for (i = 0; i < size; i++)
		{
			srcX   = i * MINERIOS_TEX_SIZE / size;
			px     = src + (srcY * MINERIOS_TEX_SIZE + srcX) * 4;
			row[i] = px[3] ? BitmapCol_Make(px[0], px[1], px[2], 255) : base[i];
		}
	}
}

void Minerios_PatchAtlas(struct Bitmap* bmp) {
	int size = bmp->width / ATLAS2D_TILES_PER_ROW;
	int rows = size ? bmp->height / size : 0;
	int i, tile, x, y;

	for (i = 0; i < MINERIOS_TEX_COUNT; i++)
	{
		tile = MINERIOS_TILE_BASE + i;
		x    = Atlas2D_TileX(tile) * size;
		y    = Atlas2D_TileY(tile);
		if (y >= rows) return;
		y   *= size;

		/* Nao sobrescreve texturas que o pacote de texturas ja tem */
		if (!TileVazio(bmp, x, y, size)) continue;
		DesenharTile(bmp, x, y, size, minerios_texData[i], 1 /* pedra */);
	}
}


/*########################################################################################################################*
*-------------------------------------------------------Monstros----------------------------------------------------------*
*#########################################################################################################################*/
#define MONSTROS_MAX     8
#define MONSTRO_ID_BASE  232 /* 200-223 ficam com o mod Criaturas do Bioma */
#define DIST_SUMIR       (48.0f * 48.0f)

struct MonstroDef {
	const char* nome;
	const char* modelo;
	float escala, vel;
	cc_bool bravo; /* persegue e empurra o jogador */
	int chance;
};

static const struct MonstroDef monstro_defs[] = {
/*NOME                         MODELO      ESCALA VEL   BRAVO  CHANCE */
{ "&fGalinha chocada",         "chicken",  1.0f,  2.0f, false, 20 },
{ "&dLeitaozinho",             "pig",      1.0f,  2.0f, false, 15 },
{ "&fCarneirinho",             "sheep",    1.0f,  2.0f, false, 15 },
{ "&7Aranha da caverna",       "spider",   1.0f,  3.5f, true,  15 },
{ "&cAranha Gigante",          "spider",   2.6f,  3.0f, true,  12 },
{ "&2Zumbi Titan",             "zombie",   2.4f,  2.4f, true,  10 },
{ "&aCreeper Colosso",         "creeper",  3.0f,  2.2f, true,   8 },
{ "&6Galinha Monstro",         "chicken",  4.5f,  3.2f, true,   5 },
};
#define MONSTRO_DEFS_COUNT Array_Elems(monstro_defs)

struct Monstro {
	struct Entity base;
	cc_bool usado;
	int tipo;
	float vida, pensarTimer, empurraoTimer;
	Vec3 destino;
};
static struct Monstro monstros[MONSTROS_MAX];
static RNGState rnd;

static float Dist2XZ(Vec3 a, Vec3 b) {
	float dx = a.x - b.x, dz = a.z - b.z;
	return dx * dx + dz * dz;
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

/* Empurra o jogador para longe (e para cima), mais forte quanto maior o monstro */
static void Monstro_Empurrar(struct Monstro* m, struct Entity* jogador) {
	const struct MonstroDef* def = &monstro_defs[m->tipo];
	float dx = jogador->Position.x - m->base.next.pos.x;
	float dz = jogador->Position.z - m->base.next.pos.z;
	float len = Math_SqrtF(dx * dx + dz * dz) + 0.01f;
	float forca = 0.5f + def->escala * 0.25f;

	jogador->Velocity.x = dx / len * forca;
	jogador->Velocity.z = dz / len * forca;
	jogador->Velocity.y = 0.35f + def->escala * 0.05f;
	m->empurraoTimer = 1.5f;
}

static void Monstro_Tick(struct Entity* e, float delta) {
	struct Monstro* m = (struct Monstro*)e;
	const struct MonstroDef* def = &monstro_defs[m->tipo];
	struct Entity* jogador = &Entities.CurPlayer->Base;
	Vec3 pos = e->next.pos;
	float d2, alcance, ang;

	e->prev = e->next;
	m->vida          -= delta;
	m->pensarTimer   -= delta;
	m->empurraoTimer -= delta;

	d2 = Dist2XZ(pos, jogador->Position);
	if (m->vida <= 0 || d2 > DIST_SUMIR) { Monstro_Remover(m); return; }

	if (def->bravo && d2 < 16 * 16 && Math_AbsF(jogador->Position.y - pos.y) < 6) {
		/* persegue o jogador */
		alcance = 0.9f + def->escala * 0.6f;
		if (d2 > alcance * alcance) {
			Monstro_Andar(m, jogador->Position, def->vel, delta);
		} else if (m->empurraoTimer <= 0) {
			Monstro_Empurrar(m, jogador);
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
	Vec3 eyePos = Entity_GetEyePosition(e);
	IVec3 pos;
	IVec3_Floor(&pos, &eyePos);
	return Lighting.Color(pos.x, pos.y, pos.z);
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
	cc_string nome   = String_FromReadonly(def->nome);
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
	m->vida    = def->bravo ? 90.0f : 300.0f;
	m->destino = pos;
	m->empurraoTimer = 1.0f; /* da um tempinho para o jogador correr */

	m->base.Model = Model_Get(&modelo);
	Vec3_Set(m->base.ModelScale, def->escala, def->escala, def->escala);
	Entity_UpdateModelBounds(&m->base);
	Entity_SetName(&m->base, &nome);

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

static void OnBlockChanged(void* obj, IVec3 coords, BlockID old, BlockID now) {
	if (!Minerios_Enabled) return;
	if (!Server.IsSinglePlayer || old != BLOCK_OVO_MIN || now != BLOCK_AIR) return;
	if (!Block_IsCustomDefined(BLOCK_OVO_MIN)) return;
	ChocarOvo(coords.x, coords.y, coords.z, -1);
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

	if (argsCount && String_CaselessEqualsConst(&args[0], "ovo")) {
		if (argsCount > 1 && String_CaselessEqualsConst(&args[1], "todos")) {
			count = MONSTRO_DEFS_COUNT;
		} else if (argsCount > 1) {
			Convert_ParseInt(&args[1], &tipo);
			if (tipo < 1 || tipo > MONSTRO_DEFS_COUNT) { Chat_AddRaw("&cNumero de monstro invalido."); return; }
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

	Chat_AddRaw("&eMinerios & Monstros - blocos (ID):");
	for (i = 0; i < MINERIOS_COUNT; i++)
	{
		id = MINERIOS_BLOCK_BASE + i;
		Chat_Add2("&f  %i &7- %c", &id, minerio_defs[i].name);
	}
	Chat_AddRaw("&eQuebre um &fMinerio de ovo&e para chocar uma criatura!");
	for (i = 0; i < MONSTRO_DEFS_COUNT; i++)
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
		"&a/client minerios &e- lista os blocos e monstros do mod",
		"&a/client minerios ovo [numero|todos] &e- choca um ovo na sua frente",
		"&a/client minerios limpar &e- remove os monstros",
	}
};


/*########################################################################################################################*
*-------------------------------------------------------Componente--------------------------------------------------------*
*#########################################################################################################################*/
static void OnNewMapLoaded(void) {
	BlockID block;
	int i;
	/* No multijogador o servidor decide quais blocos existem */
	if (!Minerios_Enabled || !Server.IsSinglePlayer || !Game_AllowCustomBlocks) return;

	for (i = 0; i < MINERIOS_COUNT; i++)
	{
		block = (BlockID)(MINERIOS_BLOCK_BASE + i);
		if (block > BLOCK_MAX_DEFINED) break;
		/* Mundos salvos podem ja definir o bloco (talvez diferente) */
		if (Block_IsCustomDefined(block)) continue;
		DefineBlock(block, &minerio_defs[i]);
	}
}

static void Minerios_Init(void) {
	Minerios_Enabled = Options_GetBool(MINERIOS_OPTION, true);
	Random_SeedFromCurrentTime(&rnd);
	Commands_Register(&MineriosCommand);
	Event_Register_(&UserEvents.BlockChanged, NULL, OnBlockChanged);
}

static void Minerios_Free(void) { RemoverTodos(); }

struct IGameComponent Minerios_Component = {
	Minerios_Init,  /* Init  */
	Minerios_Free,  /* Free  */
	RemoverTodos,   /* Reset */
	RemoverTodos,   /* OnNewMap */
	OnNewMapLoaded  /* OnNewMapLoaded */
};

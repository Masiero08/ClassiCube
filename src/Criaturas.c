#include "Criaturas.h"
#include "Entity.h"
#include "EntityRenderers.h"
#include "Model.h"
#include "Game.h"
#include "World.h"
#include "Block.h"
#include "BlockID.h"
#include "Chat.h"
#include "Options.h"
#include "Event.h"
#include "ExtMath.h"
#include "Funcs.h"
#include "Server.h"
#include "Lighting.h"
#include "Graphics.h"
#include "Stream.h"
#include "Commands.h"
#include "Picking.h"
#include "Physics.h"
#include "String_.h"
#include "Utils.h"
#include "Particle.h"
#include "Menus.h"
#include "Gui.h"
#include "DayNight.h"
#include "Screens.h"
#include "Widgets.h"
#include "Drawer2D.h"
#include "CriaturasDados.h"

/* Criaturas do Bioma: criaturas capturaveis para o modo um jogador.
   Os modelos, texturas e especies ficam em CriaturasDados.h, gerado por
   misc/criaturas/criaturas.py. Este arquivo cuida de desenhar, nascer,
   andar, lutar, capturar, evoluir e salvar a equipe.
*/

/*########################################################################################################################*
*---------------------------------------------------------Modelos---------------------------------------------------------*
*#########################################################################################################################*/
#define MAX_PECAS 14

struct Especie {
	struct Model model; /* precisa ser o primeiro campo (Models.Active -> Especie) */
	struct ModelTex tex;
	char texName[24];
	int id, plano;
	const cc_uint8* png;
	int pngLen;
};
/* as especies e, depois delas, os modelos dos casulos de captura */
static struct Especie especies[ESPECIES_COUNT + CASULOS_COUNT + 1]; /* + o bau */
static struct ModelVertex plano_vertices[PLANOS_COUNT][MAX_PECAS * MODEL_BOX_VERTICES];
static struct ModelPart   plano_partes[PLANOS_COUNT][MAX_PECAS];

static void Criatura_MakeParts(void) {
	struct Especie* sp = (struct Especie*)Models.Active;
	const struct PlanoDef* plano = &planos_def[sp->plano];
	struct ModelPart* partes     = plano_partes[sp->plano];
	struct BoxDesc desc;
	int i;

	for (i = 0; i < plano->count; i++)
	{
		const struct PecaDef* p = &plano->pecas[i];
		desc.texX  = p->u; desc.texY  = p->v;
		desc.sizeX = p->x2 - p->x1; desc.sizeY = p->y2 - p->y1; desc.sizeZ = p->z2 - p->z1;
		desc.x1 = p->x1 / 16.0f; desc.y1 = p->y1 / 16.0f; desc.z1 = p->z1 / 16.0f;
		desc.x2 = p->x2 / 16.0f; desc.y2 = p->y2 / 16.0f; desc.z2 = p->z2 / 16.0f;
		desc.rotX = p->px / 16.0f; desc.rotY = p->py / 16.0f; desc.rotZ = p->pz / 16.0f;
		BoxDesc_BuildBox(&partes[i], &desc);
	}
}

/* Carrega a textura embutida quando ainda nao existe (ou depois de perder o contexto grafico) */
static void Criatura_CheckTexture(struct Especie* sp) {
	struct Stream mem;
	cc_string name;
	if (sp->tex.texID) return;

	Stream_ReadonlyMemory(&mem, (void*)sp->png, sp->pngLen);
	name = String_FromReadonly(sp->texName);
	Game_UpdateTexture(&sp->tex.texID, &mem, &name, &sp->tex.skinType, NULL);
}

static void Criatura_Draw(struct Entity* e) {
	struct Especie* sp = (struct Especie*)e->Model;
	int idPlano = sp->plano;
	const struct PlanoDef* plano = &planos_def[idPlano];
	struct ModelPart* partes     = plano_partes[idPlano];
	float pitch = -e->Pitch * MATH_DEG2RAD;
	float bate  = Math_AbsF(Math_SinF((float)Game.Time * (idPlano == PLANO_BORBOLETA ? 9.0f : 5.0f)));
	float asa   = Math_AbsF(e->Anim.LeftArmX) + 0.15f + bate * (idPlano == PLANO_BORBOLETA ? 0.8f : 0.15f);
	int i;

	Criatura_CheckTexture(sp);
	Model_ApplyTexture(e);
	Model_LockVB(e, plano->count * MODEL_BOX_VERTICES);

	for (i = 0; i < plano->count; i++)
	{
		struct ModelPart* part = &partes[i];
		switch (plano->pecas[i].anim)
		{
		case ANIM_CABECA:
			Model_DrawRotate(pitch, 0, 0, part, true); break;
		case ANIM_PERNA_A:
			Model_DrawRotate(e->Anim.LeftLegX,  0, 0, part, false); break;
		case ANIM_PERNA_B:
			Model_DrawRotate(e->Anim.RightLegX, 0, 0, part, false); break;
		case ANIM_CAUDA:
			Model_DrawRotate(0.2f, e->Anim.LeftLegX * 0.8f, 0, part, false); break;
		/* borboleta abre e fecha as asas para tras; aves batem para cima */
		case ANIM_ASA_E:
			if (idPlano == PLANO_BORBOLETA) Model_DrawRotate(0,  asa, 0, part, false);
			else Model_DrawRotate(0, 0, -asa, part, false);
			break;
		case ANIM_ASA_D:
			if (idPlano == PLANO_BORBOLETA) Model_DrawRotate(0, -asa, 0, part, false);
			else Model_DrawRotate(0, 0,  asa, part, false);
			break;
		default:
			Model_DrawPart(part); break;
		}
	}

	Model_UnlockVB();
	Gfx_DrawVb_IndexedTris(plano->count * MODEL_BOX_VERTICES);
}

static float Criatura_GetNameY(struct Entity* e) {
	struct Especie* sp = (struct Especie*)e->Model;
	return planos_def[sp->plano].nameY;
}

static float Criatura_GetEyeY(struct Entity* e) {
	struct Especie* sp = (struct Especie*)e->Model;
	return planos_def[sp->plano].eyeY;
}

static void Criatura_GetSize(struct Entity* e) {
	struct Especie* sp = (struct Especie*)e->Model;
	const cc_int8* bb  = planos_def[sp->plano].bb;
	float w = (bb[3] - bb[0]) / 16.0f, h = (bb[4] - bb[1]) / 16.0f;
	/* colisao quadrada, como os outros bichos do jogo */
	Vec3_Set(e->Size, w, h, w);
}

static void Criatura_GetBounds(struct Entity* e) {
	struct Especie* sp = (struct Especie*)e->Model;
	const cc_int8* bb  = planos_def[sp->plano].bb;
	Vec3_Set(e->ModelAABB.Min, bb[0] / 16.0f, bb[1] / 16.0f, bb[2] / 16.0f);
	Vec3_Set(e->ModelAABB.Max, bb[3] / 16.0f, bb[4] / 16.0f, bb[5] / 16.0f);
}

static void Especies_Register(void) {
	cc_string name;
	int i, j, plano;

	for (i = 0; i < ESPECIES_COUNT + CASULOS_COUNT + 1; i++)
	{
		struct Especie* sp = &especies[i];
		const char* nome;
		sp->id = i;
		if (i < ESPECIES_COUNT) {
			nome       = especies_def[i].nome;
			sp->plano  = especies_def[i].plano;
			sp->png    = especies_def[i].png;
			sp->pngLen = especies_def[i].pngLen;
		} else if (i == ESPECIES_COUNT + CASULOS_COUNT) {
			nome       = "bau_criaturas";
			sp->plano  = PLANO_BAU;
			sp->png    = png_bau;
			sp->pngLen = sizeof(png_bau);
		} else {
			nome       = casulos_def[i - ESPECIES_COUNT].nome;
			sp->plano  = PLANO_CASULO_ITEM;
			sp->png    = casulos_def[i - ESPECIES_COUNT].png;
			sp->pngLen = casulos_def[i - ESPECIES_COUNT].pngLen;
		}
		plano = sp->plano;

		/* textura "<nome>.png" (minusculo), pode ser trocada por um texture pack */
		String_InitArray_NT(name, sp->texName);
		String_AppendConst(&name, nome);
		String_AppendConst(&name, ".png");
		sp->texName[name.length] = '\0';
		for (j = 0; j < name.length; j++) { Char_MakeLower(sp->texName[j]); } /* ex.: faiscol.png */
		sp->tex.name     = sp->texName;
		sp->tex.skinType = SKIN_64x64;
		Model_RegisterTexture(&sp->tex);

		sp->model.name       = nome;
		sp->model.vertices   = plano_vertices[plano];
		sp->model.defaultTex = &sp->tex;
		sp->model.MakeParts  = Criatura_MakeParts;
		sp->model.Draw       = Criatura_Draw;
		sp->model.GetNameY   = Criatura_GetNameY;
		sp->model.GetEyeY    = Criatura_GetEyeY;
		sp->model.GetCollisionSize = Criatura_GetSize;
		sp->model.GetPickingBounds = Criatura_GetBounds;

		Model_Init(&sp->model);
		sp->model.usesSkin    = false;
		sp->model.maxVertices = planos_def[plano].count * MODEL_BOX_VERTICES;
		Model_Register(&sp->model);
	}
}


/*########################################################################################################################*
*---------------------------------------------------------Equipe----------------------------------------------------------*
*#########################################################################################################################*/
#define EQUIPE_MAX 6
#define NIVEL_MAX  30
#define OPT_EQUIPE "criaturas-equipe"

struct Membro { int especie, nivel, vida, xp; };
static struct Membro equipe[EQUIPE_MAX];
static int equipeCount;
static int membroAtivo = -1; /* indice do membro que esta fora do casulo (-1 = nenhum) */

static int VidaMax(int especie, int nivel) {
	return especies_def[especie].vida + (nivel - 1) * 2;
}

static int XpParaSubir(int nivel) { return 4 + nivel * 3; }

/* Salva como "especie:nivel:vida:xp,...;ativo" */
static void Equipe_Salvar(void) {
	cc_string str; char strBuffer[STRING_SIZE * 2];
	int i;
	String_InitArray(str, strBuffer);

	for (i = 0; i < equipeCount; i++)
	{
		if (i) String_Append(&str, ',');
		String_AppendInt(&str, equipe[i].especie); String_Append(&str, ':');
		String_AppendInt(&str, equipe[i].nivel);   String_Append(&str, ':');
		String_AppendInt(&str, equipe[i].vida);    String_Append(&str, ':');
		String_AppendInt(&str, equipe[i].xp);
	}
	String_Append(&str, ';');
	String_AppendInt(&str, membroAtivo);
	Options_Set(OPT_EQUIPE, &str);
}

static void Equipe_Carregar(void) {
	cc_string str; char strBuffer[STRING_SIZE * 2];
	cc_string membros, ativo, partes[EQUIPE_MAX], campos[4];
	int i, n, count, v[4];
	String_InitArray(str, strBuffer);
	Options_Get(OPT_EQUIPE, &str, "");
	equipeCount = 0; membroAtivo = -1;
	if (!str.length) return;

	String_UNSAFE_Separate(&str, ';', &membros, &ativo);
	count = String_UNSAFE_Split(&membros, ',', partes, EQUIPE_MAX);

	for (i = 0; i < count; i++)
	{
		if (String_UNSAFE_Split(&partes[i], ':', campos, 4) != 4) continue;
		for (n = 0; n < 4; n++) { if (!Convert_ParseInt(&campos[n], &v[n])) break; }
		if (n < 4 || v[0] < 0 || v[0] >= ESPECIES_COUNT) continue;

		equipe[equipeCount].especie = v[0];
		equipe[equipeCount].nivel   = max(1, min(v[1], NIVEL_MAX));
		equipe[equipeCount].vida    = max(0, min(v[2], VidaMax(v[0], equipe[equipeCount].nivel)));
		equipe[equipeCount].xp      = max(0, v[3]);
		equipeCount++;
	}
	if (Convert_ParseInt(&ativo, &v[0]) && v[0] >= 0 && v[0] < equipeCount && equipe[v[0]].vida > 0) membroAtivo = v[0];
}


/*########################################################################################################################*
*---------------------------------------------------Diario das Criaturas--------------------------------------------------*
*#########################################################################################################################*/
#define OPT_VISTAS     "criaturas-vistas"
#define OPT_CAPTURADAS "criaturas-capturadas"
static int diarioVistas, diarioCapturadas; /* um bit por especie */

static void Diario_Carregar(void) {
	int i;
	diarioVistas     = Options_GetInt(OPT_VISTAS,     0, 0x7FFFFFFF, 0);
	diarioCapturadas = Options_GetInt(OPT_CAPTURADAS, 0, 0x7FFFFFFF, 0);
	/* quem ja esta na equipe conta como capturada */
	for (i = 0; i < equipeCount; i++) diarioCapturadas |= 1 << equipe[i].especie;
	diarioVistas |= diarioCapturadas;
}

static void Diario_Marcar(int especie, cc_bool capturada) {
	int bit = 1 << especie;
	if (!(diarioVistas & bit)) {
		diarioVistas |= bit;
		Options_SetInt(OPT_VISTAS, diarioVistas);
		Chat_Add2("&bNova criatura no Diario: %c%c&b!", especies_def[especie].cor, especies_def[especie].nome);
	}
	if (capturada && !(diarioCapturadas & bit)) {
		diarioCapturadas |= bit;
		Options_SetInt(OPT_CAPTURADAS, diarioCapturadas);
	}
}


/*########################################################################################################################*
*--------------------------------------------------------Entidades--------------------------------------------------------*
*#########################################################################################################################*/
#define CRIATURAS_MAX     23 /* 223 e do bau */
#define CRIATURA_ID_BASE  200
#define SELVAGENS_MAX     6
#define DIST_SUMIR        (48.0f * 48.0f)

struct Criatura {
	struct Entity base;
	cc_bool usada, selvagem;
	int especie, nivel, vida;
	float hurtTimer, atkTimer, fugaTimer, pensarTimer, curaTimer;
	Vec3 destino;
	int alvo;       /* indice em criaturas[] que esta sendo atacada (-1 = nenhum) */
	cc_bool ajudou; /* a criatura do jogador ajudou a derrotar esta (ganha xp) */
	cc_bool nomeSujo;
	cc_bool avisouTipo; /* ja avisou no chat que o tipo e forte/fraco contra esta */
	cc_bool doMestre;   /* criatura de um Mestre de Bioma (nao foge, nao pode ser capturada) */
	cc_bool capturando; /* dentro de um casulo que ainda esta balancando */
};
static struct Criatura criaturas[CRIATURAS_MAX];
static RNGState rnd;
static float spawnTimer;

/* Ligado/desligado na tela "Mods" do menu inicial (lido a cada mundo carregado) */
#define OPT_CRIATURAS "mod-criaturas"
static cc_bool criaturasLigadas = true;

static cc_bool Ativo(void) { return criaturasLigadas && Server.IsSinglePlayer && World.Loaded; }

static int Criatura_Index(struct Criatura* c) { return (int)(c - criaturas); }

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

/* Acha onde a criatura fica em pe na coluna (x, z), procurando perto de yRef. */
/* Agua conta como chao so para quem nada (tipo agua). */
static cc_bool AcharChaoEntre(int x, int z, int yTopo, int yFim, cc_bool nada, float* yOut) {
	int y;
	BlockID b, cima;
	if (x < 0 || z < 0 || x >= World.Width || z >= World.Length) return false;

	yTopo = min(yTopo, World.MaxY);
	yFim  = max(yFim, 0);

	for (y = yTopo; y >= yFim; y--)
	{
		b    = World_GetBlock(x, y, z);
		cima = y + 1 <= World.MaxY ? World_GetBlock(x, y + 1, z) : BLOCK_AIR;
		if (Solido(cima)) continue;

		if (Solido(b)) { *yOut = y + Blocks.MaxBB[b].y; return true; }
		if (Blocks.IsLiquid[b]) {
			if (!nada) return false;
			if (Blocks.IsLiquid[cima]) continue;
			*yOut = y + 0.7f; return true;
		}
	}
	return false;
}

static cc_bool AcharChao(int x, int z, int yRef, cc_bool nada, float* yOut) {
	return AcharChaoEntre(x, z, yRef + 2, yRef - 4, nada, yOut);
}

static void Criatura_AtualizarNome(struct Criatura* c) {
	const struct EspecieDef* def = &especies_def[c->especie];
	cc_string str; char strBuffer[STRING_SIZE];
	int vidaMax = VidaMax(c->especie, c->nivel);
	String_InitArray(str, strBuffer);

	String_AppendConst(&str, def->cor);
	String_AppendConst(&str, def->nome);
	String_Format1(&str, " &fNv.%i ", &c->nivel);
	String_AppendConst(&str, c->vida * 3 <= vidaMax ? "&c" : "&a");
	String_Format2(&str, "%i/%i", &c->vida, &vidaMax);
	if (!c->selvagem) String_AppendConst(&str, " &7(sua)");

	Entity_SetName(&c->base, &str);
	c->nomeSujo = false;
}

static void Criatura_DefinirEspecie(struct Criatura* c, int especie) {
	float s = especies_def[especie].escala;
	cc_string name = String_FromReadonly(especies_def[especie].nome);
	c->especie = especie;
	/* Model_Get monta as pecas do modelo na primeira vez */
	c->base.Model = Model_Get(&name);
	if (!c->base.Model) c->base.Model = &especies[especie].model;
	if (c->base.Flags & ENTITY_FLAG_HAS_MODELVB) Gfx_DeleteScratchVb(&c->base.ModelVB);
	Vec3_Set(c->base.ModelScale, s, s, s);
	Entity_UpdateModelBounds(&c->base);
	c->nomeSujo = true;
}

static void Criatura_Teleportar(struct Criatura* c, Vec3 pos) {
	c->base.prev.pos = pos; c->base.next.pos = pos;
	c->base.Position = pos;
}

/* --- VTABLE --- */
static void Criatura_Tick(struct Entity* e, float delta);

static void Criatura_Despawn(struct Entity* e) {
	EntityNames_Delete(e);
	if (e->Flags & ENTITY_FLAG_HAS_MODELVB) Gfx_DeleteScratchVb(&e->ModelVB);
}

static void Criatura_SetLocation(struct Entity* e, struct LocationUpdate* update) {
	if (update->flags & LU_HAS_POS) {
		e->prev.pos = update->pos; e->next.pos = update->pos;
	}
}

static PackedCol Criatura_GetCol(struct Entity* e) {
	struct Criatura* c = (struct Criatura*)e;
	Vec3 eyePos = Entity_GetEyePosition(e);
	IVec3 pos; PackedCol col;
	IVec3_Floor(&pos, &eyePos);
	col = Lighting.Color(pos.x, pos.y, pos.z);

	/* pisca vermelho ao levar dano */
	if (c->hurtTimer > 0) col = PackedCol_Tint(col, PackedCol_Make(255, 90, 90, 255));
	return col;
}

static void Criatura_RenderModel(struct Entity* e, float delta, float t) {
	if (((struct Criatura*)e)->capturando) { e->ShouldRender = false; return; }
	Vec3_Lerp(&e->Position, &e->prev.pos, &e->next.pos, t);
	Entity_LerpAngles(e, t);
	AnimatedComp_GetCurrent(e, t);

	e->ShouldRender = Model_ShouldRender(e);
	if (e->ShouldRender) Model_Render(e->Model, e);
}

static cc_bool Criatura_ShouldRenderName(struct Entity* e) {
	if (!e->ShouldRender) return false;
	return Model_RenderDistance(e) <= 24.0f * 24.0f;
}

static const struct EntityVTABLE criatura_VTABLE = {
	Criatura_Tick,        Criatura_Despawn,         Criatura_SetLocation, Criatura_GetCol,
	Criatura_RenderModel, Criatura_ShouldRenderName
};

static struct Criatura* Criatura_Criar(int especie, int nivel, Vec3 pos, cc_bool selvagem) {
	struct Criatura* c;
	int i;
	for (i = 0; i < CRIATURAS_MAX; i++)
	{
		if (!criaturas[i].usada) break;
	}
	if (i == CRIATURAS_MAX) return NULL;
	c = &criaturas[i];

	Mem_Set(c, 0, sizeof(struct Criatura));
	Entity_Init(&c->base);
	c->base.VTABLE = &criatura_VTABLE;
	c->usada    = true;
	c->selvagem = selvagem;
	c->nivel    = nivel;
	c->alvo     = -1;
	Criatura_DefinirEspecie(c, especie);
	c->vida     = VidaMax(especie, nivel);
	c->destino  = pos;
	c->base.next.yaw = c->base.prev.yaw = (float)Random_Next(&rnd, 360);
	c->base.next.rotY = c->base.prev.rotY = c->base.next.yaw;
	Criatura_Teleportar(c, pos);
	Criatura_AtualizarNome(c);
	Diario_Marcar(especie, !selvagem);

	Entities.List[CRIATURA_ID_BASE + i] = &c->base;
	return c;
}

static void Criatura_Remover(struct Criatura* c) {
	int i, idx = Criatura_Index(c);
	if (!c->usada) return;
	Entities_Remove(CRIATURA_ID_BASE + idx);
	c->usada = false;

	for (i = 0; i < CRIATURAS_MAX; i++)
	{
		if (criaturas[i].alvo == idx) criaturas[i].alvo = -1;
	}
}

static struct Criatura* Criatura_DoJogador(void) {
	int i;
	for (i = 0; i < CRIATURAS_MAX; i++)
	{
		if (criaturas[i].usada && !criaturas[i].selvagem) return &criaturas[i];
	}
	return NULL;
}

static void RemoverTodas(void) {
	int i;
	for (i = 0; i < CRIATURAS_MAX; i++) Criatura_Remover(&criaturas[i]);
}


/*########################################################################################################################*
*---------------------------------------------------------Combate---------------------------------------------------------*
*#########################################################################################################################*/
/* Quem ganha de quem: fogo > planta > agua > fogo, eletrico > agua/vento, */
/* vento > planta, pedra > fogo/vento, agua/planta > pedra, inseto > planta, fogo/vento > inseto */
static float Multiplicador(int atk, int def) {
	switch (atk)
	{
	case TIPO_FOGO:     if (def == TIPO_PLANTA || def == TIPO_INSETO || def == TIPO_GELO) return 1.5f; if (def == TIPO_AGUA || def == TIPO_PEDRA) return 0.6f; break;
	case TIPO_AGUA:     if (def == TIPO_FOGO || def == TIPO_PEDRA) return 1.5f; if (def == TIPO_PLANTA) return 0.6f; break;
	case TIPO_PLANTA:   if (def == TIPO_AGUA || def == TIPO_PEDRA) return 1.5f; if (def == TIPO_FOGO) return 0.6f; break;
	case TIPO_ELETRICO: if (def == TIPO_AGUA || def == TIPO_VENTO) return 1.5f; if (def == TIPO_PEDRA) return 0.6f; break;
	case TIPO_VENTO:    if (def == TIPO_PLANTA || def == TIPO_INSETO) return 1.5f; if (def == TIPO_PEDRA || def == TIPO_ELETRICO) return 0.6f; break;
	case TIPO_PEDRA:    if (def == TIPO_FOGO || def == TIPO_VENTO || def == TIPO_GELO) return 1.5f; break;
	case TIPO_INSETO:   if (def == TIPO_PLANTA) return 1.5f; if (def == TIPO_FOGO || def == TIPO_PEDRA || def == TIPO_VENTO || def == TIPO_FANTASMA) return 0.6f; break;
	case TIPO_FANTASMA: if (def == TIPO_FANTASMA || def == TIPO_ELETRICO) return 1.5f; if (def == TIPO_PEDRA) return 0.6f; break;
	case TIPO_GELO:     if (def == TIPO_PLANTA || def == TIPO_VENTO) return 1.5f; if (def == TIPO_FOGO || def == TIPO_AGUA || def == TIPO_GELO) return 0.6f; break;
	}
	return 1.0f;
}

static void Membro_SincronizarDe(struct Criatura* c) {
	if (membroAtivo < 0 || membroAtivo >= equipeCount) return;
	equipe[membroAtivo].especie = c->especie;
	equipe[membroAtivo].nivel   = c->nivel;
	equipe[membroAtivo].vida    = c->vida;
}

static void Criatura_GanharXp(struct Criatura* c, int xp) {
	struct Membro* m;
	const struct EspecieDef* def;
	cc_string msg; char msgBuffer[STRING_SIZE];
	if (membroAtivo < 0) return;
	m = &equipe[membroAtivo];
	m->xp += xp;

	while (m->xp >= XpParaSubir(c->nivel) && c->nivel < NIVEL_MAX) {
		m->xp -= XpParaSubir(c->nivel);
		c->nivel++;
		c->vida = VidaMax(c->especie, c->nivel);
		def     = &especies_def[c->especie];

		String_InitArray(msg, msgBuffer);
		String_Format3(&msg, "%c%c &esubiu para o nivel %i!", def->cor, def->nome, &c->nivel);
		Chat_Add(&msg);

		if (def->evolui >= 0 && c->nivel >= def->nivelEvolui) {
			String_InitArray(msg, msgBuffer);
			String_Format4(&msg, "&dQue isso?! %c%c &devoluiu para %c%c&d!",
				def->cor, def->nome, especies_def[def->evolui].cor, especies_def[def->evolui].nome);
			Chat_Add(&msg);
			Criatura_DefinirEspecie(c, def->evolui);
			c->vida = VidaMax(c->especie, c->nivel);
			Diario_Marcar(c->especie, true);
		}
	}
	c->nomeSujo = true;
	Membro_SincronizarDe(c);
	Equipe_Salvar();
}

static void Criatura_Desmaiou(struct Criatura* c);
static void Mestre_CriaturaCaiu(void);
static void Casulos_Recompensa(struct Criatura* c);

static void Criatura_Derrotada(struct Criatura* c) {
	struct Criatura* minha = Criatura_DoJogador();
	cc_string msg; char msgBuffer[STRING_SIZE];
	const struct EspecieDef* def = &especies_def[c->especie];

	String_InitArray(msg, msgBuffer);
	if (c->doMestre) String_Format2(&msg, "%c%c &7do Mestre foi derrotado!", def->cor, def->nome);
	else String_Format2(&msg, "%c%c &7selvagem foi derrotado.", def->cor, def->nome);
	Chat_Add(&msg);

	if (minha && c->ajudou) Criatura_GanharXp(minha, (3 + c->nivel * 2) * (c->doMestre ? 2 : 1));
	if (c->doMestre) Mestre_CriaturaCaiu();
	else if (c->ajudou) Casulos_Recompensa(c);
	Criatura_Remover(c);
}

static void Criatura_Dano(struct Criatura* c, int dano, Vec3 origem) {
	Vec3 dir;
	float len;
	if (c->capturando) return;
	c->vida -= max(1, dano);
	c->hurtTimer = 0.4f;
	c->nomeSujo  = true;

	/* empurraozinho para tras */
	dir.x = c->base.next.pos.x - origem.x; dir.z = c->base.next.pos.z - origem.z;
	len = Math_SqrtF(dir.x * dir.x + dir.z * dir.z);
	if (len > 0.01f) {
		c->destino.x = c->base.next.pos.x + dir.x / len * 3.0f;
		c->destino.z = c->base.next.pos.z + dir.z / len * 3.0f;
	}

	if (c->selvagem) {
		/* briga enquanto tem forca; fraca, tenta fugir */
		if (!c->doMestre && (c->alvo < 0 || c->vida * 10 < VidaMax(c->especie, c->nivel) * 3)) c->fugaTimer = 3.0f;
		if (c->vida <= 0) Criatura_Derrotada(c);
	} else {
		if (c->vida < 0) c->vida = 0;
		Membro_SincronizarDe(c);
		if (c->vida == 0) Criatura_Desmaiou(c);
	}
}

/* Faz particulas da cor do tipo no alvo */
static const BlockID efeitoBloco[TIPO_COUNT] = {
	BLOCK_LAVA, BLOCK_WATER, BLOCK_LEAVES, BLOCK_GOLD, BLOCK_WHITE, BLOCK_STONE, BLOCK_LIME, BLOCK_VIOLET, BLOCK_ICE
};
static void Efeito_Golpe(struct Criatura* alvo, int tipo) {
	Vec3 p = alvo->base.next.pos;
	IVec3 coords;
	p.y += 0.4f;
	IVec3_Floor(&coords, &p);
	Particles_BreakBlockEffect(coords, efeitoBloco[tipo], BLOCK_AIR);
}

static void Criatura_Olhar(struct Criatura* c, Vec3 alvo) {
	float dx = alvo.x - c->base.next.pos.x, dz = alvo.z - c->base.next.pos.z;
	c->base.next.yaw  = Math_Atan2f(-dz, dx) * MATH_RAD2DEG;
	c->base.next.rotY = c->base.next.yaw;
}

static void Criatura_Golpear(struct Criatura* atk, struct Criatura* alvo) {
	const struct EspecieDef* da = &especies_def[atk->especie];
	float mult = Multiplicador(da->tipo, especies_def[alvo->especie].tipo);
	int dano   = (int)((da->dano + atk->nivel / 2) * mult * (atk->selvagem ? 0.7f : 1.0f));

	atk->atkTimer = atk->selvagem ? 1.4f : 1.0f;
	Criatura_Olhar(atk, alvo->base.next.pos);
	Efeito_Golpe(alvo, da->tipo);

	if (!atk->selvagem) {
		alvo->ajudou = true;
		if (alvo->alvo < 0) alvo->alvo = Criatura_Index(atk); /* a selvagem revida */
		if (!alvo->avisouTipo && mult != 1.0f) {
			Chat_AddRaw(mult > 1.0f ? "&aGolpe em cheio! O tipo dela e fraco contra o seu."
			                        : "&7Golpe fraco... o tipo dela resiste ao seu.");
			alvo->avisouTipo = true;
		}
	}
	Criatura_Dano(alvo, dano, atk->base.next.pos);
}


/*########################################################################################################################*
*--------------------------------------------------------Movimento--------------------------------------------------------*
*#########################################################################################################################*/
static void Criatura_Andar(struct Criatura* c, Vec3 alvo, float vel, float delta) {
	struct Entity* e = &c->base;
	const struct EspecieDef* def = &especies_def[c->especie];
	Vec3 pos = e->next.pos;
	float dx = alvo.x - pos.x, dz = alvo.z - pos.z;
	float len = Math_SqrtF(dx * dx + dz * dz), passo, novoY;
	float nx, nz;
	cc_bool nada = def->tipo == TIPO_AGUA;

	if (len < 0.3f) return;
	passo = min(vel * delta, len);
	nx = pos.x + dx / len * passo;
	nz = pos.z + dz / len * passo;
	e->next.yaw  = Math_Atan2f(-dz, dx) * MATH_RAD2DEG;
	e->next.rotY = e->next.yaw; /* corpo vira junto com a cabeca */

	if (!AcharChao(Math_Floor(nx), Math_Floor(nz), Math_Floor(pos.y + 0.5f), nada, &novoY)) {
		/* bloqueado: escolhe outro caminho depois */
		c->pensarTimer = 0;
		return;
	}
	if (novoY - pos.y > 1.05f) { c->pensarTimer = 0; return; }

	/* sobe degraus na hora, desce caindo aos poucos */
	if (novoY < pos.y) novoY = max(novoY, pos.y - 0.5f);
	pos.x = nx; pos.z = nz; pos.y = novoY;
	e->next.pos = pos;
}

static void Criatura_PensarSelvagem(struct Criatura* c, struct Entity* jogador, float delta) {
	const struct EspecieDef* def = &especies_def[c->especie];
	Vec3 pos = c->base.next.pos;
	float vel = def->vel * 0.45f;
	float ang, d2;
	struct Criatura* alvo = c->alvo >= 0 ? &criaturas[c->alvo] : NULL;

	if (alvo && (!alvo->usada || alvo->selvagem)) { c->alvo = -1; alvo = NULL; }
	if (alvo && c->fugaTimer <= 0) {
		d2 = Dist2XZ(pos, alvo->base.next.pos);
		if (d2 > 18 * 18) {
			c->alvo = -1;
		} else if (d2 > 1.6f * 1.6f) {
			Criatura_Andar(c, alvo->base.next.pos, def->vel * 0.9f, delta);
			return;
		} else {
			if (c->atkTimer <= 0) Criatura_Golpear(c, alvo);
			return;
		}
	}

	if (c->fugaTimer > 0) {
		/* foge do jogador */
		float dx = pos.x - jogador->Position.x, dz = pos.z - jogador->Position.z;
		float len = Math_SqrtF(dx * dx + dz * dz) + 0.01f;
		c->destino.x = pos.x + dx / len * 4.0f;
		c->destino.z = pos.z + dz / len * 4.0f;
		vel = def->vel;
	} else if (c->pensarTimer <= 0) {
		c->pensarTimer = 2.0f + Random_Float(&rnd) * 4.0f;
		if (Random_Next(&rnd, 3) == 0) {
			c->destino = pos; /* fica parado um pouco */
		} else {
			ang = Random_Float(&rnd) * 2 * MATH_PI;
			c->destino.x = pos.x + Math_CosF(ang) * 5.0f;
			c->destino.z = pos.z + Math_SinF(ang) * 5.0f;
		}
	}
	Criatura_Andar(c, c->destino, vel, delta);
}

static void Criatura_PensarDoJogador(struct Criatura* c, struct Entity* jogador, float delta) {
	const struct EspecieDef* def = &especies_def[c->especie];
	struct Criatura* alvo = c->alvo >= 0 ? &criaturas[c->alvo] : NULL;
	Vec3 pos = c->base.next.pos, dest;
	float d2;

	if (alvo && (!alvo->usada || Dist2(alvo->base.next.pos, jogador->Position) > 20 * 20)) {
		c->alvo = -1; alvo = NULL;
	}

	if (alvo) {
		/* corre ate o alvo e ataca */
		d2 = Dist2XZ(pos, alvo->base.next.pos);
		if (d2 > 1.6f * 1.6f) {
			Criatura_Andar(c, alvo->base.next.pos, def->vel * 1.1f, delta);
		} else if (c->atkTimer <= 0) {
			Criatura_Golpear(c, alvo);
		}
		return;
	}

	d2 = Dist2XZ(pos, jogador->Position);
	if (d2 > 24 * 24) {
		/* ficou muito para tras: aparece perto do jogador */
		dest = jogador->Position; dest.x += 1.0f;
		Criatura_Teleportar(c, dest);
	} else if (d2 > 3.0f * 3.0f) {
		Criatura_Andar(c, jogador->Position, def->vel * (d2 > 8 * 8 ? 1.4f : 1.0f), delta);
	}
}

static void Criatura_Tick(struct Entity* e, float delta) {
	struct Criatura* c = (struct Criatura*)e;
	struct Entity* jogador = &Entities.CurPlayer->Base;

	e->prev = e->next;
	c->hurtTimer   -= delta;
	c->atkTimer    -= delta;
	c->fugaTimer   -= delta;
	c->pensarTimer -= delta;
	c->curaTimer   -= delta;

	if (c->capturando) {
		/* esta dentro do casulo: nao anda nem pensa */
		return;
	}
	if (c->selvagem) {
		if (!c->doMestre && Dist2XZ(e->next.pos, jogador->Position) > DIST_SUMIR) { Criatura_Remover(c); return; }
		Criatura_PensarSelvagem(c, jogador, delta);
	} else {
		Criatura_PensarDoJogador(c, jogador, delta);
	}

	if (c->nomeSujo) Criatura_AtualizarNome(c);
	AnimatedComp_Update(e, e->prev.pos, e->next.pos, delta);
}


/*########################################################################################################################*
*---------------------------------------------------------Nascer----------------------------------------------------------*
*#########################################################################################################################*/
static int EspecieDoHabitat(int habitat) {
	int opcoes[ESPECIES_COUNT], count = 0, i;
	const struct EspecieDef* def;

	for (i = 0; i < ESPECIES_COUNT; i++)
	{
		def = &especies_def[i];
		if (def->habitat != habitat) continue;
		/* formas evoluidas sao raras na natureza */
		if (!def->inicial && i > 0 && especies_def[i - 1].evolui == i && Random_Next(&rnd, 4)) continue;
		opcoes[count++] = i;
	}
	return count ? opcoes[Random_Next(&rnd, count)] : -1;
}

static void Spawner_Tick(float delta) {
	struct Entity* jogador = &Entities.CurPlayer->Base;
	int i, selvagens = 0, x, z, habitat, especie, nivel;
	float ang, dist, y;
	BlockID chao;
	Vec3 pos;

	spawnTimer -= delta;
	if (spawnTimer > 0) return;
	spawnTimer = 3.0f;

	for (i = 0; i < CRIATURAS_MAX; i++)
	{
		if (criaturas[i].usada && criaturas[i].selvagem) selvagens++;
	}
	if (selvagens >= SELVAGENS_MAX) return;

	ang  = Random_Float(&rnd) * 2 * MATH_PI;
	dist = 10.0f + Random_Float(&rnd) * 14.0f;
	x = Math_Floor(jogador->Position.x + Math_CosF(ang) * dist);
	z = Math_Floor(jogador->Position.z + Math_SinF(ang) * dist);
	if (!AcharChaoEntre(x, z, Math_Floor(jogador->Position.y) + 10, Math_Floor(jogador->Position.y) - 12, true, &y)) return;

	chao = World_GetBlock(x, Math_Floor(y - 0.01f), z);
	if (chao == BLOCK_SNOW || chao == BLOCK_ICE)     habitat = HABITAT_NEVE;
	else if ((DayNight_IsNight() && Random_Next(&rnd, 3) == 0) ||
		(chao == BLOCK_STONE && y < Env.EdgeHeight - 8)) habitat = HABITAT_NOITE;
	else if (y > Env.EdgeHeight + 18 && Random_Next(&rnd, 2) == 0) habitat = HABITAT_NEVE;
	else if (Blocks.IsLiquid[chao])                  habitat = HABITAT_AGUA;
	else if (chao == BLOCK_SAND || chao == BLOCK_SANDSTONE) habitat = HABITAT_AREIA;
	else if (chao == BLOCK_STONE || chao == BLOCK_GRAVEL || chao == BLOCK_COBBLE) habitat = HABITAT_PEDRA;
	else if (y > Env.EdgeHeight + 10)                habitat = HABITAT_ALTO;
	else if (chao == BLOCK_GRASS)                    habitat = Random_Next(&rnd, 5) == 0 ? HABITAT_ALTO : HABITAT_CAMPO;
	else return;

	especie = EspecieDoHabitat(habitat);
	if (especie < 0) return;
	nivel = 1 + Random_Next(&rnd, 4);
	if (especies_def[especie].nivelEvolui == 0 || !especies_def[especie].inicial) nivel += 5;

	Vec3_Set(pos, x + 0.5f, y, z + 0.5f);
	Criatura_Criar(especie, nivel, pos, true);
}

static void SoltarMembro(int idx) {
	struct Entity* jogador = &Entities.CurPlayer->Base;
	struct Criatura* atual = Criatura_DoJogador();
	struct Criatura* c;
	Vec3 pos;

	if (atual) { Membro_SincronizarDe(atual); Criatura_Remover(atual); }
	membroAtivo = idx;
	if (idx < 0 || idx >= equipeCount || equipe[idx].vida <= 0) { membroAtivo = -1; Equipe_Salvar(); return; }

	pos = jogador->Position; pos.x += 1.0f;
	c = Criatura_Criar(equipe[idx].especie, equipe[idx].nivel, pos, false);
	if (c) { c->vida = equipe[idx].vida; c->nomeSujo = true; }
	Equipe_Salvar();
}

/* proximo membro (depois de 'atual') que ainda tem vida, ou -1 */
static int ProximoSaudavel(int atual) {
	int i, idx;
	for (i = 1; i <= equipeCount; i++)
	{
		idx = (atual + i) % equipeCount;
		if (idx != atual && equipe[idx].vida > 0) return idx;
	}
	return -1;
}

static void Criatura_Desmaiou(struct Criatura* c) {
	const struct EspecieDef* def = &especies_def[c->especie];
	int prox = ProximoSaudavel(membroAtivo);
	Chat_Add2("%c%c &cdesmaiou e voltou para o casulo!", def->cor, def->nome);

	SoltarMembro(prox);
	if (prox >= 0) {
		def = &especies_def[equipe[prox].especie];
		Chat_Add2("&eVai, %c%c&e!", def->cor, def->nome);
	} else {
		Chat_AddRaw("&cSua equipe esta cansada. No casulo elas recuperam a vida aos poucos.");
	}
}

/* Membros guardados no casulo descansam e recuperam vida aos poucos */
static float descansoTimer;
static void Equipe_Descansar(float delta) {
	int i, vidaMax, curou = 0;
	descansoTimer -= delta;
	if (descansoTimer > 0) return;
	descansoTimer = 3.0f;

	for (i = 0; i < equipeCount; i++)
	{
		if (i == membroAtivo) continue;
		vidaMax = VidaMax(equipe[i].especie, equipe[i].nivel);
		if (equipe[i].vida >= vidaMax) continue;
		equipe[i].vida = min(vidaMax, equipe[i].vida + max(1, vidaMax / 10));
		curou++;
	}
	if (curou) Equipe_Salvar();
}

/*########################################################################################################################*
*--------------------------------------------------------Casulos----------------------------------------------------------*
*#########################################################################################################################*/
/* Casulos de captura: o comum nao acaba; os outros se ganham derrotando selvagens e Mestres */
enum { CASULO_COMUM, CASULO_REFORCADO, CASULO_MARE, CASULO_MESTRE, CASULO_COUNT };
static const char* const casuloNomes[CASULO_COUNT] = { "Comum", "Reforcado", "Mare", "Mestre" };
static const char* const casuloCores[CASULO_COUNT] = { "&f", "&7", "&b", "&d" };
static const BlockID casuloBloco[CASULO_COUNT]     = { BLOCK_RED, BLOCK_IRON, BLOCK_AQUA, BLOCK_VIOLET };
static int casulos[CASULO_COUNT];
static int casuloEscolhido = -1; /* -1 = automatico (o melhor que tiver, sem gastar o Mestre) */
static int insignias;            /* um bit por Mestre de Bioma vencido */

#define OPT_CASULOS   "criaturas-casulos"
#define OPT_INSIGNIAS "criaturas-insignias"

/* Salva como "reforcado,mare,mestre,escolhido" */
static void Casulos_Salvar(void) {
	cc_string str; char strBuffer[64];
	String_InitArray(str, strBuffer);
	String_Format4(&str, "%i,%i,%i,%i", &casulos[CASULO_REFORCADO], &casulos[CASULO_MARE],
		&casulos[CASULO_MESTRE], &casuloEscolhido);
	Options_Set(OPT_CASULOS, &str);
	Options_SetInt(OPT_INSIGNIAS, insignias);
}

static void Casulos_Carregar(void) {
	cc_string str; char strBuffer[64];
	cc_string partes[4];
	int i, v;
	String_InitArray(str, strBuffer);
	Options_Get(OPT_CASULOS, &str, "");
	casuloEscolhido = -1;

	if (String_UNSAFE_Split(&str, ',', partes, 4) == 4) {
		for (i = 0; i < 3; i++)
		{
			if (Convert_ParseInt(&partes[i], &v)) casulos[CASULO_REFORCADO + i] = max(0, min(v, 99));
		}
		if (Convert_ParseInt(&partes[3], &v) && v >= -1 && v < CASULO_COUNT) casuloEscolhido = v;
	}
	insignias = Options_GetInt(OPT_INSIGNIAS, 0, (1 << MESTRES_COUNT) - 1, 0);
}

static float Casulo_Mult(int tipo, struct Criatura* c) {
	switch (tipo)
	{
	case CASULO_REFORCADO: return 1.6f;
	case CASULO_MARE:      return especies_def[c->especie].tipo == TIPO_AGUA ? 3.0f : 1.0f;
	case CASULO_MESTRE:    return 100.0f;
	}
	return 1.0f;
}

static int Casulo_Escolher(struct Criatura* c) {
	int i, melhor = CASULO_COMUM;
	if (casuloEscolhido > CASULO_COMUM) {
		if (casulos[casuloEscolhido] > 0) return casuloEscolhido;
		Chat_Add2("&eAcabaram os Casulos %c%c&e. Voltando para o automatico.", casuloCores[casuloEscolhido], casuloNomes[casuloEscolhido]);
		casuloEscolhido = -1;
	}
	if (casuloEscolhido == CASULO_COMUM) return CASULO_COMUM;

	for (i = CASULO_REFORCADO; i < CASULO_MESTRE; i++)
	{
		if (casulos[i] > 0 && Casulo_Mult(i, c) > Casulo_Mult(melhor, c)) melhor = i;
	}
	return melhor;
}

static void Casulos_Ganhar(int tipo, int n) {
	casulos[tipo] = min(99, casulos[tipo] + n);
	Chat_Add3("&aVoce ganhou %i Casulo(s) %c%c&a!", &n, casuloCores[tipo], casuloNomes[tipo]);
	Casulos_Salvar();
}

/* As vezes uma selvagem derrotada deixa um casulo para tras */
static void Casulos_Recompensa(struct Criatura* c) {
	int r = Random_Next(&rnd, 10);
	if (r < 2)      Casulos_Ganhar(CASULO_REFORCADO, 1);
	else if (r < 3) Casulos_Ganhar(CASULO_MARE, 1);
}


/*########################################################################################################################*
*-----------------------------------------------------Mestres de Bioma----------------------------------------------------*
*#########################################################################################################################*/
/* Treinadores (um por tipo) que aparecem pelo mundo. Vencer a equipe deles da uma insignia. */
#define MESTRE_ID 224

struct Treinador { struct Entity base; cc_bool usado; int idx; };
static struct Treinador treinador;
static struct ModelTex mestreTex[MESTRES_COUNT];
static char mestreTexNome[MESTRES_COUNT][16];

static struct {
	cc_bool ativa;   /* batalha em andamento */
	int etapa;       /* qual criatura da equipe do mestre */
	int criatura;    /* indice em criaturas[] da criatura do mestre em campo (-1 = nenhuma) */
	float timer;     /* espera ate mandar a proxima */
} luta;
static float mestreSpawnTimer = 45.0f;
static float mestreSairTimer;

static int Mestre_Proximo(void) {
	int i;
	for (i = 0; i < MESTRES_COUNT; i++)
	{
		if (!(insignias & (1 << i))) return i;
	}
	return -1;
}

static void Mestre_Falar(const char* fala) {
	const struct MestreDef* m = &mestres_def[treinador.idx];
	Chat_Add3("%c%c: &f%c", m->cor, m->nome, fala);
}

static void Mestres_RegistrarTexturas(void) {
	cc_string name;
	int i;
	for (i = 0; i < MESTRES_COUNT; i++)
	{
		String_InitArray_NT(name, mestreTexNome[i]);
		String_Format1(&name, "mestre%i.png", &i);
		mestreTexNome[i][name.length] = '\0';
		mestreTex[i].name     = mestreTexNome[i];
		mestreTex[i].skinType = SKIN_64x64;
		Model_RegisterTexture(&mestreTex[i]);
	}
}

static void Mestre_CheckTexture(int idx) {
	const struct MestreDef* m = &mestres_def[idx];
	struct Stream mem;
	cc_string name;
	if (mestreTex[idx].texID) return;

	Stream_ReadonlyMemory(&mem, (void*)m->png, m->pngLen);
	name = String_FromReadonly(mestreTexNome[idx]);
	Game_UpdateTexture(&mestreTex[idx].texID, &mem, &name, &mestreTex[idx].skinType, NULL);
}

static void Treinador_Tick(struct Entity* e, float delta) {
	struct Entity* jogador = &Entities.CurPlayer->Base;
	float dx = jogador->Position.x - e->next.pos.x, dz = jogador->Position.z - e->next.pos.z;
	e->prev = e->next;
	/* fica olhando para o jogador */
	e->next.yaw  = Math_Atan2f(-dz, dx) * MATH_RAD2DEG;
	e->next.rotY = e->next.yaw;
	AnimatedComp_Update(e, e->prev.pos, e->next.pos, delta);
}

static void Treinador_RenderModel(struct Entity* e, float delta, float t) {
	struct Treinador* tr = (struct Treinador*)e;
	Mestre_CheckTexture(tr->idx);
	e->TextureId = mestreTex[tr->idx].texID;
	e->SkinType  = SKIN_64x64;

	Vec3_Lerp(&e->Position, &e->prev.pos, &e->next.pos, t);
	Entity_LerpAngles(e, t);
	AnimatedComp_GetCurrent(e, t);
	e->ShouldRender = Model_ShouldRender(e);
	if (e->ShouldRender) Model_Render(e->Model, e);
}

static PackedCol Treinador_GetCol(struct Entity* e) {
	Vec3 eyePos = Entity_GetEyePosition(e);
	IVec3 pos;
	IVec3_Floor(&pos, &eyePos);
	return Lighting.Color(pos.x, pos.y, pos.z);
}

static cc_bool Treinador_ShouldRenderName(struct Entity* e) {
	return e->ShouldRender && Model_RenderDistance(e) <= 32.0f * 32.0f;
}

static const struct EntityVTABLE treinador_VTABLE = {
	Treinador_Tick,        Criatura_Despawn,          Criatura_SetLocation, Treinador_GetCol,
	Treinador_RenderModel, Treinador_ShouldRenderName
};

static void Mestre_EncerrarLuta(void) {
	struct Criatura* minha = Criatura_DoJogador();
	if (luta.criatura >= 0 && criaturas[luta.criatura].usada && criaturas[luta.criatura].doMestre)
		Criatura_Remover(&criaturas[luta.criatura]);
	if (minha) minha->alvo = -1;
	luta.ativa = false; luta.criatura = -1;
}

static void Mestre_Remover(void) {
	if (luta.ativa) Mestre_EncerrarLuta();
	if (!treinador.usado) return;
	Entities_Remove(MESTRE_ID);
	treinador.usado = false;
	mestreSairTimer = 0;
}

static void Mestre_Aparecer(int idx, Vec3 pos) {
	const struct MestreDef* m = &mestres_def[idx];
	struct Entity* e = &treinador.base;
	cc_string str; char strBuffer[STRING_SIZE];

	Mestre_Remover();
	Mem_Set(&treinador, 0, sizeof(treinador));
	Entity_Init(e);
	e->VTABLE = &treinador_VTABLE;
	treinador.usado = true;
	treinador.idx   = idx;
	e->prev.pos = e->next.pos = e->Position = pos;

	String_InitArray(str, strBuffer);
	String_Format3(&str, "%c%c &7(%c)", m->cor, m->nome, m->titulo);
	Entity_SetName(e, &str);
	Entities.List[MESTRE_ID] = e;

	Mem_Set(&luta, 0, sizeof(luta));
	luta.criatura = -1;
	Chat_Add2("&eUm Mestre de Bioma apareceu por perto: %c%c&e!", m->cor, m->nome);
	Chat_AddRaw("&eBata nele (quebrar bloco / R) para desafiar.");
}

static void Mestre_Desafiar(void) {
	const struct MestreDef* m = &mestres_def[treinador.idx];
	cc_string msg; char msgBuffer[STRING_SIZE];
	int prox;

	if (!equipeCount) { Mestre_Falar("Primeiro escolha sua criatura com /client inicial."); return; }
	if (!Criatura_DoJogador()) {
		prox = ProximoSaudavel(-1);
		if (prox < 0) { Mestre_Falar("Suas criaturas estao cansadas. Deixe elas descansarem no casulo."); return; }
		SoltarMembro(prox);
	}
	String_InitArray(msg, msgBuffer);
	String_Format1(&msg, "Quer a Insignia %c? Mostre o que sabe!", m->insignia);
	msgBuffer[msg.length] = '\0';
	Mestre_Falar(msgBuffer);

	luta.ativa = true; luta.etapa = 0; luta.criatura = -1; luta.timer = 1.5f;
}

static void Mestre_CriaturaCaiu(void) {
	luta.etapa++;
	luta.criatura = -1;
	luta.timer    = 2.0f;
}

static void Mestre_Vencido(void) {
	const struct MestreDef* m = &mestres_def[treinador.idx];
	cc_string msg; char msgBuffer[STRING_SIZE];
	int total = 0, i;

	insignias |= 1 << treinador.idx;
	for (i = 0; i < MESTRES_COUNT; i++) { if (insignias & (1 << i)) total++; }

	Mestre_Falar("Que batalha! Voce mereceu. Procure o proximo Mestre.");
	String_InitArray(msg, msgBuffer);
	String_Format2(&msg, "%cInsignia %c!", m->cor, m->insignia);
	Chat_AddOf(&msg, MSG_TYPE_ANNOUNCEMENT);
	Chat_Add3("&6Voce ganhou a Insignia %c! &7(%i de %i)", m->insignia, &total, &i);

	luta.ativa = false; luta.criatura = -1;
	mestreSairTimer  = 6.0f;
	mestreSpawnTimer = 180.0f; /* o proximo Mestre demora um pouco para aparecer */
	casulos[CASULO_REFORCADO] = min(99, casulos[CASULO_REFORCADO] + 3);
	Casulos_Ganhar(CASULO_MESTRE, 1);
	Chat_AddRaw("&aE mais 3 Casulos &7Reforcados&a.");
	if (total == MESTRES_COUNT) Chat_AddRaw("&dVoce tem todas as insignias! Agora voce e Campeao dos Biomas!");
}

static void Mestre_MandarCriatura(void) {
	const struct MestreDef* m = &mestres_def[treinador.idx];
	struct Entity* jogador = &Entities.CurPlayer->Base;
	struct Criatura* c;
	Vec3 pos = treinador.base.next.pos;
	float dx = jogador->Position.x - pos.x, dz = jogador->Position.z - pos.z;
	float len = Math_SqrtF(dx * dx + dz * dz) + 0.01f, y;
	int especie = m->especies[luta.etapa], nivel = m->niveis[luta.etapa];

	pos.x += dx / len * 1.5f; pos.z += dz / len * 1.5f;
	if (AcharChao(Math_Floor(pos.x), Math_Floor(pos.z), Math_Floor(pos.y + 0.5f), true, &y)) pos.y = y;

	c = Criatura_Criar(especie, nivel, pos, true);
	if (!c) { luta.timer = 1.0f; return; }
	c->doMestre   = true;
	c->avisouTipo = false;
	luta.criatura = Criatura_Index(c);
	Chat_Add4("%c%c &fmandou %c%c&f!", m->cor, m->nome, especies_def[especie].cor, especies_def[especie].nome);
}

static void Mestre_Tick(float delta) {
	struct Entity* jogador = &Entities.CurPlayer->Base;
	struct Criatura* minha;
	struct Criatura* cur;
	int idx, prox;
	float ang, d2, y;
	Vec3 pos;

	if (!treinador.usado) {
		/* aparece de vez em quando um Mestre ainda nao vencido */
		idx = Mestre_Proximo();
		if (!equipeCount || idx < 0) return;
		mestreSpawnTimer -= delta;
		if (mestreSpawnTimer > 0) return;
		mestreSpawnTimer = 5.0f; /* tenta de novo logo se nao achar chao */

		ang = Random_Float(&rnd) * 2 * MATH_PI;
		pos.x = jogador->Position.x + Math_CosF(ang) * 12.0f;
		pos.z = jogador->Position.z + Math_SinF(ang) * 12.0f;
		if (!AcharChaoEntre(Math_Floor(pos.x), Math_Floor(pos.z), Math_Floor(jogador->Position.y) + 8,
			Math_Floor(jogador->Position.y) - 8, false, &y)) return;
		pos.x = Math_Floor(pos.x) + 0.5f; pos.z = Math_Floor(pos.z) + 0.5f; pos.y = y;
		Mestre_Aparecer(idx, pos);
		mestreSpawnTimer = 240.0f;
		return;
	}

	d2 = Dist2XZ(treinador.base.next.pos, jogador->Position);
	if (mestreSairTimer > 0) {
		mestreSairTimer -= delta;
		if (mestreSairTimer <= 0) Mestre_Remover();
		return;
	}
	if (!luta.ativa) {
		if (d2 > 64 * 64) Mestre_Remover();
		return;
	}
	if (d2 > 32 * 32) {
		Chat_AddRaw("&eVoce se afastou e a batalha contra o Mestre acabou.");
		Mestre_EncerrarLuta();
		return;
	}

	minha = Criatura_DoJogador();
	if (!minha) {
		prox = ProximoSaudavel(-1);
		if (prox < 0) {
			Mestre_Falar("Suas criaturas cansaram. Volte quando estiver mais forte!");
			Mestre_EncerrarLuta();
			return;
		}
		SoltarMembro(prox);
		minha = Criatura_DoJogador();
	}

	cur = luta.criatura >= 0 ? &criaturas[luta.criatura] : NULL;
	if (cur && (!cur->usada || !cur->doMestre)) { cur = NULL; luta.criatura = -1; }
	if (cur) {
		if (minha && minha->alvo < 0) minha->alvo = luta.criatura;
		if (minha && cur->alvo < 0)   cur->alvo   = Criatura_Index(minha);
		return;
	}

	luta.timer -= delta;
	if (luta.timer > 0) return;
	if (luta.etapa >= mestres_def[treinador.idx].count) { Mestre_Vencido(); return; }
	Mestre_MandarCriatura();
}

static cc_bool MestreNaMira(Vec3 eye, Vec3 dir, float maxT, float* t) {
	float t0, t1;
	if (!treinador.usado) return false;
	if (!Intersection_RayIntersectsRotatedBox(eye, dir, &treinador.base, &t0, &t1)) return false;
	if (t0 < 0 || t0 > maxT) return false;
	*t = t0;
	return true;
}

/*########################################################################################################################*
*-----------------------------------------------------------HUD-----------------------------------------------------------*
*#########################################################################################################################*/
/* Linhas no canto de cima a direita: sua criatura, o alvo e a equipe */
static char hudTexto[3][STRING_SIZE];
static float hudTimer;

static void Hud_Linha(int linha, const cc_string* str) {
	cc_string antigo = String_FromReadonly(hudTexto[linha]);
	if (String_Equals(&antigo, str)) return;
	String_CopyToRawArray(hudTexto[linha], str);
	Chat_AddOf(str, MSG_TYPE_STATUS_1 + linha);
}

static void Hud_Barra(cc_string* str, int vida, int vidaMax) {
	int i, cheio = vidaMax > 0 ? (vida * 10 + vidaMax - 1) / vidaMax : 0;
	String_AppendConst(str, vida * 3 <= vidaMax ? "&c" : (vida * 3 <= vidaMax * 2 ? "&e" : "&a"));
	for (i = 0; i < 10; i++)
	{
		if (i == cheio) String_AppendConst(str, "&8");
		String_Append(str, '|');
	}
	String_Format2(str, " &f%i/%i", &vida, &vidaMax);
}

static void Hud_Criatura(cc_string* str, const char* titulo, struct Criatura* c) {
	const struct EspecieDef* def = &especies_def[c->especie];
	int vidaMax = VidaMax(c->especie, c->nivel);
	String_AppendConst(str, titulo);
	String_AppendConst(str, def->cor);
	String_AppendConst(str, def->nome);
	String_Format1(str, " &fNv%i ", &c->nivel);
	Hud_Barra(str, c->vida, vidaMax);
}

static void Hud_Limpar(void) {
	int i;
	for (i = 0; i < 3; i++)
	{
		if (!hudTexto[i][0]) continue;
		hudTexto[i][0] = '\0';
		Chat_AddOf(&String_Empty, MSG_TYPE_STATUS_1 + i);
	}
}

static void Hud_Atualizar(float delta) {
	cc_string str; char strBuffer[STRING_SIZE];
	struct Criatura* minha;
	int i;

	hudTimer -= delta;
	if (hudTimer > 0) return;
	hudTimer = 0.2f;
	if (!equipeCount) { Hud_Limpar(); return; }
	minha = Criatura_DoJogador();

	String_InitArray(str, strBuffer);
	if (minha) Hud_Criatura(&str, "&fSua: ", minha);
	else String_AppendConst(&str, "&7Nenhuma criatura fora do casulo");
	Hud_Linha(0, &str);

	String_InitArray(str, strBuffer);
	if (minha && minha->alvo >= 0 && criaturas[minha->alvo].usada)
		Hud_Criatura(&str, "&fContra: ", &criaturas[minha->alvo]);
	Hud_Linha(1, &str);

	String_InitArray(str, strBuffer);
	String_AppendConst(&str, "&fEquipe: ");
	for (i = 0; i < equipeCount; i++)
	{
		if (i == membroAtivo)           String_AppendConst(&str, "&e");
		else if (equipe[i].vida <= 0)   String_AppendConst(&str, "&8");
		else                            String_AppendConst(&str, "&a");
		String_Append(&str, 'o');
	}
	String_Format3(&str, " &7Casulos &fR%i &bM%i &dS%i", &casulos[CASULO_REFORCADO], &casulos[CASULO_MARE], &casulos[CASULO_MESTRE]);
	for (i = 0; i < MESTRES_COUNT; i++) { if (insignias & (1 << i)) String_AppendConst(&str, " &6*"); }
	Hud_Linha(2, &str);
}

static float capturaTimer;
static void Bau_Tick(float delta);
static cc_bool Bau_Clique(void);
static void Criaturas_TickGlobal(struct ScheduledTask* task) {
	float delta = (float)task->interval;
	capturaTimer -= delta;
	if (!Ativo()) { Hud_Limpar(); return; }

	/* traz a criatura ativa de volta depois de carregar o mundo */
	if (membroAtivo >= 0 && !Criatura_DoJogador()) SoltarMembro(membroAtivo);
	Spawner_Tick(delta);
	Equipe_Descansar(delta);
	Mestre_Tick(delta);
	Bau_Tick(delta);
	Hud_Atualizar(delta);
}


/*########################################################################################################################*
*---------------------------------------------------------Clique----------------------------------------------------------*
*#########################################################################################################################*/
static struct Criatura* CriaturaNaMira(float* tOut) {
	struct LocalPlayer* p = Entities.CurPlayer;
	Vec3 eye = Entity_GetEyePosition(&p->Base);
	Vec3 dir = Vec3_GetDirVector(p->Base.Yaw * MATH_DEG2RAD, p->Base.Pitch * MATH_DEG2RAD);
	struct Criatura* melhor = NULL;
	float t0, t1, melhorT = p->ReachDistance + 1.0f, blocoT;
	int i;

	/* nao atravessa paredes: compara com o bloco mirado */
	if (Game_SelectedPos.valid) {
		blocoT = Math_SqrtF(Dist2(eye, Game_SelectedPos.intersect));
		if (blocoT < melhorT) melhorT = blocoT;
	}

	for (i = 0; i < CRIATURAS_MAX; i++)
	{
		if (!criaturas[i].usada || criaturas[i].capturando) continue;
		if (!Intersection_RayIntersectsRotatedBox(eye, dir, &criaturas[i].base, &t0, &t1)) continue;
		if (t0 < 0 || t0 > melhorT) continue;
		melhorT = t0; melhor = &criaturas[i];
	}
	*tOut = melhorT;
	return melhor;
}

/* --- Casulo jogado: voa ate a criatura, prende ela e balanca antes de decidir --- */
#define LANCAMENTO_ID 225
#define VOO_TEMPO     0.5f
#define BALANCO_TEMPO 0.7f

static struct Lancamento {
	struct Entity base;
	cc_bool usado, sucesso, pousou;
	int tipo, alvo, balancos;
	float t;
	Vec3 de, para;
} lanc;

static void Lancamento_Efeito(Vec3 p, BlockID bloco) {
	IVec3 coords;
	p.y += 0.3f;
	IVec3_Floor(&coords, &p);
	Particles_BreakBlockEffect(coords, bloco, BLOCK_AIR);
}

static void Lancamento_Remover(void) {
	if (!lanc.usado) return;
	if (lanc.alvo >= 0 && criaturas[lanc.alvo].usada) criaturas[lanc.alvo].capturando = false;
	Entities_Remove(LANCAMENTO_ID);
	lanc.usado = false;
}

static void Captura_Concluir(struct Criatura* c) {
	const struct EspecieDef* def = &especies_def[c->especie];
	Vec3 pos = lanc.para;

	c->capturando = false;
	if (lanc.sucesso && equipeCount < EQUIPE_MAX) {
		equipe[equipeCount].especie = c->especie;
		equipe[equipeCount].nivel   = c->nivel;
		equipe[equipeCount].vida    = max(1, c->vida);
		equipe[equipeCount].xp      = 0;
		equipeCount++;
		Diario_Marcar(c->especie, true);

		Chat_Add2("&aPegou! Voce capturou %c%c&a!", def->cor, def->nome);
		Lancamento_Efeito(pos, BLOCK_GOLD);
		Criatura_Remover(c);
		if (!Criatura_DoJogador()) SoltarMembro(equipeCount - 1);
		else Equipe_Salvar();
	} else {
		Chat_Add2("%c%c &eescapou do casulo! Enfraqueca ele antes.", def->cor, def->nome);
		Lancamento_Efeito(pos, casuloBloco[lanc.tipo]);
		c->fugaTimer = 2.0f;
	}
	lanc.alvo = -1;
	Lancamento_Remover();
}

static void Lancamento_Tick(struct Entity* e, float delta) {
	struct Criatura* c = &criaturas[lanc.alvo];
	float s, local;
	int k;
	Vec3 pos;

	e->prev = e->next;
	lanc.t += delta;
	if (!c->usada) { lanc.alvo = -1; Lancamento_Remover(); return; }

	if (lanc.t < VOO_TEMPO) {
		/* voa em arco ate a criatura, girando */
		s = lanc.t / VOO_TEMPO;
		Vec3_Lerp(&pos, &lanc.de, &lanc.para, s);
		pos.y += Math_SinF(s * MATH_PI) * 1.2f;
		e->next.pos  = pos;
		e->next.rotX = s * 720.0f;
		return;
	}
	if (!lanc.pousou) {
		/* abriu: a criatura entra no casulo */
		lanc.pousou   = true;
		c->capturando = true;
		e->next.pos   = lanc.para;
		e->prev.pos   = lanc.para;
		e->next.rotX  = 0; e->prev.rotX = 0;
		Lancamento_Efeito(lanc.para, casuloBloco[lanc.tipo]);
	}

	/* balanca uma vez para cada "quase"; quando escapa, para antes do terceiro */
	local = lanc.t - VOO_TEMPO;
	k     = (int)(local / BALANCO_TEMPO);
	if (k < lanc.balancos) {
		local -= k * BALANCO_TEMPO;
		e->next.rotZ = local < 0.35f ? Math_SinF(local / 0.35f * 2 * MATH_PI) * 25.0f : 0.0f;
		return;
	}
	Captura_Concluir(c);
}

static void Lancamento_RenderModel(struct Entity* e, float delta, float t) {
	Vec3_Lerp(&e->Position, &e->prev.pos, &e->next.pos, t);
	Entity_LerpAngles(e, t);
	e->ShouldRender = Model_ShouldRender(e);
	if (e->ShouldRender) Model_Render(e->Model, e);
}

static cc_bool Lancamento_ShouldRenderName(struct Entity* e) { return false; }

static const struct EntityVTABLE lancamento_VTABLE = {
	Lancamento_Tick,        Criatura_Despawn,            Criatura_SetLocation, Treinador_GetCol,
	Lancamento_RenderModel, Lancamento_ShouldRenderName
};

static void TentarCapturar(struct Criatura* c) {
	struct Entity* jogador = &Entities.CurPlayer->Base;
	struct Entity* e = &lanc.base;
	float chance;
	int vidaMax = VidaMax(c->especie, c->nivel), tipo;
	cc_string name;

	if (capturaTimer > 0 || lanc.usado) return;
	capturaTimer = 1.0f;

	if (c->doMestre) {
		Chat_AddRaw("&cNao da para capturar a criatura de um Mestre! Venca ela na batalha.");
		return;
	}
	if (equipeCount >= EQUIPE_MAX) {
		Chat_AddRaw("&cSua equipe esta cheia (6). Use &f/client equipe &cpara ver.");
		return;
	}

	chance = 1.0f - (float)c->vida / vidaMax;
	if (chance < 0.15f) chance = 0.15f;

	/* casulos melhores: chance = 1 - (1 - base) ^ mult */
	tipo   = Casulo_Escolher(c);
	chance = 1.0f - (float)Math_Exp2(Math_Log2(1.0f - chance) * Casulo_Mult(tipo, c));
	if (tipo != CASULO_COMUM) { casulos[tipo]--; Casulos_Salvar(); }
	Chat_Add2("&7Voce jogou um Casulo %c%c&7...", casuloCores[tipo], casuloNomes[tipo]);

	Mem_Set(&lanc, 0, sizeof(lanc));
	Entity_Init(e);
	e->VTABLE    = &lancamento_VTABLE;
	lanc.usado   = true;
	lanc.tipo    = tipo;
	lanc.alvo    = Criatura_Index(c);
	lanc.sucesso = Random_Float(&rnd) < chance;
	lanc.balancos = lanc.sucesso ? 3 : Random_Next(&rnd, 3);

	lanc.de   = Entity_GetEyePosition(jogador);
	lanc.de.y -= 0.3f;
	lanc.para = c->base.next.pos;
	e->prev.pos = e->next.pos = e->Position = lanc.de;
	e->next.yaw = e->prev.yaw = jogador->Yaw;

	name = String_FromReadonly(casulos_def[tipo].nome);
	e->Model = Model_Get(&name);
	Vec3_Set(e->ModelScale, 1.3f, 1.3f, 1.3f);
	Entity_UpdateModelBounds(e);
	Entities.List[LANCAMENTO_ID] = e;
}

/* Diario: tela com a lista de especies */
static const char* const tipoNomes[TIPO_COUNT] = { "Fogo", "Agua", "Planta", "Eletrico", "Vento", "Pedra", "Inseto", "Fantasma", "Gelo" };
static const char* const habitatNomes[]        = { "na grama", "na areia", "na agua", "em lugares altos", "nas pedras", "na neve e no gelo", "a noite e em cavernas fundas" };

static void Diario_Contar(int* vistas, int* capturadas) {
	int i;
	*vistas = 0; *capturadas = 0;
	for (i = 0; i < ESPECIES_COUNT; i++)
	{
		if (diarioVistas     & (1 << i)) (*vistas)++;
		if (diarioCapturadas & (1 << i)) (*capturadas)++;
	}
}

static void Diario_CarregarLista(struct StringsBuffer* entries) {
	cc_string str; char strBuffer[STRING_SIZE];
	int i, n;
	for (i = 0; i < ESPECIES_COUNT; i++)
	{
		String_InitArray(str, strBuffer);
		n = i + 1;
		if (n < 10) String_Append(&str, '0');
		String_AppendInt(&str, n);
		String_Append(&str, ' ');

		if (!(diarioVistas & (1 << i))) {
			String_AppendConst(&str, "&8???");
		} else {
			String_AppendConst(&str, especies_def[i].cor);
			String_AppendConst(&str, especies_def[i].nome);
			String_AppendConst(&str, (diarioCapturadas & (1 << i)) ? " &a(capturada)" : " &7(vista)");
		}
		StringsBuffer_Add(entries, &str);
	}
}

static void Diario_MostrarEspecie(int i) {
	const struct EspecieDef* def = &especies_def[i];
	struct Screen* tela = Gui_GetInputGrab();
	int n = i + 1, vel = (int)def->vel, vida = def->vida, dano = def->dano;
	if (i < 0 || i >= ESPECIES_COUNT) return;
	if (tela) Gui_Remove(tela);

	if (!(diarioVistas & (1 << i))) {
		Chat_Add1("&8#%i ???: &7ainda nao vista. Explore outros lugares!", &n);
		return;
	}
	Chat_Add4("&f#%i %c%c &7- tipo %c", &n, def->cor, def->nome, tipoNomes[def->tipo]);
	Chat_Add1("&7%c", def->desc);
	Chat_Add4("&7Vive %c. Vida %i, ataque %i, velocidade %i.", habitatNomes[def->habitat], &vida, &dano, &vel);
	if (def->evolui >= 0) {
		Chat_Add3("&7Evolui para %c%c &7no nivel %i.", especies_def[def->evolui].cor, especies_def[def->evolui].nome, &def->nivelEvolui);
	}
	Chat_AddRaw((diarioCapturadas & (1 << i)) ? "&aVoce ja capturou uma!" : "&eAinda nao capturada.");
}

static char diarioTitulo[64];
static void Diario_Abrir(void) {
	cc_string str;
	int vistas, capturadas, total = ESPECIES_COUNT;
	Diario_Contar(&vistas, &capturadas);
	String_InitArray_NT(str, diarioTitulo);
	String_Format3(&str, "Diario: %i vistas, %i capturadas de %i", &vistas, &capturadas, &total);
	diarioTitulo[str.length] = '\0';
	ModListScreen_Show(diarioTitulo, "Fechar", Diario_CarregarLista, Diario_MostrarEspecie);
	Chat_AddRaw("&7Toque numa criatura do Diario para ver os detalhes.");
}

/* --- Acoes (usadas pelos botoes L/R e pelos botoes de toque da tela de baixo) --- */
static void Acao_Atacar(struct Criatura* c) {
	struct Entity* jogador = &Entities.CurPlayer->Base;
	struct Criatura* minha;
	/* sua criatura entra na briga */
	c->ajudou = c->ajudou || Criatura_DoJogador() != NULL;
	Criatura_Dano(c, 2, jogador->Position);
	minha = Criatura_DoJogador();
	if (minha && c->usada) {
		minha->alvo = Criatura_Index(c);
		if (c->alvo < 0) c->alvo = Criatura_Index(minha);
	}
}

static void Acao_Curar(struct Criatura* c) {
	if (c->curaTimer > 0) { Chat_AddRaw("&eA fruta-cura ainda esta crescendo, espere um pouco."); return; }
	if (c->vida >= VidaMax(c->especie, c->nivel)) { Chat_AddRaw("&eSua criatura ja esta com a vida cheia."); return; }
	c->vida = VidaMax(c->especie, c->nivel);
	c->curaTimer = 20.0f;
	c->nomeSujo  = true;
	Membro_SincronizarDe(c);
	Equipe_Salvar();
	Chat_AddRaw("&aSua criatura comeu uma fruta-cura e recuperou a vida!");
}

static void Acao_Trocar(struct Criatura* c) {
	int prox = ProximoSaudavel(membroAtivo);
	if (prox < 0) { Chat_AddRaw("&eNao tem outra criatura pronta na equipe."); return; }
	if (capturaTimer > 0) return;
	capturaTimer = 1.0f;
	if (c) Chat_Add2("&eVolta, %c%c&e!", especies_def[c->especie].cor, especies_def[c->especie].nome);
	SoltarMembro(prox);
	Chat_Add2("&eVai, %c%c&e!", especies_def[equipe[prox].especie].cor, especies_def[equipe[prox].especie].nome);
}

cc_bool Criaturas_HandleClick(cc_bool place) {
	struct Criatura* c;
	struct LocalPlayer* p;
	float tc, tm;
	if (!Ativo()) return false;

	if (Bau_Clique()) return true;
	c = CriaturaNaMira(&tc);
	p = Entities.CurPlayer;
	if (MestreNaMira(Entity_GetEyePosition(&p->Base), Vec3_GetDirVector(p->Base.Yaw * MATH_DEG2RAD, p->Base.Pitch * MATH_DEG2RAD),
			c ? tc : p->ReachDistance + 1.0f, &tm)) {
		if (luta.ativa)              Chat_AddRaw("&eA batalha ja comecou! Ajude sua criatura batendo na do Mestre.");
		else if (mestreSairTimer > 0) Mestre_Falar("Boa sorte com o proximo Mestre!");
		else                          Mestre_Desafiar();
		return true;
	}
	if (!c) return false;

	if (!c->selvagem) {
		/* colocar na sua criatura: vida cheia = troca pela proxima da equipe, */
		/* senao da uma fruta-cura */
		if (place && c->vida >= VidaMax(c->especie, c->nivel)) Acao_Trocar(c);
		else if (place) Acao_Curar(c);
		return true;
	}

	if (place) TentarCapturar(c);
	else       Acao_Atacar(c);
	return true;
}


/*########################################################################################################################*
*--------------------------------------------------------Comandos---------------------------------------------------------*
*#########################################################################################################################*/
static void Equipe_Listar(void) {
	cc_string msg; char msgBuffer[STRING_SIZE];
	const struct EspecieDef* def;
	int i, n;

	if (!equipeCount) {
		Chat_AddRaw("&eVoce ainda nao tem criaturas. Use &f/client inicial&e para escolher uma!");
		return;
	}
	Chat_AddRaw("&fSua equipe:");
	for (i = 0; i < equipeCount; i++)
	{
		def = &especies_def[equipe[i].especie];
		n   = i + 1;
		String_InitArray(msg, msgBuffer);
		String_Format4(&msg, "&f%i. %c%c &fNv.%i", &n, def->cor, def->nome, &equipe[i].nivel);
		if (i == membroAtivo) String_AppendConst(&msg, " &a(fora do casulo)");
		else if (equipe[i].vida <= 0) String_AppendConst(&msg, " &8(descansando)");
		Chat_Add(&msg);
	}
	Chat_AddRaw("&7/client equipe [numero] solta, /client equipe guardar guarda");
}

static void EquipeCommand_Execute(const cc_string* args, int argsCount) {
	int n;

	if (!criaturasLigadas) { Chat_AddRaw("&eO mod de criaturas esta desligado (menu Mods)."); return; }
	if (!argsCount) { Equipe_Listar(); return; }

	if (String_CaselessEqualsConst(&args[0], "guardar")) {
		SoltarMembro(-1);
		Chat_AddRaw("&eSua criatura voltou para o casulo.");
	} else if (String_CaselessEqualsConst(&args[0], "soltar") && argsCount > 1 && Convert_ParseInt(&args[1], &n) && n >= 1 && n <= equipeCount) {
		/* /client equipe soltar 2 = libera a criatura 2 na natureza */
		if (membroAtivo == n - 1) SoltarMembro(-1);
		else if (membroAtivo > n - 1) membroAtivo--;
		for (; n < equipeCount; n++) equipe[n - 1] = equipe[n];
		equipeCount--;
		Equipe_Salvar();
		Chat_AddRaw("&eVoce libertou a criatura. Tchau!");
	} else if (Convert_ParseInt(&args[0], &n) && n >= 1 && n <= equipeCount) {
		if (equipe[n - 1].vida <= 0) { Chat_AddRaw("&eEssa criatura esta descansando no casulo. Espere ela recuperar a vida."); return; }
		SoltarMembro(n - 1);
	} else {
		Equipe_Listar();
	}
}

static struct ChatCommand EquipeCommand = {
	"Equipe", EquipeCommand_Execute,
	COMMAND_FLAG_SINGLEPLAYER_ONLY,
	{
		"&a/client equipe &e- mostra suas criaturas",
		"&a/client equipe [numero] &e- solta essa criatura para te seguir",
		"&a/client equipe guardar &e- guarda a criatura no casulo",
		"&a/client equipe soltar [numero] &e- liberta a criatura",
	}
};

static void Ajuda_Batalha(void) {
	Chat_AddRaw("&eComo batalhar: chegue perto de uma criatura selvagem e toque em &fAtacar");
	Chat_AddRaw("&e(tela de baixo) ou mire e aperte &fR&e. Com a vida dela baixa, toque em &fCapturar &e(ou &fL&e).");
	Chat_AddRaw("&f Curar &ee &fTrocar &ecuidam da sua criatura; &fCriaturas &eabre Diario, equipe e Mestres.");
}

static void Inicial_Escolher(int escolha) {
	if (equipeCount) { Chat_AddRaw("&eVoce ja tem sua criatura inicial!"); return; }
	equipe[0].especie = escolha;
	equipe[0].nivel   = 5;
	equipe[0].vida    = VidaMax(escolha, 5);
	equipe[0].xp      = 0;
	equipeCount = 1;
	SoltarMembro(0);
	Chat_Add2("&aVoce escolheu %c%c&a! Cuide bem dele.", especies_def[escolha].cor, especies_def[escolha].nome);
	Ajuda_Batalha();
}

static void InicialCommand_Execute(const cc_string* args, int argsCount) {
	int i, escolha = -1;
	cc_string nome;

	if (!criaturasLigadas) { Chat_AddRaw("&eO mod de criaturas esta desligado (menu Mods)."); return; }

	if (equipeCount) {
		Chat_AddRaw("&eVoce ja tem sua criatura inicial!");
		return;
	}
	if (argsCount) {
		for (i = 0; i < ESPECIES_COUNT; i++)
		{
			if (!especies_def[i].inicial) continue;
			nome = String_FromReadonly(especies_def[i].nome);
			if (String_CaselessEquals(&args[0], &nome)) escolha = i;
		}
	}
	if (escolha < 0) {
		Chat_AddRaw("&eEscolha sua criatura inicial:");
		for (i = 0; i < ESPECIES_COUNT; i++)
		{
			if (especies_def[i].inicial) Chat_Add2("  %c%c", especies_def[i].cor, especies_def[i].nome);
		}
		Chat_AddRaw("&eDigite &f/client inicial [nome]");
		return;
	}

	Inicial_Escolher(escolha);
}

static struct ChatCommand InicialCommand = {
	"Inicial", InicialCommand_Execute,
	COMMAND_FLAG_SINGLEPLAYER_ONLY,
	{
		"&a/client inicial [nome]",
		"&eEscolhe sua primeira criatura: Faiscol, Gotinha ou Brotinho.",
	}
};


/* Faz nascer criaturas selvagens na frente do jogador (para testar ou mostrar) */
static void Bau_Reiniciar(void);
static void Barra_AlternarPC(void);
static void Barra_Tocar(int i);
static void CriaturaCommand_Execute(const cc_string* args, int argsCount) {
	struct Entity* jogador = &Entities.CurPlayer->Base;
	struct Criatura* c;
	cc_string nome;
	int i, nivel = 5, escolha = -1, count = 0, total;
	float yaw = jogador->Yaw * MATH_DEG2RAD, ang, dist, y;
	Vec3 pos;

	if (!criaturasLigadas) { Chat_AddRaw("&eO mod de criaturas esta desligado (menu Mods)."); return; }
	if (argsCount > 1) Convert_ParseInt(&args[1], &nivel);
	nivel = max(1, min(nivel, NIVEL_MAX));
	if (argsCount) {
		for (i = 0; i < ESPECIES_COUNT; i++)
		{
			nome = String_FromReadonly(especies_def[i].nome);
			if (String_CaselessEquals(&args[0], &nome)) escolha = i;
		}
	}
	if (argsCount && String_CaselessEqualsConst(&args[0], "botoes")) {
		Barra_AlternarPC();
		return;
	}
	if (argsCount && String_CaselessEqualsConst(&args[0], "toque")) {
		Barra_Tocar(nivel);
		return;
	}
	if (argsCount && String_CaselessEqualsConst(&args[0], "bau")) {
		Bau_Reiniciar();
		return;
	}
	if (argsCount && String_CaselessEqualsConst(&args[0], "limpar")) {
		for (i = 0; i < CRIATURAS_MAX; i++)
		{
			if (criaturas[i].usada && criaturas[i].selvagem) Criatura_Remover(&criaturas[i]);
		}
		Chat_AddRaw("&eCriaturas selvagens removidas.");
		return;
	}
	if (escolha < 0 && !(argsCount && String_CaselessEqualsConst(&args[0], "todas"))) {
		Chat_AddRaw("&e/client criatura [nome|todas|limpar] [nivel]");
		return;
	}

	total = escolha >= 0 ? 1 : ESPECIES_COUNT;
	for (i = 0; i < total; i++)
	{
		/* em arco na frente do jogador */
		/* varias: duas fileiras em arco; uma so: logo na frente */
		ang = yaw + (total > 1 ? ((i % 8) - 3.5f) * 0.16f : 0.0f);
		dist = total > 1 ? 6.0f + (i / 8) * 4.0f : 3.0f;
		pos.x = jogador->Position.x + Math_SinF(ang) * dist;
		pos.z = jogador->Position.z - Math_CosF(ang) * dist;
		if (!AcharChaoEntre(Math_Floor(pos.x), Math_Floor(pos.z), Math_Floor(jogador->Position.y) + 8, Math_Floor(jogador->Position.y) - 16, true, &y)) continue;
		pos.y = y;

		c = Criatura_Criar(escolha >= 0 ? escolha : i, nivel, pos, true);
		if (!c) break;
		/* olha para o jogador e fica parada um pouco */
		c->base.prev.yaw  = c->base.next.yaw  = jogador->Yaw + 180.0f;
		c->base.prev.rotY = c->base.next.rotY = jogador->Yaw + 180.0f;
		c->pensarTimer   = 10.0f;
		count++;
	}
	Chat_Add1("&e%i criatura(s) apareceram.", &count);
}

static struct ChatCommand CriaturaCommand = {
	"Criatura", CriaturaCommand_Execute,
	COMMAND_FLAG_SINGLEPLAYER_ONLY,
	{
		"&a/client criatura [nome|todas|limpar] [nivel]",
		"&eFaz uma criatura selvagem aparecer na sua frente.",
	}
};


static void CasuloCommand_Execute(const cc_string* args, int argsCount) {
	int i, escolha = -2;
	cc_string nome;

	if (!criaturasLigadas) { Chat_AddRaw("&eO mod de criaturas esta desligado (menu Mods)."); return; }
	if (argsCount) {
		if (String_CaselessEqualsConst(&args[0], "auto")) escolha = -1;
		for (i = 0; i < CASULO_COUNT; i++)
		{
			nome = String_FromReadonly(casuloNomes[i]);
			if (String_CaselessEquals(&args[0], &nome)) escolha = i;
		}
		if (escolha != -2) {
			casuloEscolhido = escolha;
			Casulos_Salvar();
		}
	}

	Chat_AddRaw("&fSeus casulos:");
	Chat_AddRaw("  &fComum &7- sempre tem, chance normal");
	Chat_Add1("  &7Reforcado &7x%i - chance maior", &casulos[CASULO_REFORCADO]);
	Chat_Add1("  &bMare &7x%i - muito bom contra criaturas de agua", &casulos[CASULO_MARE]);
	Chat_Add1("  &dMestre &7x%i - captura sempre", &casulos[CASULO_MESTRE]);
	if (casuloEscolhido < 0) Chat_AddRaw("&eUsando: automatico (o melhor que tiver, sem gastar o Mestre)");
	else Chat_Add2("&eUsando: %c%c", casuloCores[casuloEscolhido], casuloNomes[casuloEscolhido]);
	Chat_AddRaw("&7/client casulo [auto|comum|reforcado|mare|mestre]");
}

static struct ChatCommand CasuloCommand = {
	"Casulo", CasuloCommand_Execute,
	COMMAND_FLAG_SINGLEPLAYER_ONLY,
	{
		"&a/client casulo [auto|comum|reforcado|mare|mestre]",
		"&eMostra seus casulos e escolhe qual jogar com colocar bloco (L).",
	}
};

static void Mestre_Chamar(void);
static void Mestre_Listar(void);

static void MestreCommand_Execute(const cc_string* args, int argsCount) {
	if (!criaturasLigadas) { Chat_AddRaw("&eO mod de criaturas esta desligado (menu Mods)."); return; }
	if (argsCount && String_CaselessEqualsConst(&args[0], "chamar")) {
		Mestre_Chamar();
		return;
	}
	Mestre_Listar();
}

static void Mestre_Chamar(void) {
	struct Entity* jogador = &Entities.CurPlayer->Base;
	float yaw = jogador->Yaw * MATH_DEG2RAD, y;
	int idx;
	Vec3 pos;
	{
		idx = Mestre_Proximo();
		if (idx < 0) { Chat_AddRaw("&dVoce ja venceu todos os Mestres!"); return; }
		if (luta.ativa) { Chat_AddRaw("&eTermine a batalha atual primeiro."); return; }
		pos.x = jogador->Position.x + Math_SinF(yaw) * 5.0f;
		pos.z = jogador->Position.z - Math_CosF(yaw) * 5.0f;
		if (!AcharChaoEntre(Math_Floor(pos.x), Math_Floor(pos.z), Math_Floor(jogador->Position.y) + 8,
			Math_Floor(jogador->Position.y) - 16, false, &y)) { Chat_AddRaw("&eNao achei chao na sua frente."); return; }
		pos.x = Math_Floor(pos.x) + 0.5f; pos.z = Math_Floor(pos.z) + 0.5f; pos.y = y;
		Mestre_Aparecer(idx, pos);
	}
}

static void Mestre_Listar(void) {
	const struct MestreDef* m;
	int i;
	Chat_AddRaw("&fMestres de Bioma:");
	for (i = 0; i < MESTRES_COUNT; i++)
	{
		m = &mestres_def[i];
		Chat_Add4("  %c%c &7(%c) %c", m->cor, m->nome, m->titulo,
			(insignias & (1 << i)) ? "&6- Insignia ganha!" : "&8- ainda nao");
	}
	Chat_AddRaw("&7Eles aparecem pelo mundo. &f/client mestre chamar &7chama o proximo.");
}

static void DiarioCommand_Execute(const cc_string* args, int argsCount) {
	if (!criaturasLigadas) { Chat_AddRaw("&eO mod de criaturas esta desligado (menu Mods)."); return; }
	Diario_Abrir();
}

static struct ChatCommand DiarioCommand = {
	"Diario", DiarioCommand_Execute,
	COMMAND_FLAG_SINGLEPLAYER_ONLY,
	{
		"&a/client diario",
		"&eAbre o Diario das Criaturas: todas as especies que voce ja viu e capturou.",
	}
};

static struct ChatCommand MestreCommand = {
	"Mestre", MestreCommand_Execute,
	COMMAND_FLAG_SINGLEPLAYER_ONLY,
	{
		"&a/client mestre &e- lista os Mestres de Bioma e suas insignias",
		"&a/client mestre chamar &e- chama o proximo Mestre para batalhar",
	}
};

/*########################################################################################################################*
*----------------------------------------------------------Menu-----------------------------------------------------------*
*#########################################################################################################################*/
/* Menu das criaturas (botao "Criaturas..." no menu de pausa), para jogar sem digitar comandos */
enum { MENU_PRINCIPAL, MENU_INICIAL, MENU_EQUIPE };
static int menuAtual;
static int menuIniciais[ESPECIES_COUNT], menuIniciaisCount;

cc_bool Criaturas_Ativo(void) { return Ativo(); }

static void Menu_Fechar(void) {
	struct Screen* tela = Gui_GetInputGrab();
	if (tela) Gui_Remove(tela);
}

static void Menu_Add(struct StringsBuffer* entries, const cc_string* str) { StringsBuffer_Add(entries, str); }

static void Menu_Carregar(struct StringsBuffer* entries) {
	cc_string str; char strBuffer[STRING_SIZE];
	const struct EspecieDef* def;
	int i;

	if (menuAtual == MENU_INICIAL) {
		menuIniciaisCount = 0;
		for (i = 0; i < ESPECIES_COUNT; i++)
		{
			def = &especies_def[i];
			if (!def->inicial) continue;
			menuIniciais[menuIniciaisCount++] = i;
			String_InitArray(str, strBuffer);
			String_Format3(&str, "%c%c &7- %c", def->cor, def->nome, tipoNomes[def->tipo]);
			Menu_Add(entries, &str);
		}
		return;
	}
	if (menuAtual == MENU_EQUIPE) {
		for (i = 0; i < equipeCount; i++)
		{
			def = &especies_def[equipe[i].especie];
			String_InitArray(str, strBuffer);
			String_Format3(&str, "%c%c &fNv.%i ", def->cor, def->nome, &equipe[i].nivel);
			if (i == membroAtivo)          String_AppendConst(&str, "&a(fora)");
			else if (equipe[i].vida <= 0)  String_AppendConst(&str, "&8(cansada)");
			else                           String_AppendConst(&str, "&7(no casulo)");
			Menu_Add(entries, &str);
		}
		String_InitArray(str, strBuffer);
		String_AppendConst(&str, "&eGuardar a criatura no casulo");
		Menu_Add(entries, &str);
		return;
	}

	/* principal */
	String_InitArray(str, strBuffer);
	String_AppendConst(&str, "Diario das Criaturas");
	Menu_Add(entries, &str);

	String_InitArray(str, strBuffer);
	String_AppendConst(&str, equipeCount ? "Minha equipe" : "&aEscolher criatura inicial");
	Menu_Add(entries, &str);

	String_InitArray(str, strBuffer);
	String_AppendConst(&str, "Casulo: ");
	if (casuloEscolhido < 0) String_AppendConst(&str, "&fautomatico");
	else { String_AppendConst(&str, casuloCores[casuloEscolhido]); String_AppendConst(&str, casuloNomes[casuloEscolhido]); }
	String_Format3(&str, " &7(R%i M%i S%i)", &casulos[CASULO_REFORCADO], &casulos[CASULO_MARE], &casulos[CASULO_MESTRE]);
	Menu_Add(entries, &str);

	String_InitArray(str, strBuffer);
	String_AppendConst(&str, "Chamar Mestre de Bioma");
	Menu_Add(entries, &str);

	String_InitArray(str, strBuffer);
	String_AppendConst(&str, "Mestres e insignias");
	Menu_Add(entries, &str);

	String_InitArray(str, strBuffer);
	String_AppendConst(&str, "&eComo batalhar");
	Menu_Add(entries, &str);
}

static void Menu_Mostrar(int menu);

static void Menu_Clique(int i) {
	if (menuAtual == MENU_INICIAL) {
		Menu_Fechar();
		if (i < menuIniciaisCount) Inicial_Escolher(menuIniciais[i]);
		return;
	}
	if (menuAtual == MENU_EQUIPE) {
		Menu_Fechar();
		if (i == equipeCount) { SoltarMembro(-1); Chat_AddRaw("&eSua criatura voltou para o casulo."); return; }
		if (i > equipeCount) return;
		if (equipe[i].vida <= 0) { Chat_AddRaw("&eEssa criatura esta descansando no casulo. Espere ela recuperar a vida."); return; }
		if (i == membroAtivo) { Chat_AddRaw("&eEssa criatura ja esta fora do casulo."); return; }
		SoltarMembro(i);
		Chat_Add2("&eVai, %c%c&e!", especies_def[equipe[i].especie].cor, especies_def[equipe[i].especie].nome);
		return;
	}

	switch (i)
	{
	case 0:
		Menu_Fechar();
		Diario_Abrir();
		break;
	case 1:
		Menu_Mostrar(equipeCount ? MENU_EQUIPE : MENU_INICIAL);
		break;
	case 2:
		/* troca o casulo: automatico > comum > reforcado > mare > mestre > automatico */
		casuloEscolhido++;
		if (casuloEscolhido >= CASULO_COUNT) casuloEscolhido = -1;
		Casulos_Salvar();
		Menu_Mostrar(MENU_PRINCIPAL);
		break;
	case 3:
		Menu_Fechar();
		Mestre_Chamar();
		break;
	case 4:
		Menu_Fechar();
		Mestre_Listar();
		break;
	case 5:
		Menu_Fechar();
		Ajuda_Batalha();
		break;
	}
}

static void Menu_Mostrar(int menu) {
	static const char* const titulos[] = { "Criaturas", "Escolha sua criatura inicial", "Minha equipe" };
	Menu_Fechar();
	menuAtual = menu;
	ModListScreen_Show(titulos[menu], "Fechar", Menu_Carregar, Menu_Clique);
}

void Criaturas_AbrirMenu(void) {
	if (!Ativo()) { Chat_AddRaw("&eO mod de criaturas esta desligado (menu Mods)."); return; }
	Menu_Mostrar(MENU_PRINCIPAL);
}


/*########################################################################################################################*
*----------------------------------------------------Bau das Criaturas----------------------------------------------------*
*#########################################################################################################################*/
/* Kit do comeco: um bau perto de onde voce nasce, com casulos e o Diario. Abre uma vez so. */
#define BAU_ID     223
#define OPT_BAU    "criaturas-bau"
#define BAU_ABRIR  0.6f
#define BAU_SUMIR  4.0f

static struct Bau {
	struct Entity base;
	cc_bool usado, aberto, menuAberto;
	float t;
} bau;
static cc_bool bauPendente;

static void Bau_Remover(void) {
	if (!bau.usado) return;
	Entities_Remove(BAU_ID);
	bau.usado = false;
}

static void Bau_EntityTick(struct Entity* e, float delta) {
	e->prev = e->next;
	if (!bau.aberto) return;
	bau.t += delta;
	/* a tampa gira para tras pela dobradica */
	e->next.pitch = -75.0f * min(1.0f, bau.t / BAU_ABRIR);
}

static const struct EntityVTABLE bau_VTABLE = {
	Bau_EntityTick,         Criatura_Despawn,            Criatura_SetLocation, Treinador_GetCol,
	Lancamento_RenderModel, Lancamento_ShouldRenderName
};

static void Bau_Aparecer(void) {
	struct Entity* jogador = &Entities.CurPlayer->Base;
	struct Entity* e = &bau.base;
	float yaw = jogador->Yaw * MATH_DEG2RAD, y;
	int x, z;
	cc_string name;
	Vec3 pos;

	/* dois blocos na frente de onde voce nasce, virado para voce */
	x = Math_Floor(jogador->Position.x + Math_SinF(yaw) * 2.5f);
	z = Math_Floor(jogador->Position.z - Math_CosF(yaw) * 2.5f);
	if (!AcharChaoEntre(x, z, Math_Floor(jogador->Position.y) + 4, Math_Floor(jogador->Position.y) - 8, false, &y)) {
		x = Math_Floor(jogador->Position.x); z = Math_Floor(jogador->Position.z); y = jogador->Position.y;
	}

	Bau_Remover();
	Mem_Set(&bau, 0, sizeof(bau));
	Entity_Init(e);
	e->VTABLE = &bau_VTABLE;
	bau.usado = true;
	Vec3_Set(pos, x + 0.5f, y, z + 0.5f);
	e->prev.pos = e->next.pos = e->Position = pos;
	e->next.yaw = e->prev.yaw = e->Yaw = jogador->Yaw + 180.0f;
	e->next.rotY = e->prev.rotY = e->RotY = e->Yaw;

	name = String_FromReadonly("bau_criaturas");
	e->Model = Model_Get(&name);
	Vec3_Set(e->ModelScale, 1.0f, 1.0f, 1.0f);
	Entity_UpdateModelBounds(e);
	Entities.List[BAU_ID] = e;

	Chat_AddRaw("&eUm &6Bau das Criaturas &eapareceu na sua frente!");
	Chat_AddRaw("&eBata nele (R) ou toque nele (L) para abrir.");
}

static void Bau_Abrir(void) {
	IVec3 coords;
	Vec3 p = bau.base.Position;
	if (bau.aberto) return;
	bau.aberto = true;
	bau.t      = 0;

	p.y += 0.6f;
	IVec3_Floor(&coords, &p);
	Particles_BreakBlockEffect(coords, BLOCK_GOLD, BLOCK_AIR);

	Chat_AddRaw("&6Voce abriu o Bau das Criaturas!");
	Casulos_Ganhar(CASULO_REFORCADO, 10);
	Casulos_Ganhar(CASULO_MARE,       5);
	Casulos_Ganhar(CASULO_MESTRE,     1);
	Chat_AddRaw("&aVoce ganhou o &fDiario das Criaturas&a! Abra em Start > Criaturas... > Diario.");

	Options_SetBool(OPT_BAU, true);
	Options_SaveIfChanged();
}

static void Bau_Tick(float delta) {
	if (bauPendente) {
		bauPendente = false;
		if (!Options_GetBool(OPT_BAU, false)) Bau_Aparecer();
	}
	if (!bau.usado || !bau.aberto) return;

	/* depois de abrir: mostra a escolha da criatura inicial (ou o menu) e some */
	if (!bau.menuAberto && bau.t >= BAU_ABRIR + 0.4f) {
		bau.menuAberto = true;
		Menu_Mostrar(equipeCount ? MENU_PRINCIPAL : MENU_INICIAL);
	}
	if (bau.t >= BAU_SUMIR) Bau_Remover();
}

static cc_bool Bau_Clique(void) {
	struct LocalPlayer* p = Entities.CurPlayer;
	float t0, t1;
	if (!bau.usado) return false;
	if (!Intersection_RayIntersectsRotatedBox(Entity_GetEyePosition(&p->Base),
			Vec3_GetDirVector(p->Base.Yaw * MATH_DEG2RAD, p->Base.Pitch * MATH_DEG2RAD), &bau.base, &t0, &t1)) return false;
	if (t0 < 0 || t0 > p->ReachDistance + 1.0f) return false;
	Bau_Abrir();
	return true;
}

static void Bau_Reiniciar(void) {
	Options_SetBool(OPT_BAU, false);
	Bau_Aparecer();
}


/*########################################################################################################################*
*-----------------------------------------------Botoes de toque (tela de baixo)--------------------------------------------*
*#########################################################################################################################*/
/* No 3DS a tela de toque mostra botoes para batalhar sem precisar mirar: */
/* eles agem na criatura da mira ou, se nao tiver, na selvagem mais perto. */
#define BARRA_BTNS   5
#define BARRA_ALCANCE 12.0f
#define GUI_PRIORITY_CRIATURAS 22 /* entre a tela de toque (25) e o inventario (20) */
static struct BarraScreen {
	Screen_Body
	struct ButtonWidget btns[BARRA_BTNS];
	struct FontDesc font;
} BarraScreen;
static struct Widget* barra_widgets[BARRA_BTNS];
static cc_bool barraNoPC; /* /client criatura botoes: mostra os botoes no PC para testar */

static float Distancia2(const Vec3* a, const Vec3* b) {
	float dx = a->x - b->x, dy = a->y - b->y, dz = a->z - b->z;
	return dx * dx + dy * dy + dz * dz;
}

static struct Criatura* SelvagemPerto(void) {
	struct Entity* jogador = &Entities.CurPlayer->Base;
	struct Criatura* c;
	struct Criatura* melhor = NULL;
	float d, melhorD = BARRA_ALCANCE * BARRA_ALCANCE, tc;
	int i;

	c = CriaturaNaMira(&tc);
	if (c && c->selvagem && !c->capturando) return c;

	for (i = 0; i < CRIATURAS_MAX; i++)
	{
		c = &criaturas[i];
		if (!c->usada || !c->selvagem || c->capturando) continue;
		d = Distancia2(&c->base.Position, &jogador->Position);
		if (d < melhorD) { melhorD = d; melhor = c; }
	}
	return melhor;
}

static cc_bool MestrePerto(void) {
	struct Entity* jogador = &Entities.CurPlayer->Base;
	if (!treinador.usado || luta.ativa || mestreSairTimer > 0) return false;
	return Distancia2(&treinador.base.Position, &jogador->Position) < BARRA_ALCANCE * BARRA_ALCANCE;
}

static void Barra_Atacar(void* s, void* w) {
	struct Criatura* c;
	if (!Ativo()) return;
	if (bau.usado && !bau.aberto) { Bau_Abrir(); return; }
	c = SelvagemPerto();
	if (c) { Acao_Atacar(c); return; }
	if (MestrePerto()) { Mestre_Desafiar(); return; }
	Chat_AddRaw("&eNenhuma criatura selvagem por perto. Ande pela grama, areia, agua ou pedras.");
}

static void Barra_Capturar(void* s, void* w) {
	struct Criatura* c;
	if (!Ativo()) return;
	c = SelvagemPerto();
	if (c) TentarCapturar(c);
	else   Chat_AddRaw("&eNenhuma criatura selvagem por perto para capturar.");
}

static void Barra_Curar(void* s, void* w) {
	struct Criatura* c;
	if (!Ativo()) return;
	c = Criatura_DoJogador();
	if (c) Acao_Curar(c);
	else   Chat_AddRaw("&eSua criatura esta no casulo. Toque em Trocar para chamar uma.");
}

static void Barra_Trocar(void* s, void* w) {
	if (!Ativo()) return;
	if (!equipeCount) { Menu_Mostrar(MENU_INICIAL); return; }
	Acao_Trocar(Criatura_DoJogador());
}

static void Barra_Menu(void* s, void* w) { Criaturas_AbrirMenu(); }

static const struct { const char* texto; Widget_LeftClick click; } barraDescs[BARRA_BTNS] = {
	{ "Atacar",    Barra_Atacar   },
	{ "Capturar",  Barra_Capturar },
	{ "Curar",     Barra_Curar    },
	{ "Trocar",    Barra_Trocar   },
	{ "Criaturas", Barra_Menu     },
};

static void BarraScreen_ContextLost(void* screen) {
	struct BarraScreen* s = (struct BarraScreen*)screen;
	Font_Free(&s->font);
	Screen_ContextLost(screen);
}

static void BarraScreen_ContextRecreated(void* screen) {
	struct BarraScreen* s = (struct BarraScreen*)screen;
	int i;
	Screen_UpdateVb(screen);
	Gui_MakeTitleFont(&s->font);
	for (i = 0; i < BARRA_BTNS; i++)
	{
		ButtonWidget_SetConst(&s->btns[i], barraDescs[i].texto, &s->font);
	}
}

static void BarraScreen_Layout(void* screen) {
	struct BarraScreen* s = (struct BarraScreen*)screen;
	int i;
	/* coluna no canto de cima a esquerda (o resto da tela de toque fica livre) */
	for (i = 0; i < BARRA_BTNS; i++)
	{
		Widget_SetLocation(&s->btns[i], ANCHOR_MIN, ANCHOR_MIN, 10, 10 + i * 42);
	}
}

static void BarraScreen_Render(void* screen, float delta) {
	if (Gui.InputGrab || !Ativo()) return;
	Screen_Render2Widgets(screen, delta);
}

static int BarraScreen_PointerDown(void* screen, int id, int x, int y) {
	if (Gui.InputGrab || !Ativo()) return false;
	return Screen_DoPointerDown(screen, id, x, y) >= 0;
}

static void BarraScreen_Init(void* screen) {
	struct BarraScreen* s = (struct BarraScreen*)screen;
	int i;
	s->widgets     = barra_widgets;
	s->numWidgets  = BARRA_BTNS;
	for (i = 0; i < BARRA_BTNS; i++)
	{
		ButtonWidget_Init(&s->btns[i], 110, barraDescs[i].click);
		s->btns[i].color = PackedCol_Make(255, 255, 255, 200);
		barra_widgets[i] = (struct Widget*)&s->btns[i];
	}
	s->maxVertices = BARRA_BTNS * BUTTONWIDGET_MAX;
}

static const struct ScreenVTABLE BarraScreen_VTABLE = {
	BarraScreen_Init,        Screen_NullUpdate,     Screen_NullFunc,
	BarraScreen_Render,      Screen_BuildMesh,
	Screen_FInput,           Screen_InputUp,        Screen_FKeyPress, Screen_FText,
	BarraScreen_PointerDown, Screen_PointerUp,      Screen_FPointer,  Screen_FMouseScroll,
	BarraScreen_Layout,      BarraScreen_ContextLost, BarraScreen_ContextRecreated
};

static void Barra_Mostrar(void) {
	struct BarraScreen* s = &BarraScreen;
	if (!(Gui.TouchUI || barraNoPC) || !criaturasLigadas || !Server.IsSinglePlayer) return;
	if (Gui_GetScreen(GUI_PRIORITY_CRIATURAS)) return;
	s->VTABLE = &BarraScreen_VTABLE;
	Gui_Add((struct Screen*)s, GUI_PRIORITY_CRIATURAS);
}

static void Barra_Esconder(void) {
	if (Gui_GetScreen(GUI_PRIORITY_CRIATURAS)) Gui_Remove((struct Screen*)&BarraScreen);
}

/* /client criatura toque 1..5: aperta um botao de toque (para testar no PC) */
static void Barra_Tocar(int i) {
	if (i >= 1 && i <= BARRA_BTNS) barraDescs[i - 1].click(&BarraScreen, &BarraScreen.btns[i - 1]);
}

static void Barra_AlternarPC(void) {
	barraNoPC = !barraNoPC;
	if (barraNoPC) Barra_Mostrar(); else Barra_Esconder();
}


/*########################################################################################################################*
*--------------------------------------------------------Componente-------------------------------------------------------*
*#########################################################################################################################*/
static void Criaturas_Init(void) {
	Random_SeedFromCurrentTime(&rnd);
	Especies_Register();
	Equipe_Carregar();
	Casulos_Carregar();
	Diario_Carregar();
	Mestres_RegistrarTexturas();
	Commands_Register(&EquipeCommand);
	Commands_Register(&CasuloCommand);
	Commands_Register(&MestreCommand);
	Commands_Register(&DiarioCommand);
	Commands_Register(&InicialCommand);
	Commands_Register(&CriaturaCommand);
	ScheduledTask_Add(GAME_DEF_TICKS, Criaturas_TickGlobal);
}

static void Criaturas_Reset(void) { Lancamento_Remover(); Mestre_Remover(); Bau_Remover(); RemoverTodas(); }

static void Criaturas_OnNewMap(void) {
	struct Criatura* minha = Criatura_DoJogador();
	if (minha) Membro_SincronizarDe(minha);
	Lancamento_Remover();
	Mestre_Remover();
	Bau_Remover();
	RemoverTodas();
	Hud_Limpar();
	spawnTimer = 2.0f;
}

static void Criaturas_OnNewMapLoaded(void) {
	criaturasLigadas = Options_GetBool(OPT_CRIATURAS, true);
	bauPendente      = true;
	if (criaturasLigadas) Barra_Mostrar(); else Barra_Esconder();
}

static void Criaturas_Free(void) {
	struct Criatura* minha = Criatura_DoJogador();
	if (minha) { Membro_SincronizarDe(minha); Equipe_Salvar(); }
	Lancamento_Remover();
	Mestre_Remover();
	Bau_Remover();
	RemoverTodas();
}

struct IGameComponent Criaturas_Component = {
	Criaturas_Init,     /* Init  */
	Criaturas_Free,     /* Free  */
	Criaturas_Reset,    /* Reset */
	Criaturas_OnNewMap, /* OnNewMap */
	Criaturas_OnNewMapLoaded /* OnNewMapLoaded */
};

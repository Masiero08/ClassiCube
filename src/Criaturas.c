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
	int id;
};
static struct Especie especies[ESPECIES_COUNT];
static struct ModelVertex plano_vertices[PLANOS_COUNT][MAX_PECAS * MODEL_BOX_VERTICES];
static struct ModelPart   plano_partes[PLANOS_COUNT][MAX_PECAS];

static void Criatura_MakeParts(void) {
	struct Especie* sp = (struct Especie*)Models.Active;
	const struct PlanoDef* plano = &planos_def[especies_def[sp->id].plano];
	struct ModelPart* partes     = plano_partes[especies_def[sp->id].plano];
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
	const struct EspecieDef* def = &especies_def[sp->id];
	struct Stream mem;
	cc_string name;
	if (sp->tex.texID) return;

	Stream_ReadonlyMemory(&mem, (void*)def->png, def->pngLen);
	name = String_FromReadonly(sp->texName);
	Game_UpdateTexture(&sp->tex.texID, &mem, &name, &sp->tex.skinType, NULL);
}

static void Criatura_Draw(struct Entity* e) {
	struct Especie* sp = (struct Especie*)e->Model;
	int idPlano = especies_def[sp->id].plano;
	const struct PlanoDef* plano = &planos_def[idPlano];
	struct ModelPart* partes     = plano_partes[idPlano];
	float pitch = -e->Pitch * MATH_DEG2RAD;
	float asa   = Math_AbsF(e->Anim.LeftArmX) + 0.15f;
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
		case ANIM_ASA_E:
			Model_DrawRotate(0, 0, -asa, part, false); break;
		case ANIM_ASA_D:
			Model_DrawRotate(0, 0,  asa, part, false); break;
		default:
			Model_DrawPart(part); break;
		}
	}

	Model_UnlockVB();
	Gfx_DrawVb_IndexedTris(plano->count * MODEL_BOX_VERTICES);
}

static float Criatura_GetNameY(struct Entity* e) {
	struct Especie* sp = (struct Especie*)e->Model;
	return planos_def[especies_def[sp->id].plano].nameY;
}

static float Criatura_GetEyeY(struct Entity* e) {
	struct Especie* sp = (struct Especie*)e->Model;
	return planos_def[especies_def[sp->id].plano].eyeY;
}

static void Criatura_GetSize(struct Entity* e) {
	struct Especie* sp = (struct Especie*)e->Model;
	const cc_int8* bb  = planos_def[especies_def[sp->id].plano].bb;
	float w = (bb[3] - bb[0]) / 16.0f, h = (bb[4] - bb[1]) / 16.0f;
	/* colisao quadrada, como os outros bichos do jogo */
	Vec3_Set(e->Size, w, h, w);
}

static void Criatura_GetBounds(struct Entity* e) {
	struct Especie* sp = (struct Especie*)e->Model;
	const cc_int8* bb  = planos_def[especies_def[sp->id].plano].bb;
	Vec3_Set(e->ModelAABB.Min, bb[0] / 16.0f, bb[1] / 16.0f, bb[2] / 16.0f);
	Vec3_Set(e->ModelAABB.Max, bb[3] / 16.0f, bb[4] / 16.0f, bb[5] / 16.0f);
}

static void Especies_Register(void) {
	cc_string name;
	int i, j, plano;

	for (i = 0; i < ESPECIES_COUNT; i++)
	{
		struct Especie* sp = &especies[i];
		plano  = especies_def[i].plano;
		sp->id = i;

		/* textura "<nome>.png" (minusculo), pode ser trocada por um texture pack */
		String_InitArray_NT(name, sp->texName);
		String_AppendConst(&name, especies_def[i].nome);
		String_AppendConst(&name, ".png");
		sp->texName[name.length] = '\0';
		for (j = 0; j < name.length; j++) { Char_MakeLower(sp->texName[j]); } /* ex.: faiscol.png */
		sp->tex.name     = sp->texName;
		sp->tex.skinType = SKIN_64x64;
		Model_RegisterTexture(&sp->tex);

		sp->model.name       = especies_def[i].nome;
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
		equipe[equipeCount].vida    = max(1, min(v[2], VidaMax(v[0], equipe[equipeCount].nivel)));
		equipe[equipeCount].xp      = max(0, v[3]);
		equipeCount++;
	}
	if (Convert_ParseInt(&ativo, &v[0]) && v[0] >= 0 && v[0] < equipeCount) membroAtivo = v[0];
}


/*########################################################################################################################*
*--------------------------------------------------------Entidades--------------------------------------------------------*
*#########################################################################################################################*/
#define CRIATURAS_MAX     24
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
/* vento > planta, pedra > fogo/vento, agua/planta > pedra */
static float Multiplicador(int atk, int def) {
	switch (atk)
	{
	case TIPO_FOGO:     if (def == TIPO_PLANTA) return 1.5f; if (def == TIPO_AGUA || def == TIPO_PEDRA) return 0.6f; break;
	case TIPO_AGUA:     if (def == TIPO_FOGO || def == TIPO_PEDRA) return 1.5f; if (def == TIPO_PLANTA) return 0.6f; break;
	case TIPO_PLANTA:   if (def == TIPO_AGUA || def == TIPO_PEDRA) return 1.5f; if (def == TIPO_FOGO) return 0.6f; break;
	case TIPO_ELETRICO: if (def == TIPO_AGUA || def == TIPO_VENTO) return 1.5f; if (def == TIPO_PEDRA) return 0.6f; break;
	case TIPO_VENTO:    if (def == TIPO_PLANTA) return 1.5f; if (def == TIPO_PEDRA || def == TIPO_ELETRICO) return 0.6f; break;
	case TIPO_PEDRA:    if (def == TIPO_FOGO || def == TIPO_VENTO) return 1.5f; break;
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
		}
	}
	c->nomeSujo = true;
	Membro_SincronizarDe(c);
	Equipe_Salvar();
}

static void Criatura_Derrotada(struct Criatura* c) {
	struct Criatura* minha = Criatura_DoJogador();
	cc_string msg; char msgBuffer[STRING_SIZE];
	const struct EspecieDef* def = &especies_def[c->especie];

	String_InitArray(msg, msgBuffer);
	String_Format2(&msg, "%c%c &7selvagem foi derrotado.", def->cor, def->nome);
	Chat_Add(&msg);

	if (minha && c->ajudou) Criatura_GanharXp(minha, 3 + c->nivel * 2);
	Criatura_Remover(c);
}

static void Criatura_Dano(struct Criatura* c, int dano, Vec3 origem) {
	Vec3 dir;
	float len;
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
		c->fugaTimer = 3.0f;
		if (c->vida <= 0) Criatura_Derrotada(c);
	} else {
		if (c->vida < 1) c->vida = 1;
		Membro_SincronizarDe(c);
	}
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
	e->next.yaw  = Math_Atan2f(dx, -dz) * MATH_RAD2DEG;
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
	float ang;

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
	int dano;

	if (alvo && (!alvo->usada || Dist2(alvo->base.next.pos, jogador->Position) > 20 * 20)) {
		c->alvo = -1; alvo = NULL;
	}

	if (alvo) {
		/* corre ate o alvo e ataca */
		d2 = Dist2XZ(pos, alvo->base.next.pos);
		if (d2 > 1.6f * 1.6f) {
			Criatura_Andar(c, alvo->base.next.pos, def->vel * 1.1f, delta);
		} else if (c->atkTimer <= 0) {
			c->atkTimer = 1.0f;
			dano = (int)((def->dano + c->nivel / 2) * Multiplicador(def->tipo, especies_def[alvo->especie].tipo));
			alvo->ajudou = true;
			Criatura_Dano(alvo, dano, pos);
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

	if (c->selvagem) {
		if (Dist2XZ(e->next.pos, jogador->Position) > DIST_SUMIR) { Criatura_Remover(c); return; }
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
	if (Blocks.IsLiquid[chao])                       habitat = HABITAT_AGUA;
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
	if (idx < 0 || idx >= equipeCount) { membroAtivo = -1; Equipe_Salvar(); return; }

	pos = jogador->Position; pos.x += 1.0f;
	c = Criatura_Criar(equipe[idx].especie, equipe[idx].nivel, pos, false);
	if (c) { c->vida = equipe[idx].vida; c->nomeSujo = true; }
	Equipe_Salvar();
}

static float capturaTimer;
static void Criaturas_TickGlobal(struct ScheduledTask* task) {
	capturaTimer -= (float)task->interval;
	if (!Ativo()) return;

	/* traz a criatura ativa de volta depois de carregar o mundo */
	if (membroAtivo >= 0 && !Criatura_DoJogador()) SoltarMembro(membroAtivo);
	Spawner_Tick((float)task->interval);
}


/*########################################################################################################################*
*---------------------------------------------------------Clique----------------------------------------------------------*
*#########################################################################################################################*/
static struct Criatura* CriaturaNaMira(void) {
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
		if (!criaturas[i].usada) continue;
		if (!Intersection_RayIntersectsRotatedBox(eye, dir, &criaturas[i].base, &t0, &t1)) continue;
		if (t0 < 0 || t0 > melhorT) continue;
		melhorT = t0; melhor = &criaturas[i];
	}
	return melhor;
}

static void TentarCapturar(struct Criatura* c) {
	const struct EspecieDef* def = &especies_def[c->especie];
	cc_string msg; char msgBuffer[STRING_SIZE];
	float chance;
	int vidaMax = VidaMax(c->especie, c->nivel);
	String_InitArray(msg, msgBuffer);

	if (capturaTimer > 0) return;
	capturaTimer = 1.0f;

	if (equipeCount >= EQUIPE_MAX) {
		Chat_AddRaw("&cSua equipe esta cheia (6). Use &f/client equipe &cpara ver.");
		return;
	}

	chance = 1.0f - (float)c->vida / vidaMax;
	if (chance < 0.15f) chance = 0.15f;

	if (Random_Float(&rnd) < chance) {
		equipe[equipeCount].especie = c->especie;
		equipe[equipeCount].nivel   = c->nivel;
		equipe[equipeCount].vida    = max(1, c->vida);
		equipe[equipeCount].xp      = 0;
		equipeCount++;

		String_Format2(&msg, "&aVoce capturou %c%c&a!", def->cor, def->nome);
		Chat_Add(&msg);
		Criatura_Remover(c);
		if (!Criatura_DoJogador()) SoltarMembro(equipeCount - 1);
		else Equipe_Salvar();
	} else {
		String_Format2(&msg, "%c%c &eescapou do casulo! Enfraqueca ele antes.", def->cor, def->nome);
		Chat_Add(&msg);
		c->fugaTimer = 2.0f;
	}
}

cc_bool Criaturas_HandleClick(cc_bool place) {
	struct Entity* jogador;
	struct Criatura* c;
	struct Criatura* minha;
	if (!Ativo()) return false;

	c = CriaturaNaMira();
	if (!c) return false;
	jogador = &Entities.CurPlayer->Base;

	if (!c->selvagem) {
		/* colocar na sua criatura = dar uma fruta-cura */
		if (place && c->curaTimer <= 0) {
			c->vida = VidaMax(c->especie, c->nivel);
			c->curaTimer = 20.0f;
			c->nomeSujo  = true;
			Membro_SincronizarDe(c);
			Equipe_Salvar();
			Chat_AddRaw("&aSua criatura comeu uma fruta-cura e recuperou a vida!");
		} else if (place) {
			Chat_AddRaw("&eA fruta-cura ainda esta crescendo, espere um pouco.");
		}
		return true;
	}

	if (place) {
		TentarCapturar(c);
	} else {
		/* atacar: sua criatura entra na briga */
		c->ajudou = c->ajudou || Criatura_DoJogador() != NULL;
		Criatura_Dano(c, 2, jogador->Position);
		minha = Criatura_DoJogador();
		if (minha && c->usada) minha->alvo = Criatura_Index(c);
	}
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

	equipe[0].especie = escolha;
	equipe[0].nivel   = 5;
	equipe[0].vida    = VidaMax(escolha, 5);
	equipe[0].xp      = 0;
	equipeCount = 1;
	SoltarMembro(0);
	Chat_Add2("&aVoce escolheu %c%c&a! Cuide bem dele.", especies_def[escolha].cor, especies_def[escolha].nome);
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
		dist = total > 1 ? (i < 8 ? 6.0f : 10.0f) : 3.0f;
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


/*########################################################################################################################*
*--------------------------------------------------------Componente-------------------------------------------------------*
*#########################################################################################################################*/
static void Criaturas_Init(void) {
	Random_SeedFromCurrentTime(&rnd);
	Especies_Register();
	Equipe_Carregar();
	Commands_Register(&EquipeCommand);
	Commands_Register(&InicialCommand);
	Commands_Register(&CriaturaCommand);
	ScheduledTask_Add(GAME_DEF_TICKS, Criaturas_TickGlobal);
}

static void Criaturas_Reset(void) { RemoverTodas(); }

static void Criaturas_OnNewMap(void) {
	struct Criatura* minha = Criatura_DoJogador();
	if (minha) Membro_SincronizarDe(minha);
	RemoverTodas();
	spawnTimer = 2.0f;
}

static void Criaturas_OnNewMapLoaded(void) {
	criaturasLigadas = Options_GetBool(OPT_CRIATURAS, true);
}

static void Criaturas_Free(void) {
	struct Criatura* minha = Criatura_DoJogador();
	if (minha) { Membro_SincronizarDe(minha); Equipe_Salvar(); }
	RemoverTodas();
}

struct IGameComponent Criaturas_Component = {
	Criaturas_Init,     /* Init  */
	Criaturas_Free,     /* Free  */
	Criaturas_Reset,    /* Reset */
	Criaturas_OnNewMap, /* OnNewMap */
	Criaturas_OnNewMapLoaded /* OnNewMapLoaded */
};

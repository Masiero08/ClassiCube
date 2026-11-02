#include "Sons.h"
#include "Audio.h"
#include "Entity.h"
#include "Model.h"
#include "Game.h"
#include "ExtMath.h"
#include "Funcs.h"
#include "String_.h"
#include "World.h"
#include "Server.h"
#include "Survival.h"
#include "DayNight.h"
#include "Criaturas.h"
#include "Minerios.h"
#include "LateGame.h"

/* Sons proprios: efeitos posicionados, vozes ambientes das criaturas/monstros
   e o clima da musica. Tudo cai em silencio se audio/efeitos.zip nao existir.
*/
#define SONS_ALCANCE 22.0f /* depois disso nao se ouve mais nada */
static RNGState sons_rnd;

static int Sons_Variar(int tom) {
	/* cada vez um pouquinho diferente, para nao soar repetido */
	return tom * (94 + Random_Next(&sons_rnd, 13)) / 100;
}

void Sons_Tocar(const char* nome) {
	Audio_PlayEffect(nome, 100, Sons_Variar(100));
}

static int Sons_VolumeEm(Vec3 pos) {
	struct Entity* p;
	float dx, dy, dz, d, k;
	if (!Entities.CurPlayer) return 100;
	p  = &Entities.CurPlayer->Base;
	dx = pos.x - p->Position.x; dy = pos.y - p->Position.y; dz = pos.z - p->Position.z;
	d  = Math_SqrtF(dx * dx + dy * dy + dz * dz);
	if (d >= SONS_ALCANCE) return 0;
	k = 1.0f - d / SONS_ALCANCE;
	return (int)(100 * k * k + 0.5f);
}

void Sons_TocarTom(const char* nome, Vec3 pos, int tom) {
	int vol = Sons_VolumeEm(pos);
	if (vol <= 2) return;
	Audio_PlayEffect(nome, vol, Sons_Variar(tom));
}

void Sons_TocarEm(const char* nome, Vec3 pos) { Sons_TocarTom(nome, pos, 100); }


/*########################################################################################################################*
*----------------------------------------------------------Vozes----------------------------------------------------------*
*#########################################################################################################################*/
static const char* const vozCriatura[] = {
	"cri_fogo", "cri_agua", "cri_planta", "cri_eletrico", "cri_vento",
	"cri_pedra", "cri_inseto", "cri_fantasma", "cri_gelo"
};

static const struct VozModelo {
	const char* modelo; const char* voz;
	short tom;       /* tom base */
	short intervalo; /* segundos (em media) entre uma voz e outra; 0 = so quando apanha */
} vozes[] = {
	{ "zombie",           "zumbi",     100,  7 },
	{ "skeleton",         "esqueleto", 100,  8 },
	{ "spider",           "aranha",    100,  7 },
	{ "creeper",          "golpe",      90,  0 }, /* creeper e silencioso ate o pavio */
	{ "pig",              "porco",     100,  9 },
	{ "sheep",            "ovelha",    100, 10 },
	{ "sheep_nofur",      "ovelha",    100, 10 },
	{ "chicken",          "galinha",   100,  8 },
	{ "mosquito",         "mosquito",  110,  2 },
	{ "rainha_mosquito",  "mosquito",   70,  2 },
	{ "golem_titanio",    "golem",     100,  4 },
	{ "aranha_cristal",   "aranha",     70,  5 },
	{ "rei_creeper",      "chefao",    110,  9 },
	{ "dragao_esmeralda", "dragao",    110, 10 },
	{ "dragao_fogo",      "dragao",     95, 10 },
	{ "dragao_gelo",      "dragao",    120, 10 },
	{ "dragao_fim",       "chefao",     75,  8 },
};

/* Acha a voz pelo nome do modelo; tom e intervalo saem em *tom e *intervalo */
static const char* Voz_DoModelo(struct Entity* e, int* tom, int* intervalo) {
	const char* nome;
	cc_string str;
	int i, tipo;
	if (!e->Model || !e->Model->name) return NULL;
	nome = e->Model->name;
	str  = String_FromReadonly(nome);

	for (i = 0; i < Array_Elems(vozes); i++)
	{
		if (!String_CaselessEqualsConst(&str, vozes[i].modelo)) continue;
		*tom = vozes[i].tom; *intervalo = vozes[i].intervalo;
		return vozes[i].voz;
	}
	tipo = Criaturas_TipoDoModelo(nome);
	if (tipo < 0 || tipo >= Array_Elems(vozCriatura)) return NULL;
	*tom = 100; *intervalo = 12;
	return vozCriatura[tipo];
}

/* Criaturas e monstros maiores tem voz mais grossa */
static int Voz_TomDoTamanho(struct Entity* e, int tom) {
	float s = e->ModelScale.y;
	if (s <= 0.05f) s = 1.0f;
	tom = (int)(tom / Math_SqrtF(s));
	return tom < 55 ? 55 : (tom > 170 ? 170 : tom);
}

void Sons_Voz(struct Entity* e, int tom) {
	int base, intervalo;
	const char* voz;
	if (!e) return;
	voz = Voz_DoModelo(e, &base, &intervalo);
	if (!voz) return;
	Sons_TocarTom(voz, e->Position, Voz_TomDoTamanho(e, base * tom / 100));
}

/* De vez em quando alguem perto do jogador faz barulho */
static void Vozes_Tick(float delta) {
	struct Entity* jogador = &Entities.CurPlayer->Base;
	struct Entity* e;
	const char* voz;
	int i, tom, intervalo, vol;
	float dx, dz, chance;

	for (i = 0; i < ENTITIES_SELF_ID; i++)
	{
		e = Entities.List[i];
		if (!e || e == jogador) continue;
		dx = e->Position.x - jogador->Position.x; dz = e->Position.z - jogador->Position.z;
		if (dx * dx + dz * dz > SONS_ALCANCE * SONS_ALCANCE) continue;

		voz = Voz_DoModelo(e, &tom, &intervalo);
		if (!voz || !intervalo) continue;
		chance = delta / intervalo;
		if (Random_Float(&sons_rnd) >= chance) continue;

		vol = Sons_VolumeEm(e->Position);
		if (vol <= 2) continue;
		/* vozes ambientes um pouco mais baixas que as de acao */
		Audio_PlayEffect(voz, vol * 3 / 4, Sons_Variar(Voz_TomDoTamanho(e, tom)));
		return; /* uma por vez */
	}
}


/*########################################################################################################################*
*---------------------------------------------------------Musica----------------------------------------------------------*
*#########################################################################################################################*/
static void Musica_Atualizar(void) {
	int clima = MUSIC_MOOD_DAY, batalha;

	if (World.Loaded && Server.IsSinglePlayer) {
		if (DayNight_GetMode() == DAYNIGHT_NETHER)   clima = MUSIC_MOOD_NETHER;
		else if (DayNight_GetMode() == DAYNIGHT_END) clima = MUSIC_MOOD_END;
		else if (Survival_IsActive() && DayNight_IsNight()) clima = MUSIC_MOOD_NIGHT;

		batalha = Criaturas_Batalha();
		if (batalha == 1) clima = MUSIC_MOOD_BATTLE;
		if (batalha == 2 || Minerios_ChefePerto() || LateGame_DragaoVivo()) clima = MUSIC_MOOD_BOSS;
	}
	Audio_SetMusicMood(clima);
}


/*########################################################################################################################*
*--------------------------------------------------------Componente-------------------------------------------------------*
*#########################################################################################################################*/
static float musicaTimer;
static cc_bool Sons_Tick(struct ScheduledTask2* task) {
	float delta = task->interval;
	if (!Entities.CurPlayer || !World.Loaded) return true;

	musicaTimer += delta;
	if (musicaTimer >= 0.5f) { musicaTimer = 0; Musica_Atualizar(); }
	if (Audio_SoundsVolume) Vozes_Tick(delta);
	return true;
}
static struct ScheduledTask2 sons_task = { 0, 1.0f / 10, Sons_Tick };

static void OnInit(void) {
	Random_SeedFromCurrentTime(&sons_rnd);
	ScheduledTask2_Add(&sons_task);
}

static void OnNewMap(void) { Audio_SetMusicMood(MUSIC_MOOD_DAY); }

struct IGameComponent Sons_Component = {
	OnInit,   /* Init  */
	NULL,     /* Free  */
	NULL,     /* Reset */
	OnNewMap, /* OnNewMap */
	NULL      /* OnNewMapLoaded */
};

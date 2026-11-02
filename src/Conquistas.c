#include "Conquistas.h"
#include "Survival.h"
#include "Equipamento.h"
#include "Minerios.h"
#include "LateGame.h"
#include "DayNight.h"
#include "Criaturas.h"
#include "Block.h"
#include "BlockID.h"
#include "Chat.h"
#include "Commands.h"
#include "Drawer2D.h"
#include "Event.h"
#include "Formats.h"
#include "Funcs.h"
#include "Game.h"
#include "Graphics.h"
#include "Gui.h"
#include "Menus.h"
#include "Options.h"
#include "Platform.h"
#include "Server.h"
#include "Stream.h"
#include "String_.h"
#include "Utils.h"
#include "Widgets.h"
#include "World.h"
#include "Deflate.h"
#include "Window.h"
#include "Screens.h"
#include "Errors.h"
#include "Logger.h"
#include "IsometricDrawer.h"
#include "Paisagem.h"

/* Conquistas originais em portugues, inspiradas nas do Minecraft: cada uma tem um nome, */
/*  o que fazer e e ganha uma vez so. Elas usam o que os outros mods ja guardam */
/*  (itens da sobrevivencia, armadura, dimensoes, criaturas) e alguns avisos (Conquistas_Evento). */


/*########################################################################################################################*
*------------------------------------------------------Lista de conquistas------------------------------------------------*
*#########################################################################################################################*/
enum Conquista {
	CQ_MADEIRA, CQ_FERRAMENTA, CQ_PEDRA, CQ_FERRO, CQ_DIAMANTE, CQ_BAU, CQ_COZINHAR,
	CQ_ARMADURA, CQ_ARMADURA_TODA, CQ_DORMIR, CQ_NOITE, CQ_MONSTRO, CQ_MONSTROS_50,
	CQ_CONSTRUTOR, CQ_MINERADOR, CQ_SALVAR, CQ_NETHER, CQ_FIM, CQ_ENDER, CQ_OUTROS_MUNDOS,
	CQ_CHEFAO, CQ_DOMAR, CQ_SORTE, CQ_CAPTURA, CQ_COLECAO, CQ_EVOLUCAO,
	CQ_INSIGNIA, CQ_INSIGNIAS, CQ_CAMPEAO, CQ_LENDARIA,
	CQ_COUNT
};

static const struct ConquistaDef { const char* nome; const char* desc; BlockID icone; } conquistas_def[CQ_COUNT] = {
	{ "Lenhador",           "Pegue sua primeira madeira de uma arvore", BLOCK_LOG },
	{ "Maos a obra",        "Faca sua primeira ferramenta no crafting", BLOCK_WOOD },
	{ "Idade da pedra",     "Faca uma picareta de pedra", BLOCK_COBBLE },
	{ "Idade do ferro",     "Derreta minerio e tenha uma barra de ferro", EITEM_BARRA_FERRO },
	{ "Brilho raro",        "Encontre um diamante bem fundo no subsolo", LITEM_DIAMOND },
	{ "Tudo no lugar",      "Coloque um bau no mundo", EBLOCK_BAU },
	{ "Cozinheiro",         "Asse carne na fornalha e tenha um bife", LITEM_COOKED_MEAT },
	{ "Protegido",          "Vista uma peca de armadura", EITEM_PEITORAL_FERRO },
	{ "Cavaleiro",          "Vista as 4 pecas de armadura ao mesmo tempo", EITEM_PEITORAL_DIAMANTE },
	{ "Bom dia!",           "Durma numa cama durante a noite", LBLOCK_BED },
	{ "Sobrevivente",       "Passe uma noite inteira acordado sem morrer", LBLOCK_GLOWSTONE },
	{ "Primeira luta",      "Derrote um monstro", BLOCK_TNT },
	{ "Guardiao da noite",  "Derrote 50 monstros", BLOCK_OBSIDIAN },
	{ "Construtor",         "Coloque 100 blocos", BLOCK_BRICK },
	{ "Minerador",          "Quebre 500 blocos", BLOCK_IRON_ORE },
	{ "Precavido",          "Salve o mundo pelo menu de pausa", BLOCK_BOOKSHELF },
	{ "Mais quente",        "Entre no Nether por um portal", LBLOCK_NETHERRACK },
	{ "O Fim?",             "Chegue ao Fim", LBLOCK_END_STONE },
	{ "Libertacao",         "Derrote o Ender Dragon", LBLOCK_DRAGON_EGG },
	{ "Outros mundos",      "Visite a Utopia ou a Dimensao de Cristal", MBLOCK_CRISTAL },
	{ "Matador de chefoes", "Derrote um chefao do Minerios & Monstros", MBLOCK_ALTAR_RAINHA },
	{ "Domador",            "Dome um dragao", MBLOCK_OVO_DRAGAO },
	{ "Sortudo",            "Quebre um Bloco da Sorte", MBLOCK_SORTE },
	{ "Primeira captura",   "Capture uma criatura com um casulo", BLOCK_GOLD },
	{ "Colecionador",       "Capture 10 especies de criaturas", BLOCK_BOOKSHELF },
	{ "Que isso?!",         "Faca uma criatura evoluir", LBLOCK_GLOWSTONE },
	{ "Primeira insignia",  "Venca um Mestre de Bioma", PBLOCK_EMBLEMA_PLANTA },
	{ "Rumo a Liga",        "Ganhe as 6 insignias dos ginasios", PBLOCK_EMBLEMA_FOGO },
	{ "Campeao",            "Venca a Liga dos Biomas", BLOCK_GOLD },
	{ "Lenda viva",         "Capture uma criatura lendaria", LBLOCK_DIAMOND },
};

#define OPT_CONQUISTAS "conquistas"
#define OPT_QUEBRADOS  "conquistas-quebrados"
#define OPT_COLOCADOS  "conquistas-colocados"
#define OPT_MONSTROS   "conquistas-monstros"

static int ganhas; /* um bit por conquista */
static int quebrados, colocados, monstros;
static cc_bool contagemMudou;

static int ContarGanhas(void) {
	int i, total = 0;
	for (i = 0; i < CQ_COUNT; i++) { if (ganhas & (1 << i)) total++; }
	return total;
}

static void SalvarContagem(void) {
	if (!contagemMudou) return;
	Options_SetInt(OPT_QUEBRADOS, quebrados);
	Options_SetInt(OPT_COLOCADOS, colocados);
	Options_SetInt(OPT_MONSTROS,  monstros);
	contagemMudou = false;
}

static void Toast_Mostrar(int conquista);
static void Conquistas_EventoNoite(int evento);

static void Ganhar(int conquista) {
	const struct ConquistaDef* def;
	int total;
	if (conquista < 0 || conquista >= CQ_COUNT || (ganhas & (1 << conquista))) return;

	ganhas |= 1 << conquista;
	def     = &conquistas_def[conquista];
	total   = ContarGanhas();
	Chat_Add2("&6Conquista obtida: &e%c &7- %c", def->nome, def->desc);
	Chat_Add1("&7(%i de 30 conquistas. Veja todas em Pausa > Conquistas)", &total);

	Options_SetInt(OPT_CONQUISTAS, ganhas);
	SalvarContagem();
	/* grava ja, assim nada se perde se o 3DS for desligado no botao HOME */
	Options_SaveIfChanged();
	Toast_Mostrar(conquista);
}

void Conquistas_Evento(int evento) {
	switch (evento) {
	case CONQ_EV_MONSTRO:
	case CONQ_EV_CHEFAO:
		monstros++; contagemMudou = true;
		Ganhar(CQ_MONSTRO);
		if (monstros >= 50) Ganhar(CQ_MONSTROS_50);
		if (evento == CONQ_EV_CHEFAO) Ganhar(CQ_CHEFAO);
		break;
	case CONQ_EV_ENDER:    Ganhar(CQ_ENDER);    break;
	case CONQ_EV_DOMAR:    Ganhar(CQ_DOMAR);    break;
	case CONQ_EV_CAPTURA:  Ganhar(CQ_CAPTURA);  break;
	case CONQ_EV_LENDARIA: Ganhar(CQ_CAPTURA); Ganhar(CQ_LENDARIA); break;
	case CONQ_EV_EVOLUCAO: Ganhar(CQ_EVOLUCAO); break;
	case CONQ_EV_SALVAR:   Ganhar(CQ_SALVAR);   break;
	case CONQ_EV_DORMIR:   Ganhar(CQ_DORMIR);   break;
	}
	Conquistas_EventoNoite(evento);
}


/*########################################################################################################################*
*------------------------------------------------------Verificacao--------------------------------------------------------*
*#########################################################################################################################*/
/* "Sobrevivente": a noite tem que comecar e acabar sem o jogador morrer ou dormir */
static cc_bool estavaNoite, noiteLimpa;

static void Conquistas_EventoNoite(int evento) {
	if (evento == CONQ_EV_MORTE || evento == CONQ_EV_DORMIR) noiteLimpa = false;
}

static void VerificarNoite(void) {
	cc_bool noite = Survival_IsActive() && DayNight_GetMode() == DAYNIGHT_NORMAL && DayNight_IsNight();

	if (noite && !estavaNoite) noiteLimpa = true;
	if (!noite && estavaNoite && noiteLimpa && Survival_IsActive()) Ganhar(CQ_NOITE);
	estavaNoite = noite;
}

static void VerificarItens(void) {
	int tier, uses, maxUses, k, pecas = 0;
	if (!Survival_IsActive()) return;

	if (Survival_GetCount(BLOCK_LOG) > 0)          Ganhar(CQ_MADEIRA);
	if (Survival_GetCount(EITEM_BARRA_FERRO) > 0)  Ganhar(CQ_FERRO);
	if (Survival_GetCount(LITEM_DIAMOND) > 0)      Ganhar(CQ_DIAMANTE);
	if (Survival_GetCount(LITEM_COOKED_MEAT) > 0)  Ganhar(CQ_COZINHAR);

	/* ferramentas: 0 = picareta (ToolKind em Survival.c), nivel 1 madeira, 2 pedra... */
	for (k = 0; k < 4; k++)
	{
		Survival_ToolInfo(k, &tier, &uses, &maxUses);
		if (tier > 0) Ganhar(CQ_FERRAMENTA);
		if (k == 0 && tier >= 2) Ganhar(CQ_PEDRA);
	}

	for (k = 0; k < ARMOR_PIECES; k++)
	{
		if (Equip_Worn(k) != BLOCK_AIR) pecas++;
	}
	if (pecas > 0)            Ganhar(CQ_ARMADURA);
	if (pecas == ARMOR_PIECES) Ganhar(CQ_ARMADURA_TODA);
}

static void VerificarMundo(void) {
	int dim = Minerios_DimensaoAtual();
	int especies, insignias;

	if (dim == MINERIOS_DIM_NETHER) Ganhar(CQ_NETHER);
	if (dim == MINERIOS_DIM_FIM)    Ganhar(CQ_FIM);
	if (dim == MINERIOS_DIM_UTOPIA || dim == MINERIOS_DIM_CRISTAL) Ganhar(CQ_OUTROS_MUNDOS);

	if (!Criaturas_Ativo()) return;
	Criaturas_Progresso(&especies, &insignias);
	if (especies >= 10)        Ganhar(CQ_COLECAO);
	if (insignias & 0x3F)      Ganhar(CQ_INSIGNIA);
	if ((insignias & 0x3F) == 0x3F) Ganhar(CQ_INSIGNIAS);
	if (insignias & (1 << 10)) Ganhar(CQ_CAMPEAO); /* a Campea Aurora e o ultimo Mestre */
}

static void OnBlockChanged(void* obj, IVec3 coords, BlockID old, BlockID now) {
	if (!Server.IsSinglePlayer) return;

	if (now == BLOCK_AIR && old != BLOCK_AIR) {
		quebrados++; contagemMudou = true;
		if (quebrados >= 500) Ganhar(CQ_MINERADOR);
		if (old == MBLOCK_SORTE) Ganhar(CQ_SORTE);
	} else if (now != BLOCK_AIR) {
		colocados++; contagemMudou = true;
		if (colocados >= 100) Ganhar(CQ_CONSTRUTOR);
		if (now == EBLOCK_BAU) Ganhar(CQ_BAU);
	}
}

static float salvarContagemTimer;
static cc_bool Conquistas_Tick(struct ScheduledTask2* task) {
	if (!World.Loaded || !Server.IsSinglePlayer) return true;
	VerificarItens();
	VerificarNoite();
	VerificarMundo();

	salvarContagemTimer += task->interval;
	if (salvarContagemTimer >= 30.0f) { salvarContagemTimer = 0; SalvarContagem(); }
	return true;
}
static struct ScheduledTask2 conquistas_task = { 0, 0.5f, Conquistas_Tick };


/*########################################################################################################################*
*--------------------------------------------------------Lista------------------------------------------------------------*
*#########################################################################################################################*/
static char listaTitulo[64];

static void Lista_Carregar(struct StringsBuffer* entries) {
	cc_string str; char strBuffer[STRING_SIZE];
	int i;
	for (i = 0; i < CQ_COUNT; i++)
	{
		String_InitArray(str, strBuffer);
		if (ganhas & (1 << i)) String_Format1(&str, "&e* %c", conquistas_def[i].nome);
		else                   String_Format1(&str, "&7%c", conquistas_def[i].nome);
		StringsBuffer_Add(entries, &str);
	}
}

static void Lista_Clique(int i) {
	if (i < 0 || i >= CQ_COUNT) return;
	if (ganhas & (1 << i)) {
		Chat_Add2("&e%c &a(ganha!) &7- %c", conquistas_def[i].nome, conquistas_def[i].desc);
	} else {
		Chat_Add2("&f%c &7- %c", conquistas_def[i].nome, conquistas_def[i].desc);
	}
	if (i == CQ_MONSTROS_50) Chat_Add1("&7  Monstros derrotados: %i de 50", &monstros);
	if (i == CQ_CONSTRUTOR)  Chat_Add1("&7  Blocos colocados: %i de 100", &colocados);
	if (i == CQ_MINERADOR)   Chat_Add1("&7  Blocos quebrados: %i de 500", &quebrados);
}

void Conquistas_Mostrar(void) {
	cc_string str;
	int total = ContarGanhas(), max = CQ_COUNT;

	String_InitArray_NT(str, listaTitulo);
	String_Format2(&str, "Conquistas: %i de %i", &total, &max);
	listaTitulo[str.length] = '\0';
	ModListScreen_Show(listaTitulo, "Fechar", Lista_Carregar, Lista_Clique);
	Chat_AddRaw("&7Toque numa conquista para ver o que precisa fazer. &e* &7= ganha");
}


/*########################################################################################################################*
*------------------------------------------------Aviso de conquista (toast)-----------------------------------------------*
*#########################################################################################################################*/
/* Uma faixa no alto da tela de cima, como no Minecraft, que some depois de alguns segundos */
#define GUI_PRIORITY_CONQUISTA 14 /* entre o chat (15) e os textos especiais (13) */
#define TOAST_TEMPO 4.5f
#define TOAST_FILA  8

static struct ToastScreen {
	Screen_Body
	struct TextWidget titulo, nome;
	struct FontDesc fontTitulo, fontNome;
	float timer;
	int x, y, width, height;
	GfxResourceID iconVb;
	int iconState[ISOMETRICDRAWER_MAXVERTICES / 4], iconCount;
} ToastScreen;
static struct Widget* toast_widgets[2];
static int toastFila[TOAST_FILA], toastCount;
static int toastAtual = -1;

static void ToastScreen_SetTexts(struct ToastScreen* s) {
	if (toastAtual < 0) return;
	TextWidget_SetConst(&s->titulo, "&eConquista obtida!", &s->fontTitulo);
	TextWidget_SetConst(&s->nome,   conquistas_def[toastAtual].nome, &s->fontNome);
}

static void ToastScreen_Layout(void* screen) {
	struct ToastScreen* s = (struct ToastScreen*)screen;
	int pad = Display_ScaleY(5), icone;

	s->height = s->titulo.height + s->nome.height + pad * 2;
	icone     = s->height - pad * 2;
	s->width  = max(s->titulo.width, s->nome.width) + icone + pad * 4;
	s->width  = max(s->width, Display_ScaleX(170));
	s->x      = (Window_Main.Width - s->width) / 2;
	s->y      = Display_ScaleY(4);

	/* ANCHOR_MIN com o deslocamento em pixels da tela de cima */
	s->titulo.horAnchor = ANCHOR_MIN; s->titulo.verAnchor = ANCHOR_MIN;
	s->nome.horAnchor   = ANCHOR_MIN; s->nome.verAnchor   = ANCHOR_MIN;
	s->titulo.xOffset = s->x + icone + pad * 3; s->titulo.yOffset = s->y + pad;
	s->nome.xOffset   = s->titulo.xOffset;      s->nome.yOffset   = s->titulo.yOffset + s->titulo.height;
	Widget_Layout(&s->titulo);
	Widget_Layout(&s->nome);
	s->dirty = true;
}

static void ToastScreen_ContextLost(void* screen) {
	struct ToastScreen* s = (struct ToastScreen*)screen;
	Font_Free(&s->fontTitulo);
	Font_Free(&s->fontNome);
	Gfx_DeleteDynamicVb(&s->iconVb);
	Screen_ContextLost(screen);
}

static void ToastScreen_ContextRecreated(void* screen) {
	struct ToastScreen* s = (struct ToastScreen*)screen;
	Screen_UpdateVb(screen);
	s->iconVb = Gfx_CreateDynamicVb(VERTEX_FORMAT_TEXTURED, ISOMETRICDRAWER_MAXVERTICES);
	Font_Make(&s->fontTitulo, 14, FONT_FLAGS_PADDING);
	Font_Make(&s->fontNome,   16, FONT_FLAGS_BOLD | FONT_FLAGS_PADDING);
	ToastScreen_SetTexts(s);
	ToastScreen_Layout(s);
}

/* o icone e o bloco/item da conquista, desenhado como na barra de itens */
static void ToastScreen_BuildMesh(void* screen) {
	struct ToastScreen* s = (struct ToastScreen*)screen;
	struct VertexTextured* data;
	int pad = Display_ScaleY(5), icone = s->height - pad * 2;
	Screen_BuildMesh(screen);

	s->iconCount = 0;
	if (!s->iconVb || toastAtual < 0) return;
	data = (struct VertexTextured*)Gfx_LockDynamicVb(s->iconVb, VERTEX_FORMAT_TEXTURED, ISOMETRICDRAWER_MAXVERTICES);
	IsometricDrawer_BeginBatch(data, s->iconState);
	IsometricDrawer_AddBatch(conquistas_def[toastAtual].icone, icone * 0.42f,
		s->x + pad + icone / 2.0f, s->y + pad + icone / 2.0f);
	s->iconCount = IsometricDrawer_EndBatch();
	Gfx_UnlockDynamicVb(s->iconVb);
}

static void ToastScreen_Render(void* screen, float delta) {
	struct ToastScreen* s = (struct ToastScreen*)screen;
	int pad = Display_ScaleY(5), b = Display_ScaleY(2), icone = s->height - pad * 2;
	if (Game_HideGui || toastAtual < 0) return;

	Gfx_3DS_SetRenderScreen(TOP_SCREEN);
	/* moldura dourada, fundo escuro e um "trofeu" quadrado a esquerda */
	Gfx_Draw2DFlat(s->x - b, s->y - b, s->width + b * 2, s->height + b * 2, PackedCol_Make(200, 160, 40, 230));
	Gfx_Draw2DGradient(s->x, s->y, s->width, s->height,
		PackedCol_Make(40, 30, 50, 235), PackedCol_Make(20, 15, 25, 235));
	Gfx_Draw2DFlat(s->x + pad, s->y + pad, icone, icone, PackedCol_Make(70, 55, 85, 255));

	if (s->iconCount) {
		Gfx_SetVertexFormat(VERTEX_FORMAT_TEXTURED);
		Gfx_BindDynamicVb(s->iconVb);
		IsometricDrawer_Render(s->iconCount, 0, s->iconState);
	}
	Screen_Render2Widgets(screen, delta);
	Gfx_3DS_SetRenderScreen(BOTTOM_SCREEN);
}

static void ToastScreen_Update(void* screen, float delta) {
	struct ToastScreen* s = (struct ToastScreen*)screen;
	int i;
	s->timer -= delta;
	if (s->timer > 0) return;

	if (!toastCount) { toastAtual = -1; Gui_Remove((struct Screen*)s); return; }
	toastAtual = toastFila[0];
	for (i = 1; i < toastCount; i++) toastFila[i - 1] = toastFila[i];
	toastCount--;

	s->timer = TOAST_TEMPO;
	ToastScreen_SetTexts(s);
	ToastScreen_Layout(s);
}

static void ToastScreen_Init(void* screen) {
	struct ToastScreen* s = (struct ToastScreen*)screen;
	s->widgets     = toast_widgets;
	s->numWidgets  = 0;
	s->maxWidgets  = Array_Elems(toast_widgets);
	TextWidget_Add(s, &s->titulo);
	TextWidget_Add(s, &s->nome);
	s->titulo.flags |= WIDGET_FLAG_MAINSCREEN;
	s->nome.flags   |= WIDGET_FLAG_MAINSCREEN;
	s->maxVertices = Screen_CalcDefaultMaxVertices(s);
}

static void ToastScreen_Free(void* screen) { toastAtual = -1; toastCount = 0; }

static const struct ScreenVTABLE ToastScreen_VTABLE = {
	ToastScreen_Init,   ToastScreen_Update, ToastScreen_Free,
	ToastScreen_Render, ToastScreen_BuildMesh,
	Screen_FInput,      Screen_InputUp,     Screen_FKeyPress, Screen_FText,
	Screen_FPointer,    Screen_PointerUp,   Screen_FPointer,  Screen_FMouseScroll,
	ToastScreen_Layout, ToastScreen_ContextLost, ToastScreen_ContextRecreated
};

static void Toast_Mostrar(int conquista) {
	struct ToastScreen* s = &ToastScreen;
	if (Gui_GetScreen(GUI_PRIORITY_CONQUISTA)) {
		if (toastCount < TOAST_FILA) toastFila[toastCount++] = conquista;
		return;
	}
	toastAtual = conquista;
	toastCount = 0;
	s->timer   = TOAST_TEMPO;
	s->VTABLE  = &ToastScreen_VTABLE;
	Gui_Add((struct Screen*)s, GUI_PRIORITY_CONQUISTA);
}


/*########################################################################################################################*
*----------------------------------------------------------Salvar---------------------------------------------------------*
*#########################################################################################################################*/
static float autoTimer;

static cc_bool ArquivoExiste(const char* arquivo) {
	cc_string path = String_FromReadonly(arquivo);
	cc_filepath raw;
	Platform_EncodePath(&raw, &path);
	return File_Exists(&raw);
}

cc_bool Salvar_ExisteMundo(void) { return ArquivoExiste(SALVAR_ARQUIVO); }

static cc_result Gravar(const char* arquivo) {
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

/* Copia o mundo salvo para o arquivo "anterior" antes de um mundo novo ocupar o lugar */
static void GuardarAnterior(void) {
	cc_string origem  = String_FromConst(SALVAR_ARQUIVO);
	cc_string destino = String_FromConst(SALVAR_ANTERIOR);
	struct Stream in, out;
	cc_uint8 buffer[4096];
	cc_uint32 lidos;
	cc_result res;

	if (Stream_OpenFile(&in, &origem)) return;
	if (Stream_CreateFile(&out, &destino)) { in.Close(&in); return; }

	for (;;)
	{
		res = in.Read(&in, buffer, sizeof(buffer), &lidos);
		if (res || !lidos) break;
		if ((res = Stream_Write(&out, buffer, lidos))) break;
	}
	out.Close(&out);
	in.Close(&in);
}

cc_bool Salvar_IniciarMundo(void) {
	cc_string path = String_FromConst(SALVAR_ARQUIVO);
	cc_bool continuar = Options_GetBool(SALVAR_OPT_CONTINUAR, false);
	/* o botao do inicio escolhe de novo a cada vez */
	Options_SetBool(SALVAR_OPT_CONTINUAR, false);
	autoTimer = 0;

	if (continuar && Salvar_ExisteMundo()) {
		Map_LoadFrom(&path);
		return true;
	}
	if (Salvar_ExisteMundo()) GuardarAnterior();
	return false;
}

cc_bool Salvar_Agora(cc_bool automatico) {
	if (!Server.IsSinglePlayer || !World.Loaded) {
		if (!automatico) Chat_AddRaw("&cSo da para salvar no modo um jogador.");
		return false;
	}
	/* as outras dimensoes ficam nos arquivos do Minerios (maps/orespawn_*.cw) */
	if (Minerios_DimensaoAtual() != MINERIOS_DIM_NORMAL) {
		if (!automatico) {
			Chat_AddRaw("&eSo da para salvar no mundo normal.");
			Chat_AddRaw("&eVolte pelo portal e salve de la (a outra dimensao fica guardada).");
		}
		return false;
	}

	if (Gravar(SALVAR_ARQUIVO)) {
		Chat_AddRaw("&cNao foi possivel salvar o mundo (o cartao SD esta cheio?)");
		return false;
	}
	World.LastSave = Game.Time;
	autoTimer = 0;
	SalvarContagem();
	Options_SaveIfChanged();

	Chat_AddOf(&(cc_string)String_FromConst(automatico ? "&7Mundo salvo automaticamente" : "&aMundo salvo!"),
		MSG_TYPE_SMALLANNOUNCEMENT);
	if (!automatico) {
		Chat_AddRaw("&aMundo salvo! &7Para voltar a ele, toque em &fContinuar &7na tela inicial.");
		Conquistas_Evento(CONQ_EV_SALVAR);
	}
	return true;
}

static cc_bool AutoSalvar_Tick(struct ScheduledTask2* task) {
	int minutos = Options_GetInt(SALVAR_OPT_AUTO, 0, 60, 5);
	if (!World.Loaded || !Server.IsSinglePlayer || !minutos) { autoTimer = 0; return true; }

	autoTimer += task->interval;
	if (autoTimer < minutos * 60.0f) return true;
	if (!Salvar_Agora(true)) autoTimer = 0; /* em outra dimensao: tenta de novo mais tarde */
	return true;
}
static struct ScheduledTask2 autosalvar_task = { 0, 1.0f, AutoSalvar_Tick };

/* Ao fechar o jogo (Sair, ou HOME > fechar no 3DS) o mundo e salvo sozinho */
static void OnClosing(void* obj) {
	if (!World.Loaded || !Server.IsSinglePlayer) return;
	if (!Options_GetInt(SALVAR_OPT_AUTO, 0, 60, 5)) return;
	Salvar_Agora(true);
}


/*########################################################################################################################*
*--------------------------------------------------------Comandos---------------------------------------------------------*
*#########################################################################################################################*/
static void ConquistasCommand_Execute(const cc_string* args, int argsCount) {
	int i;
	if (argsCount && String_CaselessEqualsConst(&args[0], "salvar")) {
		Salvar_Agora(false);
	} else if (argsCount >= 2 && String_CaselessEqualsConst(&args[0], "ganhar") && Convert_ParseInt(&args[1], &i)) {
		Ganhar(i); /* para testar o aviso */
	} else if (argsCount && String_CaselessEqualsConst(&args[0], "zerar")) {
		ganhas = 0; quebrados = colocados = monstros = 0; contagemMudou = true;
		Options_SetInt(OPT_CONQUISTAS, 0);
		SalvarContagem();
		Chat_AddRaw("&eConquistas zeradas.");
	} else {
		Conquistas_Mostrar();
	}
}

static struct ChatCommand ConquistasCommand = {
	"Conquistas", ConquistasCommand_Execute,
	COMMAND_FLAG_SINGLEPLAYER_ONLY,
	{
		"&a/client conquistas &e- mostra a lista de conquistas",
		"&a/client conquistas salvar &e- salva o mundo agora",
		"&a/client conquistas zerar &e- comeca as conquistas do zero",
	}
};


/*########################################################################################################################*
*-------------------------------------------------------Componente--------------------------------------------------------*
*#########################################################################################################################*/
static void OnInit(void) {
	ganhas    = Options_GetInt(OPT_CONQUISTAS, 0, 0x7FFFFFFF, 0);
	quebrados = Options_GetInt(OPT_QUEBRADOS,  0, 0x7FFFFFFF, 0);
	colocados = Options_GetInt(OPT_COLOCADOS,  0, 0x7FFFFFFF, 0);
	monstros  = Options_GetInt(OPT_MONSTROS,   0, 0x7FFFFFFF, 0);

	Commands_Register(&ConquistasCommand);
	ScheduledTask2_Add(&conquistas_task);
	ScheduledTask2_Add(&autosalvar_task);
	Event_Register_(&UserEvents.BlockChanged, NULL, OnBlockChanged);
	Event_Register_(&WindowEvents.Closing,    NULL, OnClosing);
}

static void OnNewMap(void) {
	estavaNoite = false;
	noiteLimpa  = false;
	autoTimer   = 0;
}

static void OnFree(void) { SalvarContagem(); }

struct IGameComponent Conquistas_Component = {
	OnInit,   /* Init  */
	OnFree,   /* Free  */
	NULL,     /* Reset */
	OnNewMap, /* OnNewMap */
	NULL      /* OnNewMapLoaded */
};

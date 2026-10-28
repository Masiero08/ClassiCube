#ifndef CC_MINERIOS_H
#define CC_MINERIOS_H
#include "Core.h"
CC_BEGIN_HEADER

/* Mod "Minerios & Monstros" (inspirado no OreSpawn) para o modo um jogador:
   minerios de rubi, titanio e uranio com seus blocos, o minerio de ovo
   (choca uma criatura, as vezes gigante), espadas famosas (titanio, suprema
   e a Big Bertha), armadura de titanio, mosquitos, blocos da sorte e as dimensoes
   Utopia e Cristal.
*/
struct Bitmap;
struct IGameComponent;
extern struct IGameComponent Minerios_Component;

/* Blocos e itens do mod. Itens sao blocos "sprite" que nao podem ser colocados. */
enum MineriosBloco {
	MBLOCK_RUBI_MINERIO = 100, MBLOCK_TITANIO_MINERIO, MBLOCK_URANIO_MINERIO, MBLOCK_OVO_MINERIO,
	MBLOCK_RUBI, MBLOCK_TITANIO, MBLOCK_URANIO,
	MITEM_ESPADA_TITANIO, MITEM_ESPADA_SUPREMA, MITEM_BIG_BERTHA, MITEM_ARMADURA_TITANIO,
	MBLOCK_PORTAL_UTOPIA, MBLOCK_PORTAL_CRISTAL,
	MBLOCK_PEDRA_CRISTAL, MBLOCK_GRAMA_CRISTAL, MBLOCK_TRONCO_CRISTAL, MBLOCK_FOLHAS_CRISTAL, MBLOCK_CRISTAL,
	MBLOCK_SORTE,
	MITEM_FERRAO, MITEM_NUCLEO, MITEM_OLHO, MITEM_CORACAO,
	MBLOCK_ALTAR_RAINHA, MBLOCK_ALTAR_GOLEM, MBLOCK_ALTAR_ARANHA, MBLOCK_ALTAR_CREEPER,
	MITEM_DEVASTADORA, MBLOCK_OVO_DRAGAO,
	MBLOCK_FIM
};

/* Se o mod esta ligado (opcao "mod-minerios", padrao ligado). */
/* O menu de mods pode mudar isso; vale a partir do proximo mundo carregado/gerado. */
extern cc_bool Minerios_Enabled;
#define MINERIOS_OPTION "mod-minerios"

/* Desenha as texturas do mod em tiles livres do atlas de terreno */
void Minerios_PatchAtlas(struct Bitmap* bmp);

/* Veios de minerio que o gerador de mundos espalha na pedra */
struct MinerioVeio { float abundancia; const char* estado; cc_uint8 bloco; };
extern const struct MinerioVeio Minerios_Veios[];
extern const int Minerios_VeiosCount;
/* Ultimo passo do gerador: transforma o mundo gerado numa dimensao (se for o caso) */
void Minerios_PosGeracao(void);
/* Dimensao sendo gerada agora (-1 ou 0 = mundo normal) */
int Minerios_DimensaoGerando(void);

/* Chamado quando o jogador aperta colocar (place = true) ou quebrar bloco. */
/* Retorna true se o clique foi usado pelo mod (bater num monstro, usar um item). */
cc_bool Minerios_HandleClick(cc_bool place);

CC_END_HEADER
#endif

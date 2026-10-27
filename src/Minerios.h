#ifndef CC_MINERIOS_H
#define CC_MINERIOS_H
#include "Core.h"
CC_BEGIN_HEADER

/* Mod "Minerios & Monstros" (inspirado no OreSpawn) para o modo um jogador:
   minerios de rubi, titanio e uranio com seus blocos, e o minerio de ovo,
   que choca uma criatura (as vezes gigante) quando e quebrado.
*/
struct Bitmap;
struct IGameComponent;
extern struct IGameComponent Minerios_Component;

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

CC_END_HEADER
#endif

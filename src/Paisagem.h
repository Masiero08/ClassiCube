#ifndef CC_PAISAGEM_H
#define CC_PAISAGEM_H
#include "Core.h"
/* Paisagem: a livelier overworld for singleplayer (tall grass, more flowers, birch, cherry
   and pine trees, berry bushes, sugar cane, pumpkins, snowy peaks and taller hills)
*/
CC_BEGIN_HEADER

struct Bitmap;
struct IGameComponent;
extern struct IGameComponent Paisagem_Component;

/* Block IDs 160-175 and atlas tiles 176-191 are reserved for these blocks */
enum PaisagemBlock {
	PBLOCK_GRAMA_ALTA = 160, PBLOCK_SAMAMBAIA, PBLOCK_TULIPA, PBLOCK_FLOR_AZUL, PBLOCK_MARGARIDAS,
	PBLOCK_TRONCO_BETULA, PBLOCK_FOLHAS_BETULA, PBLOCK_ARBUSTO, PITEM_FRUTINHAS,
	PBLOCK_CANA, PBLOCK_ABOBORA, PBLOCK_FOLHAS_CEREJEIRA, PBLOCK_FOLHAS_PINHEIRO, PBLOCK_TRONCO_PINHEIRO,
	PBLOCK_FIM
};

/* Draws the textures for these blocks into unused tiles of the given terrain atlas */
void Paisagem_PatchAtlas(struct Bitmap* bmp);
/* Decorates a newly generated overworld: plants, more varied trees, sugar cane and snow */
void Paisagem_GenOverworld(void);

CC_END_HEADER
#endif

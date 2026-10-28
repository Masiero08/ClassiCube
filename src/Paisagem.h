#ifndef CC_PAISAGEM_H
#define CC_PAISAGEM_H
#include "Core.h"
#include "Vectors.h"
/* Paisagem: a livelier overworld for singleplayer (tall grass, more flowers, birch, cherry
   and pine trees, berry bushes, sugar cane, pumpkins, snowy peaks and taller hills)
*/
CC_BEGIN_HEADER

struct Bitmap;
struct IGameComponent;
extern struct IGameComponent Paisagem_Component;

/* Block IDs 160-191 and atlas tiles 176-207 are reserved for these blocks */
enum PaisagemBlock {
	PBLOCK_GRAMA_ALTA = 160, PBLOCK_SAMAMBAIA, PBLOCK_TULIPA, PBLOCK_FLOR_AZUL, PBLOCK_MARGARIDAS,
	PBLOCK_TRONCO_BETULA, PBLOCK_FOLHAS_BETULA, PBLOCK_ARBUSTO, PITEM_FRUTINHAS,
	PBLOCK_CANA, PBLOCK_ABOBORA, PBLOCK_FOLHAS_CEREJEIRA, PBLOCK_FOLHAS_PINHEIRO, PBLOCK_TRONCO_PINHEIRO,
	/* Emblems in the gyms of the Mestres de Bioma, in the same order as mestres_def (Criaturas) */
	PBLOCK_EMBLEMA_PLANTA, PBLOCK_EMBLEMA_AGUA, PBLOCK_EMBLEMA_PEDRA,
	PBLOCK_EMBLEMA_ELETRICO, PBLOCK_EMBLEMA_VENTO, PBLOCK_EMBLEMA_FOGO,
	PBLOCK_FIM
};

/* Draws the textures for these blocks into unused tiles of the given terrain atlas */
void Paisagem_PatchAtlas(struct Bitmap* bmp);
/* Decorates a newly generated overworld: plants, more varied trees, sugar cane and snow */
void Paisagem_GenOverworld(void);
/* Finds a gym emblem within 'radius' blocks (horizontally, 8 vertically) of the given position. */
/* Returns the gym/Mestre index (0 = plants .. 5 = fire, as in mestres_def) or -1 if none. */
/* 'pos' is set to the emblem block; the Mestre should stand on the floor in front of it. */
int Paisagem_AcharGinasio(int x, int y, int z, int radius, IVec3* pos);

CC_END_HEADER
#endif

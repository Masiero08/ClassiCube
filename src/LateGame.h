#ifndef CC_LATEGAME_H
#define CC_LATEGAME_H
#include "Core.h"
/* Diamonds, the Nether and the End for singleplayer, inspired by newer Minecraft versions
*/
CC_BEGIN_HEADER

struct Bitmap;
struct IGameComponent;
extern struct IGameComponent LateGame_Component;

/* Block IDs 130-159 and atlas tiles 128-159 are reserved for these blocks */
enum LateGameBlock {
	LBLOCK_DIAMOND_ORE = 130, LITEM_DIAMOND, LBLOCK_DIAMOND,
	LBLOCK_NETHERRACK, LBLOCK_NETHER_BRICKS, LBLOCK_SOUL_SAND, LBLOCK_GLOWSTONE,
	LBLOCK_QUARTZ_ORE, LITEM_QUARTZ, LBLOCK_NETHER_PORTAL,
	LBLOCK_END_STONE, LBLOCK_END_PORTAL, LBLOCK_DRAGON_EGG, LITEM_ENDER_PEARL,
	LBLOCK_END
};

/* Draws the textures for these blocks into unused tiles of the given terrain atlas */
void LateGame_PatchAtlas(struct Bitmap* bmp);
/* Adds obsidian next to lava and diamond ore deep underground in a newly generated world */
void LateGame_GenOverworld(void);
/* Turns a newly generated world into the Nether (netherrack, lava sea, ceiling, fortress bridges) */
void LateGame_GenNether(void);
/* Turns a newly generated world into the End (a floating island with obsidian pillars) */
void LateGame_GenEnd(void);
/* Hits the Ender Dragon if the player is looking at it. Returns whether it was hit */
cc_bool LateGame_HandleAttack(void);

CC_END_HEADER
#endif

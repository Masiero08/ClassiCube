#ifndef CC_SURVIVAL_H
#define CC_SURVIVAL_H
#include "Core.h"
/* Basic survival mode (health, damage, collecting blocks) for singleplayer
*/
CC_BEGIN_HEADER

struct IGameComponent;
extern struct IGameComponent Survival_Component;
/* Whether the user has enabled survival mode for singleplayer */
extern cc_bool Survival_Enabled;

/* Whether the player has hit the given block enough times to break it */
cc_bool Survival_CanBreak(BlockID block, int x, int y, int z);
/* Called after the player has broken the given block */
void Survival_OnBlockBroken(BlockID block);
/* Whether the player has any of the given block left to place */
cc_bool Survival_CanPlace(BlockID block);
/* Called after the player has placed the given block */
void Survival_OnBlockPlaced(BlockID block);
/* Whether survival mode is currently being played (enabled and in singleplayer) */
cc_bool Survival_IsActive(void);

/* Number of crafting recipes */
int  Survival_RecipesCount(void);
/* Appends a description of the given recipe (output, inputs and how many the player has) */
void Survival_DescribeRecipe(int i, cc_string* str);
/* Crafts the given recipe if the player has enough of its inputs */
cc_bool Survival_Craft(int i);

/* Current health (0 to 20), for saving the world */
int  Survival_GetHealth(void);
/* Writes how many of each block the player has, as BLOCK_COUNT big endian 16 bit values */
void Survival_GetCounts(cc_uint8* data);
/* Health read from a saved world, applied once it has finished loading */
void Survival_LoadHealth(int value);
/* Block counts read from a saved world (see Survival_GetCounts), applied once it has finished loading */
void Survival_LoadCounts(const cc_uint8* data, int len);

CC_END_HEADER
#endif

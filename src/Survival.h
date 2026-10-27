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

/* Whether the player is allowed to break the given block */
cc_bool Survival_CanBreak(BlockID block);
/* Called after the player has broken the given block */
void Survival_OnBlockBroken(BlockID block);
/* Whether the player has any of the given block left to place */
cc_bool Survival_CanPlace(BlockID block);
/* Called after the player has placed the given block */
void Survival_OnBlockPlaced(BlockID block);

CC_END_HEADER
#endif

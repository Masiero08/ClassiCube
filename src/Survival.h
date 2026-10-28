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
/* Handles right clicking a bed (sleep and set spawn). Returns whether the click was used */
cc_bool Survival_HandleUse(void);
/* Hurts the player (2 = one heart), if survival mode is being played */
void Survival_Damage(int amount);
/* Damage the player's next hit does to a monster (and wears out their sword) */
int  Survival_AttackDamage(void);
/* Gives the player some of the given block, e.g. dropped by a monster */
void Survival_AddBlocks(BlockID block, int count);

/* Number of crafting recipes */
int  Survival_RecipesCount(void);
/* Appends a description of the given recipe (output, inputs and how many the player has) */
void Survival_DescribeRecipe(int i, cc_string* str);
/* Crafts the given recipe if the player has enough of its inputs */
cc_bool Survival_Craft(int i);

/* The crafting grid is 3x3 cells, stored row by row (BLOCK_AIR = empty cell) */
#define CRAFT_GRID_SIZE  3
#define CRAFT_GRID_CELLS 9
enum RecipeKind { RECIPE_KIND_FOOD, RECIPE_KIND_TOOL, RECIPE_KIND_BLOCK };
/* Returns the recipe made by what is in the crafting grid, or -1 if none */
int  Survival_MatchGrid(const BlockID* grid);
/* Fills the crafting grid with where the ingredients of the given recipe go */
void Survival_RecipeLayout(int i, BlockID* grid);
/* Block shown for the result of a recipe, and how many are made (0 for tools and food) */
BlockID Survival_RecipeIcon(int i, int* count);
/* Whether the recipe makes food, a tool or a block (see RecipeKind) */
int  Survival_RecipeKind(int i);
/* Which tool the recipe makes (0 = pickaxe, 1 = axe, 2 = shovel, 3 = sword), or -1 if not a tool */
int  Survival_RecipeTool(int i);
/* Appends the name of what the recipe makes */
void Survival_RecipeName(int i, cc_string* str);
/* Why the recipe can't be made right now, or NULL if it can */
const char* Survival_RecipeProblem(int i);
/* How many of the given block the player has */
int  Survival_GetCount(BlockID block);
/* Short Portuguese name of the given block */
cc_string Survival_BlockName(BlockID block);

/* Size of the data written by Survival_GetTools */
#define SURVIVAL_TOOLS_SIZE 12

/* Current health (0 to 20), for saving the world */
int  Survival_GetHealth(void);
/* Current food level (0 to 20), for saving the world */
int  Survival_GetFood(void);
/* Writes the tier and remaining uses of each tool (SURVIVAL_TOOLS_SIZE bytes) */
void Survival_GetTools(cc_uint8* data);
/* Food level read from a saved world, applied once it has finished loading */
void Survival_LoadFood(int value);
/* Tools read from a saved world (see Survival_GetTools), applied once it has finished loading */
void Survival_LoadTools(const cc_uint8* data, int len);
/* Writes how many of each block the player has, as BLOCK_COUNT big endian 16 bit values */
void Survival_GetCounts(cc_uint8* data);
/* Health read from a saved world, applied once it has finished loading */
void Survival_LoadHealth(int value);
/* Block counts read from a saved world (see Survival_GetCounts), applied once it has finished loading */
void Survival_LoadCounts(const cc_uint8* data, int len);

CC_END_HEADER
#endif

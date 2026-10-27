#ifndef CC_MODERNBLOCKS_H
#define CC_MODERNBLOCKS_H
#include "Core.h"
/* Blocks inspired by newer Minecraft versions, available in singleplayer
*/
CC_BEGIN_HEADER

struct Bitmap;
struct IGameComponent;
extern struct IGameComponent ModernBlocks_Component;

/* Draws the modern block textures into unused tiles of the given terrain atlas */
void ModernBlocks_PatchAtlas(struct Bitmap* bmp);

CC_END_HEADER
#endif

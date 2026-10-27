#ifndef CC_DAYNIGHT_H
#define CC_DAYNIGHT_H
#include "Core.h"
/* Day and night cycle, with monsters at night, for survival mode
*/
CC_BEGIN_HEADER

struct IGameComponent;
extern struct IGameComponent DayNight_Component;

/* Whether it is currently night (monsters appear) */
cc_bool DayNight_IsNight(void);
/* Hits the monster the player is looking at, if any. Returns whether one was hit */
cc_bool DayNight_HandleAttack(void);
/* Seconds since sunrise, for saving the world */
int  DayNight_GetTime(void);
/* Restores the time of day read from a saved world */
void DayNight_SetTime(int value);

CC_END_HEADER
#endif

#ifndef CC_DAYNIGHT_H
#define CC_DAYNIGHT_H
#include "Core.h"
#include "PackedCol.h"
/* Day and night cycle, with monsters at night, for survival mode
*/
CC_BEGIN_HEADER

struct IGameComponent;
extern struct IGameComponent DayNight_Component;

/* Whether it is currently night (monsters appear) */
cc_bool DayNight_IsNight(void);
/* Hits the monster the player is looking at, if any. Returns whether one was hit */
cc_bool DayNight_HandleAttack(void);
/* Sky, fog and clouds colours used during the day (e.g. for another dimension) */
void DayNight_SetDayColors(PackedCol sky, PackedCol fog, PackedCol clouds);
enum DayNightMode { DAYNIGHT_NORMAL, DAYNIGHT_NETHER, DAYNIGHT_END };
/* Sets how the sky and monsters behave (other dimensions have no day/night cycle) */
void DayNight_SetMode(int mode);
/* Seconds since sunrise, for saving the world */
int  DayNight_GetTime(void);
/* Restores the time of day read from a saved world */
void DayNight_SetTime(int value);

CC_END_HEADER
#endif

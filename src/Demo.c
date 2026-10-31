/* Scripted gameplay for recording videos on PC: CC_DEMO=<script file> (see misc/video) */
#include "Core.h"
#if defined CC_BUILD_LINUX
#include "Game.h"
#include "Chat.h"
#include "Entity.h"
#include "Input.h"
#include "ExtMath.h"
#include "Platform.h"
#include "String_.h"
#include "Utils.h"
#include "Window.h"
#include "World.h"
#include "Survival.h"
#include "Menus.h"
#include "Gui.h"
#include "Paisagem.h"
#include "Block.h"
#include "Server.h"
#include "Model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>

static char* lines[2048];
static int lineCount, cur;
static float waitLeft, startDelay = 2.0f;
static float turnLeft, turnYaw, turnPitch; /* degrees per second */
static int releaseMouse = -1;
static char aimModel[64];
static float aimLeft;

static double NowSecs(void) {
	struct timeval tv; gettimeofday(&tv, NULL);
	return tv.tv_sec + tv.tv_usec / 1e6;
}

static void SetRot(float yaw, float pitch) {
	struct Entity* e = &Entities.CurPlayer->Base;
	struct LocationUpdate u;
	u.flags = LU_HAS_YAW | LU_HAS_PITCH;
	u.yaw = yaw; u.pitch = Math_ClampAngle(pitch);
	e->VTABLE->SetLocation(e, &u);
}


static void TeleportLook(float x, float y, float z, float yaw, float pitch) {
	struct Entity* e = &Entities.CurPlayer->Base;
	struct LocationUpdate u;
	u.flags = LU_HAS_POS | LU_HAS_YAW | LU_HAS_PITCH | LU_POS_ABSOLUTE_INSTANT;
	u.pos.x = x; u.pos.y = y; u.pos.z = z;
	u.yaw = yaw; u.pitch = Math_ClampAngle(pitch);
	e->VTABLE->SetLocation(e, &u);
}

static cc_bool IsAir(int x, int y, int z) {
	return World_Contains(x, y, z) && Blocks.Collide[World_GetBlock(x, y, z)] != COLLIDE_SOLID;
}
static cc_bool IsSolid(int x, int y, int z) {
	return World_Contains(x, y, z) && Blocks.Collide[World_GetBlock(x, y, z)] == COLLIDE_SOLID;
}

static void GoGym(cc_bool inside) {
	IVec3 p; int d, idx;
	idx = Paisagem_AcharGinasio(World.Width / 2, World.Height / 2, World.Length / 2, 256, &p);
	if (idx < 0) {
		int y;
		for (y = 0; y < World.Height && idx < 0; y += 16)
			idx = Paisagem_AcharGinasio(World.Width / 2, y, World.Length / 2, 256, &p);
	}
	fprintf(stderr, "DEMO gym %d at %d %d %d\n", idx, p.x, p.y, p.z);
	if (idx < 0) return;
	if (inside) { TeleportLook(p.x + 0.5f, (float)(p.y - 1), p.z - 5.5f, 180.0f, 3.0f); return; }
	/* outside the door (gym faces -Z), looking in */
	{
		int x = p.x, z = p.z - 12, y = p.y + 6;
		while (y > 1 && !IsSolid(x, y - 1, z)) y--;
		TeleportLook(x + 0.5f, (float)y, z + 0.5f, 180.0f, 0.0f);
	}
}

/* Finds an open cave below the surface with ores in sight */
static void GoCave(void) {
	int x, y, z, dx, dy, dz, air, ores, score, best = -1;
	int bx = 0, by = 0, bz = 0;
	int top = World.Height / 2 - 6;
	for (y = 6; y < top; y += 2)
	for (z = 8; z < World.Length - 8; z += 3)
	for (x = 8; x < World.Width - 8; x += 3)
	{
		if (!IsAir(x, y, z) || !IsAir(x, y + 1, z) || !IsSolid(x, y - 1, z)) continue;
		if (World_GetBlock(x, y, z) != BLOCK_AIR) continue;
		air = 0; ores = 0;
		for (dy = 0; dy < 4; dy++)
		for (dz = -4; dz <= 4; dz++)
		for (dx = -4; dx <= 4; dx++)
		{
			BlockID b;
			if (!World_Contains(x + dx, y + dy, z + dz)) continue;
			b = World_GetBlock(x + dx, y + dy, z + dz);
			if (b == BLOCK_AIR) air++;
			else if (b == BLOCK_COAL_ORE || b == BLOCK_IRON_ORE || b == BLOCK_GOLD_ORE || (b >= 100 && b <= 159)) ores++;
			else if (b == BLOCK_WATER || b == BLOCK_STILL_WATER || b == BLOCK_LAVA || b == BLOCK_STILL_LAVA) air -= 50;
		}
		if (air < 120) continue;
		score = air + ores * 25;
		if (score > best) { best = score; bx = x; by = y; bz = z; }
	}
	fprintf(stderr, "DEMO cave score %d at %d %d %d\n", best, bx, by, bz);
	if (best < 0) return;
	{
		int i, k, run, bestRun = -1; float bestYaw = 0;
		for (i = 0; i < 16; i++) {
			float yaw = i * 22.5f, r = yaw * MATH_DEG2RAD;
			for (k = 1, run = 0; k < 20; k++, run++) {
				int xx = Math_Floor(bx + 0.5f + Math_SinF(r) * k), zz = Math_Floor(bz + 0.5f - Math_CosF(r) * k);
				if (!IsAir(xx, by + 1, zz)) break;
			}
			if (run > bestRun) { bestRun = run; bestYaw = yaw; }
		}
		TeleportLook(bx + 0.5f, (float)by, bz + 0.5f, bestYaw, 5.0f);
	}
}

/* Highest open ground near the middle of the map, for a view over the landscape */
static void GoHigh(void) {
	int x, y, z, best = -1, bx = 0, bz = 0, cx = World.Width / 2, cz = World.Length / 2;
	for (z = cz - 40; z <= cz + 40; z += 2)
	for (x = cx - 40; x <= cx + 40; x += 2)
	{
		BlockID b;
		for (y = World.Height - 2; y > 0 && !IsSolid(x, y, z); y--) {}
		b = World_GetBlock(x, y, z);
		if (b != BLOCK_GRASS && b != BLOCK_DIRT && b != BLOCK_STONE && b != BLOCK_SAND) continue;
		if (y > best) { best = y; bx = x; bz = z; }
	}
	fprintf(stderr, "DEMO high %d at %d %d\n", best, bx, bz);
	if (best >= 0) TeleportLook(bx + 0.5f, (float)(best + 1), bz + 0.5f, 0.0f, 8.0f);
}

/* Open grassy spot (no trees around) near the middle of the map, as high as possible */
static int FindOpen(int cx, int cz, int r, int* bx, int* bz) {
	int x, y, z, dx, dz, yy, best = -1;
	for (z = cz - 50; z <= cz + 50; z += 2)
	for (x = cx - 50; x <= cx + 50; x += 2)
	{
		cc_bool ok = true;
		for (y = World.Height - 2; y > 0 && !IsSolid(x, y, z); y--) {}
		if (World_GetBlock(x, y, z) != BLOCK_GRASS || y <= best) continue;
		for (dz = -r; dz <= r && ok; dz++)
		for (dx = -r; dx <= r && ok; dx++)
		{
			/* flat ground around, nothing in the way */
			if (!IsSolid(x + dx, y, z + dz) && !IsSolid(x + dx, y - 1, z + dz)) ok = false;
			for (yy = y + 1; yy <= y + 6 && ok; yy++)
			{
				if (!World_Contains(x + dx, yy, z + dz) || IsSolid(x + dx, yy, z + dz)) ok = false;
			}
		}
		if (!ok) continue;
		best = y; *bx = x; *bz = z;
	}
	return best;
}

/* Open flat grassy spot (no trees around) near the middle of the map, as high as possible */
static void GoOpen(int r) {
	int best = -1, bx = 0, bz = 0;
	for (; r >= 3 && best < 0; r--) best = FindOpen(World.Width / 2, World.Length / 2, r, &bx, &bz);
	fprintf(stderr, "DEMO open %d at %d %d r %d\n", best, bx, bz, r + 1);
	if (best >= 0) TeleportLook(bx + 0.5f, (float)(best + 1), bz + 0.5f, 0.0f, 5.0f);
}

static void AimTick(float delta) {
	struct Entity* me = &Entities.CurPlayer->Base;
	struct Entity* e; int i;
	float dx, dy, dz, yaw, pitch, cur, diff, k;
	if (aimLeft <= 0) return;
	aimLeft -= delta;
	for (i = 0; i < ENTITIES_MAX_COUNT; i++) {
		e = Entities.List[i];
		if (!e || e == me || !e->Model) continue;
		if (!strcmp(e->Model->name, aimModel) || (aimModel[0] == '*' && strstr(e->Model->name, aimModel + 1))) break;
	}
	if (i == ENTITIES_MAX_COUNT) return;
	dx = e->Position.x - me->Position.x;
	dy = e->Position.y + e->Size.y * 0.5f - (me->Position.y + 1.6f);
	dz = e->Position.z - me->Position.z;
	yaw   = (float)(atan2(dx, -dz) * MATH_RAD2DEG);
	pitch = (float)(-atan2(dy, sqrt(dx * dx + dz * dz)) * MATH_RAD2DEG);
	/* ease towards the target so the camera moves smoothly */
	k = delta * 4.0f; if (k > 1) k = 1;
	cur = me->next.yaw; diff = yaw - cur;
	while (diff > 180) diff -= 360; while (diff < -180) diff += 360;
	yaw = cur + diff * k;
	cur = me->next.pitch > 180 ? me->next.pitch - 360 : me->next.pitch;
	pitch = cur + (pitch - cur) * k;
	SetRot(yaw, pitch);
}

static int KeyByName(const char* n) {
	int i;
	for (i = 0; i < INPUT_COUNT; i++) {
		if (Input_StorageNames[i] && !strcasecmp(Input_StorageNames[i], n)) return i;
	}
	return atoi(n);
}

static void RunLine(char* l) {
	char cmd[16] = { 0 };
	float a = 0, b = 0, c = 0;
	char arg[256] = { 0 };
	int n;
	sscanf(l, "%15s", cmd);
	if (!strcmp(cmd, "w")) {
		sscanf(l + 1, "%f", &a); waitLeft = a;
	} else if (!strcmp(cmd, "c")) {
		cc_string s = String_FromReadonly(l + 2);
		Chat_Send(&s, false);
	} else if (!strcmp(cmd, "k")) {
		sscanf(l + 1, "%255s %d", arg, &n);
		Input_Set(KeyByName(arg), n);
	} else if (!strcmp(cmd, "click")) {
		sscanf(l + 5, "%255s", arg);
		releaseMouse = !strcmp(arg, "right") ? CCMOUSE_R : CCMOUSE_L;
		Input_Set(releaseMouse, true);
	} else if (!strcmp(cmd, "l")) {
		struct Entity* e = &Entities.CurPlayer->Base;
		float dy, dp;
		sscanf(l + 1, "%f %f %f", &a, &b, &c);
		dy = a - e->next.yaw;   while (dy > 180) dy -= 360; while (dy < -180) dy += 360;
		dp = b - (e->next.pitch > 180 ? e->next.pitch - 360 : e->next.pitch);
		if (c <= 0) { SetRot(a, b); return; }
		turnYaw = dy / c; turnPitch = dp / c; turnLeft = c;
	} else if (!strcmp(cmd, "t")) {
		sscanf(l + 1, "%f %f %f", &a, &b, &c);
		if (c <= 0) c = 0.01f;
		turnYaw = a / c; turnPitch = b / c; turnLeft = c;
	} else if (!strcmp(cmd, "tap")) {
		sscanf(l + 3, "%f %f", &a, &b);
		Pointer_SetPosition(0, (int)a, (int)b);
		releaseMouse = CCMOUSE_L;
		Input_Set(releaseMouse, true);
	} else if (!strcmp(cmd, "give")) {
		sscanf(l + 4, "%d %f", &n, &a);
		Survival_AddBlocks((BlockID)n, (int)a);
	} else if (!strcmp(cmd, "craft")) {
		CraftingScreen_Show();
	} else if (!strcmp(cmd, "fechar")) {
		if (Gui_GetInputGrab()) Gui_Remove(Gui_GetInputGrab());
	} else if (!strcmp(cmd, "ginasio") || !strcmp(cmd, "ginasio2")) {
		GoGym(cmd[7] == '2');
	} else if (!strcmp(cmd, "aberto")) {
		n = 7; sscanf(l + 6, "%d", &n);
		GoOpen(n);
	} else if (!strcmp(cmd, "mirar")) {
		sscanf(l + 5, "%63s %f", aimModel, &a);
		aimLeft = a;
	} else if (!strcmp(cmd, "alto")) {
		GoHigh();
	} else if (!strcmp(cmd, "recua")) {
		struct Entity* e = &Entities.CurPlayer->Base;
		float r = e->next.yaw * MATH_DEG2RAD;
		sscanf(l + 5, "%f", &a);
		TeleportLook(e->next.pos.x - Math_SinF(r) * a, e->next.pos.y + 1, e->next.pos.z + Math_CosF(r) * a, e->next.yaw, 0);
	} else if (!strcmp(cmd, "caverna")) {
		GoCave();
	} else if (!strcmp(cmd, "pos")) {
		sscanf(l + 3, "%f %f %f", &a, &b, &c);
		TeleportLook(a, b, c, Entities.CurPlayer->Base.next.yaw, 0);
	} else if (!strcmp(cmd, "limpa")) {
		for (n = 0; n < 12; n++) Chat_AddRaw("");
	} else if (!strcmp(cmd, "mark")) {
		fprintf(stderr, "DEMO-MARK %.3f %s\n", NowSecs(), l + 5); fflush(stderr);
	} else if (!strcmp(cmd, "q")) {
		Window_RequestClose();
	}
}

static cc_bool Demo_Tick(struct ScheduledTask2* task) {
	float delta = task->interval;
	struct Entity* e;
	if (!lineCount || !World.Loaded || !Entities.CurPlayer) return true;
	if (startDelay > 0) { startDelay -= delta; return true; }

	if (releaseMouse >= 0) { Input_Set(releaseMouse, false); releaseMouse = -1; }

	if (turnLeft > 0) {
		float d = turnLeft < delta ? turnLeft : delta;
		float pitch;
		e = &Entities.CurPlayer->Base;
		pitch = e->next.pitch > 180 ? e->next.pitch - 360 : e->next.pitch;
		SetRot(e->next.yaw + turnYaw * d, pitch + turnPitch * d);
		turnLeft -= d;
	}

	AimTick(delta);
	if (waitLeft > 0) { waitLeft -= delta; return true; }
	while (cur < lineCount && waitLeft <= 0) {
		RunLine(lines[cur++]);
	}
	return true;
}
static struct ScheduledTask2 demo_task = { 0, 1.0f / 60, Demo_Tick };

static void OnInit(void) {
	const char* path = getenv("CC_DEMO");
	FILE* f; char buf[512];
	if (!path || !(f = fopen(path, "r"))) return;
	while (lineCount < 2048 && fgets(buf, sizeof(buf), f)) {
		size_t len = strcspn(buf, "\r\n"); buf[len] = 0;
		if (!len || buf[0] == '#') continue;
		lines[lineCount++] = strdup(buf);
	}
	fclose(f);
	ScheduledTask2_Add(&demo_task);
}

struct IGameComponent Demo_Component = { OnInit };
#else
#include "Game.h"
struct IGameComponent Demo_Component;
#endif

#include "Equipamento.h"
#include "Menus.h"
#include "Gui.h"
#include "Graphics.h"
#include "Drawer2D.h"
#include "IsometricDrawer.h"
#include "Survival.h"
#include "Block.h"
#include "BlockID.h"
#include "Input.h"
#include "InputHandler.h"
#include "Inventory.h"
#include "Window.h"
#include "Funcs.h"
#include "String_.h"
#include "ExtMath.h"
#include "Event.h"
#include "Screens.h"

/* Inventory, chest and furnace screens in the style of Minecraft, made for the 3DS touch screen. */
/* They work like the crafting table (CraftingScreen.c): everything is tapped, nothing is dragged. */
/* All drawing here is original. */

/* Everything is laid out on a 320x240 panel (the 3DS bottom screen), scaled to fit the window */
#define IS_WIDTH  320
#define IS_HEIGHT 240
#define IS_SLOT    24

#define IS_ITEMS_X    16
#define IS_ITEMS_COLS 12
#define IS_MAX_ICONS  100
#define IS_MAX_VERTICES (IS_MAX_ICONS * ISOMETRICDRAWER_MAXVERTICES)

/* Inventory */
#define IS_DOLL_X     8
#define IS_DOLL_Y     16
#define IS_ARMOR_X    62
#define IS_ARMOR_Y    18
#define IS_TOOLS_X    122
#define IS_TOOLS_Y    16
#define IS_INV_ITEMS_Y 96
#define IS_HOTBAR_X   52
#define IS_HOTBAR_Y   164

/* Chest */
#define IS_BAU_X      52
#define IS_BAU_Y      16
#define IS_BAU_COLS   9

/* Furnace */
#define IS_IN_X       60
#define IS_IN_Y       16
#define IS_FLAME_Y    42
#define IS_FUEL_Y     64
#define IS_ARROW_X    96
#define IS_OUT_X      146
#define IS_OUT_Y      36
#define IS_OUT_SIZE   32

#define IS_STORE_ITEMS_Y 104

enum InvMode { MODE_INV, MODE_BAU, MODE_FORNALHA };
enum InvButton { IS_BTN_PREV, IS_BTN_NEXT, IS_BTN_USE, IS_BTN_CRAFT, IS_BTN_MOVE, IS_BTN_CLOSE };
struct InvButtonDef { short x, w; cc_uint8 id; };

static const struct InvButtonDef inv_buttons[] = {
	{   8, 36, IS_BTN_PREV  }, {  48, 36, IS_BTN_NEXT }, { 90, 66, IS_BTN_USE },
	{ 162, 74, IS_BTN_CRAFT }, { 242, 70, IS_BTN_CLOSE },
};
static const struct InvButtonDef store_buttons[] = {
	{   8, 36, IS_BTN_PREV  }, { 108, 36, IS_BTN_NEXT }, { 152, 76, IS_BTN_MOVE }, { 236, 76, IS_BTN_CLOSE },
};

static struct InventarioScreen {
	Screen_Body
	struct FontDesc font;
	struct Texture back, front;
	int state[IS_MAX_VERTICES / 4];
	int iconVertices;
	float scale; int ox, oy;
	cc_bool releasedKey;

	int mode;
	struct EquipBau* bau;
	struct EquipFornalha* fornalha;
	BlockID held;
	BlockID items[BLOCK_COUNT];
	int itemsCount, invPage;
	cc_bool moveOne;
	float redrawTimer;

	cc_string msg;
	char _msgBuffer[STRING_SIZE];
} InventarioScreen;


/*########################################################################################################################*
*---------------------------------------------------------Layout----------------------------------------------------------*
*#########################################################################################################################*/
static int IS_ItemsY(struct InventarioScreen* s)    { return s->mode == MODE_INV ? IS_INV_ITEMS_Y : IS_STORE_ITEMS_Y; }
static int IS_ItemsRows(struct InventarioScreen* s) { return s->mode == MODE_INV ? 2 : 3; }
static int IS_ItemsPage(struct InventarioScreen* s) { return IS_ItemsRows(s) * IS_ITEMS_COLS; }
static int IS_ButtonsY(struct InventarioScreen* s)  { return s->mode == MODE_INV ? 196 : 190; }

static const struct InvButtonDef* IS_Buttons(struct InventarioScreen* s, int* count) {
	if (s->mode == MODE_INV) { *count = Array_Elems(inv_buttons); return inv_buttons; }
	*count = Array_Elems(store_buttons); return store_buttons;
}

static int IS_Pages(struct InventarioScreen* s) { return max(1, Math_CeilDiv(s->itemsCount, IS_ItemsPage(s))); }


/*########################################################################################################################*
*---------------------------------------------------------State-----------------------------------------------------------*
*#########################################################################################################################*/
static void InventarioScreen_SetDefaultMessage(struct InventarioScreen* s) {
	cc_string name;
	if (s->held != BLOCK_AIR) {
		name = Survival_BlockName(s->held);
		if (s->mode == MODE_FORNALHA) {
			String_Format1(&s->msg, "&8Na mao: &1%s&8. Toque em Fundir ou Fogo", &name);
		} else if (Equip_ArmorPiece(s->held) >= 0) {
			String_Format1(&s->msg, "&8%s: toque em Usar para vestir", &name);
		} else if (Survival_IsFood(s->held)) {
			String_Format1(&s->msg, "&8%s: toque em Usar para comer", &name);
		} else {
			String_Format1(&s->msg, "&8Na mao: &1%s&8. Toque na barra", &name);
		}
		return;
	}

	switch (s->mode)
	{
	case MODE_INV:      String_AppendConst(&s->msg, "&8Toque num item para escolher"); break;
	case MODE_BAU:      String_AppendConst(&s->msg, "&8Toque num item para guardar ou pegar"); break;
	case MODE_FORNALHA: String_AppendConst(&s->msg, "&8Ponha minerio/comida e combustivel"); break;
	}
}

static void InventarioScreen_Refresh(struct InventarioScreen* s) {
	int i;
	s->itemsCount = 0;
	for (i = 1; i < BLOCK_COUNT; i++)
	{
		if (Survival_GetCount((BlockID)i) > 0) s->items[s->itemsCount++] = (BlockID)i;
	}
	if (s->held != BLOCK_AIR && !Survival_GetCount(s->held)) s->held = BLOCK_AIR;

	s->invPage = min(s->invPage, IS_Pages(s) - 1);
	if (!s->msg.length) InventarioScreen_SetDefaultMessage(s);
	s->dirty = true;
}

static int IS_MoveAmount(struct InventarioScreen* s) { return s->moveOne ? 1 : BAU_STACK; }


/*########################################################################################################################*
*--------------------------------------------------------Actions----------------------------------------------------------*
*#########################################################################################################################*/
static void InventarioScreen_Close(struct InventarioScreen* s) { Gui_Remove((struct Screen*)s); }

static void IS_SayMoved(struct InventarioScreen* s, int count, BlockID block, const char* where) {
	cc_string name = Survival_BlockName(block);
	String_Format3(&s->msg, "&2%i %s %c", &count, &name, where);
}

/* Puts the tapped item on the hotbar slot, swapping places if it already is on the hotbar */
static void InventarioScreen_TapHotbar(struct InventarioScreen* s, int slot) {
	BlockID old = Inventory_Get(slot);
	int i;

	if (s->held == BLOCK_AIR) {
		Inventory_SetSelectedIndex(slot);
		return;
	}
	for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR; i++)
	{
		if (i != slot && Inventory_Get(i) == s->held) Inventory_Set(i, old);
	}
	Inventory_Set(slot, s->held);
	Inventory_SetSelectedIndex(slot);
	Event_RaiseVoid(&UserEvents.HeldBlockChanged);
	s->held = BLOCK_AIR;
}

static void InventarioScreen_TapArmor(struct InventarioScreen* s, int piece) {
	if (s->held != BLOCK_AIR && Equip_ArmorPiece(s->held) == piece) {
		Equip_Wear(s->held);
		String_AppendConst(&s->msg, "&2Vestiu!");
		s->held = BLOCK_AIR;
	} else if (s->held != BLOCK_AIR) {
		String_AppendConst(&s->msg, "&cEsse item nao vai nesse lugar");
	} else if (Equip_TakeOff(piece)) {
		String_AppendConst(&s->msg, "&2Voce tirou a armadura");
	}
}

static void InventarioScreen_Use(struct InventarioScreen* s) {
	if (s->held == BLOCK_AIR) {
		String_AppendConst(&s->msg, "&cEscolha um item primeiro"); return;
	}
	if (Equip_ArmorPiece(s->held) >= 0) {
		Equip_Wear(s->held);
		String_AppendConst(&s->msg, "&2Vestiu!");
		s->held = BLOCK_AIR;
	} else if (!Survival_EatItem(s->held, &s->msg)) {
		String_AppendConst(&s->msg, "&cEsse item nao se usa aqui");
	}
}

/* Moves the player's item into the chest, filling stacks of it first */
static void InventarioScreen_StoreItem(struct InventarioScreen* s, BlockID block) {
	int pass, i, moved = 0, want = IS_MoveAmount(s);
	struct EquipSlot* slots = s->bau->slots;

	for (pass = 0; pass < 2 && moved < want; pass++)
		for (i = 0; i < BAU_SLOTS && moved < want; i++)
		{
			if (pass == 0 && slots[i].block != block) continue;
			moved += Equip_PutInSlot(&slots[i], block, want - moved);
		}

	if (moved) IS_SayMoved(s, moved, block, "guardado(s) no bau");
	else String_AppendConst(&s->msg, "&cO bau esta cheio!");
}

static void InventarioScreen_TakeSlot(struct InventarioScreen* s, struct EquipSlot* slot) {
	BlockID block = slot->block;
	int moved;
	if (block == BLOCK_AIR) return;

	moved = Equip_TakeFromSlot(slot, IS_MoveAmount(s));
	if (moved) IS_SayMoved(s, moved, block, "de volta para voce");
	else String_AppendConst(&s->msg, "&cVoce ja tem muitos desse item");
}

static void InventarioScreen_PutFurnace(struct InventarioScreen* s, int which, BlockID block) {
	struct EquipSlot* slot = &s->fornalha->slots[which];
	int moved;

	if (which == FORNALHA_INPUT && Equip_SmeltResult(block) == BLOCK_AIR) {
		String_AppendConst(&s->msg, "&cIsso nao derrete nem cozinha"); return;
	}
	if (which == FORNALHA_FUEL && Equip_FuelTime(block) <= 0) {
		String_AppendConst(&s->msg, "&cIsso nao queima"); return;
	}
	moved = Equip_PutInSlot(slot, block, IS_MoveAmount(s));
	if (moved) IS_SayMoved(s, moved, block, which == FORNALHA_INPUT ? "para fundir" : "para o fogo");
	else String_AppendConst(&s->msg, "&cJa tem outra coisa ai");
	s->held = BLOCK_AIR;
}

static void InventarioScreen_TapItem(struct InventarioScreen* s, int index) {
	BlockID block;
	cc_bool smelts, burns;
	if (index >= s->itemsCount) { s->held = BLOCK_AIR; return; }
	block = s->items[index];

	if (s->mode == MODE_BAU) { InventarioScreen_StoreItem(s, block); return; }
	if (s->mode == MODE_FORNALHA) {
		smelts = Equip_SmeltResult(block) != BLOCK_AIR;
		burns  = Equip_FuelTime(block) > 0;
		/* Only ask where it goes when it could go in either slot (e.g. logs) */
		if (smelts && !burns) { InventarioScreen_PutFurnace(s, FORNALHA_INPUT, block); return; }
		if (burns && !smelts) { InventarioScreen_PutFurnace(s, FORNALHA_FUEL,  block); return; }
		if (!smelts) { String_AppendConst(&s->msg, "&cIsso nao derrete nem queima"); return; }
	}
	s->held = s->held == block ? BLOCK_AIR : block;
}

static void InventarioScreen_TapFurnaceSlot(struct InventarioScreen* s, int which) {
	if (which != FORNALHA_OUTPUT && s->held != BLOCK_AIR) {
		InventarioScreen_PutFurnace(s, which, s->held);
	} else {
		InventarioScreen_TakeSlot(s, &s->fornalha->slots[which]);
	}
}


/*########################################################################################################################*
*--------------------------------------------------------Drawing----------------------------------------------------------*
*#########################################################################################################################*/
#define IS_PANEL     BitmapColor_RGB(198, 198, 198)
#define IS_LIGHT     BitmapColor_RGB(255, 255, 255)
#define IS_SHADOW    BitmapColor_RGB( 85,  85,  85)
#define IS_SLOT_BACK BitmapColor_RGB(139, 139, 139)
#define IS_SLOT_DARK BitmapColor_RGB( 55,  55,  55)
#define IS_HELD_BACK BitmapColor_RGB(250, 250, 200)

static int IS_X(struct InventarioScreen* s, int x) { return (int)(x * s->scale); }
static int IS_Y(struct InventarioScreen* s, int y) { return (int)(y * s->scale); }

static void IS_Fill(struct InventarioScreen* s, struct Context2D* ctx, BitmapCol col, int x, int y, int w, int h) {
	int x1 = IS_X(s, x), y1 = IS_Y(s, y);
	Context2D_Clear(ctx, col, x1, y1, IS_X(s, x + w) - x1, IS_Y(s, y + h) - y1);
}

/* Draws a raised (buttons, panel) or sunken (slots) box with bevelled edges */
static void IS_Box(struct InventarioScreen* s, struct Context2D* ctx, int x, int y, int w, int h,
					BitmapCol fill, cc_bool sunken) {
	BitmapCol tl = sunken ? IS_SLOT_DARK : IS_LIGHT;
	BitmapCol br = sunken ? IS_LIGHT : IS_SHADOW;
	int b = max(1, (int)s->scale);
	int x1 = IS_X(s, x), y1 = IS_Y(s, y), x2 = IS_X(s, x + w), y2 = IS_Y(s, y + h);

	Context2D_Clear(ctx, fill, x1, y1, x2 - x1, y2 - y1);
	Context2D_Clear(ctx, tl, x1,     y1,     x2 - x1, b);
	Context2D_Clear(ctx, tl, x1,     y1,     b,       y2 - y1);
	Context2D_Clear(ctx, br, x1,     y2 - b, x2 - x1, b);
	Context2D_Clear(ctx, br, x2 - b, y1,     b,       y2 - y1);
}

static void IS_Text(struct InventarioScreen* s, struct Context2D* ctx, const cc_string* text,
					int x, int y, int align, cc_bool shadow) {
	struct DrawTextArgs args;
	int width;
	DrawTextArgs_Make(&args, text, &s->font, shadow);
	width = Drawer2D_TextWidth(&args);

	x = IS_X(s, x);
	if (align == 1) x -= width / 2;
	if (align == 2) x -= width;
	Context2D_DrawText(ctx, &args, max(0, x), IS_Y(s, y));
}

static void IS_TextConst(struct InventarioScreen* s, struct Context2D* ctx, const char* text,
						int x, int y, int align, cc_bool shadow) {
	cc_string str = String_FromReadonly(text);
	IS_Text(s, ctx, &str, x, y, align, shadow);
}

/* Small white number in the bottom right corner of a slot, like in Minecraft */
static void IS_Count(struct InventarioScreen* s, struct Context2D* ctx, int count, int x, int y, int size) {
	struct DrawTextArgs args;
	cc_string str; char strBuffer[16];
	int width, height;
	if (count <= 1) return;

	String_InitArray(str, strBuffer);
	String_AppendInt(&str, count);
	DrawTextArgs_Make(&args, &str, &s->font, true);
	width  = Drawer2D_TextWidth(&args);
	height = Drawer2D_TextHeight(&args);
	Context2D_DrawText(ctx, &args, IS_X(s, x + size) - width, IS_Y(s, y + size) - height + (int)s->scale);
}

/* Draws a small pixel art picture ('#' = colour), centred in a slot */
static void IS_Pixels(struct InventarioScreen* s, struct Context2D* ctx, const char* const* rows, int w, int h,
					BitmapCol col, int x, int y, int size) {
	int px = max(1, (int)s->scale);
	int i, j, x0, y0;

	x0 = IS_X(s, x + size / 2) - (w * px) / 2;
	y0 = IS_Y(s, y + size / 2) - (h * px) / 2;
	for (j = 0; j < h; j++)
		for (i = 0; i < w; i++)
		{
			if (rows[j][i] != '.') Context2D_Clear(ctx, col, x0 + i * px, y0 + j * px, px, px);
		}
}

/* Outlines of the armour pieces, shown in empty armour slots */
static const char* const armorOutlines[ARMOR_PIECES][10] = {
	{ "..........", "...####...", "..#....#..", ".#......#.", ".#.####.#.", ".#.#..#.#.", ".###..###.", "..........", "..........", ".........." },
	{ ".##....##.", "#..####..#", "#........#", "####..####", "...#..#...", "...#..#...", "...#..#...", "...####...", "..........", ".........." },
	{ "..######..", "..#....#..", "..#.##.#..", "..#.##.#..", "..#.##.#..", "..#.##.#..", "..##..##..", "..........", "..........", ".........." },
	{ "..........", "..........", "..#....#..", "..#....#..", "..#....#..", ".##....##.", "#.#...#.#.", "###..####.", "..........", ".........." },
};

/* Small pixel art pictures of the tools (h = head, s = stick), since tools aren't blocks */
#define IS_TOOL_PX 12
static const char* const is_toolArt[4][IS_TOOL_PX] = {
	{ "...hhhhh....", "..h....hh...", "......s.hh..", ".....s...h..", "....s....hh.", "...s......h.",
	  "..s.......h.", ".s..........", "s...........", "............", "............", "............" },
	{ "......hh....", ".....hhhh...", "....shhhhh..", "...s.hhhh...", "..s...hh....", ".s..........",
	  "s...........", "............", "............", "............", "............", "............" },
	{ "........hhh.", ".......hhhh.", ".......hhh..", "......s.....", ".....s......", "....s.......",
	  "...s........", "..s.........", ".s..........", "s...........", "............", "............" },
	{ "..........hh", ".........hhh", "........hhh.", ".......hhh..", "......hhh...", "..s..hhh....",
	  "...shhh.....", "....ss......", "...s..s.....", "..s.........", "ss..........", "............" },
};
static const char* const toolTierNames[5] = { "nenhuma", "madeira", "pedra", "ferro", "diamante" };
static const char* const toolKindNames[4] = { "Picareta", "Machado", "Pa", "Espada" };

static BitmapCol IS_ToolColor(int tier) {
	switch (tier)
	{
	case 1: return BitmapColor_RGB(176, 132,  80);
	case 2: return BitmapColor_RGB(125, 125, 125);
	case 3: return BitmapColor_RGB(230, 230, 230);
	case 4: return BitmapColor_RGB( 90, 230, 225);
	}
	return BitmapColor_RGB(120, 120, 120);
}

static void IS_Tool(struct InventarioScreen* s, struct Context2D* ctx, int kind, int tier, int x, int y) {
	BitmapCol head  = IS_ToolColor(tier);
	BitmapCol stick = tier ? BitmapColor_RGB(110, 75, 40) : BitmapColor_RGB(120, 120, 120);
	BitmapCol line  = tier ? BitmapColor_RGB(40, 30, 20)  : BitmapColor_RGB(139, 139, 139);
	int px = max(1, (int)s->scale);
	int i, j, x0, y0;
	char c;

	x0 = IS_X(s, x + IS_SLOT / 2) - (IS_TOOL_PX * px) / 2;
	y0 = IS_Y(s, y + IS_SLOT / 2) - (IS_TOOL_PX * px) / 2 + px;

	for (j = 0; j < IS_TOOL_PX; j++)
		for (i = 0; i < IS_TOOL_PX; i++)
		{
			if (is_toolArt[kind][j][i] == '.') continue;
			Context2D_Clear(ctx, line, x0 + i * px - 1, y0 + j * px - 1, px + 2, px + 2);
		}
	if (!tier) return; /* just the outline when the player has none */

	for (j = 0; j < IS_TOOL_PX; j++)
		for (i = 0; i < IS_TOOL_PX; i++)
		{
			c = is_toolArt[kind][j][i];
			if (c == '.') continue;
			Context2D_Clear(ctx, c == 'h' ? head : stick, x0 + i * px, y0 + j * px, px, px);
		}
}

/* Draws a triangle pointing left (0), right (1), up (2) or down (3), centred on the given point */
static void IS_Triangle(struct InventarioScreen* s, struct Context2D* ctx, int cx, int cy, int dir) {
	BitmapCol col = BitmapColor_RGB(60, 60, 60);
	int i, len = 6;
	for (i = 0; i < len; i++)
	{
		int a = dir == 0 || dir == 2 ? i : len - 1 - i;
		if (dir < 2) IS_Fill(s, ctx, col, cx - len / 2 + i, cy - a, 1, a * 2 + 1);
		else         IS_Fill(s, ctx, col, cx - a, cy - len / 2 + i, a * 2 + 1, 1);
	}
}

/* A thin bar showing how much is left of something (e.g. a tool's uses) */
static void IS_Bar(struct InventarioScreen* s, struct Context2D* ctx, int x, int y, int w, int value, int max) {
	int filled = max > 0 ? w * value / max : 0;
	BitmapCol col = value * 4 < max ? BitmapColor_RGB(220, 60, 50) : BitmapColor_RGB(80, 200, 70);
	IS_Fill(s, ctx, BitmapColor_RGB(40, 40, 40), x, y, w, 3);
	IS_Fill(s, ctx, col, x, y, filled, 3);
}

static BitmapCol IS_ArmorColor(BlockID block) {
	switch (Equip_ArmorTier(block))
	{
	case ARMOR_TIER_LA:       return BitmapColor_RGB(235, 232, 220);
	case ARMOR_TIER_OURO:     return BitmapColor_RGB(240, 195,  50);
	case ARMOR_TIER_FERRO:    return BitmapColor_RGB(200, 200, 205);
	case ARMOR_TIER_DIAMANTE: return BitmapColor_RGB( 80, 220, 215);
	case ARMOR_TIER_TITANIO:  return BitmapColor_RGB(130, 150, 185);
	}
	return 0;
}

/* A little blocky player figure, showing the armour being worn */
static void IS_Doll(struct InventarioScreen* s, struct Context2D* ctx) {
	BitmapCol skin  = BitmapColor_RGB(222, 170, 130);
	BitmapCol shirt = BitmapColor_RGB( 60, 140, 200);
	BitmapCol pants = BitmapColor_RGB( 60,  60, 150);
	BitmapCol shoes = BitmapColor_RGB( 70,  60,  50);
	BitmapCol hair  = BitmapColor_RGB( 90,  60,  35);
	BitmapCol col;
	int x = IS_DOLL_X + 18, y = IS_DOLL_Y + 4;

	IS_Box(s, ctx, IS_DOLL_X, IS_DOLL_Y, 48, 64, BitmapColor_RGB(105, 105, 115), true);

	IS_Fill(s, ctx, skin,  x,      y,      12, 12); /* head */
	IS_Fill(s, ctx, hair,  x,      y,      12, 3);
	IS_Fill(s, ctx, BitmapColor_RGB(40, 40, 60), x + 3, y + 6, 2, 2); /* eyes */
	IS_Fill(s, ctx, BitmapColor_RGB(40, 40, 60), x + 7, y + 6, 2, 2);
	IS_Fill(s, ctx, shirt, x,      y + 12, 12, 18); /* body */
	IS_Fill(s, ctx, shirt, x - 5,  y + 12, 5,  10); /* arms */
	IS_Fill(s, ctx, shirt, x + 12, y + 12, 5,  10);
	IS_Fill(s, ctx, skin,  x - 5,  y + 22, 5,  8);
	IS_Fill(s, ctx, skin,  x + 12, y + 22, 5,  8);
	IS_Fill(s, ctx, pants, x,      y + 30, 12, 26); /* legs */
	IS_Fill(s, ctx, BitmapColor_RGB(45, 45, 115), x + 6, y + 31, 1, 25);
	IS_Fill(s, ctx, shoes, x,      y + 52, 12, 4);

	if ((col = IS_ArmorColor(Equip_Worn(ARMOR_HELMET)))) {
		IS_Fill(s, ctx, col, x - 1, y - 1, 14, 5);
		IS_Fill(s, ctx, col, x - 1, y + 4, 2, 6);
		IS_Fill(s, ctx, col, x + 11, y + 4, 2, 6);
	}
	if ((col = IS_ArmorColor(Equip_Worn(ARMOR_CHEST)))) {
		IS_Fill(s, ctx, col, x - 1, y + 12, 14, 18);
		IS_Fill(s, ctx, col, x - 6, y + 12, 6, 8);
		IS_Fill(s, ctx, col, x + 12, y + 12, 6, 8);
	}
	if ((col = IS_ArmorColor(Equip_Worn(ARMOR_LEGS)))) {
		IS_Fill(s, ctx, col, x - 1, y + 30, 14, 16);
		IS_Fill(s, ctx, BitmapColor_RGB(60, 60, 60), x + 6, y + 34, 1, 12);
	}
	if ((col = IS_ArmorColor(Equip_Worn(ARMOR_BOOTS)))) {
		IS_Fill(s, ctx, col, x - 1, y + 48, 14, 9);
		IS_Fill(s, ctx, BitmapColor_RGB(60, 60, 60), x + 6, y + 48, 1, 9);
	}
}

static void IS_DrawItemsBack(struct InventarioScreen* s, struct Context2D* ctx) {
	int i, x, y, index, page = IS_ItemsPage(s);
	BitmapCol fill;

	for (i = 0; i < page; i++)
	{
		index = s->invPage * page + i;
		x = IS_ITEMS_X + (i % IS_ITEMS_COLS) * IS_SLOT;
		y = IS_ItemsY(s) + (i / IS_ITEMS_COLS) * IS_SLOT;

		fill = IS_SLOT_BACK;
		if (index < s->itemsCount && s->items[index] == s->held) fill = IS_HELD_BACK;
		IS_Box(s, ctx, x, y, IS_SLOT, IS_SLOT, fill, true);
	}
}

static void IS_DrawButtons(struct InventarioScreen* s, struct Context2D* ctx) {
	cc_string str; char strBuffer[64];
	const struct InvButtonDef* btns;
	int i, count, page, pages, y = IS_ButtonsY(s);
	const char* text;

	btns = IS_Buttons(s, &count);
	for (i = 0; i < count; i++)
	{
		IS_Box(s, ctx, btns[i].x, y, btns[i].w, 40, BitmapColor_RGB(170, 170, 170), false);
		switch (btns[i].id)
		{
		case IS_BTN_PREV:  IS_Triangle(s, ctx, btns[i].x + btns[i].w / 2, y + 20, 0); continue;
		case IS_BTN_NEXT:  IS_Triangle(s, ctx, btns[i].x + btns[i].w / 2, y + 20, 1); continue;
		case IS_BTN_USE:   text = "Usar";     break;
		case IS_BTN_CRAFT: text = "Crafting"; break;
		case IS_BTN_MOVE:  text = s->moveOne ? "Mover: 1" : "Mover: tudo"; break;
		default:           text = "Fechar";   break;
		}
		IS_TextConst(s, ctx, text, btns[i].x + btns[i].w / 2, y + 15, 1, true);
	}

	String_InitArray(str, strBuffer);
	page = s->invPage + 1; pages = IS_Pages(s);
	if (s->mode == MODE_INV) {
		String_Format2(&str, "&8Seus itens %i/%i", &page, &pages);
		IS_Text(s, ctx, &str, IS_ITEMS_X, IS_INV_ITEMS_Y - 12, 0, false);
	} else {
		String_Format2(&str, "&8Itens %i/%i", &page, &pages);
		IS_Text(s, ctx, &str, 76, y + 15, 1, false);
	}
}

static void IS_DrawInventoryBack(struct InventarioScreen* s, struct Context2D* ctx) {
	cc_string str; char strBuffer[64];
	int i, x, tier, uses, maxUses, defense, percent, health, food;
	BitmapCol fill;

	IS_TextConst(s, ctx, "&8Inventario", IS_DOLL_X, 3, 0, false);
	IS_Doll(s, ctx);

	/* Armour slots: helmet, chestplate / leggings, boots */
	for (i = 0; i < ARMOR_PIECES; i++)
	{
		int ax = IS_ARMOR_X + (i % 2) * IS_SLOT, ay = IS_ARMOR_Y + (i / 2) * IS_SLOT;
		fill = IS_SLOT_BACK;
		if (s->held != BLOCK_AIR && Equip_ArmorPiece(s->held) == i) fill = BitmapColor_RGB(150, 200, 140);
		IS_Box(s, ctx, ax, ay, IS_SLOT, IS_SLOT, fill, true);
		if (Equip_Worn(i) == BLOCK_AIR) {
			IS_Pixels(s, ctx, armorOutlines[i], 10, 10, BitmapColor_RGB(110, 110, 110), ax, ay, IS_SLOT);
		} else {
			IS_Bar(s, ctx, ax + 3, ay + IS_SLOT - 5, IS_SLOT - 6, Equip_WornUses(i), Equip_MaxUses(Equip_Worn(i)));
		}
	}

	String_InitArray(str, strBuffer);

	/* Tools */
	IS_TextConst(s, ctx, "&8Ferramentas", IS_TOOLS_X, 3, 0, false);
	for (i = 0; i < 4; i++)
	{
		x = IS_TOOLS_X + i * 48;
		Survival_ToolInfo(i, &tier, &uses, &maxUses);
		IS_Box(s, ctx, x, IS_TOOLS_Y, IS_SLOT, IS_SLOT, IS_SLOT_BACK, true);
		IS_Tool(s, ctx, i, tier, x, IS_TOOLS_Y);

		str.length = 0;
		String_Format1(&str, tier ? "&0%c" : "&7%c", toolTierNames[tier]);
		IS_Text(s, ctx, &str, x + IS_SLOT / 2, IS_TOOLS_Y + IS_SLOT + 1, 1, false);
		if (tier) IS_Bar(s, ctx, x - 6, IS_TOOLS_Y + IS_SLOT + 14, IS_SLOT + 12, uses, maxUses);
	}

	health = Survival_GetHealth(); food = Survival_GetFood();
	str.length = 0;
	String_Format2(&str, "&4Vida %i/20  &6Fome %i/20", &health, &food);
	IS_Text(s, ctx, &str, IS_TOOLS_X, 60, 0, false);

	defense = Equip_Defense();
	percent = Equip_DefensePercent();
	str.length = 0;
	String_Format2(&str, "&8Defesa &1%i &8(-%i", &defense, &percent);
	String_AppendConst(&str, "% de dano)");
	IS_Text(s, ctx, &str, IS_TOOLS_X, 72, 0, false);

	IS_DrawItemsBack(s, ctx);

	/* Hotbar */
	for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR; i++)
	{
		x = IS_HOTBAR_X + i * IS_SLOT;
		fill = i == Inventory.SelectedIndex ? BitmapColor_RGB(175, 175, 150) : IS_SLOT_BACK;
		IS_Box(s, ctx, x, IS_HOTBAR_Y, IS_SLOT, IS_SLOT, fill, true);
	}
	IS_TextConst(s, ctx, "&8Barra", IS_HOTBAR_X - 4, IS_HOTBAR_Y + 6, 2, false);
}

static void IS_DrawBauBack(struct InventarioScreen* s, struct Context2D* ctx) {
	int i;
	IS_TextConst(s, ctx, "&8Bau", IS_BAU_X, 3, 0, false);
	for (i = 0; i < BAU_SLOTS; i++)
	{
		IS_Box(s, ctx, IS_BAU_X + (i % IS_BAU_COLS) * IS_SLOT, IS_BAU_Y + (i / IS_BAU_COLS) * IS_SLOT,
			IS_SLOT, IS_SLOT, BitmapColor_RGB(150, 120, 90), true);
	}
	IS_DrawItemsBack(s, ctx);
}

static void IS_DrawFlame(struct InventarioScreen* s, struct Context2D* ctx, float fraction) {
	int i, h, total = 18, lit = (int)(total * fraction + 0.99f);
	int x = IS_IN_X + 3, y = IS_FLAME_Y + 2;

	for (i = 0; i < 9; i++)
	{
		/* Flame shaped columns, tallest in the middle */
		h = total - Math_AbsI(i - 4) * 3;
		IS_Fill(s, ctx, BitmapColor_RGB(150, 150, 150), x + i * 2, y + total - h, 2, h);
		if (!lit) continue;
		h = min(h, lit);
		IS_Fill(s, ctx, i % 2 ? BitmapColor_RGB(255, 200, 60) : BitmapColor_RGB(250, 120, 30),
			x + i * 2, y + total - h, 2, h);
	}
}

static void IS_DrawFornalhaBack(struct InventarioScreen* s, struct Context2D* ctx) {
	struct EquipFornalha* f = s->fornalha;
	int i, filled, cy = IS_OUT_Y + IS_OUT_SIZE / 2;
	BitmapCol fill;

	IS_TextConst(s, ctx, "&8Fornalha", 8, 3, 0, false);
	IS_TextConst(s, ctx, "&8Fundir", IS_IN_X - 4, IS_IN_Y + 6, 2, false);
	IS_TextConst(s, ctx, "&8Fogo",   IS_IN_X - 4, IS_FUEL_Y + 6, 2, false);

	fill = s->held != BLOCK_AIR ? BitmapColor_RGB(150, 200, 140) : IS_SLOT_BACK;
	IS_Box(s, ctx, IS_IN_X, IS_IN_Y,   IS_SLOT, IS_SLOT, fill, true);
	IS_Box(s, ctx, IS_IN_X, IS_FUEL_Y, IS_SLOT, IS_SLOT, fill, true);
	IS_DrawFlame(s, ctx, f->burnMax > 0 ? f->burnLeft / f->burnMax : 0);

	/* Arrow, filled in as the item cooks */
	filled = (int)(40 * f->cook / FORNALHA_COOK_TIME);
	IS_Fill(s, ctx, IS_SLOT_BACK, IS_ARROW_X, cy - 3, 32, 6);
	for (i = 0; i < 9; i++) IS_Fill(s, ctx, IS_SLOT_BACK, IS_ARROW_X + 32 + i, cy - 9 + i, 1, 18 - i * 2);
	IS_Fill(s, ctx, BitmapColor_RGB(250, 250, 250), IS_ARROW_X, cy - 3, min(filled, 32), 6);
	for (i = 0; i < 9 && 32 + i < filled; i++)
	{
		IS_Fill(s, ctx, BitmapColor_RGB(250, 250, 250), IS_ARROW_X + 32 + i, cy - 9 + i, 1, 18 - i * 2);
	}

	IS_Box(s, ctx, IS_OUT_X, IS_OUT_Y, IS_OUT_SIZE, IS_OUT_SIZE, IS_SLOT_BACK, true);

	IS_TextConst(s, ctx, "&0Derrete/cozinha:", 190, 14, 0, false);
	IS_TextConst(s, ctx, "&8Minerio > barra",  190, 27, 0, false);
	IS_TextConst(s, ctx, "&8Areia > vidro",    190, 39, 0, false);
	IS_TextConst(s, ctx, "&8Carne > bife",     190, 51, 0, false);
	IS_TextConst(s, ctx, "&8Tronco > carvao",  190, 63, 0, false);
	IS_TextConst(s, ctx, "&0Fogo: carvao, madeira", 190, 77, 0, false);

	IS_DrawItemsBack(s, ctx);
}

static void InventarioScreen_DrawBack(struct InventarioScreen* s, struct Context2D* ctx) {
	IS_Box(s, ctx, 0, 0, IS_WIDTH, IS_HEIGHT, IS_PANEL, false);
	switch (s->mode)
	{
	case MODE_INV:      IS_DrawInventoryBack(s, ctx); break;
	case MODE_BAU:      IS_DrawBauBack(s, ctx);       break;
	case MODE_FORNALHA: IS_DrawFornalhaBack(s, ctx);  break;
	}
	IS_DrawButtons(s, ctx);
}

static void InventarioScreen_DrawFront(struct InventarioScreen* s, struct Context2D* ctx) {
	int i, index, page = IS_ItemsPage(s);
	struct EquipSlot* slots;

	for (i = 0; i < page; i++)
	{
		index = s->invPage * page + i;
		if (index >= s->itemsCount) break;
		IS_Count(s, ctx, Survival_GetCount(s->items[index]), IS_ITEMS_X + (i % IS_ITEMS_COLS) * IS_SLOT,
			IS_ItemsY(s) + (i / IS_ITEMS_COLS) * IS_SLOT, IS_SLOT);
	}

	if (s->mode == MODE_INV) {
		for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR; i++)
		{
			IS_Count(s, ctx, Survival_GetCount(Inventory_Get(i)), IS_HOTBAR_X + i * IS_SLOT, IS_HOTBAR_Y, IS_SLOT);
		}
	} else if (s->mode == MODE_BAU) {
		for (i = 0; i < BAU_SLOTS; i++)
		{
			IS_Count(s, ctx, s->bau->slots[i].count, IS_BAU_X + (i % IS_BAU_COLS) * IS_SLOT,
				IS_BAU_Y + (i / IS_BAU_COLS) * IS_SLOT, IS_SLOT);
		}
	} else {
		slots = s->fornalha->slots;
		IS_Count(s, ctx, slots[FORNALHA_INPUT].count,  IS_IN_X,  IS_IN_Y,   IS_SLOT);
		IS_Count(s, ctx, slots[FORNALHA_FUEL].count,   IS_IN_X,  IS_FUEL_Y, IS_SLOT);
		IS_Count(s, ctx, slots[FORNALHA_OUTPUT].count, IS_OUT_X, IS_OUT_Y,  IS_OUT_SIZE);
	}

	IS_Text(s, ctx, &s->msg, IS_WIDTH / 2, s->mode == MODE_INV ? 148 : 92, 1, false);
}

static void InventarioScreen_MakeTexture(struct InventarioScreen* s, struct Texture* tex, cc_bool front) {
	struct Context2D ctx;
	int width  = IS_X(s, IS_WIDTH);
	int height = IS_Y(s, IS_HEIGHT);

	Gfx_DeleteTexture(&tex->ID);
	Context2D_Alloc(&ctx, width, height);
	{
		if (front) InventarioScreen_DrawFront(s, &ctx);
		else       InventarioScreen_DrawBack(s, &ctx);
		Context2D_MakeTexture(tex, &ctx);
	}
	Context2D_Free(&ctx);
	tex->x = s->ox; tex->y = s->oy;
}

static void IS_AddIcon(struct InventarioScreen* s, BlockID block, int x, int y, int size, float iconSize) {
	if (block == BLOCK_AIR) return;
	IsometricDrawer_AddBatch(block, iconSize * s->scale,
		s->ox + (x + size / 2.0f) * s->scale, s->oy + (y + size / 2.0f) * s->scale);
}

static void InventarioScreen_BuildMesh(void* screen) {
	struct InventarioScreen* s = (struct InventarioScreen*)screen;
	struct VertexTextured* data;
	struct EquipSlot* slots;
	int i, index, page = IS_ItemsPage(s);

	InventarioScreen_MakeTexture(s, &s->back,  false);
	InventarioScreen_MakeTexture(s, &s->front, true);
	if (!s->vb) return;

	data = Screen_LockVb(s);
	IsometricDrawer_BeginBatch(data, s->state);

	for (i = 0; i < page; i++)
	{
		index = s->invPage * page + i;
		if (index >= s->itemsCount) break;
		IS_AddIcon(s, s->items[index], IS_ITEMS_X + (i % IS_ITEMS_COLS) * IS_SLOT,
			IS_ItemsY(s) + (i / IS_ITEMS_COLS) * IS_SLOT, IS_SLOT, 7.5f);
	}

	if (s->mode == MODE_INV) {
		for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR; i++)
		{
			IS_AddIcon(s, Inventory_Get(i), IS_HOTBAR_X + i * IS_SLOT, IS_HOTBAR_Y, IS_SLOT, 7.5f);
		}
		for (i = 0; i < ARMOR_PIECES; i++)
		{
			IS_AddIcon(s, Equip_Worn(i), IS_ARMOR_X + (i % 2) * IS_SLOT, IS_ARMOR_Y + (i / 2) * IS_SLOT - 2, IS_SLOT, 7.5f);
		}
	} else if (s->mode == MODE_BAU) {
		for (i = 0; i < BAU_SLOTS; i++)
		{
			IS_AddIcon(s, s->bau->slots[i].block, IS_BAU_X + (i % IS_BAU_COLS) * IS_SLOT,
				IS_BAU_Y + (i / IS_BAU_COLS) * IS_SLOT, IS_SLOT, 7.5f);
		}
	} else {
		slots = s->fornalha->slots;
		IS_AddIcon(s, slots[FORNALHA_INPUT].block,  IS_IN_X,  IS_IN_Y,   IS_SLOT,     7.5f);
		IS_AddIcon(s, slots[FORNALHA_FUEL].block,   IS_IN_X,  IS_FUEL_Y, IS_SLOT,     7.5f);
		IS_AddIcon(s, slots[FORNALHA_OUTPUT].block, IS_OUT_X, IS_OUT_Y,  IS_OUT_SIZE, 10.0f);
	}

	s->iconVertices = IsometricDrawer_EndBatch();
	Gfx_UnlockDynamicVb(s->vb);
}

static void InventarioScreen_Render(void* screen, float delta) {
	struct InventarioScreen* s = (struct InventarioScreen*)screen;
	Texture_Render(&s->back);

	if (s->iconVertices) {
		Gfx_SetVertexFormat(VERTEX_FORMAT_TEXTURED);
		Gfx_BindDynamicVb(s->vb);
		IsometricDrawer_Render(s->iconVertices, 0, s->state);
	}
	Texture_Render(&s->front);
}

/* The furnace keeps working while its screen is open, so redraw it now and then */
static void InventarioScreen_Update(void* screen, float delta) {
	struct InventarioScreen* s = (struct InventarioScreen*)screen;
	if (s->mode != MODE_FORNALHA) return;

	s->redrawTimer += delta;
	if (s->redrawTimer < 0.25f) return;
	s->redrawTimer = 0;
	InventarioScreen_Refresh(s);
}


/*########################################################################################################################*
*---------------------------------------------------------Input-----------------------------------------------------------*
*#########################################################################################################################*/
static cc_bool IS_In(int x, int y, int rx, int ry, int rw, int rh) {
	return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

/* Returns the index of the slot of a grid of slots at the given position, or -1 if none */
static int IS_SlotAt(int x, int y, int gx, int gy, int cols, int rows) {
	if (!IS_In(x, y, gx, gy, cols * IS_SLOT, rows * IS_SLOT)) return -1;
	return ((y - gy) / IS_SLOT) * cols + (x - gx) / IS_SLOT;
}

static void IS_ToPanel(struct InventarioScreen* s, int* x, int* y) {
	*x = (int)((*x - s->ox) / s->scale);
	*y = (int)((*y - s->oy) / s->scale);
}

static cc_bool InventarioScreen_TapButton(struct InventarioScreen* s, int x, int y) {
	const struct InvButtonDef* btns;
	int i, count, pages = IS_Pages(s);

	btns = IS_Buttons(s, &count);
	for (i = 0; i < count; i++)
	{
		if (!IS_In(x, y, btns[i].x, IS_ButtonsY(s), btns[i].w, 40)) continue;

		switch (btns[i].id)
		{
		case IS_BTN_PREV:  s->invPage = (s->invPage + pages - 1) % pages; break;
		case IS_BTN_NEXT:  s->invPage = (s->invPage + 1) % pages; break;
		case IS_BTN_USE:   InventarioScreen_Use(s); break;
		case IS_BTN_MOVE:  s->moveOne = !s->moveOne; break;
		case IS_BTN_CRAFT: InventarioScreen_Close(s); CraftingScreen_Show(); return true;
		case IS_BTN_CLOSE: InventarioScreen_Close(s); return true;
		}
	}
	return false;
}

static int InventarioScreen_PointerDown(void* screen, int id, int x, int y) {
	struct InventarioScreen* s = (struct InventarioScreen*)screen;
	int slot;
	IS_ToPanel(s, &x, &y);
	s->msg.length = 0;

	if ((slot = IS_SlotAt(x, y, IS_ITEMS_X, IS_ItemsY(s), IS_ITEMS_COLS, IS_ItemsRows(s))) >= 0) {
		InventarioScreen_TapItem(s, s->invPage * IS_ItemsPage(s) + slot);
	} else if (s->mode == MODE_INV) {
		if ((slot = IS_SlotAt(x, y, IS_HOTBAR_X, IS_HOTBAR_Y, INVENTORY_BLOCKS_PER_HOTBAR, 1)) >= 0) {
			InventarioScreen_TapHotbar(s, slot);
		} else if ((slot = IS_SlotAt(x, y, IS_ARMOR_X, IS_ARMOR_Y, 2, 2)) >= 0) {
			InventarioScreen_TapArmor(s, slot);
		} else if (InventarioScreen_TapButton(s, x, y)) {
			return TOUCH_TYPE_GUI;
		}
	} else if (s->mode == MODE_BAU) {
		if ((slot = IS_SlotAt(x, y, IS_BAU_X, IS_BAU_Y, IS_BAU_COLS, BAU_SLOTS / IS_BAU_COLS)) >= 0) {
			InventarioScreen_TakeSlot(s, &s->bau->slots[slot]);
		} else if (InventarioScreen_TapButton(s, x, y)) {
			return TOUCH_TYPE_GUI;
		}
	} else {
		if (IS_In(x, y, IS_IN_X, IS_IN_Y, IS_SLOT, IS_SLOT)) {
			InventarioScreen_TapFurnaceSlot(s, FORNALHA_INPUT);
		} else if (IS_In(x, y, IS_IN_X, IS_FUEL_Y, IS_SLOT, IS_SLOT)) {
			InventarioScreen_TapFurnaceSlot(s, FORNALHA_FUEL);
		} else if (IS_In(x, y, IS_OUT_X, IS_OUT_Y, IS_OUT_SIZE, IS_OUT_SIZE)) {
			InventarioScreen_TapFurnaceSlot(s, FORNALHA_OUTPUT);
		} else if (InventarioScreen_TapButton(s, x, y)) {
			return TOUCH_TYPE_GUI;
		}
	}

	InventarioScreen_Refresh(s);
	return TOUCH_TYPE_GUI;
}

static int InventarioScreen_KeyDown(void* screen, int key, struct InputDevice* device) {
	struct InventarioScreen* s = (struct InventarioScreen*)screen;

	if (InputBind_Claims(BIND_INVENTORY, key, device)) {
		if (s->releasedKey) InventarioScreen_Close(s);
	} else if (key == CCPAD_2 || InputDevice_IsPause(key, device)) {
		InventarioScreen_Close(s);
	} else if (s->mode == MODE_INV && InputDevice_IsEnter(key, device)) {
		s->msg.length = 0;
		InventarioScreen_Use(s);
		InventarioScreen_Refresh(s);
	}
	return true;
}

static void InventarioScreen_KeyUp(void* screen, int key, struct InputDevice* device) {
	struct InventarioScreen* s = (struct InventarioScreen*)screen;
	s->releasedKey = true;
}

static int InventarioScreen_PointerMove(void* screen, int id, int x, int y) { return true; }
static void InventarioScreen_PointerUp(void* screen, int id, int x, int y) { }

static int InventarioScreen_MouseScroll(void* screen, float delta) {
	struct InventarioScreen* s = (struct InventarioScreen*)screen;
	int pages = IS_Pages(s);
	s->invPage = (s->invPage + (delta < 0 ? 1 : pages - 1)) % pages;
	InventarioScreen_Refresh(s);
	return true;
}


/*########################################################################################################################*
*--------------------------------------------------------Screen-----------------------------------------------------------*
*#########################################################################################################################*/
static void InventarioScreen_Layout(void* screen) {
	struct InventarioScreen* s = (struct InventarioScreen*)screen;
	float sx = Window_UI.Width  / (float)IS_WIDTH;
	float sy = Window_UI.Height / (float)IS_HEIGHT;

	s->scale = min(sx, sy);
	if (s->scale > 1.0f) s->scale = (float)Math_Floor(s->scale);
	s->ox = (Window_UI.Width  - (int)(IS_WIDTH  * s->scale)) / 2;
	s->oy = (Window_UI.Height - (int)(IS_HEIGHT * s->scale)) / 2;
	s->dirty = true;
}

static void InventarioScreen_ContextLost(void* screen) {
	struct InventarioScreen* s = (struct InventarioScreen*)screen;
	Font_Free(&s->font);
	Gfx_DeleteTexture(&s->back.ID);
	Gfx_DeleteTexture(&s->front.ID);
	Gfx_DeleteDynamicVb(&s->vb);
}

static void InventarioScreen_ContextRecreated(void* screen) {
	struct InventarioScreen* s = (struct InventarioScreen*)screen;
	Gui_MakeBodyFont(&s->font);
	Screen_UpdateVb(s);
	s->dirty = true;
}

static void InventarioScreen_NeedRedraw(void* screen) { ((struct InventarioScreen*)screen)->dirty = true; }

static void InventarioScreen_Init(void* screen) {
	struct InventarioScreen* s = (struct InventarioScreen*)screen;
	s->maxVertices = IS_MAX_VERTICES;
	s->numWidgets  = 0;
	s->releasedKey = false;
	s->held        = BLOCK_AIR;
	s->invPage     = 0;
	s->redrawTimer = 0;

	String_InitArray(s->msg, s->_msgBuffer);
	InventarioScreen_Refresh(s);
	Event_Register_(&TextureEvents.AtlasChanged, s, InventarioScreen_NeedRedraw);
}

static void InventarioScreen_Free(void* screen) {
	struct InventarioScreen* s = (struct InventarioScreen*)screen;
	Event_Unregister_(&TextureEvents.AtlasChanged, s, InventarioScreen_NeedRedraw);
}

static const struct ScreenVTABLE InventarioScreen_VTABLE = {
	InventarioScreen_Init,        InventarioScreen_Update,      InventarioScreen_Free,
	InventarioScreen_Render,      InventarioScreen_BuildMesh,
	InventarioScreen_KeyDown,     InventarioScreen_KeyUp,       Screen_TKeyPress,             Screen_TText,
	InventarioScreen_PointerDown, InventarioScreen_PointerUp,   InventarioScreen_PointerMove, InventarioScreen_MouseScroll,
	InventarioScreen_Layout,      InventarioScreen_ContextLost, InventarioScreen_ContextRecreated
};

static void InventarioScreen_Open(int mode) {
	struct InventarioScreen* s = &InventarioScreen;
	/* Switching from one of these screens to another re-opens it in the new mode */
	Gui_Remove((struct Screen*)s);

	s->mode       = mode;
	s->grabsInput = true;
	s->closable   = true;
	s->VTABLE     = &InventarioScreen_VTABLE;
	Gui_Add((struct Screen*)s, GUI_PRIORITY_MENU);
}

void InventarioScreen_Show(void) { InventarioScreen_Open(MODE_INV); }

void BauScreen_Show(struct EquipBau* bau) {
	InventarioScreen.bau = bau;
	InventarioScreen_Open(MODE_BAU);
}

void FornalhaScreen_Show(struct EquipFornalha* fornalha) {
	InventarioScreen.fornalha = fornalha;
	InventarioScreen_Open(MODE_FORNALHA);
}

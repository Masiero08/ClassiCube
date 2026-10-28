#include "Menus.h"
#include "Gui.h"
#include "Graphics.h"
#include "Drawer2D.h"
#include "IsometricDrawer.h"
#include "Survival.h"
#include "Block.h"
#include "BlockID.h"
#include "Input.h"
#include "Window.h"
#include "Funcs.h"
#include "String_.h"
#include "ExtMath.h"
#include "Event.h"
#include "Screens.h"

/* Crafting screen in the style of Minecraft's crafting table, made for the 3DS touch screen: */
/*  tap an item from your items, then tap the 3x3 grid to place it, and tap the result to craft. */
/*  The recipe book on the right fills in the grid for you. All drawing here is original. */
/* Items placed in the grid are only reserved, they are taken when the result is crafted. */

/* Everything is laid out on a 320x240 panel (the 3DS bottom screen), scaled to fit the window */
#define CS_WIDTH  320
#define CS_HEIGHT 240
#define CS_SLOT    24

#define CS_GRID_X   12
#define CS_GRID_Y   18
#define CS_ARROW_X  92
#define CS_RESULT_X 124
#define CS_RESULT_Y 38
#define CS_RESULT_SIZE 32

#define CS_BOOK_X    200
#define CS_BOOK_Y    18
#define CS_BOOK_COLS 4
#define CS_BOOK_ROWS 3
#define CS_BOOK_PAGE (CS_BOOK_COLS * CS_BOOK_ROWS)
#define CS_BOOK_ARROW_X (CS_BOOK_X + CS_BOOK_COLS * CS_SLOT + 2)
#define CS_BOOK_ARROW_W 18
#define CS_BOOK_ARROW_H 35

#define CS_MSG_Y     94
#define CS_INV_X     16
#define CS_INV_Y     106
#define CS_INV_COLS  12
#define CS_INV_ROWS  3
#define CS_INV_PAGE  (CS_INV_COLS * CS_INV_ROWS)

#define CS_BTN_Y     190
#define CS_BTN_H     40

#define CS_MAX_RECIPES 128
#define CS_MAX_ICONS   (CRAFT_GRID_CELLS + 1 + CS_BOOK_PAGE + CS_INV_PAGE)
#define CS_MAX_VERTICES (CS_MAX_ICONS * ISOMETRICDRAWER_MAXVERTICES)

enum CraftingButton { CS_BTN_PREV, CS_BTN_NEXT, CS_BTN_CLEAR, CS_BTN_CLOSE, CS_BTN_COUNT };
static const struct { short x, w; const char* text; } cs_buttons[CS_BTN_COUNT] = {
	{   8, 36, "<"      },
	{ 108, 36, ">"      },
	{ 152, 76, "Limpar" },
	{ 236, 76, "Fechar" },
};

static struct CraftingScreen {
	Screen_Body
	struct FontDesc font;
	struct Texture back, front;
	int state[CS_MAX_VERTICES / 4];
	int iconVertices;
	float scale; int ox, oy;
	cc_bool releasedSelect, painting;

	BlockID grid[CRAFT_GRID_CELLS];
	BlockID held;
	int match;
	BlockID items[BLOCK_COUNT];
	int itemsCount, invPage;
	short book[CS_MAX_RECIPES];
	int bookCount, bookPage;

	cc_string msg;
	char _msgBuffer[STRING_SIZE];
} CraftingScreen;


/*########################################################################################################################*
*---------------------------------------------------------State-----------------------------------------------------------*
*#########################################################################################################################*/
static int CraftingScreen_Reserved(struct CraftingScreen* s, BlockID block) {
	int i, count = 0;
	for (i = 0; i < CRAFT_GRID_CELLS; i++) { if (s->grid[i] == block) count++; }
	return count;
}

/* How many of the block the player has that are not already placed in the grid */
static int CraftingScreen_Available(struct CraftingScreen* s, BlockID block) {
	return Survival_GetCount(block) - CraftingScreen_Reserved(s, block);
}

/* Removes blocks from the grid that the player no longer has enough of */
static void CraftingScreen_FixGrid(struct CraftingScreen* s) {
	int i;
	for (i = CRAFT_GRID_CELLS - 1; i >= 0; i--)
	{
		if (s->grid[i] == BLOCK_AIR || CraftingScreen_Available(s, s->grid[i]) >= 0) continue;
		s->grid[i] = BLOCK_AIR;
	}
}

/* Recipes using blocks that don't exist (e.g. from a mod that is turned off) are hidden */
static cc_bool CS_BlockExists(BlockID block) {
	return block <= BLOCK_MAX_CPE || Block_IsCustomDefined(block);
}

static cc_bool CS_RecipeExists(int recipe) {
	BlockID layout[CRAFT_GRID_CELLS];
	int i, count;
	if (!CS_BlockExists(Survival_RecipeIcon(recipe, &count))) return false;

	Survival_RecipeLayout(recipe, layout);
	for (i = 0; i < CRAFT_GRID_CELLS; i++)
	{
		if (!CS_BlockExists(layout[i])) return false;
	}
	return true;
}

static void CraftingScreen_SortBook(struct CraftingScreen* s) {
	int i, pass, count = min(Survival_RecipesCount(), CS_MAX_RECIPES);
	s->bookCount = 0;

	/* Recipes that can be made right now come first */
	for (pass = 0; pass < 2; pass++)
		for (i = 0; i < count; i++)
		{
			if ((Survival_RecipeProblem(i) == NULL) != (pass == 0)) continue;
			if (CS_RecipeExists(i)) s->book[s->bookCount++] = i;
		}
}

static void CraftingScreen_SetDefaultMessage(struct CraftingScreen* s) {
	const char* problem;
	cc_string name;
	int i;

	for (i = 0; i < CRAFT_GRID_CELLS; i++) { if (s->grid[i] != BLOCK_AIR) break; }

	if (s->match >= 0) {
		problem = Survival_RecipeProblem(s->match);
		if (problem) String_Format1(&s->msg, "&c%c", problem);
		else String_AppendConst(&s->msg, "&2Toque no resultado para fazer!");
	} else if (i < CRAFT_GRID_CELLS) {
		String_AppendConst(&s->msg, "&8Isso nao forma nenhuma receita");
	} else if (s->held != BLOCK_AIR) {
		name = Survival_BlockName(s->held);
		String_Format1(&s->msg, "&8Na mao: &1%s&8. Toque na grade!", &name);
	} else {
		String_AppendConst(&s->msg, "&8Toque num item e depois na grade");
	}
}

static void CraftingScreen_Refresh(struct CraftingScreen* s) {
	int i, max = BLOCK_COUNT;
	CraftingScreen_FixGrid(s);
	s->match = Survival_MatchGrid(s->grid);

	s->itemsCount = 0;
	for (i = 1; i < max; i++)
	{
		if (CraftingScreen_Available(s, (BlockID)i) > 0) s->items[s->itemsCount++] = (BlockID)i;
	}
	if (s->held != BLOCK_AIR && CraftingScreen_Available(s, s->held) <= 0 && !Survival_GetCount(s->held)) {
		s->held = BLOCK_AIR;
	}

	s->invPage = min(s->invPage, max(0, (s->itemsCount - 1) / CS_INV_PAGE));
	CraftingScreen_SortBook(s);
	s->bookPage = min(s->bookPage, max(0, (s->bookCount - 1) / CS_BOOK_PAGE));

	if (!s->msg.length) CraftingScreen_SetDefaultMessage(s);
	s->dirty = true;
}


/*########################################################################################################################*
*--------------------------------------------------------Actions----------------------------------------------------------*
*#########################################################################################################################*/
static void CraftingScreen_TapGrid(struct CraftingScreen* s, int cell, cc_bool dragged) {
	BlockID cur = s->grid[cell];

	if (s->held == BLOCK_AIR) {
		/* Tapping a filled cell with an empty hand gives the item back */
		if (!dragged) s->grid[cell] = BLOCK_AIR;
		return;
	}
	if (cur == s->held) {
		if (!dragged) s->grid[cell] = BLOCK_AIR;
		return;
	}
	if (dragged && cur != BLOCK_AIR) return;

	s->grid[cell] = BLOCK_AIR;
	if (CraftingScreen_Available(s, s->held) > 0) {
		s->grid[cell] = s->held;
	} else {
		s->grid[cell] = cur;
		String_AppendConst(&s->msg, "&cAcabou esse item!");
	}
}

static void CraftingScreen_TapItem(struct CraftingScreen* s, int index) {
	BlockID block;
	if (index >= s->itemsCount) { s->held = BLOCK_AIR; return; }

	block   = s->items[index];
	s->held = s->held == block ? BLOCK_AIR : block;
}

static void CraftingScreen_Craft(struct CraftingScreen* s) {
	cc_string name; char nameBuffer[STRING_SIZE];
	const char* problem;
	int recipe = s->match;
	if (recipe < 0) return;

	problem = Survival_RecipeProblem(recipe);
	if (problem) { String_Format1(&s->msg, "&c%c", problem); return; }
	if (!Survival_Craft(recipe)) { String_AppendConst(&s->msg, "&cNao deu para fazer"); return; }

	String_InitArray(name, nameBuffer);
	Survival_RecipeName(recipe, &name);
	if (Survival_RecipeKind(recipe) == RECIPE_KIND_FOOD) {
		String_Format1(&s->msg, "&2Nham! %s", &name);
	} else {
		String_Format1(&s->msg, "&2Voce fez: %s!", &name);
	}
}

static void CraftingScreen_DescribeMissing(struct CraftingScreen* s, const BlockID* layout) {
	cc_string name;
	int i, j, needed, missing;
	String_AppendConst(&s->msg, "&8Falta:");

	for (i = 0; i < CRAFT_GRID_CELLS; i++)
	{
		if (layout[i] == BLOCK_AIR) continue;
		for (j = 0; j < i; j++) { if (layout[j] == layout[i]) break; }
		if (j < i) continue; /* already counted this block */

		for (needed = 0, j = i; j < CRAFT_GRID_CELLS; j++) { if (layout[j] == layout[i]) needed++; }
		missing = needed - Survival_GetCount(layout[i]);
		if (missing <= 0) continue;

		name = Survival_BlockName(layout[i]);
		String_Format2(&s->msg, " &c%i %s", &missing, &name);
	}
}

static void CraftingScreen_TapBook(struct CraftingScreen* s, int index) {
	BlockID layout[CRAFT_GRID_CELLS];
	int i, recipe;
	if (index >= s->bookCount) return;
	recipe = s->book[index];

	Survival_RecipeLayout(recipe, layout);
	for (i = 0; i < CRAFT_GRID_CELLS; i++) s->grid[i] = BLOCK_AIR;

	for (i = 0; i < CRAFT_GRID_CELLS; i++)
	{
		if (layout[i] == BLOCK_AIR) continue;
		if (Survival_GetCount(layout[i]) < CraftingScreen_Reserved(s, layout[i]) + 1) break;
		s->grid[i] = layout[i];
	}

	if (i < CRAFT_GRID_CELLS) {
		/* Not enough of an ingredient: say what is missing instead */
		for (i = 0; i < CRAFT_GRID_CELLS; i++) s->grid[i] = BLOCK_AIR;
		CraftingScreen_DescribeMissing(s, layout);
	}
	s->held = BLOCK_AIR;
}

static void CraftingScreen_Clear(struct CraftingScreen* s) {
	int i;
	for (i = 0; i < CRAFT_GRID_CELLS; i++) s->grid[i] = BLOCK_AIR;
	s->held = BLOCK_AIR;
}

static void CraftingScreen_Close(struct CraftingScreen* s) { Gui_Remove((struct Screen*)s); }


/*########################################################################################################################*
*--------------------------------------------------------Drawing----------------------------------------------------------*
*#########################################################################################################################*/
#define CS_PANEL     BitmapColor_RGB(198, 198, 198)
#define CS_LIGHT     BitmapColor_RGB(255, 255, 255)
#define CS_SHADOW    BitmapColor_RGB( 85,  85,  85)
#define CS_SLOT_BACK BitmapColor_RGB(139, 139, 139)
#define CS_SLOT_DARK BitmapColor_RGB( 55,  55,  55)

static int CS_X(struct CraftingScreen* s, int x) { return (int)(x * s->scale); }
static int CS_Y(struct CraftingScreen* s, int y) { return (int)(y * s->scale); }

static void CS_Fill(struct CraftingScreen* s, struct Context2D* ctx, BitmapCol col, int x, int y, int w, int h) {
	int x1 = CS_X(s, x), y1 = CS_Y(s, y);
	Context2D_Clear(ctx, col, x1, y1, CS_X(s, x + w) - x1, CS_Y(s, y + h) - y1);
}

/* Draws a raised (buttons, panel) or sunken (slots) box with bevelled edges */
static void CS_Box(struct CraftingScreen* s, struct Context2D* ctx, int x, int y, int w, int h,
					BitmapCol fill, cc_bool sunken) {
	BitmapCol tl = sunken ? CS_SLOT_DARK : CS_LIGHT;
	BitmapCol br = sunken ? CS_LIGHT : CS_SHADOW;
	int b = max(1, (int)s->scale);
	int x1 = CS_X(s, x), y1 = CS_Y(s, y), x2 = CS_X(s, x + w), y2 = CS_Y(s, y + h);

	Context2D_Clear(ctx, fill, x1, y1, x2 - x1, y2 - y1);
	Context2D_Clear(ctx, tl, x1,     y1,     x2 - x1, b);
	Context2D_Clear(ctx, tl, x1,     y1,     b,       y2 - y1);
	Context2D_Clear(ctx, br, x1,     y2 - b, x2 - x1, b);
	Context2D_Clear(ctx, br, x2 - b, y1,     b,       y2 - y1);
}

static void CS_Text(struct CraftingScreen* s, struct Context2D* ctx, const cc_string* text,
					int x, int y, int align, cc_bool shadow) {
	struct DrawTextArgs args;
	int width;
	DrawTextArgs_Make(&args, text, &s->font, shadow);
	width = Drawer2D_TextWidth(&args);

	x = CS_X(s, x);
	if (align == 1) x -= width / 2;
	if (align == 2) x -= width;
	Context2D_DrawText(ctx, &args, max(0, x), CS_Y(s, y));
}

static void CS_TextConst(struct CraftingScreen* s, struct Context2D* ctx, const char* text,
						int x, int y, int align, cc_bool shadow) {
	cc_string str = String_FromReadonly(text);
	CS_Text(s, ctx, &str, x, y, align, shadow);
}

/* Small white number in the bottom right corner of a slot, like in Minecraft */
static void CS_Count(struct CraftingScreen* s, struct Context2D* ctx, int count, int x, int y, int size) {
	struct DrawTextArgs args;
	cc_string str; char strBuffer[16];
	int width, height;
	if (count <= 1) return;

	String_InitArray(str, strBuffer);
	String_AppendInt(&str, count);
	DrawTextArgs_Make(&args, &str, &s->font, true);
	width  = Drawer2D_TextWidth(&args);
	height = Drawer2D_TextHeight(&args);
	Context2D_DrawText(ctx, &args, CS_X(s, x + size) - width, CS_Y(s, y + size) - height + (int)s->scale);
}

/* Small pixel art pictures of the tools (h = head, s = stick), since tools aren't blocks */
#define CS_TOOL_PX 12
static const char* const cs_toolArt[4][CS_TOOL_PX] = {
	{ "...hhhhh....", "..h....hh...", "......s.hh..", ".....s...h..", "....s....hh.", "...s......h.",
	  "..s.......h.", ".s..........", "s...........", "............", "............", "............" },
	{ "......hh....", ".....hhhh...", "....shhhhh..", "...s.hhhh...", "..s...hh....", ".s..........",
	  "s...........", "............", "............", "............", "............", "............" },
	{ "........hhh.", ".......hhhh.", ".......hhh..", "......s.....", ".....s......", "....s.......",
	  "...s........", "..s.........", ".s..........", "s...........", "............", "............" },
	{ "..........hh", ".........hhh", "........hhh.", ".......hhh..", "......hhh...", "..s..hhh....",
	  "...shhh.....", "....ss......", "...s..s.....", "..s.........", "ss..........", "............" },
};

static BitmapCol CS_ToolColor(BlockID material) {
	switch (material)
	{
	case BLOCK_WOOD:   return BitmapColor_RGB(176, 132,  80);
	case BLOCK_COBBLE: return BitmapColor_RGB(125, 125, 125);
	case BLOCK_IRON:   return BitmapColor_RGB(230, 230, 230);
	}
	return BitmapColor_RGB(90, 230, 225);
}

static BitmapCol CS_Fade(BitmapCol col) {
	return BitmapColor_RGB((BitmapCol_R(col) + 2 * 139) / 3, (BitmapCol_G(col) + 2 * 139) / 3, (BitmapCol_B(col) + 2 * 139) / 3);
}

static void CS_Tool(struct CraftingScreen* s, struct Context2D* ctx, int kind, BlockID material,
					int x, int y, int size, cc_bool faded) {
	BitmapCol head  = CS_ToolColor(material);
	BitmapCol stick = BitmapColor_RGB(110, 75, 40);
	BitmapCol line  = BitmapColor_RGB(40, 30, 20);
	int px = max(1, (int)(s->scale * (size >= CS_RESULT_SIZE ? 2 : 1)));
	int i, j, x0, y0;
	char c;

	if (faded) { head = CS_Fade(head); stick = CS_Fade(stick); line = CS_Fade(line); }

	/* Centre the picture in the slot, with a dark outline so it stands out */
	x0 = CS_X(s, x + size / 2) - (CS_TOOL_PX * px) / 2;
	y0 = CS_Y(s, y + size / 2) - (CS_TOOL_PX * px) / 2 + px;

	for (j = 0; j < CS_TOOL_PX; j++)
		for (i = 0; i < CS_TOOL_PX; i++)
		{
			if (cs_toolArt[kind][j][i] == '.') continue;
			Context2D_Clear(ctx, line, x0 + i * px - 1, y0 + j * px - 1, px + 2, px + 2);
		}
	for (j = 0; j < CS_TOOL_PX; j++)
		for (i = 0; i < CS_TOOL_PX; i++)
		{
			c = cs_toolArt[kind][j][i];
			if (c == '.') continue;
			Context2D_Clear(ctx, c == 'h' ? head : stick, x0 + i * px, y0 + j * px, px, px);
		}
}

/* Draws a triangle pointing left (0), right (1), up (2) or down (3), centred on the given point */
static void CS_Triangle(struct CraftingScreen* s, struct Context2D* ctx, int cx, int cy, int dir) {
	BitmapCol col = BitmapColor_RGB(60, 60, 60);
	int i, len = 6;
	for (i = 0; i < len; i++)
	{
		int a = dir == 0 || dir == 2 ? i : len - 1 - i;
		if (dir < 2) CS_Fill(s, ctx, col, cx - len / 2 + i, cy - a, 1, a * 2 + 1);
		else         CS_Fill(s, ctx, col, cx - a, cy - len / 2 + i, a * 2 + 1, 1);
	}
}

static void CS_Arrow(struct CraftingScreen* s, struct Context2D* ctx, BitmapCol col) {
	int i, cy = CS_RESULT_Y + CS_RESULT_SIZE / 2;
	CS_Fill(s, ctx, col, CS_ARROW_X, cy - 3, 16, 6);
	for (i = 0; i < 9; i++)
	{
		CS_Fill(s, ctx, col, CS_ARROW_X + 16 + i, cy - 9 + i, 1, 18 - i * 2);
	}
}

static void CraftingScreen_DrawBack(struct CraftingScreen* s, struct Context2D* ctx) {
	cc_string str; char strBuffer[64];
	BitmapCol fill;
	int i, x, y, page, pages;
	const char* problem;

	CS_Box(s, ctx, 0, 0, CS_WIDTH, CS_HEIGHT, CS_PANEL, false);
	CS_TextConst(s, ctx, "&8Mesa de trabalho", CS_GRID_X, 4, 0, false);

	/* 3x3 crafting grid */
	for (i = 0; i < CRAFT_GRID_CELLS; i++)
	{
		x = CS_GRID_X + (i % CRAFT_GRID_SIZE) * CS_SLOT;
		y = CS_GRID_Y + (i / CRAFT_GRID_SIZE) * CS_SLOT;
		CS_Box(s, ctx, x, y, CS_SLOT, CS_SLOT, CS_SLOT_BACK, true);
	}

	/* Arrow and result slot */
	problem = s->match >= 0 ? Survival_RecipeProblem(s->match) : "";
	CS_Arrow(s, ctx, problem ? CS_SLOT_BACK : BitmapColor_RGB(80, 160, 60));
	if (s->match >= 0) {
		fill = problem ? BitmapColor_RGB(170, 110, 110) : BitmapColor_RGB(120, 190, 110);
	} else {
		fill = CS_SLOT_BACK;
	}
	CS_Box(s, ctx, CS_RESULT_X, CS_RESULT_Y, CS_RESULT_SIZE, CS_RESULT_SIZE, fill, true);

	/* Recipe book */
	pages = max(1, Math_CeilDiv(s->bookCount, CS_BOOK_PAGE));
	String_InitArray(str, strBuffer);
	page = s->bookPage + 1;
	String_Format2(&str, "&8Receitas %i/%i", &page, &pages);
	CS_Text(s, ctx, &str, CS_BOOK_X, 4, 0, false);

	for (i = 0; i < CS_BOOK_PAGE; i++)
	{
		int index = s->bookPage * CS_BOOK_PAGE + i;
		x = CS_BOOK_X + (i % CS_BOOK_COLS) * CS_SLOT;
		y = CS_BOOK_Y + (i / CS_BOOK_COLS) * CS_SLOT;

		fill = CS_SLOT_BACK;
		if (index < s->bookCount) {
			fill = Survival_RecipeProblem(s->book[index]) ? BitmapColor_RGB(120, 120, 120) : BitmapColor_RGB(120, 180, 110);
		}
		CS_Box(s, ctx, x, y, CS_SLOT, CS_SLOT, fill, true);
	}
	CS_Box(s, ctx, CS_BOOK_ARROW_X, CS_BOOK_Y, CS_BOOK_ARROW_W, CS_BOOK_ARROW_H, CS_PANEL, false);
	CS_Triangle(s, ctx, CS_BOOK_ARROW_X + CS_BOOK_ARROW_W / 2, CS_BOOK_Y + CS_BOOK_ARROW_H / 2, 2);
	CS_Box(s, ctx, CS_BOOK_ARROW_X, CS_BOOK_Y + CS_BOOK_ARROW_H + 2, CS_BOOK_ARROW_W, CS_BOOK_ARROW_H, CS_PANEL, false);
	CS_Triangle(s, ctx, CS_BOOK_ARROW_X + CS_BOOK_ARROW_W / 2, CS_BOOK_Y + CS_BOOK_ARROW_H + 2 + CS_BOOK_ARROW_H / 2, 3);

	/* Player's items */
	for (i = 0; i < CS_INV_PAGE; i++)
	{
		int index = s->invPage * CS_INV_PAGE + i;
		x = CS_INV_X + (i % CS_INV_COLS) * CS_SLOT;
		y = CS_INV_Y + (i / CS_INV_COLS) * CS_SLOT;

		fill = CS_SLOT_BACK;
		if (index < s->itemsCount && s->items[index] == s->held) fill = BitmapColor_RGB(250, 250, 200);
		CS_Box(s, ctx, x, y, CS_SLOT, CS_SLOT, fill, true);
	}

	/* Buttons */
	for (i = 0; i < CS_BTN_COUNT; i++)
	{
		CS_Box(s, ctx, cs_buttons[i].x, CS_BTN_Y, cs_buttons[i].w, CS_BTN_H, BitmapColor_RGB(170, 170, 170), false);
		if (i < CS_BTN_CLEAR) {
			CS_Triangle(s, ctx, cs_buttons[i].x + cs_buttons[i].w / 2, CS_BTN_Y + CS_BTN_H / 2, i);
		} else {
			CS_TextConst(s, ctx, cs_buttons[i].text, cs_buttons[i].x + cs_buttons[i].w / 2, CS_BTN_Y + 15, 1, true);
		}
	}

	pages = max(1, Math_CeilDiv(s->itemsCount, CS_INV_PAGE));
	String_InitArray(str, strBuffer);
	page = s->invPage + 1;
	String_Format2(&str, "&8Itens %i/%i", &page, &pages);
	CS_Text(s, ctx, &str, 76, CS_BTN_Y + 15, 1, false);
}

static void CraftingScreen_DrawFront(struct CraftingScreen* s, struct Context2D* ctx) {
	cc_string str; char strBuffer[STRING_SIZE];
	BlockID icon;
	int i, x, y, count, index;
	cc_bool faded;

	/* Counts of the player's items */
	for (i = 0; i < CS_INV_PAGE; i++)
	{
		index = s->invPage * CS_INV_PAGE + i;
		if (index >= s->itemsCount) break;
		x = CS_INV_X + (i % CS_INV_COLS) * CS_SLOT;
		y = CS_INV_Y + (i / CS_INV_COLS) * CS_SLOT;
		CS_Count(s, ctx, CraftingScreen_Available(s, s->items[index]), x, y, CS_SLOT);
	}

	/* Recipes that can't be made yet are shown faded */
	for (i = 0; i < CS_BOOK_PAGE; i++)
	{
		index = s->bookPage * CS_BOOK_PAGE + i;
		if (index >= s->bookCount) break;
		x = CS_BOOK_X + (i % CS_BOOK_COLS) * CS_SLOT;
		y = CS_BOOK_Y + (i / CS_BOOK_COLS) * CS_SLOT;

		icon = Survival_RecipeIcon(s->book[index], &count);
		CS_Count(s, ctx, count, x, y, CS_SLOT);
		faded = Survival_RecipeProblem(s->book[index]) != NULL;
		if (Survival_RecipeTool(s->book[index]) >= 0) {
			CS_Tool(s, ctx, Survival_RecipeTool(s->book[index]), icon, x, y, CS_SLOT, faded);
		} else if (faded) {
			CS_Fill(s, ctx, BitmapCol_Make(120, 120, 120, 150), x + 1, y + 1, CS_SLOT - 2, CS_SLOT - 2);
		}
	}

	/* Result name */
	if (s->match >= 0) {
		icon = Survival_RecipeIcon(s->match, &count);
		CS_Count(s, ctx, count, CS_RESULT_X, CS_RESULT_Y, CS_RESULT_SIZE);
		if (Survival_RecipeTool(s->match) >= 0) {
			CS_Tool(s, ctx, Survival_RecipeTool(s->match), icon, CS_RESULT_X, CS_RESULT_Y, CS_RESULT_SIZE, false);
		}

		String_InitArray(str, strBuffer);
		String_AppendConst(&str, "&0");
		Survival_RecipeName(s->match, &str);
		CS_Text(s, ctx, &str, CS_RESULT_X + CS_RESULT_SIZE / 2, CS_RESULT_Y + CS_RESULT_SIZE + 4, 1, false);
	}

	CS_Text(s, ctx, &s->msg, CS_WIDTH / 2, CS_MSG_Y, 1, false);
}

static void CraftingScreen_MakeTexture(struct CraftingScreen* s, struct Texture* tex, cc_bool front) {
	struct Context2D ctx;
	int width  = CS_X(s, CS_WIDTH);
	int height = CS_Y(s, CS_HEIGHT);

	Gfx_DeleteTexture(&tex->ID);
	Context2D_Alloc(&ctx, width, height);
	{
		if (front) CraftingScreen_DrawFront(s, &ctx);
		else       CraftingScreen_DrawBack(s, &ctx);
		Context2D_MakeTexture(tex, &ctx);
	}
	Context2D_Free(&ctx);
	tex->x = s->ox; tex->y = s->oy;
}

static void CS_AddIcon(struct CraftingScreen* s, BlockID block, int x, int y, int size, float iconSize) {
	if (block == BLOCK_AIR) return;
	IsometricDrawer_AddBatch(block, iconSize * s->scale,
		s->ox + (x + size / 2.0f) * s->scale, s->oy + (y + size / 2.0f) * s->scale);
}

static void CraftingScreen_BuildMesh(void* screen) {
	struct CraftingScreen* s = (struct CraftingScreen*)screen;
	struct VertexTextured* data;
	int i, index, count;
	BlockID icon;

	CraftingScreen_MakeTexture(s, &s->back,  false);
	CraftingScreen_MakeTexture(s, &s->front, true);
	if (!s->vb) return;

	data = Screen_LockVb(s);
	IsometricDrawer_BeginBatch(data, s->state);

	for (i = 0; i < CRAFT_GRID_CELLS; i++)
	{
		CS_AddIcon(s, s->grid[i], CS_GRID_X + (i % CRAFT_GRID_SIZE) * CS_SLOT,
			CS_GRID_Y + (i / CRAFT_GRID_SIZE) * CS_SLOT, CS_SLOT, 7.5f);
	}
	if (s->match >= 0) {
		icon = Survival_RecipeIcon(s->match, &count);
		if (Survival_RecipeTool(s->match) < 0) CS_AddIcon(s, icon, CS_RESULT_X, CS_RESULT_Y, CS_RESULT_SIZE, 10.0f);
	}
	for (i = 0; i < CS_BOOK_PAGE; i++)
	{
		index = s->bookPage * CS_BOOK_PAGE + i;
		if (index >= s->bookCount) break;
		icon = Survival_RecipeIcon(s->book[index], &count);
		if (Survival_RecipeTool(s->book[index]) >= 0) continue;
		CS_AddIcon(s, icon, CS_BOOK_X + (i % CS_BOOK_COLS) * CS_SLOT,
			CS_BOOK_Y + (i / CS_BOOK_COLS) * CS_SLOT, CS_SLOT, 7.0f);
	}
	for (i = 0; i < CS_INV_PAGE; i++)
	{
		index = s->invPage * CS_INV_PAGE + i;
		if (index >= s->itemsCount) break;
		CS_AddIcon(s, s->items[index], CS_INV_X + (i % CS_INV_COLS) * CS_SLOT,
			CS_INV_Y + (i / CS_INV_COLS) * CS_SLOT, CS_SLOT, 7.5f);
	}

	s->iconVertices = IsometricDrawer_EndBatch();
	Gfx_UnlockDynamicVb(s->vb);
}

static void CraftingScreen_Render(void* screen, float delta) {
	struct CraftingScreen* s = (struct CraftingScreen*)screen;
	Texture_Render(&s->back);

	if (s->iconVertices) {
		Gfx_SetVertexFormat(VERTEX_FORMAT_TEXTURED);
		Gfx_BindDynamicVb(s->vb);
		IsometricDrawer_Render(s->iconVertices, 0, s->state);
	}
	Texture_Render(&s->front);
}


/*########################################################################################################################*
*---------------------------------------------------------Input-----------------------------------------------------------*
*#########################################################################################################################*/
static cc_bool CS_In(int x, int y, int rx, int ry, int rw, int rh) {
	return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

/* Returns the index of the slot of a grid of slots at the given position, or -1 if none */
static int CS_SlotAt(int x, int y, int gx, int gy, int cols, int rows) {
	if (!CS_In(x, y, gx, gy, cols * CS_SLOT, rows * CS_SLOT)) return -1;
	return ((y - gy) / CS_SLOT) * cols + (x - gx) / CS_SLOT;
}

static void CS_ToPanel(struct CraftingScreen* s, int* x, int* y) {
	*x = (int)((*x - s->ox) / s->scale);
	*y = (int)((*y - s->oy) / s->scale);
}

static int CraftingScreen_PointerDown(void* screen, int id, int x, int y) {
	struct CraftingScreen* s = (struct CraftingScreen*)screen;
	int i, slot, pages;
	CS_ToPanel(s, &x, &y);
	s->msg.length = 0;
	s->painting   = false;

	if ((slot = CS_SlotAt(x, y, CS_GRID_X, CS_GRID_Y, CRAFT_GRID_SIZE, CRAFT_GRID_SIZE)) >= 0) {
		CraftingScreen_TapGrid(s, slot, false);
		s->painting = s->held != BLOCK_AIR;
	} else if (CS_In(x, y, CS_ARROW_X, CS_RESULT_Y, CS_RESULT_X + CS_RESULT_SIZE - CS_ARROW_X, CS_RESULT_SIZE)) {
		CraftingScreen_Craft(s);
	} else if ((slot = CS_SlotAt(x, y, CS_BOOK_X, CS_BOOK_Y, CS_BOOK_COLS, CS_BOOK_ROWS)) >= 0) {
		CraftingScreen_TapBook(s, s->bookPage * CS_BOOK_PAGE + slot);
	} else if (CS_In(x, y, CS_BOOK_ARROW_X, CS_BOOK_Y, CS_BOOK_ARROW_W, CS_BOOK_ARROW_H)) {
		pages = max(1, Math_CeilDiv(s->bookCount, CS_BOOK_PAGE));
		s->bookPage = (s->bookPage + pages - 1) % pages;
	} else if (CS_In(x, y, CS_BOOK_ARROW_X, CS_BOOK_Y + CS_BOOK_ARROW_H, CS_BOOK_ARROW_W, CS_BOOK_ARROW_H + 2)) {
		pages = max(1, Math_CeilDiv(s->bookCount, CS_BOOK_PAGE));
		s->bookPage = (s->bookPage + 1) % pages;
	} else if ((slot = CS_SlotAt(x, y, CS_INV_X, CS_INV_Y, CS_INV_COLS, CS_INV_ROWS)) >= 0) {
		CraftingScreen_TapItem(s, s->invPage * CS_INV_PAGE + slot);
	} else {
		for (i = 0; i < CS_BTN_COUNT; i++)
		{
			if (CS_In(x, y, cs_buttons[i].x, CS_BTN_Y, cs_buttons[i].w, CS_BTN_H)) break;
		}
		pages = max(1, Math_CeilDiv(s->itemsCount, CS_INV_PAGE));

		switch (i)
		{
		case CS_BTN_PREV:  s->invPage = (s->invPage + pages - 1) % pages; break;
		case CS_BTN_NEXT:  s->invPage = (s->invPage + 1) % pages; break;
		case CS_BTN_CLEAR: CraftingScreen_Clear(s); break;
		case CS_BTN_CLOSE: CraftingScreen_Close(s); return TOUCH_TYPE_GUI;
		}
	}

	CraftingScreen_Refresh(s);
	return TOUCH_TYPE_GUI;
}

/* Dragging over the grid places the held item in each empty cell, like in Minecraft */
static int CraftingScreen_PointerMove(void* screen, int id, int x, int y) {
	struct CraftingScreen* s = (struct CraftingScreen*)screen;
	int slot;
	if (!s->painting) return true;

	CS_ToPanel(s, &x, &y);
	slot = CS_SlotAt(x, y, CS_GRID_X, CS_GRID_Y, CRAFT_GRID_SIZE, CRAFT_GRID_SIZE);
	if (slot < 0 || s->grid[slot] != BLOCK_AIR) return true;

	s->msg.length = 0;
	CraftingScreen_TapGrid(s, slot, true);
	CraftingScreen_Refresh(s);
	return true;
}

static void CraftingScreen_PointerUp(void* screen, int id, int x, int y) {
	struct CraftingScreen* s = (struct CraftingScreen*)screen;
	s->painting = false;
}

static int CraftingScreen_KeyDown(void* screen, int key, struct InputDevice* device) {
	struct CraftingScreen* s = (struct CraftingScreen*)screen;

	if (key == CCPAD_SELECT) {
		if (s->releasedSelect) CraftingScreen_Close(s);
	} else if (key == CCPAD_2 || InputDevice_IsPause(key, device)) {
		CraftingScreen_Close(s);
	} else if (InputDevice_IsEnter(key, device)) {
		s->msg.length = 0;
		CraftingScreen_Craft(s);
		CraftingScreen_Refresh(s);
	}
	return true;
}

static void CraftingScreen_KeyUp(void* screen, int key, struct InputDevice* device) {
	struct CraftingScreen* s = (struct CraftingScreen*)screen;
	if (key == CCPAD_SELECT) s->releasedSelect = true;
}


/*########################################################################################################################*
*--------------------------------------------------------Screen-----------------------------------------------------------*
*#########################################################################################################################*/
static void CraftingScreen_Layout(void* screen) {
	struct CraftingScreen* s = (struct CraftingScreen*)screen;
	float sx = Window_UI.Width  / (float)CS_WIDTH;
	float sy = Window_UI.Height / (float)CS_HEIGHT;

	s->scale = min(sx, sy);
	if (s->scale > 1.0f) s->scale = (float)Math_Floor(s->scale);
	s->ox = (Window_UI.Width  - (int)(CS_WIDTH  * s->scale)) / 2;
	s->oy = (Window_UI.Height - (int)(CS_HEIGHT * s->scale)) / 2;
	s->dirty = true;
}

static void CraftingScreen_ContextLost(void* screen) {
	struct CraftingScreen* s = (struct CraftingScreen*)screen;
	Font_Free(&s->font);
	Gfx_DeleteTexture(&s->back.ID);
	Gfx_DeleteTexture(&s->front.ID);
	Gfx_DeleteDynamicVb(&s->vb);
}

static void CraftingScreen_ContextRecreated(void* screen) {
	struct CraftingScreen* s = (struct CraftingScreen*)screen;
	Gui_MakeBodyFont(&s->font);
	Screen_UpdateVb(s);
	s->dirty = true;
}

static void CraftingScreen_NeedRedraw(void* screen) { ((struct CraftingScreen*)screen)->dirty = true; }

static void CraftingScreen_Init(void* screen) {
	struct CraftingScreen* s = (struct CraftingScreen*)screen;
	s->maxVertices    = CS_MAX_VERTICES;
	s->numWidgets     = 0;
	s->releasedSelect = false;
	s->painting       = false;
	s->invPage = 0; s->bookPage = 0;

	String_InitArray(s->msg, s->_msgBuffer);
	CraftingScreen_Clear(s);
	CraftingScreen_Refresh(s);
	Event_Register_(&TextureEvents.AtlasChanged, s, CraftingScreen_NeedRedraw);
}

static void CraftingScreen_Free(void* screen) {
	struct CraftingScreen* s = (struct CraftingScreen*)screen;
	Event_Unregister_(&TextureEvents.AtlasChanged, s, CraftingScreen_NeedRedraw);
}

static int CraftingScreen_MouseScroll(void* screen, float delta) {
	struct CraftingScreen* s = (struct CraftingScreen*)screen;
	int pages = max(1, Math_CeilDiv(s->itemsCount, CS_INV_PAGE));
	s->invPage = (s->invPage + (delta < 0 ? 1 : pages - 1)) % pages;
	CraftingScreen_Refresh(s);
	return true;
}

static const struct ScreenVTABLE CraftingScreen_VTABLE = {
	CraftingScreen_Init,        Screen_NullUpdate,          CraftingScreen_Free,
	CraftingScreen_Render,      CraftingScreen_BuildMesh,
	CraftingScreen_KeyDown,     CraftingScreen_KeyUp,       Screen_TKeyPress,           Screen_TText,
	CraftingScreen_PointerDown, CraftingScreen_PointerUp,   CraftingScreen_PointerMove, CraftingScreen_MouseScroll,
	CraftingScreen_Layout,      CraftingScreen_ContextLost, CraftingScreen_ContextRecreated
};

void CraftingScreen_Show(void) {
	struct CraftingScreen* s = &CraftingScreen;
	s->grabsInput = true;
	s->closable   = true;
	s->VTABLE     = &CraftingScreen_VTABLE;
	Gui_Add((struct Screen*)s, GUI_PRIORITY_MENU);
}

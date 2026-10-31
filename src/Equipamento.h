#ifndef CC_EQUIPAMENTO_H
#define CC_EQUIPAMENTO_H
#include "Core.h"
/* Chests, a working furnace, metal bars and armour for survival in singleplayer
*/
CC_BEGIN_HEADER

struct Bitmap;
struct IGameComponent;
extern struct IGameComponent Equipamento_Component;

/* Block IDs 192-223 and atlas tiles 208-239 are reserved for these blocks */
enum EquipamentoBlock {
	EBLOCK_BAU = 192, EBLOCK_FORNALHA_ACESA,
	EITEM_BARRA_FERRO, EITEM_BARRA_OURO, EITEM_BARRA_TITANIO,
	EITEM_CAPACETE_LA,       EITEM_PEITORAL_LA,       EITEM_CALCA_LA,       EITEM_BOTAS_LA,
	EITEM_CAPACETE_OURO,     EITEM_PEITORAL_OURO,     EITEM_CALCA_OURO,     EITEM_BOTAS_OURO,
	EITEM_CAPACETE_FERRO,    EITEM_PEITORAL_FERRO,    EITEM_CALCA_FERRO,    EITEM_BOTAS_FERRO,
	EITEM_CAPACETE_DIAMANTE, EITEM_PEITORAL_DIAMANTE, EITEM_CALCA_DIAMANTE, EITEM_BOTAS_DIAMANTE,
	/* The titanium chestplate is Minerios' MITEM_ARMADURA_TITANIO */
	EITEM_CAPACETE_TITANIO,  EITEM_CALCA_TITANIO,     EITEM_BOTAS_TITANIO,
	EBLOCK_FIM
};

enum ArmorPiece { ARMOR_HELMET, ARMOR_CHEST, ARMOR_LEGS, ARMOR_BOOTS, ARMOR_PIECES };
enum ArmorTier  { ARMOR_TIER_NONE, ARMOR_TIER_LA, ARMOR_TIER_OURO, ARMOR_TIER_FERRO, ARMOR_TIER_DIAMANTE, ARMOR_TIER_TITANIO, ARMOR_TIERS };

/* Draws the textures for these blocks into unused tiles of the given terrain atlas */
void Equip_PatchAtlas(struct Bitmap* bmp);

/* Which piece of armour the given item is, or -1 if it isn't armour */
int  Equip_ArmorPiece(BlockID block);
/* Tier of the given armour item (ARMOR_TIER_NONE if it isn't armour) */
int  Equip_ArmorTier(BlockID block);
/* The armour item of the given tier and piece */
BlockID Equip_ArmorItem(int tier, int piece);
/* The armour item worn in the given slot, or BLOCK_AIR */
BlockID Equip_Worn(int piece);
/* Remaining and maximum uses of the armour worn in the given slot */
int  Equip_WornUses(int piece);
int  Equip_MaxUses(BlockID block);
/* Puts on armour the player has (the old piece goes back to the player). Returns whether it was worn */
cc_bool Equip_Wear(BlockID block);
/* Takes off the armour in the given slot (it goes back to the player). Returns whether it was taken off */
cc_bool Equip_TakeOff(int piece);
/* Total armour points of everything worn (each point blocks 4% of damage) */
int  Equip_Defense(void);
/* Percentage of damage blocked by the worn armour */
int  Equip_DefensePercent(void);
/* Reduces damage from monsters/fire by the worn armour, which wears out a little */
int  Equip_ReduceDamage(int amount);
/* Loses all worn armour (e.g. when the player dies) */
void Equip_ClearArmor(void);

/* Handles using (right click / L) a chest, furnace or held armour. Returns whether the click was used */
cc_bool Equip_HandleUse(void);

/* A slot holding some of one item, in a chest or furnace */
struct EquipSlot { BlockID block; cc_uint16 count; };
#define BAU_SLOTS       27
#define BAU_STACK       64
struct EquipBau { cc_uint16 x, y, z; struct EquipSlot slots[BAU_SLOTS]; };

enum FornalhaSlot { FORNALHA_INPUT, FORNALHA_FUEL, FORNALHA_OUTPUT, FORNALHA_SLOTS };
struct EquipFornalha {
	cc_uint16 x, y, z;
	struct EquipSlot slots[FORNALHA_SLOTS];
	float burnLeft, burnMax, cook;
};
/* Seconds it takes to smelt or cook one item */
#define FORNALHA_COOK_TIME 5.0f

/* What smelting/cooking the given item makes, or BLOCK_AIR if it can't be smelted */
BlockID Equip_SmeltResult(BlockID block);
/* How many seconds the given item burns for as fuel, or 0 if it isn't fuel */
float Equip_FuelTime(BlockID block);

/* Moves up to 'count' of a slot's item into the player's items. Returns how many were moved */
int  Equip_TakeFromSlot(struct EquipSlot* slot, int count);
/* Moves up to 'count' of the player's 'block' into a slot. Returns how many were moved */
int  Equip_PutInSlot(struct EquipSlot* slot, BlockID block, int count);

/* Size of the data written by Equip_SaveArmor */
#define EQUIP_ARMOR_SIZE (ARMOR_PIECES * 4)
/* Armour worn by the player, for saving the world or carrying it between dimensions */
void Equip_SaveArmor(cc_uint8* data);
void Equip_LoadArmor(const cc_uint8* data, int len);
/* Chests and furnaces in the world, for saving it. Returns the number of bytes written to 'data' */
int  Equip_SaveChests(cc_uint8* data);
int  Equip_SaveFurnaces(cc_uint8* data);
void Equip_LoadChests(const cc_uint8* data, int len);
void Equip_LoadFurnaces(const cc_uint8* data, int len);
/* Maximum sizes of the data written by Equip_SaveChests and Equip_SaveFurnaces */
#define EQUIP_MAX_BAUS         64
#define EQUIP_MAX_FORNALHAS    32
#define EQUIP_BAU_SAVE_SIZE    (6 + BAU_SLOTS * 4)
#define EQUIP_FORNALHA_SAVE_SIZE (6 + FORNALHA_SLOTS * 4 + 6)
#define EQUIP_MAX_SAVE_SIZE    (EQUIP_MAX_BAUS * EQUIP_BAU_SAVE_SIZE)

/* Inventory, chest and furnace screens (InventarioScreen.c) */
void InventarioScreen_Show(void);
void BauScreen_Show(struct EquipBau* bau);
void FornalhaScreen_Show(struct EquipFornalha* fornalha);

CC_END_HEADER
#endif

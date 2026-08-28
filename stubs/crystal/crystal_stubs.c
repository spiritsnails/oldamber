
#include "gen2_pokedex.h"
#include "gen2_evos_moves.h"
#include "crystal_palettes.h"
#include "crystal_font.h"
#include "crystal_menus.h"
#include "crystal_options.h"
#include "crystal_pack.h"
#include "crystal_icons.h"
#include "crystal_stats_screen.h"
#include "crystal_tile_anims.h"
#include "crystal_emotes.h"
#include "crystal_audio.h"
#include "crystal_sprites.h"
#include "johto_trainers.h"
#include "crystal_mon_pics.h"
#include "crystal_trainer_pics.h"
#include "crystal_battle_gfx.h"

const gen2_base_stats_t gGen2BaseStats[GEN2_NUM_SPECIES] = {0};
const gen2_evos_moves_t gGen2EvosMoves[GEN2_EVOS_NUM_SPECIES] = {0};
const uint16_t gCrystalBGPalettes[CRYSTAL_NUM_BG_PALETTES][4] = {0};
const uint16_t gCrystalRoofPals[CRYSTAL_NUM_ROOF_GROUPS][4] = {0};
const uint16_t gCrystalObjectPals[4][8][4] = {0};
const uint8_t gCrystalEnvColors[CRYSTAL_NUM_ENVIRONMENTS][4][8] = {0};
const uint16_t gCrystalTextBGPal[4] = {0};
const uint8_t gCrystalFont[CRYSTAL_FONT_TILES][16] = {0};
const uint8_t gCrystalFrames[CRYSTAL_NUM_FRAMES][CRYSTAL_FRAME_TILES][16] = {0};
const uint8_t gCrystalTextboxSpace[16] = {0};
const uint8_t gCrystalBattleExtra[CRYSTAL_BATTLE_EXTRA_TILES][16] = {0};
const crystal_menu_box_t gCrystalStartMenuBox = {0};
const crystal_menu_item_t gCrystalStartMenu[CRYSTAL_START_ITEMS] = {0};
const char *const gCrystalOptionsScreen = {0};
const char *const gCrystalOptTextSpeed[CRYSTAL_OPT_TEXTSPEED_COUNT] = {0};
const char *const gCrystalOptBattleScene[CRYSTAL_OPT_BATTLESCENE_COUNT] = {0};
const char *const gCrystalOptBattleStyle[CRYSTAL_OPT_BATTLESTYLE_COUNT] = {0};
const char *const gCrystalOptSound[CRYSTAL_OPT_SOUND_COUNT] = {0};
const char *const gCrystalOptPrint[CRYSTAL_OPT_PRINT_COUNT] = {0};
const char *const gCrystalOptMenuAccount[CRYSTAL_OPT_MENUACCOUNT_COUNT] = {0};
const crystal_pocket_t gCrystalPockets[CRYSTAL_NUM_POCKETS] = {0};
const char *const gCrystalItemDesc[CRYSTAL_ITEM_DESC_COUNT] = {0};
const char *const gCrystalItemName[CRYSTAL_ITEM_DESC_COUNT] = {0};
const uint8_t gCrystalPackGFX[4][CRYSTAL_PACK_IMG_TILES][16] = {0};
const uint8_t gCrystalPackGFXOrder[4] = {0};
const uint8_t gCrystalPackMenuGFX[CRYSTAL_PACK_MENU_TILES][16] = {0};
const uint8_t gCrystalPocketNameTilemap[4][15] = {0};
const uint16_t gCrystalPackPals[8][4] = {0};
const uint8_t gCrystalSpeciesIcon[CRYSTAL_ICON_SPECIES] = {0};
const uint8_t gCrystalIcon[CRYSTAL_NUM_ICONS][CRYSTAL_ICON_TILES][16] = {0};
const uint16_t gCrystalIconOBPals[CRYSTAL_ICON_OBPALS][4] = {0};
const uint16_t gCrystalPartyMenuBGPals[4][4] = {0};
const uint16_t gCrystalStatsPageColor[CRYSTAL_STATS_PALS] = {0};
const char *const gCrystalStatsStatusType = {0};
const char *const gCrystalStatsOk = {0};
const char *const gCrystalStatsExpPoints = {0};
const char *const gCrystalStatsLevelUp = {0};
const char *const gCrystalStatsTo = {0};
const char *const gCrystalStatsPkrs = {0};
const char *const gCrystalStatsItem = {0};
const char *const gCrystalStatsThreeDashes = {0};
const char *const gCrystalStatsMove = {0};
const char *const gCrystalStatsIDNo = {0};
const char *const gCrystalStatsOT = {0};
const uint8_t gCrystalStatsStatusTypeRaw[14] = {0};
const uint8_t gCrystalStatsOkRaw[4] = {0};
const uint8_t gCrystalStatsExpPointsRaw[11] = {0};
const uint8_t gCrystalStatsLevelUpRaw[9] = {0};
const uint8_t gCrystalStatsToRaw[3] = {0};
const uint8_t gCrystalStatsPkrsRaw[5] = {0};
const uint8_t gCrystalStatsItemRaw[5] = {0};
const uint8_t gCrystalStatsThreeDashesRaw[4] = {0};
const uint8_t gCrystalStatsMoveRaw[5] = {0};
const uint8_t gCrystalStatsIDNoRaw[4] = {0};
const uint8_t gCrystalStatsOTRaw[4] = {0};
const uint16_t gCrystalStatsHPBarPals[3][4] = {0};
const uint16_t gCrystalStatsExpBarPal[1][4] = {0};
const uint16_t gCrystalStatsPagePals[CRYSTAL_STATS_PALS][4] = {0};
const crystal_stats_gfx_t gCrystalStatsGFX[CRYSTAL_STATS_GFX_SETS] = {0};
const crystal_anim_frame_t gCrystalAnimFrames[CRYSTAL_ANIM_NUM_FRAMES] = {0};
const crystal_anim_script_t gCrystalAnimScripts[CRYSTAL_ANIM_NUM_SCRIPTS] = {0};
const uint8_t gCrystalTilesetAnim[CRYSTAL_NUM_TILESETS] = {0};
const crystal_anim_descriptor_t gCrystalAnimDescriptors[CRYSTAL_ANIM_NUM_DESCRIPTORS] = {0};
const uint8_t gCrystalAnimFlower[4][16] = {0};
const uint8_t gCrystalAnimForestLeft[2][16] = {0};
const uint8_t gCrystalAnimForestRight[2][16] = {0};
const uint8_t gCrystalAnimLava[4][16] = {0};
const uint8_t gCrystalAnimWater[4][16] = {0};
const uint8_t gCrystalAnimFountain[8][16] = {0};
const uint8_t gCrystalAnimPillarSeq[8] = {0};
const crystal_emote_t gCrystalEmotes[CRYSTAL_NUM_EMOTES] = {0};
const uint8_t gCrystalGrassRustleGfx[16] = {0};
const crystal_oam_t gCrystalGrassRustleOam[CRYSTAL_GRASS_RUSTLE_FRAMES][CRYSTAL_GRASS_RUSTLE_OBJS] = {0};
const crystal_audio_track_t gCrystalMusic[CRYSTAL_NUM_MUSIC] = {0};
const crystal_audio_track_t gCrystalSfx[CRYSTAL_NUM_SFX] = {0};
const crystal_audio_track_t gCrystalCries[CRYSTAL_NUM_CRIES] = {0};
const uint8_t gCrystalTrainerEncounterMusic[CRYSTAL_NUM_TRAINER_CLASSES] = {0};
const crystal_mon_cry_t gCrystalMonCries[CRYSTAL_NUM_MON_CRIES] = {0};
const uint8_t gCrystalSpriteGfx[CRYSTAL_NUM_SPRITES][CRYSTAL_SPRITE_GFX_SIZE] = {0};
const uint8_t gCrystalSpriteTileCount[CRYSTAL_NUM_SPRITES] = {0};
const uint8_t gCrystalSpritePal[CRYSTAL_NUM_SPRITES] = {0};
const char *const gCrystalSpriteNames[CRYSTAL_NUM_SPRITES] = {0};
const uint8_t gCrystalSpriteType[CRYSTAL_NUM_SPRITES] = {0};
const johto_trainer_t gJohtoTrainers[JOHTO_TRAINER_COUNT] = {0};
const crystal_pic_t gCrystalMonPic[CRYSTAL_MON_COUNT] = {0};
const uint8_t gCrystalPicTiles[CRYSTAL_PIC_POOL][16] = {0};
const uint8_t gCrystalPicFrames[CRYSTAL_PIC_FRAMES][CRYSTAL_PIC_TILES] = {0};
const uint8_t gCrystalPicAnim[CRYSTAL_PIC_ANIM][2] = {0};
const uint8_t gCrystalMonBackPic[CRYSTAL_MON_COUNT][CRYSTAL_PIC_TILES][16] = {0};
const uint16_t gCrystalMonPalette[CRYSTAL_MON_COUNT][2] = {0};
const uint16_t gCrystalMonShinyPalette[CRYSTAL_MON_COUNT][2] = {0};
const uint8_t gCrystalTrainerPic[CRYSTAL_TRAINER_PIC_COUNT][CRYSTAL_TRAINER_PIC_TILES][16] = {0};
const uint8_t gCrystalTrainerPicValid[CRYSTAL_TRAINER_PIC_COUNT] = {0};
const uint8_t gCrystalTrainerPalValid[CRYSTAL_TRAINER_PIC_COUNT] = {0};
const uint16_t gCrystalTrainerPalette[CRYSTAL_TRAINER_PIC_COUNT][2] = {0};
const uint8_t gCrystalTrainerPicByClass [CRYSTAL_TRAINER_CLASS_COUNT][CRYSTAL_TRAINER_PIC_TILES][16] = {0};
const uint8_t gCrystalTrainerPicByClassValid[CRYSTAL_TRAINER_CLASS_COUNT] = {0};
const uint16_t gCrystalTrainerPaletteByClass[CRYSTAL_TRAINER_CLASS_COUNT][2] = {0};
const uint8_t gFontGraphics[FONTGRAPHICS_TILES][16] = {0};
const uint8_t gHpBarAndStatusGraphics[HPBARANDSTATUSGRAPHICS_TILES][16] = {0};
const uint8_t gBattleHudTiles1[BATTLEHUDTILES1_TILES][16] = {0};
const uint8_t gBattleHudTiles2[BATTLEHUDTILES2_TILES][16] = {0};
const uint8_t gBattleHudTiles3[BATTLEHUDTILES3_TILES][16] = {0};
const uint8_t gEXPBarGraphics[EXPBARGRAPHICS_TILES][16] = {0};

int Gen2EvosMoves_MovesAtLevel(uint8_t dex, uint8_t level, uint8_t out_moves[4]) { return 0; }
int JohtoTrainer_Find(const char *class_name, int trainer_no) { return 0; }

#pragma once
#include <stdint.h>

#define SCREEN_WIDTH        20
#define SCREEN_HEIGHT       18
#define SCREEN_AREA         360
#define SCREEN_WIDTH_PX     160
#define SCREEN_HEIGHT_PX    144
#define TILEMAP_WIDTH       32
#define TILEMAP_HEIGHT      32
#define TILE_SIZE           16
#define TILE_PX             8
#define SCREEN_WIDTH_MAX    42
#define SCREEN_WIDTH_MAX_PX (SCREEN_WIDTH_MAX * TILE_PX)
#define OAM_Y_OFS           16
#define OAM_X_OFS           8
#define MAX_SPRITES         114

#define BLOCK_WIDTH         4
#define BLOCK_HEIGHT        4
#define SCREEN_BLOCK_WIDTH  6
#define SCREEN_BLOCK_HEIGHT 5
#define SURROUNDING_WIDTH   24
#define SURROUNDING_HEIGHT  20
#define MAP_BORDER          3

#define PAD_A               0x01
#define PAD_B               0x02
#define PAD_SELECT          0x04
#define PAD_START           0x08
#define PAD_RIGHT           0x10
#define PAD_LEFT            0x20
#define PAD_UP              0x40
#define PAD_DOWN            0x80
#define PAD_BUTTONS         0x0F
#define PAD_CTRL_PAD        0xF0

#define BIT_BATTLE_ANIMATION 7

#define STATUS_SLP_MASK     0x07
#define STATUS_PSN          (1<<3)
#define STATUS_BRN          (1<<4)
#define STATUS_FRZ          (1<<5)
#define STATUS_PAR          (1<<6)
#define IS_ASLEEP(s)        ((s) & STATUS_SLP_MASK)
#define IS_POISONED(s)      ((s) & STATUS_PSN)
#define IS_BURNED(s)        ((s) & STATUS_BRN)
#define IS_FROZEN(s)        ((s) & STATUS_FRZ)
#define IS_PARALYZED(s)     ((s) & STATUS_PAR)

#define STAT_STAGE_MIN      1
#define STAT_STAGE_NORMAL   7
#define STAT_STAGE_MAX      13
#define NUM_STAT_MODS       6

static const uint16_t STAT_MOD_NUM[13] = {25,28,33,40,50,66,100,150,200,250,300,350,400};
static const uint16_t STAT_MOD_DEN[13] = {100,100,100,100,100,100,100,100,100,100,100,100,100};

#define PARTY_LENGTH        6
#define BOX_CAPACITY        20
#define NUM_BOXES           12
#define BAG_ITEM_CAPACITY   20
#define PC_ITEM_CAPACITY    50
#define NUM_MOVES           4
#define MAX_LEVEL           100
#define NUM_POKEMON         151
#define NUM_WILD_SLOTS      10

#define SPECIES_RHYDON      0x01
#define SPECIES_KANGASKHAN  0x02
#define SPECIES_NIDORAN_M   0x03
#define SPECIES_CLEFAIRY    0x04
#define SPECIES_SPEAROW     0x05
#define SPECIES_VOLTORB     0x06
#define SPECIES_NIDOKING    0x07
#define SPECIES_SLOWBRO     0x08
#define SPECIES_IVYSAUR     0x09
#define SPECIES_EXEGGUTOR   0x0A
#define SPECIES_LICKITUNG   0x0B
#define SPECIES_EXEGGCUTE   0x0C
#define SPECIES_GRIMER      0x0D
#define SPECIES_GENGAR      0x0E
#define SPECIES_NIDORAN_F   0x0F
#define SPECIES_NIDOQUEEN   0x10
#define SPECIES_CUBONE      0x11
#define SPECIES_RHYHORN     0x12
#define SPECIES_LAPRAS      0x13
#define SPECIES_ARCANINE    0x14
#define SPECIES_MEW         0x15
#define SPECIES_GYARADOS    0x16
#define SPECIES_SHELLDER    0x17
#define SPECIES_TENTACOOL   0x18
#define SPECIES_GASTLY      0x19
#define SPECIES_SCYTHER     0x1A
#define SPECIES_STARYU      0x1B
#define SPECIES_BLASTOISE   0x1C
#define SPECIES_PINSIR      0x1D
#define SPECIES_TANGELA     0x1E
#define SPECIES_GROWLITHE   0x21
#define SPECIES_ONIX        0x22
#define SPECIES_FEAROW      0x23
#define SPECIES_PIDGEY      0x24
#define SPECIES_SLOWPOKE    0x25
#define SPECIES_KADABRA     0x26
#define SPECIES_GRAVELER    0x27
#define SPECIES_CHANSEY     0x28
#define SPECIES_MACHOKE     0x29
#define SPECIES_MR_MIME     0x2A
#define SPECIES_HITMONLEE   0x2B
#define SPECIES_HITMONCHAN  0x2C
#define SPECIES_ARBOK       0x2D
#define SPECIES_PARASECT    0x2E
#define SPECIES_PSYDUCK     0x2F
#define SPECIES_DROWZEE     0x30
#define SPECIES_GOLEM       0x31
#define SPECIES_MAGMAR      0x33
#define SPECIES_ELECTABUZZ  0x35
#define SPECIES_MAGNETON    0x36
#define SPECIES_KOFFING     0x37
#define SPECIES_MANKEY      0x39
#define SPECIES_SEEL        0x3A
#define SPECIES_DIGLETT     0x3B
#define SPECIES_TAUROS      0x3C
#define SPECIES_FARFETCHD   0x40
#define SPECIES_VENONAT     0x41
#define SPECIES_DRAGONITE   0x42
#define SPECIES_DODUO       0x46
#define SPECIES_POLIWAG     0x47
#define SPECIES_JYNX        0x48
#define SPECIES_MOLTRES     0x49
#define SPECIES_ARTICUNO    0x4A
#define SPECIES_ZAPDOS      0x4B
#define SPECIES_DITTO       0x4C
#define SPECIES_MEOWTH      0x4D
#define SPECIES_KRABBY      0x4E
#define SPECIES_VULPIX      0x52
#define SPECIES_NINETALES   0x53
#define SPECIES_PIKACHU     0x54
#define SPECIES_RAICHU      0x55
#define SPECIES_DRATINI     0x58
#define SPECIES_DRAGONAIR   0x59
#define SPECIES_KABUTO      0x5A
#define SPECIES_KABUTOPS    0x5B
#define SPECIES_HORSEA      0x5C
#define SPECIES_SEADRA      0x5D
#define SPECIES_SANDSHREW   0x60
#define SPECIES_SANDSLASH   0x61
#define SPECIES_OMANYTE     0x62
#define SPECIES_OMASTAR     0x63
#define SPECIES_JIGGLYPUFF  0x64
#define SPECIES_WIGGLYTUFF  0x65
#define SPECIES_EEVEE       0x66
#define SPECIES_FLAREON     0x67
#define SPECIES_JOLTEON     0x68
#define SPECIES_VAPOREON    0x69
#define SPECIES_MACHOP      0x6A
#define SPECIES_ZUBAT       0x6B
#define SPECIES_EKANS       0x6C
#define SPECIES_PARAS       0x6D
#define SPECIES_POLIWHIRL   0x6E
#define SPECIES_WEEDLE      0x70
#define SPECIES_KAKUNA      0x71
#define SPECIES_BEEDRILL    0x72
#define SPECIES_DODRIO      0x74
#define SPECIES_PRIMEAPE    0x75
#define SPECIES_DUGTRIO     0x76
#define SPECIES_VENOMOTH    0x77
#define SPECIES_DEWGONG     0x78
#define SPECIES_CATERPIE    0x7B
#define SPECIES_METAPOD     0x7C
#define SPECIES_BUTTERFREE  0x7D
#define SPECIES_MACHAMP     0x7E
#define SPECIES_GOLDUCK     0x80
#define SPECIES_HYPNO       0x81
#define SPECIES_GOLBAT      0x82
#define SPECIES_MEWTWO      0x83
#define SPECIES_SNORLAX     0x84
#define SPECIES_MAGIKARP    0x85
#define SPECIES_MUK         0x88
#define SPECIES_KINGLER     0x8A
#define SPECIES_CLOYSTER    0x8B
#define SPECIES_ELECTRODE   0x8D
#define SPECIES_CLEFABLE    0x8E
#define SPECIES_WEEZING     0x8F
#define SPECIES_PERSIAN     0x90
#define SPECIES_MAROWAK     0x91
#define SPECIES_HAUNTER     0x93
#define SPECIES_ABRA        0x94
#define SPECIES_ALAKAZAM    0x95
#define SPECIES_PIDGEOTTO   0x96
#define SPECIES_PIDGEOT     0x97
#define SPECIES_STARMIE     0x98
#define SPECIES_BULBASAUR   0x99
#define SPECIES_VENUSAUR    0x9A
#define SPECIES_TENTACRUEL  0x9B
#define SPECIES_GOLDEEN     0x9D
#define SPECIES_SEAKING     0x9E
#define SPECIES_PONYTA      0xA3
#define SPECIES_RAPIDASH    0xA4
#define SPECIES_RATTATA     0xA5
#define SPECIES_RATICATE    0xA6
#define SPECIES_NIDORINO    0xA7
#define SPECIES_NIDORINA    0xA8
#define SPECIES_GEODUDE     0xA9
#define SPECIES_PORYGON     0xAA
#define SPECIES_AERODACTYL  0xAB
#define SPECIES_MAGNEMITE   0xAD
#define SPECIES_CHARMANDER  0xB0
#define SPECIES_SQUIRTLE    0xB1
#define SPECIES_CHARMELEON  0xB2
#define SPECIES_WARTORTLE   0xB3
#define SPECIES_CHARIZARD   0xB4
#define SPECIES_ODDISH      0xB9
#define SPECIES_GLOOM       0xBA
#define SPECIES_VILEPLUME   0xBB
#define SPECIES_BELLSPROUT  0xBC
#define SPECIES_WEEPINBELL  0xBD
#define SPECIES_VICTREEBEL  0xBE

#define OPP_ID_OFFSET       200
#define NUM_TRAINERS        47

#define OPP_YOUNGSTER       (OPP_ID_OFFSET + 1)
#define OPP_BUG_CATCHER     (OPP_ID_OFFSET + 2)
#define OPP_LASS            (OPP_ID_OFFSET + 3)
#define OPP_SAILOR          (OPP_ID_OFFSET + 4)
#define OPP_JR_TRAINER_M    (OPP_ID_OFFSET + 5)
#define OPP_JR_TRAINER_F    (OPP_ID_OFFSET + 6)
#define OPP_POKEMANIAC      (OPP_ID_OFFSET + 7)
#define OPP_SUPER_NERD      (OPP_ID_OFFSET + 8)
#define OPP_HIKER           (OPP_ID_OFFSET + 9)
#define OPP_BIKER           (OPP_ID_OFFSET + 10)
#define OPP_BURGLAR         (OPP_ID_OFFSET + 11)
#define OPP_ENGINEER        (OPP_ID_OFFSET + 12)
#define OPP_JUGGLER_X       (OPP_ID_OFFSET + 13)
#define OPP_FISHER          (OPP_ID_OFFSET + 14)
#define OPP_SWIMMER         (OPP_ID_OFFSET + 15)
#define OPP_CUEIST          (OPP_ID_OFFSET + 16)
#define OPP_GAMER           (OPP_ID_OFFSET + 17)
#define OPP_BEAUTY          (OPP_ID_OFFSET + 18)
#define OPP_PSYCHIC_TR      (OPP_ID_OFFSET + 19)
#define OPP_ROCKER          (OPP_ID_OFFSET + 20)
#define OPP_JUGGLER         (OPP_ID_OFFSET + 21)
#define OPP_TAMER           (OPP_ID_OFFSET + 22)
#define OPP_BIRDKEEPER      (OPP_ID_OFFSET + 23)
#define OPP_BLACKBELT       (OPP_ID_OFFSET + 24)
#define OPP_RIVAL1          (OPP_ID_OFFSET + 25)
#define OPP_PROF_OAK        (OPP_ID_OFFSET + 26)
#define OPP_CHIEF           (OPP_ID_OFFSET + 27)
#define OPP_SCIENTIST       (OPP_ID_OFFSET + 28)
#define OPP_GIOVANNI        (OPP_ID_OFFSET + 29)
#define OPP_ROCKET          (OPP_ID_OFFSET + 30)
#define OPP_COOLTRAINER_M   (OPP_ID_OFFSET + 31)
#define OPP_COOLTRAINER_F   (OPP_ID_OFFSET + 32)
#define OPP_BRUNO           (OPP_ID_OFFSET + 33)
#define OPP_BROCK           (OPP_ID_OFFSET + 34)
#define OPP_MISTY           (OPP_ID_OFFSET + 35)
#define OPP_LT_SURGE        (OPP_ID_OFFSET + 36)
#define OPP_ERIKA           (OPP_ID_OFFSET + 37)
#define OPP_KOGA            (OPP_ID_OFFSET + 38)
#define OPP_BLAINE          (OPP_ID_OFFSET + 39)
#define OPP_SABRINA         (OPP_ID_OFFSET + 40)
#define OPP_GENTLEMAN       (OPP_ID_OFFSET + 41)
#define OPP_RIVAL2          (OPP_ID_OFFSET + 42)
#define OPP_RIVAL3          (OPP_ID_OFFSET + 43)
#define OPP_LORELEI         (OPP_ID_OFFSET + 44)
#define OPP_CHANNELER       (OPP_ID_OFFSET + 45)
#define OPP_AGATHA          (OPP_ID_OFFSET + 46)
#define OPP_LANCE           (OPP_ID_OFFSET + 47)

#define TRAINER_ATK_DV      9
#define TRAINER_DEF_DV      8
#define TRAINER_SPD_DV      8
#define TRAINER_SPC_DV      8
#define TRAINER_HP_DV       8

#define NAME_LENGTH         11
#define PLAYER_NAME_LENGTH  8
#define MOVE_NAME_LENGTH    14
#define ITEM_NAME_LENGTH    13

#define MOVE_LENGTH         6

#define PP_UP_MASK          0xC0
#define PP_MASK             0x3F

#define TYPE_NORMAL         0
#define TYPE_FIGHTING       1
#define TYPE_FLYING         2
#define TYPE_POISON         3
#define TYPE_GROUND         4
#define TYPE_ROCK           5
#define TYPE_BIRD           6
#define TYPE_BUG            7
#define TYPE_GHOST          8

#define TYPE_FIRE           20
#define TYPE_WATER          21
#define TYPE_GRASS          22
#define TYPE_ELECTRIC       23
#define TYPE_PSYCHIC        24
#define TYPE_ICE            25
#define TYPE_DRAGON         26
#define NUM_TYPES           27

#define TYPE_SUPER_EFFECTIVE    2
#define TYPE_NORMAL_EFFECTIVE   1
#define TYPE_NOT_VERY_EFFECTIVE 0
#define TYPE_NO_EFFECT          0

#define TILESET_OVERWORLD   0
#define TILESET_REDS_HOUSE1 1
#define TILESET_MART        2
#define TILESET_FOREST      3
#define TILESET_REDS_HOUSE2 4
#define TILESET_DOJO        5
#define TILESET_POKECENTER  6
#define TILESET_GYM         7
#define TILESET_HOUSE       8
#define TILESET_FOREST_GATE 9
#define TILESET_MUSEUM      10
#define TILESET_UNDERGROUND 11
#define TILESET_GATE        12
#define TILESET_SHIP        13
#define TILESET_SHIP_PORT   14
#define TILESET_CEMETERY    15
#define TILESET_INTERIOR    16
#define TILESET_CAVERN      17
#define TILESET_LOBBY       18
#define TILESET_MANSION     19
#define TILESET_LAB         20
#define TILESET_CLUB        21
#define TILESET_FACILITY    22
#define TILESET_PLATEAU     23
#define NUM_TILESETS        24

#define TILEANIM_NONE           0
#define TILEANIM_WATER          1
#define TILEANIM_WATER_FLOWER   2

#define GROWTH_MEDIUM_FAST      0
#define GROWTH_SLIGHTLY_FAST    1
#define GROWTH_SLIGHTLY_SLOW    2
#define GROWTH_MEDIUM_SLOW      3
#define GROWTH_FAST             4
#define GROWTH_SLOW             5

#define EVOLVE_LEVEL        1
#define EVOLVE_ITEM         2
#define EVOLVE_TRADE        3

#define CHAN1               0
#define CHAN2               1
#define CHAN3               2
#define CHAN4               3
#define CHAN5               4
#define CHAN6               5
#define CHAN7               6
#define CHAN8               7
#define NUM_CHANNELS        8
#define NUM_MUSIC_CHANS     4
#define SFX_STOP_ALL_MUSIC  0xFF

#define OAM_FLAG_PRIORITY   0x80
#define OAM_FLAG_FLIP_Y     0x40
#define OAM_FLAG_FLIP_X     0x20
#define OAM_FLAG_PALETTE    0x10

#define MAX_WARP_EVENTS     32
#define MAX_BG_EVENTS       16
#define MAX_OBJECT_EVENTS   16
#define SPRITE_SET_LENGTH   11
#define DEST_WARP_NONE      0xFF

#define MIN_NEUTRAL_DAMAGE  2
#define MAX_NEUTRAL_DAMAGE  999
#define MAX_PREDEF          112

#define BSTAT1_STORING_ENERGY       0
#define BSTAT1_THRASHING_ABOUT      1
#define BSTAT1_ATTACKING_MULTIPLE   2
#define BSTAT1_FLINCHED             3
#define BSTAT1_CHARGING_UP          4
#define BSTAT1_USING_TRAPPING       5
#define BSTAT1_INVULNERABLE         6
#define BSTAT1_CONFUSED             7

#define BSTAT2_USING_X_ACCURACY     0
#define BSTAT2_PROTECTED_BY_MIST    1
#define BSTAT2_GETTING_PUMPED       2

#define BSTAT2_HAS_SUBSTITUTE       4
#define BSTAT2_NEEDS_TO_RECHARGE    5
#define BSTAT2_USING_RAGE           6
#define BSTAT2_SEEDED               7

#define BSTAT3_BADLY_POISONED       0
#define BSTAT3_HAS_LIGHT_SCREEN     1
#define BSTAT3_HAS_REFLECT          2
#define BSTAT3_TRANSFORMED          3

#define MOD_ATTACK   0
#define MOD_DEFENSE  1
#define MOD_SPEED    2
#define MOD_SPECIAL  3
#define MOD_ACCURACY 4
#define MOD_EVASION  5

#define HOF_MON_SIZE        16
#define HOF_TEAM_SIZE       (PARTY_LENGTH * HOF_MON_SIZE)
#define HOF_TEAM_CAPACITY   50

#define TX_START            0x00
#define TX_RAM              0x01
#define TX_BCD              0x02
#define TX_MOVE             0x03
#define TX_BOX              0x04
#define TX_LOW              0x05
#define TX_PROMPT_BUTTON    0x06
#define TX_SCROLL           0x07
#define TX_START_ASM        0x08
#define TX_NUM              0x09
#define TX_PAUSE            0x0A
#define TX_SOUND_GET_ITEM1  0x0B
#define TX_DOTS             0x0C
#define TX_WAIT_BUTTON      0x0D
#define TX_END              0x50

#define TX_SCRIPT_VENDING_MACHINE           0xF5
#define TX_SCRIPT_CABLE_CLUB_RECEPTIONIST   0xF6
#define TX_SCRIPT_PRIZE_VENDOR              0xF7
#define TX_SCRIPT_POKECENTER_PC             0xF9
#define TX_SCRIPT_PLAYERS_PC                0xFC
#define TX_SCRIPT_BILLS_PC                  0xFD
#define TX_SCRIPT_MART                      0xFE
#define TX_SCRIPT_POKECENTER_NURSE          0xFF

#define EFFECT_NONE                     0x00
#define EFFECT_POISON_SIDE1             0x02
#define EFFECT_DRAIN_HP                 0x03
#define EFFECT_BURN_SIDE                0x04
#define EFFECT_FREEZE_SIDE              0x05
#define EFFECT_PARALYZE_SIDE            0x06
#define EFFECT_EXPLODE                  0x07
#define EFFECT_DREAM_EATER              0x08
#define EFFECT_MIRROR_MOVE              0x09
#define EFFECT_ATTACK_UP1               0x0A
#define EFFECT_DEFENSE_UP1              0x0B
#define EFFECT_SPEED_UP1                0x0C
#define EFFECT_SPECIAL_UP1              0x0D
#define EFFECT_ACCURACY_UP1             0x0E
#define EFFECT_EVASION_UP1              0x0F
#define EFFECT_PAY_DAY                  0x10
#define EFFECT_SWIFT                    0x11
#define EFFECT_ATTACK_DOWN1             0x12
#define EFFECT_DEFENSE_DOWN1            0x13
#define EFFECT_SPEED_DOWN1              0x14
#define EFFECT_SPECIAL_DOWN1            0x15
#define EFFECT_ACCURACY_DOWN1           0x16
#define EFFECT_EVASION_DOWN1            0x17
#define EFFECT_CONVERSION               0x18
#define EFFECT_HAZE                     0x19
#define EFFECT_BIDE                     0x1A
#define EFFECT_THRASH                   0x1B
#define EFFECT_SWITCH_TELEPORT          0x1C
#define EFFECT_TWO_TO_FIVE_ATTACKS      0x1D
#define EFFECT_1E                       0x1E
#define EFFECT_FLINCH_SIDE1             0x1F
#define EFFECT_SLEEP                    0x20
#define EFFECT_POISON_SIDE2             0x21
#define EFFECT_BURN_SIDE2               0x22
#define EFFECT_FREEZE_SIDE2             0x23
#define EFFECT_PARALYZE_SIDE2           0x24
#define EFFECT_FLINCH_SIDE2             0x25
#define EFFECT_OHKO                     0x26
#define EFFECT_CHARGE                   0x27
#define EFFECT_SUPER_FANG               0x28
#define EFFECT_SPECIAL_DAMAGE           0x29
#define EFFECT_TRAPPING                 0x2A
#define EFFECT_FLY                      0x2B
#define EFFECT_ATTACK_TWICE             0x2C
#define EFFECT_JUMP_KICK                0x2D
#define EFFECT_MIST                     0x2E
#define EFFECT_FOCUS_ENERGY             0x2F
#define EFFECT_RECOIL                   0x30
#define EFFECT_CONFUSION                0x31
#define EFFECT_ATTACK_UP2               0x32
#define EFFECT_DEFENSE_UP2              0x33
#define EFFECT_SPEED_UP2                0x34
#define EFFECT_SPECIAL_UP2              0x35
#define EFFECT_ACCURACY_UP2             0x36
#define EFFECT_EVASION_UP2              0x37
#define EFFECT_HEAL                     0x38
#define EFFECT_TRANSFORM                0x39
#define EFFECT_ATTACK_DOWN2             0x3A
#define EFFECT_DEFENSE_DOWN2            0x3B
#define EFFECT_SPEED_DOWN2              0x3C
#define EFFECT_SPECIAL_DOWN2            0x3D
#define EFFECT_ACCURACY_DOWN2           0x3E
#define EFFECT_EVASION_DOWN2            0x3F
#define EFFECT_LIGHT_SCREEN             0x40
#define EFFECT_REFLECT                  0x41
#define EFFECT_POISON                   0x42
#define EFFECT_PARALYZE                 0x43
#define EFFECT_ATTACK_DOWN_SIDE         0x44
#define EFFECT_DEFENSE_DOWN_SIDE        0x45
#define EFFECT_SPEED_DOWN_SIDE          0x46
#define EFFECT_SPECIAL_DOWN_SIDE        0x47
#define EFFECT_CONFUSION_SIDE           0x4C
#define EFFECT_TWINEEDLE                0x4D
#define EFFECT_SUBSTITUTE               0x4F
#define EFFECT_HYPER_BEAM               0x50
#define EFFECT_RAGE                     0x51
#define EFFECT_MIMIC                    0x52
#define EFFECT_METRONOME                0x53
#define EFFECT_LEECH_SEED               0x54
#define EFFECT_SPLASH                   0x55
#define EFFECT_DISABLE                  0x56

#define BIT_STAB_DAMAGE     7

#define TYPE_SPECIAL_THRESHOLD  0x14

#define STAT_ATTACK         1
#define STAT_DEFENSE        2
#define STAT_SPEED          3
#define STAT_SPECIAL_STAT   4
#define NUM_STATS           4

#define MOVE_KARATE_CHOP    0x02
#define MOVE_RAZOR_LEAF     0x4B
#define MOVE_CRABHAMMER     0x98
#define MOVE_SLASH          0xA3

#define MOVE_RAZOR_WIND     0x0D
#define MOVE_WHIRLWIND      0x12
#define MOVE_FLY            0x13
#define MOVE_BIND           0x14
#define MOVE_SAND_ATTACK    0x1C
#define MOVE_WRAP           0x23
#define MOVE_TAIL_WHIP      0x27
#define MOVE_TWINEEDLE      0x29
#define MOVE_LEER           0x2B
#define MOVE_GROWL          0x2D
#define MOVE_ROAR           0x2E
#define MOVE_SOLARBEAM      0x4C
#define MOVE_PETAL_DANCE    0x50
#define MOVE_STRING_SHOT    0x51
#define MOVE_FIRE_SPIN      0x53
#define MOVE_DIG            0x5B
#define MOVE_TOXIC          0x5C
#define MOVE_TELEPORT       0x64
#define MOVE_MINIMIZE       0x6B
#define MOVE_SMOKESCREEN    0x6C
#define MOVE_CLAMP          0x80
#define MOVE_SKULL_BASH     0x82
#define MOVE_SKY_ATTACK     0x8F
#define MOVE_REST           0x9C
#define MOVE_STRUGGLE       0xA5

#define MOVE_BLIZZARD       0x3B
#define MOVE_BUBBLEBEAM     0x3D
#define MOVE_MEGA_DRAIN     0x48
#define MOVE_THUNDERBOLT    0x55
#define MOVE_FISSURE        0x5A
#define MOVE_BARRIER        0x70
#define MOVE_BIDE           0x75
#define MOVE_FIRE_BLAST     0x7E
#define MOVE_PSYWAVE        0x95

#define MOVE_CUT            0x0F
#define MOVE_SURF           0x39
#define MOVE_STRENGTH       0x46
#define MOVE_FLASH          0x94

#define STARTER1  SPECIES_CHARMANDER
#define STARTER2  SPECIES_SQUIRTLE
#define STARTER3  SPECIES_BULBASAUR

#define ITEM_MASTER_BALL  0x01
#define ITEM_ULTRA_BALL   0x02
#define ITEM_GREAT_BALL   0x03
#define ITEM_POKE_BALL    0x04
#define ITEM_SAFARI_BALL  0x08

#define ITEM_COIN_CASE    0x45

#define ITEM_ANTIDOTE     0x0B
#define ITEM_BURN_HEAL    0x0C
#define ITEM_ICE_HEAL     0x0D
#define ITEM_AWAKENING    0x0E
#define ITEM_PARLYZ_HEAL  0x0F

#define ITEM_FULL_RESTORE 0x10
#define ITEM_MAX_POTION   0x11
#define ITEM_HYPER_POTION 0x12
#define ITEM_SUPER_POTION 0x13
#define ITEM_POTION       0x14
#define ITEM_FRESH_WATER  0x3C
#define ITEM_SODA_POP     0x3D
#define ITEM_LEMONADE     0x3E

#define ITEM_HP_UP        0x23
#define ITEM_PROTEIN      0x24
#define ITEM_IRON         0x25
#define ITEM_CARBOS       0x26
#define ITEM_CALCIUM      0x27

#define ITEM_PP_UP        0x4F
#define ITEM_ETHER        0x50
#define ITEM_MAX_ETHER    0x51
#define ITEM_ELIXER       0x52
#define ITEM_MAX_ELIXER   0x53

#define ITEM_FULL_HEAL    0x34
#define ITEM_REVIVE       0x35
#define ITEM_MAX_REVIVE   0x36

#define ITEM_GUARD_SPEC   0x37
#define ITEM_DIRE_HIT     0x3A
#define ITEM_X_ACCURACY   0x2E
#define ITEM_X_ATTACK     0x41
#define ITEM_X_DEFEND     0x42
#define ITEM_X_SPEED      0x43
#define ITEM_X_SPECIAL    0x44

#define ITEM_POKE_DOLL    0x33

#define ITEM_POKE_FLUTE   0x49

#define ITEM_HM01         0xC4

#define LINK_STATE_BATTLING 0x04
#define LINK_STATE_TRADING  0x32

#define DAMAGE_MULT_EFFECTIVE   10

#define MOVE_THRASH             0x25
#define MOVE_SONICBOOM          0x31
#define MOVE_COUNTER            0x44
#define MOVE_SEISMIC_TOSS       0x45
#define MOVE_DRAGON_RAGE        0x52
#define MOVE_QUICK_ATTACK       0x62
#define MOVE_RAGE               0x63
#define MOVE_MIRROR_MOVE        0x77
#define MOVE_NIGHT_SHADE        0x65
#define MOVE_BIDE               0x75
#define MOVE_PSYWAVE            0x95
#define CANNOT_MOVE             0xFF

#define EFFECT_01               0x01

#define SONICBOOM_DAMAGE        20
#define DRAGON_RAGE_DAMAGE      40

#define BIT_BOULDERBADGE        0
#define BIT_CASCADEBADGE        1
#define BIT_THUNDERBADGE        2
#define BIT_RAINBOWBADGE        3
#define BIT_SOULBADGE           4
#define BIT_MARSHBADGE          5
#define BIT_VOLCANOBADGE        6
#define BIT_EARTHBADGE          7

#define MAX_STAT_VALUE          999

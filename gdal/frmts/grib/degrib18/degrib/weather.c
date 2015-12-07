/*****************************************************************************
 * weather.c
 *
 * DESCRIPTION
 *    This file contains all the utility functions needed to handle weather
 * "ugly" strings.  Originally I didn't need to parse them, but for people
 * to use them in ArcView, I had to.
 *
 * HISTORY
 *   5/2003 Arthur Taylor (MDL / RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
/*
 * Uncomment the following to have error messages stored in the UglyStringType
 * This uses myerror.*
 */
#define STORE_ERRORS

/*
 * Uncomment the following to have error messages sent to stdout.
 */
/* #define VERBOSE */
#undef VERBOSE

/*
 * Uncomment the following to test Weather names.
 */
/* #define DEBUG_WEATHER */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "weather.h"

#ifdef STORE_ERRORS
#include "myerror.h"
#endif

typedef struct {
   const char *abrev, *name;
   uChar number;
} WxTable;

/* Unknown <NoCov>:EW:<NoInten>:<NoVis>:10to20g30 */
/* Original enumeration.
enum {
   WX_NOWX, WX_K, WX_BD, WX_BS, WX_H, WX_F, WX_L, WX_R, WX_RW,
   WX_A, WX_FR, WX_ZL, WX_ZR, WX_IP, WX_S, WX_SW, WX_T
};
*/
enum {
   WX_NOWX, WX_K, WX_BD, WX_BS, WX_H, WX_F, WX_L, WX_R, WX_RW,
   WX_A, WX_FR, WX_ZL, WX_ZR, WX_IP, WX_S, WX_SW, WX_T, WX_BN,
   WX_ZF, WX_IC, WX_IF, WX_VA, WX_ZY, WX_WP, WX_UNKNOWN
};

/* SA -> Snowfall aob freezing */
/* LC -> Caution Advised on area Lakes */
/*   {"WG", "Frequent Gusts", WX_WG},*/
static const WxTable WxCode[] = {
   /* 0 */ {"<NoWx>", "No Weather", WX_NOWX},
   /* Dry Obstruction to visibility. */
   /* 14 */ {"K", "Smoke", WX_K},
   /* 15 */ {"BD", "Blowing Dust", WX_BD},
   /* 13 */ {"BS", "Blowing Snow", WX_BS},
   /* Moist Obstruction to visibility. */
   /* 12 */ {"H", "Haze", WX_H},
   /* 11 */ {"F", "Fog", WX_F},
   /* 5 */ {"L", "Drizzle", WX_L},
   /* Warm moisture. */
   /* 3 */ {"R", "Rain", WX_R},
   /* 4 */ {"RW", "Rain Showers", WX_RW},
/* 'A' has have been dropped as of 8/12/2004 */
   /* 2 */ {"A", "Hail", WX_A},
/* 'A' has have been dropped as of 8/12/2004 */
   /* Freezing / Mix moisture. */
   /* 16 */ {"FR", "Frost", WX_FR},
   /* 7 */ {"ZL", "Freezing Drizzle", WX_ZL},
   /* 6 */ {"ZR", "Freezing Rain", WX_ZR},
   /* Frozen moisture. */
   /* 10 */ {"IP", "Ice Pellets (sleet)", WX_IP},
   /* 8 */ {"S", "Snow", WX_S},
   /* 9 */ {"SW", "Snow Showers", WX_SW},
   /* Extra. */
   /* 1 */ {"T", "Thunderstorms", WX_T},
   {"BN", "Blowing Sand", WX_BN},
   {"ZF", "Freezing Fog", WX_ZF},
   {"IC", "Ice Crystals", WX_IC},
   {"IF", "Ice Fog", WX_IF},
   {"VA", "Volcanic Ash", WX_VA},
   {"ZY", "Freezing Spray", WX_ZY},
   {"WP", "Water Spouts", WX_WP},
   {"<unknown>", "Unknown Weather", WX_UNKNOWN}
};

/* GChc found in output streams... not allowed to add yet. */
/* Original enumeration.
enum {
   COV_NOCOV, COV_ISO, COV_SCT, COV_NUM, COV_WIDE, COV_OCNL, COV_SCHC,
   COV_CHC, COV_LKLY, COV_DEF, COV_PATCHY, COV_AREAS
};
*/
enum {
   COV_NOCOV, COV_ISO, COV_SCT, COV_NUM, COV_WIDE, COV_OCNL, COV_SCHC,
   COV_CHC, COV_LKLY, COV_DEF, COV_PATCHY, COV_AREAS, COV_PDS, COV_FRQ,
   COV_INTER, COV_BRIEF, COV_UNKNOWN
};

static const WxTable WxCover[] = {
   /* 0 */ {"<NoCov>", "No Coverage/Probability", COV_NOCOV},
   /* 1 */ {"Iso", "Isolated", COV_ISO},
   /* 2 */ {"Sct", "Scattered", COV_SCT},
   /* 3 */ {"Num", "Numerous", COV_NUM},
   /* 4 */ {"Wide", "Widespread", COV_WIDE},
   /* 5 */ {"Ocnl", "Occasional", COV_OCNL},
   /* 6 */ {"SChc", "Slight Chance of", COV_SCHC},
   /* 7 */ {"Chc", "Chance of", COV_CHC},
   /* 8 */ {"Lkly", "Likely", COV_LKLY},
   /* 9 */ {"Def", "Definite", COV_DEF},
   /* 10 */ {"Patchy", "Patchy", COV_PATCHY},
   /* 11 */ {"Areas", "Areas of", COV_AREAS},
/* Added 8/13/2004 */
   /* 12 */ {"Pds", "Periods of", COV_PDS},
   /* 13 */ {"Frq", "Frequent", COV_FRQ},
   /* 14 */ {"Inter", "Intermittent", COV_INTER},
   /* 15 */ {"Brf", "Brief", COV_BRIEF},
/* Finished Added 8/13/2004 */
   {"<unknown>", "Unknown Coverage", COV_UNKNOWN}
};

enum { INT_NOINT, INT_DD, INT_D, INT_M, INT_P, INT_UNKNOWN };

static const WxTable WxIntens[] = {
   /* 0 */ {"<NoInten>", "No Intensity", INT_NOINT},
   /* 1 */ {"--", "Very Light", INT_DD},
   /* 2 */ {"-", "Light", INT_D},
   /* 3 */ {"m", "Moderate", INT_M},
   /* 4 */ {"+", "Heavy", INT_P},
   {"<unknown>", "Unknown Intensity", INT_UNKNOWN}
};

enum {
   VIS_NOVIS, VIS_0, VIS_8, VIS_16, VIS_24, VIS_32, VIS_48, VIS_64, VIS_80,
   VIS_96, VIS_128, VIS_160, VIS_192, VIS_224, VIS_UNKNOWN = 255
};

static const WxTable WxVisib[] = {
   /* 0 */ {"<NoVis>", "255", VIS_NOVIS},
   /* 1 */ {"0SM", "0", VIS_0},
   /* 2 */ {"1/4SM", "8", VIS_8},
   /* 3 */ {"1/2SM", "16", VIS_16},
   /* 4 */ {"3/4SM", "24", VIS_24},
   /* 5 */ {"1SM", "32", VIS_32},
   /* 6 */ {"11/2SM", "48", VIS_48},
   /* 7 */ {"2SM", "64", VIS_64},
   /* 8 */ {"21/2SM", "80", VIS_80},
   /* 9 */ {"3SM", "96", VIS_96},
   /* 10 */ {"4SM", "128", VIS_128},
   /* 11 */ {"5SM", "160", VIS_160},
   /* 12 */ {"6SM", "192", VIS_192},
   /* Past 6 SM (encode as 7 SM). */
   /* 13 */ {"P6SM", "224", VIS_224},
   {"<unknown>", "Unknown Visibility", VIS_UNKNOWN}
};

enum {
   HAZ_NOHAZ, HAZ_FL, HAZ_GW, HAZ_HVYRN, HAZ_DMGW, HAZ_A, HAZ_LGA, HAZ_OLA,
   HAZ_OBO, HAZ_OGA, HAZ_DRY, HAZ_TOR, HAZ_UNKNOWN, HAZ_PRI1 = 253,
   HAZ_PRI2 = 254, HAZ_OR = 255
};

/* Note: HazCode currently can handle up to (21 + 4) different WxAttrib
 * numbers because it is stored in a "sInt4" (2^31 = 21,47,48,36,48) */
static const WxTable WxAttrib[] = {
   /* 0 */ {"", "None", HAZ_NOHAZ},
   /* 1 */ {"FL", "Frequent Lightning", HAZ_FL},
   /* 2 */ {"GW", "Gusty Winds", HAZ_GW},
   /* 3 */ {"HvyRn", "Heavy Rain", HAZ_HVYRN},
   /* 4 */ {"DmgW", "Damaging Wind", HAZ_DMGW},
   /* 5 */ {"SmA", "Small Hail", HAZ_A},
   /* 6 */ {"LgA", "Large Hail", HAZ_LGA},
   /* 7 */ {"OLA", "Outlying Areas", HAZ_OLA},
   /* 8 */ {"OBO", "on Bridges and Overpasses", HAZ_OBO},
/* Added 8/13/2004 */
   /* 9 */ {"OGA", "On Grassy Areas", HAZ_OGA},
   /* 10 */ {"Dry", "dry", HAZ_DRY},
   /* 11 */ {"TOR", "Tornado", HAZ_TOR},
   /* 12 */ {"Primary", "Highest Ranking", HAZ_PRI2},
   /* 13 */ {"Mention", "Include Unconditionally", HAZ_PRI1},
/* Finished Added 8/13/2004 */
   /* 14 */ {"OR", "or", HAZ_OR},
   /* 15 */ {"MX", "mixture", HAZ_OR},
   {"<unknown>", "Unknown Hazard", HAZ_UNKNOWN}
};

/*****************************************************************************
 * NDFD_WxTable1() --
 *
 * Original: makeWxImageCodes() Marc Saccucci (MDL)
 * Adapted to NDFD_WxTable() Arthur Taylor / MDL
 *
 * PURPOSE
 *   To use the same weather table scheme used by Marc Saccucci in
 * makeWxImageCodes() in the NDFD source tree.  The purpose of both
 * procedures is to simplify the weather string (aka ugly string) to a single
 * integral code number, which contains the most relevant weather.  The
 * intent is to create a simpler field which can more readily be viewed as
 * an image.
 *
 * ARGUMENTS
 * ugly = The ugly weather string to encode. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (the encoded number.)
 *
 * HISTORY
 *  11/2002 Marc Saccucci (MDL): Created matching algorithm in
 *              makeWxImageCodes().
 *   6/2003 MS: Altered matching combinations in makeWxImageCodes().
 *   7/2003 Arthur Taylor (MDL/RSIS): Created NDFD_WxTable()
 *
 * NOTES
 *  1) The table used:
 * new_code  primary weather/probability       Description (sample value)
 * ========  ===========================       ==========================
 * 0         <NoWx>                            No weather
 * 1         L/Sct,SChc,Patchy,Iso,Chc         Rain (LoProb L)
 * 2         R-/Sct,SChc,Patchy,Iso,Chc        Rain (LoProb R-)
 * 3         R/Sct,SChc,Patchy,Iso,Chc         Rain (LoProb R)
 * 4         R+/Sct,SChc,Patchy,Iso,Chc        Rain (LoProb R+)
 * 5         R/T;Sct,SChc,Patchy,Iso,Chc       Rain (LoProb R/T)
 * 6         RW/Sct,SChc,Patchy,Iso,Chc        Rain (LoProb Rw)
 * 7         RW/T;Sct,SChc,Patchy,Iso,Chc      Rain (LoProb RW/T)
 * 8         T/Sct,SChc,Patchy,Iso,Chc         Rain (LoProb T)
 * 9         L/Wide,Lkly,Num,Ocnl,Def,Areas    Rain (HiProb L)
 * 10        R-/Wide,Lkly,Num,Ocnl,Def,Areas   Rain (HiProb R-)
 * 11        R/Wide,Lkly,Num,Ocnl,Def,Areas    Rain (HiProb R)
 * 12        R+/Wide,Lkly,Num,Ocnl,Def,Areas   Rain (HiProb R+)
 * 13        R/T;Wide,Lkly,Num,Ocnl,Def,Areas  Rain (HiProb R/T)
 * 14        RW/Wide,Lkly,Num,Ocnl,Def,Areas   Rain (HiProb RW)
 * 15        RW/T;Wide,Lkly,Num,Ocnl,Def,Areas Rain (HiProb RW/T)
 * 16        T/Wide,Lkly,Num,Ocnl,Def,Areas    Rain (HiProb T)
 * 17        T+                                Severe Tstorms
 * 18        R/S;Sct,SChc,Patchy,Iso,Chc       Wintry Mix (LoProb R/S)
 * 19        RW/SW;Sct,SChc,Patchy,Iso,Chc     Wintry Mix (LoProb RW/SW)
 * 20        R/IP;Sct,SChc,Patchy,Iso,Chc      Wintry Mix (LoProb R/IP)
 * 21        S/IP;Sct,SChc,Patchy,Iso,Chc      Wintry Mix (LoProb S/IP)
 * 22        R/S;Wide,Lkly,Num,Ocnl,Def,Areas  Wintry Mix (HiProb R/S)
 * 23        RW/SW;Wide,Lkly,Num,Ocnl,Def,AreasWintry Mix (HiProb RW/SW)
 * 24        R/IP;Wide,Lkly,Num,Ocnl,Def,Areas Wintry Mix (HiProb R/IP)
 * 25        S/IP;Wide,Lkly,Num,Ocnl,Def,Areas Wintry Mix (HiProb S/IP)
 * 26        IP-/Sct,SChc,Patchy,Iso,Chc       Ice (LoProb IP-)
 * 27        IP/Sct,SChc,Patchy,Iso,Chc        Ice (LoProb IP)
 * 28        IP+/Sct,SChc,Patchy,Iso,Chc       Ice (LoProb IP+)
 * 29        ZL/Sct,SChc,Patchy,Iso,Chc        Ice (LoProb ZL)
 * 30        ZL/R;Sct,SChc,Patchy,Iso,Chc      Ice (LoProb R/ZL)
 * 31        ZR-/Sct,SChc,Patchy,Iso,Chc       Ice (LoProb ZR-)
 * 32        ZR/Sct,SChc,Patchy,Iso,Chc        Ice (LoProb ZR)
 * 33        ZR+/Sct,SChc,Patchy,Iso,Chc       Ice (LoProb ZR+)
 * 34        ZR/R;Sct,SChc,Patchy,Iso,Chc      Ice (LoProb R/ZR)
 * 35        ZR/IP;Sct,SChc,Patchy,Iso,Chc     Ice (LoProb ZR/IP)
 * 36        IP-/Wide,Lkly,Num,Ocnl,Def,Areas  Ice (HiProb IP-)
 * 37        IP/Wide,Lkly,Num,Ocnl,Def,Areas   Ice (HiProb IP)
 * 38        IP+/Wide,Lkly,Num,Ocnl,Def,Areas  Ice (HiProb IP+)
 * 39        ZL/Wide,Lkly,Num,Ocnl,Def,Areas   Ice (HiProb ZL)
 * 40        ZL/R;Wide,Lkly,Num,Ocnl,Def,Areas Ice (HiProb R/ZL)
 * 41        ZR-/Wide,Lkly,Num,Ocnl,Def,Areas  Ice (HiProb ZR-)
 * 42        ZR/Wide,Lkly,Num,Ocnl,Def,Areas   Ice (HiProb ZR)
 * 43        ZR+/Wide,Lkly,Num,Ocnl,Def,Areas  Ice (HiProb ZR+)
 * 44        ZR/R;Wide,Lkly,Num,Ocnl,Def,Areas Ice (HiProb R/ZR)
 * 45        ZR/IP;Wide,Lkly,Num,Ocnl,Def,AreasIce (HiProb ZR/IP)
 * 46        SW/Sct,SChc,Patchy,Iso,Chc        Snow (LoProb SW)
 * 47        S-/Sct,SChc,Patchy,Iso,Chc        Snow (LoProb S-)
 * 48        S/Sct,SChc,Patchy,Iso,Chc         Snow (LoProb S)
 * 49        S+/Sct,SChc,Patchy,Iso,Chc        Snow (LoProb S+)
 * 50        SW/Wide,Lkly,Num,Ocnl,Def,Areas   Snow (HiProb SW)
 * 51        S-/Wide,Lkly,Num,Ocnl,Def,Areas   Snow (HiProb S-)
 * 52        S/Wide,Lkly,Num,Ocnl,Def,Areas    Snow (HiProb S)
 * 53        S+/Wide,Lkly,Num,Ocnl,Def,Areas   Snow (HiProb S+)
 * 54        F                                 Fog
 * 55        H                                 Haze
 * 56        K                                 Smoke
 * 57        BS                                Blowing Snow
 * 58        BD                                Blowing Dust
 *****************************************************************************
 */
static int NDFD_WxTable1 (UglyStringType * ugly)
{
   switch (ugly->wx[0]) {
      case WX_NOWX:
         return 0;
      case WX_R:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_S:
               case WX_SW:
                  return 18; /* Rain/Snow Showers */
               case WX_ZR:
                  return 34; /* Rain/Freezing Rain */
               case WX_IP:
                  return 20; /* Rain/Sleet */
               case WX_ZL:
                  return 30; /* Rain/Freezing Drizzle */
               case WX_T:
                  return 5; /* Rain/Thunderstorms */
               default:
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 2; /* Light Rain */
                     case INT_P:
                        return 4; /* Heavy Rain */
                     default:
                        return 3; /* Normal Rain */
                  }
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_S:
               case WX_SW:
                  return 22; /* Rain/Snow Showers */
               case WX_ZR:
                  return 44; /* Rain/Freezing Rain */
               case WX_IP:
                  return 24; /* Rain/Sleet */
               case WX_ZL:
                  return 40; /* Rain/Freezing Drizzle */
               case WX_T:
                  return 13; /* Rain/Thunderstorms */
               default:
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 10; /* Light Rain */
                     case INT_P:
                        return 12; /* Heavy Rain */
                     default:
                        return 11; /* Normal Rain */
                  }
            }
         }
      case WX_RW:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_T:
                  return 7; /* Rain Showers/Thunderstorms */
               case WX_SW:
                  return 19; /* Rain Showers/Snow Showers */
               default:
                  return 6; /* Rain Showers */
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_T:
                  return 15; /* Rain Showers/Thunderstorms */
               case WX_SW:
                  return 23; /* Rain Showers/Snow Showers */
               default:
                  return 14; /* Rain Showers */
            }
         }
      case WX_L:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_ZL:
                  return 29; /* Drizzle/Freezing Drizzle */
               case WX_F:
               default:
                  return 1; /* Drizzle */
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_ZL:
                  return 40; /* Drizzle/Freezing Drizzle */
               case WX_F:
               default:
                  return 9; /* Drizzle */
            }
         }
      case WX_ZL:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_R:
                  return 30; /* Freezing Drizzle/Rain */
               case WX_L:
               default:
                  return 29; /* Freezing Drizzle */
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_R:
                  return 40; /* Freezing Drizzle/Rain */
               case WX_L:
               default:
                  return 39; /* Freezing Drizzle */
            }
         }
      case WX_ZR:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_R:
                  return 34; /* Freezing Rain/Rain */
               case WX_IP:
                  return 35; /* Freezing Rain/Sleet */
               default:
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 31; /* Light Freezing Rain */
                     case INT_P:
                        return 33; /* Heavy Freezing Rain */
                     default:
                        return 32; /* Normal Freezing Rain */
                  }
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_R:
                  return 44; /* Freezing Rain/Rain */
               case WX_IP:
                  return 45; /* Freezing Rain/Sleet */
               default:
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 41; /* Light Freezing Rain */
                     case INT_P:
                        return 43; /* Heavy Freezing Rain */
                     default:
                        return 42; /* Normal Freezing Rain */
                  }
            }
         }
      case WX_IP:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_R:
                  return 20; /* Sleet/Rain */
               case WX_S:
                  return 21; /* Sleet/Snow */
               case WX_ZR:
                  return 35; /* Sleet/Freezing Rain */
               default:
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 26; /* Light Sleet */
                     case INT_P:
                        return 28; /* Heavy Sleet */
                     default:
                        return 27; /* Normal Sleet */
                  }
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_R:
                  return 24; /* Sleet/Rain */
               case WX_S:
                  return 25; /* Sleet/Snow */
               case WX_ZR:
                  return 45; /* Sleet/Freezing Rain */
               default:
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 36; /* Light Sleet */
                     case INT_P:
                        return 38; /* Heavy Sleet */
                     default:
                        return 37; /* Normal Sleet */
                  }
            }
         }
      case WX_SW:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_R:
                  return 18; /* Snow Showers/Rain */
               case WX_RW:
                  return 19; /* Snow Showers/Rain Showers */
               default:
                  return 46; /* Snow Showers */
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_R:
                  return 22; /* Snow Showers/Rain */
               case WX_RW:
                  return 23; /* Snow Showers/Rain Showers */
               default:
                  return 50; /* Snow Showers */
            }
         }
      case WX_S:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_R:
               case WX_RW:
                  return 18; /* Snow/Rain */
               case WX_IP:
                  return 21; /* Snow/Sleet */
               default:
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 47; /* Light Snow */
                     case INT_P:
                        return 49; /* Heavy Snow */
                     default:
                        return 48; /* Normal Snow */
                  }
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_R:
               case WX_RW:
                  return 22; /* Snow/Rain */
               case WX_IP:
                  return 25; /* Snow/Sleet */
               default:
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 51; /* Light Snow */
                     case INT_P:
                        return 53; /* Heavy Snow */
                     default:
                        return 52; /* Normal Snow */
                  }
            }
         }
      case WX_T:
         /* 
          * Check Severe storms.  If so, this is most important weather
          * type.
          */
         if (ugly->intens[0] == INT_P) {
            return 17;  /* Severe Thunderstorms */
         }
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_R:
                  return 5; /* Thunderstorms/Rain */
               case WX_RW:
                  return 7; /* Thunderstorms/Rain Showers */
               default:
                  return 8; /* Thunderstorms. */
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_R:
                  return 13; /* Thunderstorms/Rain */
               case WX_RW:
                  return 15; /* Thunderstorms/Rain Showers */
               default:
                  return 16; /* Thunderstorms. */
            }
         }
      case WX_F:
         return 54;     /* Fog */
      case WX_H:
         return 55;     /* Haze */
      case WX_K:
         return 56;     /* Smoke */
      case WX_BS:
         return 57;     /* Blowing Snow */
      case WX_BD:
         return 58;     /* Blowing Dust */
      case WX_FR:      /* Ignore Frost */
      case WX_A:       /* Ignore Hail */
      default:
         return 0;
   }
}

/*****************************************************************************
 * NDFD_WxTable2_StdInten() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   A helper routine to NDFD_WxTable2() to assist with adjusting the
 * intensity.  For the most part if intens is INT_D or INT_DD, we want to
 * subtract 1 from the base value.  If it is INT_P, we want to add 1,
 * otherwise just return base.
 *
 * ARGUMENTS
 *   base = The base encoded number to adjust. (Input)
 * intens = The intensity of the first weather key. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (the resulting encoded number.)
 *
 * HISTORY
 *   1/2004 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
static int NDFD_WxTable2_StdInten (int base, int intens)
{
   switch (intens) {
      case INT_D:
      case INT_DD:
         return base - 1;
      case INT_P:
         return base + 1;
      default:
         return base;
   }
}

/*****************************************************************************
 * NDFD_WxTable2() --
 *
 * Original: makeWxImageCodes() Marc Saccucci Jan 2004 (MDL)
 * Adapted to NDFD_WxTable() Arthur Taylor / MDL
 *
 * PURPOSE
 *   To use the same weather table scheme used by Marc Saccucci in
 * makeWxImageCodes() in the NDFD source tree.  The purpose of both
 * procedures is to simplify the weather string (aka ugly string) to a single
 * integral code number, which contains the most relevant weather.  The
 * intent is to create a simpler field which can more readily be viewed as
 * an image.
 *
 * ARGUMENTS
 * ugly = The ugly weather string to encode. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (the encoded number.)
 *
 * HISTORY
 *  11/2002 Marc Saccucci (MDL): Created matching algorithm in
 *              makeWxImageCodes().
 *   6/2003 MS: Altered matching combinations in makeWxImageCodes().
 *   1/2004 MS: Updated to include intensity considerations for all Precip
                types.
 *   1/2004 Arthur Taylor (MDL/RSIS): Created NDFD_WxTable2()
 *
 * NOTES
 *  1) The table used:
 *  new_code  Sample Value  Legend Value    Description
 *  --------  ------------ --------------   ------------
 *  0            -        -      No Predominant Weather
 *  1            L-      Rain    (LoProb L-)
 *  2            L       Rain    (LoProb L)
 *  3            L+      Rain    (LoProb L+)
 *  4            R-      Rain    (LoProb R-)
 *  5            R       Rain    (LoProb R)
 *  6            R+      Rain    (LoProb R+)
 *  7            R/T+    Severe  (LoProb R/T+)
 *  8            T/R+    Rain    (LoProb T/R+)
 *  9            T/R-    Rain    (LoProb T/R-)
 *  10           R/T     Rain    (LoProb R/T)
 *  11           RW-     Rain    (LoProb RW-)
 *  12           RW      Rain    (LoProb RW)
 *  13           RW+     Rain    (LoProb RW+)
 *  14           RW/T+   Severe  (LoProb RW/T+)
 *  15           RW/T    Rain    (LoProb RW/T)
 *  16           T/RW+   Rain    (LoProb T/RW+)
 *  17           T/RW-   Rain    (LoProb T/RW-)
 *  18           T       Rain    (LoProb T)
 *  19           T+      Severe  (LoProb T+)
 *  20           L-      Rain    (HiProb L-)
 *  21           L       Rain    (HiProb L)
 *  22           L+      Rain    (HiProb L+)
 *  23           R-      Rain    (HiProb R-)
 *  24           R       Rain    (HiProb R)
 *  25           R+      Rain    (HiProb R+)
 *  26           R/T+    Severe  (HiProb R/T+)
 *  27           R/T     Rain    (HiProb R/T)
 *  28           T/R+    Rain    (HiProb T/R+)
 *  29           T/R-    Rain    (HiProb T/R-)
 *  30           RW-     Rain    (HiProb RW-)
 *  31           RW      Rain    (HiProb RW)
 *  32           RW+     Rain    (HiProb RW+)
 *  33           RW/T    Rain    (HiProb RW/T)
 *  34           RW/T+   Severe  (HiProb RW/T+)
 *  35           T/RW+   Rain    (HiProb T/RW+)
 *  36           T/RW-   Rain    (HiProb T/RW-)
 *  37           T       Rain    (HiProb T)
 *  38           T+      Severe  (HiProb T+)
 *  39           R/S-    Mix     (LoProb R/S-)
 *  40           R/S     Mix     (LoProb R/S)
 *  41           R/S+    Mix     (LoProb R/S+)
 *  42           RW/SW-  Mix     (LoProb RW/SW-)
 *  43           RW/SW   Mix     (LoProb RW/SW)
 *  44           RW/SW+  Mix     (LoProb RW/SW+)
 *  45           R/IP-   Mix     (LoProb R/IP-)
 *  46           R/IP    Mix     (LoProb R/IP)
 *  47           R/IP+   Mix     (LoProb R/IP+)
 *  48           S/IP-   Mix     (LoProb S/IP-)
 *  49           S/IP    Mix     (LoProb S/IP)
 *  50           S/IP+   Mix     (LoProb S/IP+)
 *  51           R/S-    Mix     (HiProb R/S-)
 *  52           R/S     Mix     (HiProb R/S)
 *  53           R/S+    Mix     (HiProb R/S+)
 *  54           RW/SW-  Mix     (HiProb RW/SW-)
 *  55           RW/SW   Mix     (HiProb RW/SW)
 *  56           RW/SW+  Mix     (HiProb RW/SW+)
 *  57           R/IP-   Mix     (HiProb R/IP-)
 *  58           R/IP    Mix     (HiProb R/IP)
 *  59           R/IP+   Mix     (HiProb R/IP+)
 *  60           S/IP-   Mix     (HiProb S/IP-)
 *  61           S/IP    Mix     (HiProb S/IP)
 *  62           S/IP+   Mix     (HiProb S/IP+)
 *  63           IP-     Ice     (LoProb IP-)
 *  64           IP      Ice     (LoProb IP)
 *  65           IP+     Ice     (LoProb IP+)
 *  66           ZL-     Ice     (LoProb ZL-)
 *  67           ZL      Ice     (LoProb ZL)
 *  68           ZL+     Ice     (LoProb ZL+)
 *  69           R/ZL-   Ice     (LoProb R/ZL-)
 *  70           R/ZL    Ice     (LoProb R/ZL)
 *  71           R/ZL+   Ice     (LoProb R/ZL+)
 *  72           ZR-     Ice     (LoProb ZR-)
 *  73           ZR      Ice     (LoProb ZR)
 *  74           ZR+     Ice     (LoProb ZR+)
 *  75           R/ZR-   Ice     (LoProb R/ZR-)
 *  76           R/ZR    Ice     (LoProb R/ZR)
 *  77           R/ZR+   Ice     (LoProb R/ZR+)
 *  78           IP/ZR-  Ice     (LoProb IP/ZR-)
 *  79           IP/ZR   Ice     (LoProb IP/ZR)
 *  80           IP/ZR+  Ice     (LoProb IP/ZR+)
 *  81           IP-     Ice     (HiProb IP-)
 *  82           IP      Ice     (HiProb IP)
 *  83           IP+     Ice     (HiProb IP+)
 *  84           ZL-     Ice     (HiProb ZL-)
 *  85           ZL      Ice     (HiProb ZL)
 *  86           ZL+     Ice     (HiProb ZL+)
 *  87           R/ZL-   Ice     (HiProb R/ZL-)
 *  88           R/ZL    Ice     (HiProb R/ZL)
 *  89           R/ZL+   Ice     (HiProb R/ZL+)
 *  90           ZR-     Ice     (HiProb ZR-)
 *  91           ZR      Ice     (HiProb ZR)
 *  92           ZR+     Ice     (HiProb ZR+)
 *  93           R/ZR-   Ice     (HiProb R/ZR-)
 *  94           R/ZR    Ice     (HiProb R/ZR)
 *  95           R/ZR+   Ice     (HiProb R/ZR+)
 *  96           IP/ZR-  Ice     (HiProb IP/ZR-)
 *  97           IP/ZR   Ice     (HiProb IP/ZR)
 *  98           IP/ZR+  Ice     (HiProb IP/ZR+)
 *  99           L/ZL-   Ice     (LoProb L/ZL-)
 *  100          L/ZL    Ice     (LoProb L/ZL)
 *  101          L/ZL+   Ice     (LoProb L/ZL+)
 *  102          L/ZL-   Ice     (HiProb L/ZL-)
 *  103          L/ZL    Ice     (HiProb L/ZL)
 *  104          L/ZL+   Ice     (HiProb L/ZL+)
 *  105          SW-     Snow    (LoProb SW-)
 *  106          SW      Snow    (LoProb SW)
 *  107          SW+     Snow    (LoProb SW+)
 *  108          S-      Snow    (LoProb S-)
 *  109          S       Snow    (LoProb S)
 *  110          S+      Snow    (LoProb S+)
 *  111          SW-     Snow    (HiProb SW-)
 *  112          SW      Snow    (HiProb SW)
 *  113          SW+     Snow    (HiProb SW+)
 *  114          S-      Snow    (HiProb S-)
 *  115          S       Snow    (HiProb S)
 *  116          S+      Snow    (HiProb S+)
 *  117          F       Fog     (Fog)
 *  118          F+      Fog     (Dense Fog)
 *  119          H       Haze
 *  120          K       Smoke
 *  121          BS      Blowing (Blowing Snow)
 *  122          BD      Blowing (Blowing Dust)
 *****************************************************************************
 */
static int NDFD_WxTable2 (UglyStringType * ugly)
{
   switch (ugly->wx[0]) {
      case WX_NOWX:
         return 0;
      case WX_R:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_S:
                  return (NDFD_WxTable2_StdInten (40, ugly->intens[0]));
               case WX_ZR: /* Rain/Freezing Rain */
                  return (NDFD_WxTable2_StdInten (76, ugly->intens[0]));
               case WX_IP: /* Rain/Sleet */
                  return (NDFD_WxTable2_StdInten (46, ugly->intens[0]));
               case WX_ZL: /* Rain/Freezing Drizzle */
                  return (NDFD_WxTable2_StdInten (70, ugly->intens[0]));
               case WX_SW: /* Rain/Snow Showers */
                  return (NDFD_WxTable2_StdInten (40, ugly->intens[0]));
               case WX_T: /* Rain/Thunderstorms */
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 9;
                     case INT_P:
                        return 8;
                     default:
                        return 27;
                  }
               default:
                  return (NDFD_WxTable2_StdInten (5, ugly->intens[0]));
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_S: /* Rain/Snow */
                  return (NDFD_WxTable2_StdInten (52, ugly->intens[0]));
               case WX_ZR: /* Rain/Freezing Rain */
                  return (NDFD_WxTable2_StdInten (94, ugly->intens[0]));
               case WX_IP: /* Rain/Sleet */
                  return (NDFD_WxTable2_StdInten (58, ugly->intens[0]));
               case WX_ZL: /* Rain/Freezing Drizzle */
                  return (NDFD_WxTable2_StdInten (88, ugly->intens[0]));
               case WX_SW: /* Rain/Snow Showers */
                  return (NDFD_WxTable2_StdInten (52, ugly->intens[0]));
               case WX_T: /* Rain/Thunderstorms */
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 29;
                     case INT_P:
                        return 28;
                     default:
                        return 27;
                  }
               default:
                  return (NDFD_WxTable2_StdInten (24, ugly->intens[0]));
            }
         }
      case WX_RW:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_T: /* Rain Showers/Thunderstorms */
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 17;
                     case INT_P:
                        return 16;
                     default:
                        return 15;
                  }
               case WX_SW: /* Rain Showers/Snow Showers */
               case WX_S: /* Rain Showers/Snow */
                  return (NDFD_WxTable2_StdInten (43, ugly->intens[0]));
               default:
                  return (NDFD_WxTable2_StdInten (12, ugly->intens[0]));
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_T: /* Rain Showers/Thunderstorms */
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 36;
                     case INT_P:
                        return 35;
                     default:
                        return 33;
                  }
               case WX_SW: /* Rain Showers/Snow Showers */
               case WX_S: /* Rain Showers/Snow */
                  return (NDFD_WxTable2_StdInten (55, ugly->intens[0]));
               default:
                  return (NDFD_WxTable2_StdInten (31, ugly->intens[0]));
            }
         }
      case WX_L:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_ZL: /* Drizzle/Freezing Drizzle */
                  return (NDFD_WxTable2_StdInten (100, ugly->intens[0]));
               case WX_F:
               default: /* Drizzle */
                  return (NDFD_WxTable2_StdInten (2, ugly->intens[0]));
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_ZL: /* Drizzle/Freezing Drizzle */
                  return (NDFD_WxTable2_StdInten (103, ugly->intens[0]));
               case WX_F:
               default: /* Drizzle */
                  return (NDFD_WxTable2_StdInten (21, ugly->intens[0]));
            }
         }
      case WX_ZL:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_R: /* Freezing Drizzle/Rain */
                  return (NDFD_WxTable2_StdInten (70, ugly->intens[0]));
               case WX_L: /* Freezing Drizzle/Drizzle */
                  return (NDFD_WxTable2_StdInten (100, ugly->intens[0]));
               default:
                  return (NDFD_WxTable2_StdInten (67, ugly->intens[0]));
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_R: /* Freezing Drizzle/Rain */
                  return (NDFD_WxTable2_StdInten (88, ugly->intens[0]));
               case WX_L: /* Freezing Drizzle/Drizzle */
                  return (NDFD_WxTable2_StdInten (103, ugly->intens[0]));
               default: /* Freezing Drizzle */
                  return (NDFD_WxTable2_StdInten (85, ugly->intens[0]));
            }
         }
      case WX_ZR:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_R: /* Freezing Rain/Rain */
                  return (NDFD_WxTable2_StdInten (76, ugly->intens[0]));
               case WX_IP: /* Freezing Rain/Sleet */
                  return (NDFD_WxTable2_StdInten (79, ugly->intens[0]));
               default:
                  return (NDFD_WxTable2_StdInten (73, ugly->intens[0]));
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_R: /* Freezing Rain/Rain */
                  return (NDFD_WxTable2_StdInten (94, ugly->intens[0]));
               case WX_IP: /* Freezing Rain/Sleet */
                  return (NDFD_WxTable2_StdInten (97, ugly->intens[0]));
               default:
                  return (NDFD_WxTable2_StdInten (91, ugly->intens[0]));
            }
         }
      case WX_IP:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_R: /* Sleet/Rain */
                  return (NDFD_WxTable2_StdInten (46, ugly->intens[0]));
               case WX_S: /* Sleet/Snow */
                  return (NDFD_WxTable2_StdInten (49, ugly->intens[0]));
               case WX_ZR: /* Sleet/Freezing Rain */
                  return (NDFD_WxTable2_StdInten (79, ugly->intens[0]));
               default:
                  return (NDFD_WxTable2_StdInten (64, ugly->intens[0]));
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_R: /* Sleet/Rain */
                  return (NDFD_WxTable2_StdInten (58, ugly->intens[0]));
               case WX_S: /* Sleet/Snow */
                  return (NDFD_WxTable2_StdInten (61, ugly->intens[0]));
               case WX_ZR: /* Sleet/Freezing Rain */
                  return (NDFD_WxTable2_StdInten (97, ugly->intens[0]));
               default:
                  return (NDFD_WxTable2_StdInten (82, ugly->intens[0]));
            }
         }
      case WX_SW:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_R: /* Snow Showers/Rain */
               case WX_RW: /* Snow Showers/Rain Showers */
                  return (NDFD_WxTable2_StdInten (43, ugly->intens[0]));
               default: /* Snow Showers */
                  return (NDFD_WxTable2_StdInten (106, ugly->intens[0]));
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_R: /* Snow Showers/Rain */
               case WX_RW: /* Snow Showers/Rain Showers */
                  return (NDFD_WxTable2_StdInten (55, ugly->intens[0]));
               default: /* Snow Showers */
                  return (NDFD_WxTable2_StdInten (112, ugly->intens[0]));
            }
         }
      case WX_S:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_R: /* Snow/Rain */
               case WX_RW: /* Snow/Rain Showers */
                  return (NDFD_WxTable2_StdInten (40, ugly->intens[0]));
               case WX_IP: /* Snow/Sleet */
                  return (NDFD_WxTable2_StdInten (49, ugly->intens[0]));
               default:
                  return (NDFD_WxTable2_StdInten (109, ugly->intens[0]));
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_R: /* Snow/Rain */
               case WX_RW:
                  return (NDFD_WxTable2_StdInten (52, ugly->intens[0]));
               case WX_IP: /* Snow/Sleet */
                  return (NDFD_WxTable2_StdInten (61, ugly->intens[0]));
               default:
                  return (NDFD_WxTable2_StdInten (115, ugly->intens[0]));
            }
         }
      case WX_T:
         /* 
          * Check Severe storms.  If so, this is most important weather
          * type.
          */
/*
         if (ugly->intens[0] == INT_P) {
            return 17;  * Severe Thunderstorms *
         }
*/
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_RW: /* Thunderstorms/Rain Showers */
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 17;
                     case INT_P:
                        return 14;
                     default:
                        return 15;
                  }
               case WX_R: /* Thunderstorms/Rain */
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 9;
                     case INT_P:
                        return 7;
                     default:
                        return 10;
                  }
               default: /* Thunderstorms. */
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 18;
                     case INT_P:
                        return 19;
                     default:
                        return 18;
                  }
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_RW: /* Thunderstorms/Rain Showers */
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 36;
                     case INT_P:
                        return 34;
                     default:
                        return 33; /* corrected from 37 */
                  }
               case WX_R: /* Thunderstorms/Rain */
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 29;
                     case INT_P:
                        return 26;
                     default:
                        return 27;
                  }
               default: /* Thunderstorms. */
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 37;
                     case INT_P:
                        return 38;
                     default:
                        return 37;
                  }
            }
         }
      case WX_A:       /* Ignore Hail */
         return 0;
      case WX_F:       /* Fog */
         switch (ugly->intens[0]) {
            case INT_P:
               return 118;
            default:
               return 117;
         }
      case WX_H:       /* Haze */
         return 119;
      case WX_K:       /* Smoke */
         return 120;
      case WX_FR:      /* Ignore Frost */
         return 0;
      case WX_BS:      /* Blowing Snow */
         return 121;
      case WX_BD:      /* Blowing Dust */
         return 122;
      default:
         return 0;
   }
}

/*****************************************************************************
 * NDFD_WxTable3() --
 *
 * Original: makeWxImageCodes() Marc Saccucci Feb 2004 (MDL)
 * Adapted to NDFD_WxTable3() Arthur Taylor / MDL
 *
 * PURPOSE
 *   To use the same weather table scheme used by Marc Saccucci in
 * makeWxImageCodes() in the NDFD source tree.  The purpose of both
 * procedures is to simplify the weather string (aka ugly string) to a single
 * integral code number, which contains the most relevant weather.  The
 * intent is to create a simpler field which can more readily be viewed as
 * an image.
 *
 * ARGUMENTS
 * ugly = The ugly weather string to encode. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (the encoded number.)
 *
 * HISTORY
 *  11/2002 Marc Saccucci (MDL): Created matching algorithm in
 *              makeWxImageCodes().
 *   6/2003 MS: Altered matching combinations in makeWxImageCodes().
 *   1/2004 MS: Updated to include intensity considerations for all Precip
 *              types.
 *   2/2004 MS: Updated to include: 123..129 ZF, IF, IC, BN, ZY, VA, WP
 *   2/2004 Arthur Taylor (MDL/RSIS): Created NDFD_WxTable3()
 *
 * NOTES
 *  1) The table used:
 *  new_code  Sample Value  Legend Value    Description
 *  --------  ------------ --------------   ------------
 *  0            -        -      No Predominant Weather
 *  1            L-      Rain    (LoProb L-)
 *  2            L       Rain    (LoProb L)
 *  3            L+      Rain    (LoProb L+)
 *  4            R-      Rain    (LoProb R-)
 *  5            R       Rain    (LoProb R)
 *  6            R+      Rain    (LoProb R+)
 *  7            R/T+    Severe  (LoProb R/T+)
 *  8            T/R+    Rain    (LoProb T/R+)
 *  9            T/R-    Rain    (LoProb T/R-)
 *  10           R/T     Rain    (LoProb R/T)
 *  11           RW-     Rain    (LoProb RW-)
 *  12           RW      Rain    (LoProb RW)
 *  13           RW+     Rain    (LoProb RW+)
 *  14           RW/T+   Severe  (LoProb RW/T+)
 *  15           RW/T    Rain    (LoProb RW/T)
 *  16           T/RW+   Rain    (LoProb T/RW+)
 *  17           T/RW-   Rain    (LoProb T/RW-)
 *  18           T       Rain    (LoProb T)
 *  19           T+      Severe  (LoProb T+)
 *  20           L-      Rain    (HiProb L-)
 *  21           L       Rain    (HiProb L)
 *  22           L+      Rain    (HiProb L+)
 *  23           R-      Rain    (HiProb R-)
 *  24           R       Rain    (HiProb R)
 *  25           R+      Rain    (HiProb R+)
 *  26           R/T+    Severe  (HiProb R/T+)
 *  27           R/T     Rain    (HiProb R/T)
 *  28           T/R+    Rain    (HiProb T/R+)
 *  29           T/R-    Rain    (HiProb T/R-)
 *  30           RW-     Rain    (HiProb RW-)
 *  31           RW      Rain    (HiProb RW)
 *  32           RW+     Rain    (HiProb RW+)
 *  33           RW/T    Rain    (HiProb RW/T)
 *  34           RW/T+   Severe  (HiProb RW/T+)
 *  35           T/RW+   Rain    (HiProb T/RW+)
 *  36           T/RW-   Rain    (HiProb T/RW-)
 *  37           T       Rain    (HiProb T)
 *  38           T+      Severe  (HiProb T+)
 *  39           R/S-    Mix     (LoProb R/S-)
 *  40           R/S     Mix     (LoProb R/S)
 *  41           R/S+    Mix     (LoProb R/S+)
 *  42           RW/SW-  Mix     (LoProb RW/SW-)
 *  43           RW/SW   Mix     (LoProb RW/SW)
 *  44           RW/SW+  Mix     (LoProb RW/SW+)
 *  45           R/IP-   Mix     (LoProb R/IP-)
 *  46           R/IP    Mix     (LoProb R/IP)
 *  47           R/IP+   Mix     (LoProb R/IP+)
 *  48           S/IP-   Mix     (LoProb S/IP-)
 *  49           S/IP    Mix     (LoProb S/IP)
 *  50           S/IP+   Mix     (LoProb S/IP+)
 *  51           R/S-    Mix     (HiProb R/S-)
 *  52           R/S     Mix     (HiProb R/S)
 *  53           R/S+    Mix     (HiProb R/S+)
 *  54           RW/SW-  Mix     (HiProb RW/SW-)
 *  55           RW/SW   Mix     (HiProb RW/SW)
 *  56           RW/SW+  Mix     (HiProb RW/SW+)
 *  57           R/IP-   Mix     (HiProb R/IP-)
 *  58           R/IP    Mix     (HiProb R/IP)
 *  59           R/IP+   Mix     (HiProb R/IP+)
 *  60           S/IP-   Mix     (HiProb S/IP-)
 *  61           S/IP    Mix     (HiProb S/IP)
 *  62           S/IP+   Mix     (HiProb S/IP+)
 *  63           IP-     Ice     (LoProb IP-)
 *  64           IP      Ice     (LoProb IP)
 *  65           IP+     Ice     (LoProb IP+)
 *  66           ZL-     Ice     (LoProb ZL-)
 *  67           ZL      Ice     (LoProb ZL)
 *  68           ZL+     Ice     (LoProb ZL+)
 *  69           R/ZL-   Ice     (LoProb R/ZL-)
 *  70           R/ZL    Ice     (LoProb R/ZL)
 *  71           R/ZL+   Ice     (LoProb R/ZL+)
 *  72           ZR-     Ice     (LoProb ZR-)
 *  73           ZR      Ice     (LoProb ZR)
 *  74           ZR+     Ice     (LoProb ZR+)
 *  75           R/ZR-   Ice     (LoProb R/ZR-)
 *  76           R/ZR    Ice     (LoProb R/ZR)
 *  77           R/ZR+   Ice     (LoProb R/ZR+)
 *  78           IP/ZR-  Ice     (LoProb IP/ZR-)
 *  79           IP/ZR   Ice     (LoProb IP/ZR)
 *  80           IP/ZR+  Ice     (LoProb IP/ZR+)
 *  81           IP-     Ice     (HiProb IP-)
 *  82           IP      Ice     (HiProb IP)
 *  83           IP+     Ice     (HiProb IP+)
 *  84           ZL-     Ice     (HiProb ZL-)
 *  85           ZL      Ice     (HiProb ZL)
 *  86           ZL+     Ice     (HiProb ZL+)
 *  87           R/ZL-   Ice     (HiProb R/ZL-)
 *  88           R/ZL    Ice     (HiProb R/ZL)
 *  89           R/ZL+   Ice     (HiProb R/ZL+)
 *  90           ZR-     Ice     (HiProb ZR-)
 *  91           ZR      Ice     (HiProb ZR)
 *  92           ZR+     Ice     (HiProb ZR+)
 *  93           R/ZR-   Ice     (HiProb R/ZR-)
 *  94           R/ZR    Ice     (HiProb R/ZR)
 *  95           R/ZR+   Ice     (HiProb R/ZR+)
 *  96           IP/ZR-  Ice     (HiProb IP/ZR-)
 *  97           IP/ZR   Ice     (HiProb IP/ZR)
 *  98           IP/ZR+  Ice     (HiProb IP/ZR+)
 *  99           L/ZL-   Ice     (LoProb L/ZL-)
 *  100          L/ZL    Ice     (LoProb L/ZL)
 *  101          L/ZL+   Ice     (LoProb L/ZL+)
 *  102          L/ZL-   Ice     (HiProb L/ZL-)
 *  103          L/ZL    Ice     (HiProb L/ZL)
 *  104          L/ZL+   Ice     (HiProb L/ZL+)
 *  105          SW-     Snow    (LoProb SW-)
 *  106          SW      Snow    (LoProb SW)
 *  107          SW+     Snow    (LoProb SW+)
 *  108          S-      Snow    (LoProb S-)
 *  109          S       Snow    (LoProb S)
 *  110          S+      Snow    (LoProb S+)
 *  111          SW-     Snow    (HiProb SW-)
 *  112          SW      Snow    (HiProb SW)
 *  113          SW+     Snow    (HiProb SW+)
 *  114          S-      Snow    (HiProb S-)
 *  115          S       Snow    (HiProb S)
 *  116          S+      Snow    (HiProb S+)
 *  117          F       Fog     (Fog)
 *  118          F+      Fog     (Dense Fog)
 *  119          H       Haze
 *  120          K       Smoke
 *  121          BS      Blowing (Blowing Snow)
 *  122          BD      Blowing (Blowing Dust)
 *  123          ZF      Fog     (Freezing Fog)
 *  124          IF      Fog     (Ice Fog)
 *  125          IC      Ice     (Ice Crystals)
 *  126          BN      Blowing (Blowing Sand)
 *  127          ZY      Blowing (Freezing Spray)
 *  128          VA      Smoke   (Volcanic Ash)
 *  129          WP      Severe  (Water Spouts)
 *****************************************************************************
 */
static int NDFD_WxTable3 (UglyStringType * ugly)
{
   switch (ugly->wx[0]) {
      case WX_NOWX:
         return 0;
      case WX_R:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_S:
                  return (NDFD_WxTable2_StdInten (40, ugly->intens[0]));
               case WX_ZR: /* Rain/Freezing Rain */
                  return (NDFD_WxTable2_StdInten (76, ugly->intens[0]));
               case WX_IP: /* Rain/Sleet */
                  return (NDFD_WxTable2_StdInten (46, ugly->intens[0]));
               case WX_ZL: /* Rain/Freezing Drizzle */
                  return (NDFD_WxTable2_StdInten (70, ugly->intens[0]));
               case WX_SW: /* Rain/Snow Showers */
                  return (NDFD_WxTable2_StdInten (40, ugly->intens[0]));
               case WX_T: /* Rain/Thunderstorms */
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 9;
                     case INT_P:
                        return 8;
                     default:
                        return 27;
                  }
               default:
                  return (NDFD_WxTable2_StdInten (5, ugly->intens[0]));
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_S: /* Rain/Snow */
                  return (NDFD_WxTable2_StdInten (52, ugly->intens[0]));
               case WX_ZR: /* Rain/Freezing Rain */
                  return (NDFD_WxTable2_StdInten (94, ugly->intens[0]));
               case WX_IP: /* Rain/Sleet */
                  return (NDFD_WxTable2_StdInten (58, ugly->intens[0]));
               case WX_ZL: /* Rain/Freezing Drizzle */
                  return (NDFD_WxTable2_StdInten (88, ugly->intens[0]));
               case WX_SW: /* Rain/Snow Showers */
                  return (NDFD_WxTable2_StdInten (52, ugly->intens[0]));
               case WX_T: /* Rain/Thunderstorms */
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 29;
                     case INT_P:
                        return 28;
                     default:
                        return 27;
                  }
               default:
                  return (NDFD_WxTable2_StdInten (24, ugly->intens[0]));
            }
         }
      case WX_RW:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_T: /* Rain Showers/Thunderstorms */
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 17;
                     case INT_P:
                        return 16;
                     default:
                        return 15;
                  }
               case WX_SW: /* Rain Showers/Snow Showers */
               case WX_S: /* Rain Showers/Snow */
                  return (NDFD_WxTable2_StdInten (43, ugly->intens[0]));
               default:
                  return (NDFD_WxTable2_StdInten (12, ugly->intens[0]));
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_T: /* Rain Showers/Thunderstorms */
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 36;
                     case INT_P:
                        return 35;
                     default:
                        return 33;
                  }
               case WX_SW: /* Rain Showers/Snow Showers */
               case WX_S: /* Rain Showers/Snow */
                  return (NDFD_WxTable2_StdInten (55, ugly->intens[0]));
               default:
                  return (NDFD_WxTable2_StdInten (31, ugly->intens[0]));
            }
         }
      case WX_L:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_ZL: /* Drizzle/Freezing Drizzle */
                  return (NDFD_WxTable2_StdInten (100, ugly->intens[0]));
               case WX_F:
               default: /* Drizzle */
                  return (NDFD_WxTable2_StdInten (2, ugly->intens[0]));
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_ZL: /* Drizzle/Freezing Drizzle */
                  return (NDFD_WxTable2_StdInten (103, ugly->intens[0]));
               case WX_F:
               default: /* Drizzle */
                  return (NDFD_WxTable2_StdInten (21, ugly->intens[0]));
            }
         }
      case WX_ZL:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_R: /* Freezing Drizzle/Rain */
                  return (NDFD_WxTable2_StdInten (70, ugly->intens[0]));
               case WX_L: /* Freezing Drizzle/Drizzle */
                  return (NDFD_WxTable2_StdInten (100, ugly->intens[0]));
               default:
                  return (NDFD_WxTable2_StdInten (67, ugly->intens[0]));
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_R: /* Freezing Drizzle/Rain */
                  return (NDFD_WxTable2_StdInten (88, ugly->intens[0]));
               case WX_L: /* Freezing Drizzle/Drizzle */
                  return (NDFD_WxTable2_StdInten (103, ugly->intens[0]));
               default: /* Freezing Drizzle */
                  return (NDFD_WxTable2_StdInten (85, ugly->intens[0]));
            }
         }
      case WX_ZR:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_R: /* Freezing Rain/Rain */
                  return (NDFD_WxTable2_StdInten (76, ugly->intens[0]));
               case WX_IP: /* Freezing Rain/Sleet */
                  return (NDFD_WxTable2_StdInten (79, ugly->intens[0]));
               default:
                  return (NDFD_WxTable2_StdInten (73, ugly->intens[0]));
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_R: /* Freezing Rain/Rain */
                  return (NDFD_WxTable2_StdInten (94, ugly->intens[0]));
               case WX_IP: /* Freezing Rain/Sleet */
                  return (NDFD_WxTable2_StdInten (97, ugly->intens[0]));
               default:
                  return (NDFD_WxTable2_StdInten (91, ugly->intens[0]));
            }
         }
      case WX_IP:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_R: /* Sleet/Rain */
                  return (NDFD_WxTable2_StdInten (46, ugly->intens[0]));
               case WX_S: /* Sleet/Snow */
                  return (NDFD_WxTable2_StdInten (49, ugly->intens[0]));
               case WX_ZR: /* Sleet/Freezing Rain */
                  return (NDFD_WxTable2_StdInten (79, ugly->intens[0]));
               default:
                  return (NDFD_WxTable2_StdInten (64, ugly->intens[0]));
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_R: /* Sleet/Rain */
                  return (NDFD_WxTable2_StdInten (58, ugly->intens[0]));
               case WX_S: /* Sleet/Snow */
                  return (NDFD_WxTable2_StdInten (61, ugly->intens[0]));
               case WX_ZR: /* Sleet/Freezing Rain */
                  return (NDFD_WxTable2_StdInten (97, ugly->intens[0]));
               default:
                  return (NDFD_WxTable2_StdInten (82, ugly->intens[0]));
            }
         }
      case WX_SW:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_R: /* Snow Showers/Rain */
               case WX_RW: /* Snow Showers/Rain Showers */
                  return (NDFD_WxTable2_StdInten (43, ugly->intens[0]));
               default: /* Snow Showers */
                  return (NDFD_WxTable2_StdInten (106, ugly->intens[0]));
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_R: /* Snow Showers/Rain */
               case WX_RW: /* Snow Showers/Rain Showers */
                  return (NDFD_WxTable2_StdInten (55, ugly->intens[0]));
               default: /* Snow Showers */
                  return (NDFD_WxTable2_StdInten (112, ugly->intens[0]));
            }
         }
      case WX_S:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_R: /* Snow/Rain */
               case WX_RW: /* Snow/Rain Showers */
                  return (NDFD_WxTable2_StdInten (40, ugly->intens[0]));
               case WX_IP: /* Snow/Sleet */
                  return (NDFD_WxTable2_StdInten (49, ugly->intens[0]));
               default:
                  return (NDFD_WxTable2_StdInten (109, ugly->intens[0]));
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_R: /* Snow/Rain */
               case WX_RW:
                  return (NDFD_WxTable2_StdInten (52, ugly->intens[0]));
               case WX_IP: /* Snow/Sleet */
                  return (NDFD_WxTable2_StdInten (61, ugly->intens[0]));
               default:
                  return (NDFD_WxTable2_StdInten (115, ugly->intens[0]));
            }
         }
      case WX_T:
         if ((ugly->cover[0] == COV_SCT) || (ugly->cover[0] == COV_SCHC) ||
             (ugly->cover[0] == COV_PATCHY) || (ugly->cover[0] == COV_ISO) ||
             (ugly->cover[0] == COV_CHC)) {
            switch (ugly->wx[1]) {
               case WX_RW: /* Thunderstorms/Rain Showers */
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 17;
                     case INT_P:
                        return 14;
                     default:
                        return 15;
                  }
               case WX_R: /* Thunderstorms/Rain */
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 9;
                     case INT_P:
                        return 7;
                     default:
                        return 10;
                  }
               default: /* Thunderstorms. */
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 18;
                     case INT_P:
                        return 19;
                     default:
                        return 18;
                  }
            }
         } else {
            switch (ugly->wx[1]) {
               case WX_RW: /* Thunderstorms/Rain Showers */
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 36;
                     case INT_P:
                        return 34;
                     default:
                        return 33;
                  }
               case WX_R: /* Thunderstorms/Rain */
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 29;
                     case INT_P:
                        return 26;
                     default:
                        return 27;
                  }
               default: /* Thunderstorms. */
                  switch (ugly->intens[0]) {
                     case INT_D:
                     case INT_DD:
                        return 37;
                     case INT_P:
                        return 38;
                     default:
                        return 37;
                  }
            }
         }
      case WX_A:       /* Ignore Hail */
         return 0;
      case WX_F:       /* Fog */
         switch (ugly->intens[0]) {
            case INT_P:
               return 118;
            default:
               return 117;
         }
      case WX_H:       /* Haze */
         return 119;
      case WX_K:       /* Smoke */
         return 120;
      case WX_FR:      /* Ignore Frost */
         return 0;
      case WX_BS:      /* Blowing Snow */
         return 121;
      case WX_BD:      /* Blowing Dust */
         return 122;
      case WX_ZF:      /* Freezing Fog */
         return 123;
      case WX_IF:      /* Ice Fog */
         return 124;
      case WX_IC:      /* Ice Crystals */
         return 125;
      case WX_BN:      /* Blowing Sand */
         return 126;
      case WX_ZY:      /* Freezing Spray */
         return 127;
      case WX_VA:      /* Volcanic Ash */
         return 128;
      case WX_WP:      /* Water Spouts */
         return 129;
      default:
         return 0;
   }
}

/*****************************************************************************
 * NDFD_Wx2Code4() --
 *
 * Original: wx2code() Mark Armstrong Nov 2004 (MDL)
 * Adapted to NDFD_Wx2Code() Arthur Taylor (MDL)
 *
 * PURPOSE
 *   Converts from a Weather Type to the code value used in makeWxImageCodes.
 *
 * ARGUMENTS
 * wxtype = The weather type to encode. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (the encoded number.)
 *
 * HISTORY
 *  11/2004 Mark Armstrong (MDL): Created matching algorithm "wx2code"
 *  11/2004 Arthur Taylor (MDL): Modified to assist with NDFD_WxTable4()
 *
 * NOTES
 *****************************************************************************
 */
static int NDFD_Wx2Code4 (int wxtype)
{
   switch (wxtype) {
      case WX_R:
         return 0;
      case WX_RW:
         return 10;
      case WX_L:
         return 20;
      case WX_ZL:
         return 30;
      case WX_ZR:
         return 40;
      case WX_IP:
         return 50;
      case WX_SW:
         return 60;
      case WX_S:
         return 70;
      case WX_T:
         return 80;
      case WX_F:
         return 90;
      default:
         return 0;
   }
}

/*****************************************************************************
 * NDFD_CodeIntens4() --
 *
 * Original: code_intensity() Mark Armstrong Nov 2004 (MDL)
 * Adapted to NDFD_CodeIntens4() Arthur Taylor (MDL)
 *
 * PURPOSE
 *   Converts from two types of weather intensities to the code value used in
 * makeWxImageCodes when dealing with intensities.
 *
 * ARGUMENTS
 * inten1 = The first intensity to encode. (Input)
 * inten2 = The second intensity to encode. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (the encoded number.)
 *
 * HISTORY
 *  11/2004 Mark Armstrong (MDL): Created matching algorithm "code_intensity"
 *  11/2004 Arthur Taylor (MDL): Modified to assist with NDFD_WxTable4()
 *
 * NOTES
 *****************************************************************************
 */
static int NDFD_CodeIntens4 (int inten1, int inten2)
{
   switch (inten2) {
      case INT_NOINT:
      case INT_UNKNOWN:
      case INT_M:
         if ((inten1 == INT_NOINT) || (inten1 == INT_UNKNOWN) ||
             (inten1 == INT_M)) {
            return 0;
         } else if ((inten1 == INT_D) || (inten1 == INT_DD)) {
            return 1;
         }
         /* Default case */
         /* else if (inten1 == INT_P) */
         return 2;
      case INT_D:
      case INT_DD:
         if ((inten1 == INT_NOINT) || (inten1 == INT_UNKNOWN) ||
             (inten1 == INT_M)) {
            return 3;
         } else if ((inten1 == INT_D) || (inten1 == INT_DD)) {
            return 4;
         }
         /* Default case */
         /* else if (inten1 == INT_P) */
         return 5;
      case INT_P:
      default:
         if ((inten1 == INT_NOINT) || (inten1 == INT_UNKNOWN) ||
             (inten1 == INT_M)) {
            return 6;
         } else if ((inten1 == INT_D) || (inten1 == INT_DD)) {
            return 7;
         }
         /* Default case */
         /* else if (inten1 == INT_P) */
         return 8;
   }
}

/*****************************************************************************
 * NDFD_WxTable4() --
 *
 * Original: makeWxImageCodes() Mark Armstrong Nov 2004 (MDL)
 * Adapted to NDFD_WxTable4() Arthur Taylor (MDL)
 *
 * PURPOSE
 *   To use the same weather table scheme used by Mark Armstrong in
 * makeWxImageCodes().  The purpose of both procedures is to simplify the
 * weather string (aka ugly string) to a single integral code number, which
 * contains the most relevant weather.  The intent is to create a simpler
 * field which can more readily be viewed as an image.
 *
 * ARGUMENTS
 * ugly = The ugly weather string to encode. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int (the encoded number.)
 *
 * HISTORY
 *  11/2004 Mark Armstrong (MDL): Created "makeWxImageCodes"
 *  11/2004 Arthur Taylor (MDL): Created NDFD_WxTable4
 *
 * NOTES
 *  1) The table used... In the past I have included the table as part of the
 * documentation here, but since the table is now > 1000 lines long, I think
 * it best to look in "/degrib/data/imageGen/colortable/Wx_200411.colortable"
 *****************************************************************************
 */
static int NDFD_WxTable4 (UglyStringType * ugly)
{
   int code = 0;
   int numValid = ugly->numValid;
   int cover1 = ugly->cover[1];
   int intens1 = ugly->intens[1];

   if (numValid > 1) {
      if ((ugly->wx[1] != WX_R) && (ugly->wx[1] != WX_S) &&
          (ugly->wx[1] != WX_RW) && (ugly->wx[1] != WX_SW) &&
          (ugly->wx[1] != WX_T) && (ugly->wx[1] != WX_ZR) &&
          (ugly->wx[1] != WX_IP) && (ugly->wx[1] != WX_ZL) &&
          (ugly->wx[1] != WX_L) && (ugly->wx[1] != WX_F)) {
         numValid = 1;
         cover1 = COV_UNKNOWN;
         intens1 = INT_UNKNOWN;
      }
   }

   switch (ugly->wx[0]) {
      case WX_NOWX:    /* NoWx */
      case WX_A:       /* Hail */
      case WX_FR:      /* Frost */
         code = 0;
         break;
      case WX_R:       /* Rain */
         code = 1;
         if (numValid > 1) {
            code = 100 + NDFD_Wx2Code4 (ugly->wx[1]);
         }
         break;
      case WX_RW:      /* Rain Showers */
         code = 4;
         if (numValid > 1) {
            code = 200 + NDFD_Wx2Code4 (ugly->wx[1]);
         }
         break;
      case WX_L:       /* Drizzle */
         code = 7;
         if (numValid > 1) {
            code = 300 + NDFD_Wx2Code4 (ugly->wx[1]);
         }
         break;
      case WX_ZL:      /* Freezing Drizzle */
         code = 10;
         if (numValid > 1) {
            code = 400 + NDFD_Wx2Code4 (ugly->wx[1]);
         }
         break;
      case WX_ZR:      /* Freezing Rain */
         code = 13;
         if (numValid > 1) {
            code = 500 + NDFD_Wx2Code4 (ugly->wx[1]);
         }
         break;
      case WX_IP:      /* Sleet */
         code = 16;
         if (numValid > 1) {
            code = 600 + NDFD_Wx2Code4 (ugly->wx[1]);
         }
         break;
      case WX_SW:      /* Snow Showers */
         code = 19;
         if (numValid > 1) {
            code = 700 + NDFD_Wx2Code4 (ugly->wx[1]);
         }
         break;
      case WX_S:       /* Snow */
         code = 22;
         if (numValid > 1) {
            code = 800 + NDFD_Wx2Code4 (ugly->wx[1]);
         }
         break;
      case WX_T:       /* Thunderstorms */
         code = 25;
         if (numValid > 1) {
            code = 900 + NDFD_Wx2Code4 (ugly->wx[1]);
         }
         break;
      case WX_F:       /* Fog */
         code = 28;
         if (numValid > 1) {
            code = 1000 + NDFD_Wx2Code4 (ugly->wx[1]);
         }
         break;
      case WX_K:       /* Smoke */
         code = 31;
         break;
      case WX_BS:      /* Blowing Snow */
         code = 32;
         break;
      case WX_BD:      /* Blowing Dust */
         code = 33;
         break;
      case WX_ZF:      /* Freezing Fog */
         code = 34;
         break;
      case WX_IF:      /* Ice Fog */
         code = 35;
         break;
      case WX_IC:      /* Ice Crystals */
         code = 36;
         break;
      case WX_BN:      /* Blowing Sand */
         code = 37;
         break;
      case WX_ZY:      /* Freezing Spray */
         code = 38;
         break;
      case WX_VA:      /* Volcanic Ash */
         code = 39;
         break;
      case WX_WP:      /* Water Spouts */
         code = 40;
         break;
      case WX_H:       /* Haze */
         code = 41;
         break;
      default:
         code = 0;
   }                    /* End of Switch statement. */

   if ((ugly->wx[0] == WX_R) || (ugly->wx[0] == WX_S) ||
       (ugly->wx[0] == WX_RW) || (ugly->wx[0] == WX_SW) ||
       (ugly->wx[0] == WX_T) || (ugly->wx[0] == WX_ZR) ||
       (ugly->wx[0] == WX_IP) || (ugly->wx[0] == WX_ZL) ||
       (ugly->wx[0] == WX_L) || (ugly->wx[0] == WX_F)) {
      code += NDFD_CodeIntens4 (ugly->intens[0], intens1);
   }

   if ((ugly->cover[0] == COV_WIDE) || (ugly->cover[0] == COV_LKLY) ||
       (ugly->cover[0] == COV_NUM) || (ugly->cover[0] == COV_OCNL) ||
       (ugly->cover[0] == COV_DEF) || (ugly->cover[0] == COV_AREAS) ||
       (ugly->cover[0] == COV_PDS) || (ugly->cover[0] == COV_FRQ) ||
       (ugly->cover[0] == COV_INTER) || (ugly->cover[0] == COV_BRIEF) ||
       (cover1 == COV_WIDE) || (cover1 == COV_LKLY) || (cover1 == COV_NUM) ||
       (cover1 == COV_OCNL) || (cover1 == COV_DEF) ||
       (cover1 == COV_AREAS) || (cover1 == COV_PDS) || (cover1 == COV_FRQ) ||
       (cover1 == COV_INTER) || (cover1 == COV_BRIEF)) {
      code += 1100;
   }

   return code;
}

/*****************************************************************************
 * FreeUglyString() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To free the data structure used to hold the parsed ugly string.
 *
 * ARGUMENTS
 * ugly = The ugly string structure to free. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   9/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
void FreeUglyString (UglyStringType * ugly)
{
   int j;               /* Used to free all the English words. */

   for (j = 0; j < NUM_UGLY_ATTRIB; j++) {
      free (ugly->english[j]);
   }
   free (ugly->errors);
}

/*****************************************************************************
 * InitUglyString() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To initialize the structure used to hold the parsed ugly string.
 *
 * ARGUMENTS
 * ugly = The ugly string structure to modify. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   5/2003 Arthur Taylor (MDL/RSIS): Created.
 *   7/2003 AAT: Made it initialize whole UglyString structure instead of
 *          just some of the word in the structure.
 *
 * NOTES
 *****************************************************************************
 */
static void InitUglyString (UglyStringType * ugly)
{
   int i;               /* Used to traverse all the words. */
   int j;               /* Used to traverse all the attributes. */

   ugly->numValid = 0;
   ugly->f_valid = 1;
   ugly->minVis = 0;
   ugly->validIndex = 0;
   ugly->SimpleCode = 0;
   ugly->errors = NULL;
   for (i = 0; i < NUM_UGLY_WORD; i++) {
      ugly->wx[i] = 0;
      ugly->cover[i] = 0;
      ugly->intens[i] = 0;
      ugly->vis[i] = VIS_UNKNOWN;
      for (j = 0; j < NUM_UGLY_ATTRIB; j++) {
         ugly->attrib[i][j] = 0;
      }
      ugly->f_or[i] = 0;
      ugly->f_priority[i] = 0;
      ugly->english[i] = NULL;
      ugly->wx_inten[i] = 0;
      ugly->HazCode[i] = 0;
   }
}

/*****************************************************************************
 * FindInTable() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Look through a given table for a particular "phrase".
 *
 * ARGUMENTS
 *    table = The table to look in. (Input)
 * tableLen = The length of the table (Input)
 *     data = The string (or phrase) to look for. (Input)
 *      ans = The index where the string was found. (Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int
 *  1 = Found it but string is "invalid" or "missing".
 *  0 = Found it.
 * -1 = Did not find it.
 *
 * HISTORY
 *   5/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
static int FindInTable (const WxTable * table, int tableLen, char *data, uChar *ans)
{
   int i;               /* Index used to walk through the table. */

   for (i = 0; i < tableLen; i++) {
      if (strcmp (data, table[i].abrev) == 0) {
         *ans = i;
         return 0;
      }
   }
   if (strcmp (data, "<Invalid>") == 0) {
      return 1;
   } else {
      return -1;
   }
}

/*****************************************************************************
 * UglyLookUp() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Determine which table to look in based on how many ':' we have seen in
 * the current word.  After looking up the data in the appropriate table,
 * Places the result in the appropriate places in the UglyStringType data
 * structure.
 *
 * ARGUMENTS
 *   ugly = The ugly string structure to modify. (Output)
 *   data = The string (or phrase) to look for. (Input)
 *   word = Which word we are currently working on. (Input)
 *  place = What part of the word (i.e. # of :'s) (Input)
 * attNum = What part of attribute piece (i.e. # of ,'s) (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int
 *  0 = No problems
 * -1 = 'place' was invalid (larger than 4)
 * -2 = Couldn't find the phrase 'data' in the look-up tables.
 *
 * HISTORY
 *   5/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
static int UglyLookUp (UglyStringType * ugly, char *data, uChar word,
                       uChar place, uChar attNum)
{
   int ans;

   switch (place) {
      case 0:          /* Cover */
         ans = FindInTable (WxCover, (sizeof (WxCover) / sizeof (WxTable)),
                            data, &(ugly->cover[word]));
         if (ans == 1) {
            ugly->f_valid = 0;
            return 0;
         } else if (ans != 0) {
            if (strlen (data) == 0) {
               ugly->cover[word] = COV_NOCOV;
            } else {
               ugly->cover[word] = COV_UNKNOWN;
#ifdef VERBOSE
               printf ("No '%s' in WxCover\n", data);
#endif
#ifdef STORE_ERRORS
               reallocSprintf (&(ugly->errors), "No '%s' in WxCover ", data);
#endif
/*
               return -2;
*/
            }
         }
         break;
      case 1:          /* Weather */
         ans = FindInTable (WxCode, (sizeof (WxCode) / sizeof (WxTable)),
                            data, &(ugly->wx[word]));
         if (ans == 1) {
            ugly->f_valid = 0;
            return 0;
         } else if (ans != 0) {
            if (strlen (data) == 0) {
               ugly->wx[word] = WX_NOWX;
            } else {
#ifdef VERBOSE
               printf ("No '%s' in WxCode\n", data);
#endif
#ifdef STORE_ERRORS
               reallocSprintf (&(ugly->errors), "No '%s' in WxCode ", data);
#endif
               return -2;
            }
         }
         break;
      case 2:          /* Intensity */
         ans = FindInTable (WxIntens, (sizeof (WxIntens) / sizeof (WxTable)),
                            data, &(ugly->intens[word]));
         if (ans == 1) {
            ugly->f_valid = 0;
            return 0;
         } else if (ans != 0) {
            if (strlen (data) == 0) {
               ugly->intens[word] = INT_NOINT;
            } else {
#ifdef VERBOSE
               printf ("No '%s' in WxIntens\n", data);
#endif
#ifdef STORE_ERRORS
               reallocSprintf (&(ugly->errors), "No '%s' in WxIntens ", data);
#endif
               return -2;
            }
         }
         break;
      case 3:          /* Vis */
         ans = FindInTable (WxVisib, (sizeof (WxVisib) / sizeof (WxTable)),
                            data, &(ugly->vis[word]));
         if (ans == 1) {
            ugly->f_valid = 0;
            return 0;
         } else if (ans != 0) {
            if (strlen (data) == 0) {
               ugly->vis[word] = 0;
            } else {
#ifdef VERBOSE
               printf ("No '%s' in WxVisib\n", data);
#endif
#ifdef STORE_ERRORS
               reallocSprintf (&(ugly->errors), "No '%s' in WxVisib ", data);
#endif
               return -2;
            }
         }
         ugly->vis[word] = atoi (WxVisib[ugly->vis[word]].name);
         if (word == 0) {
            ugly->minVis = ugly->vis[word];
         } else if (ugly->minVis > ugly->vis[word]) {
            ugly->minVis = ugly->vis[word];
         }
         break;
      case 4:          /* Attrib */
         ans = FindInTable (WxAttrib, (sizeof (WxAttrib) / sizeof (WxTable)),
                            data, &(ugly->attrib[word][attNum]));
         if (ans == 1) {
            ugly->f_valid = 0;
            return 0;
         } else if (ans != 0) {
#ifdef VERBOSE
            printf ("No '%s' in WxAttrib\n", data);
#endif
#ifdef STORE_ERRORS
            reallocSprintf (&(ugly->errors), "No '%s' in WxAttrib ", data);
#endif
            return -2;
         } else {
            /* Check if it is the "OR" or "MX" case. */
            if (ugly->attrib[word][attNum] == HAZ_OR) {
               ugly->attrib[word][attNum] = 0;
               ugly->f_or[word] = 1;
            } else if (ugly->attrib[word][attNum] == HAZ_PRI2) {
               ugly->attrib[word][attNum] = 0;
               ugly->f_priority[word] = 2;
            } else if (ugly->attrib[word][attNum] == HAZ_PRI1) {
               ugly->attrib[word][attNum] = 0;
               ugly->f_priority[word] = 1;
            }
         }
         break;
      default:
         return -1;
   }
   return 0;
}

/*****************************************************************************
 * Ugly2English() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Converts an Ugly string to an English Phrase.
 * Example: "Iso:T:<NoInten>:<NoVis>:" -> "Isolated Thunderstorms"
 *   English phrase does not include visibility.
 *
 * ARGUMENTS
 *   ugly = The ugly string structure to modify. (Input/Output)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   5/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 * 1) buffer size is chosen so each of the 8 parts has 50 bytes for the
 *    table entry and the ' ' and ', ' and ' with '.  If NUM_UGLY_ATTRIB
 *    increases (from 5), we may need more.
 * 2) Instead of static buffer, we could use myerror.c :: AllocSprintf.
 *****************************************************************************
 */
static void Ugly2English (UglyStringType * ugly)
{
   int i;               /* Loop counter over number of words. */
   int j;               /* Loop counter over number of attributes. */
   char buffer[400];    /* Temporary storage as we build up the phrase. */
   uChar f_first;       /* Flag for first attribute. */
   int HazCode[NUM_UGLY_ATTRIB]; /* Sorted list of hazard numbers used to *
                                  * create hazard code. */
   int k;               /* A counter used to help sort hazard. */
   int temp;            /* A temp variable used to help sort hazard. */

   for (i = 0; i < ugly->numValid; i++) {
      buffer[0] = '\0';
      /* Handle Coverage. */
      if (ugly->cover[i] != 0) {
         strcat (buffer, WxCover[ugly->cover[i]].name);
         strcat (buffer, " ");
      }
      /* Handle Intensity. */
      if (ugly->intens[i] != 0) {
         strcat (buffer, WxIntens[ugly->intens[i]].name);
         strcat (buffer, " ");
      }
      strcat (buffer, WxCode[ugly->wx[i]].name);
      /* Handle Attributes. */
      f_first = 1;
      for (j = 0; j < NUM_UGLY_ATTRIB; j++) {
         if (ugly->attrib[i][j] != 0) {
            if (ugly->f_priority[i] == 0) {
               if (f_first) {
                  strcat (buffer, " with ");
                  f_first = 0;
               } else {
                  strcat (buffer, ", ");
               }
               strcat (buffer, WxAttrib[ugly->attrib[i][j]].name);
            }
         }
      }
      ugly->english[i] = (char *) malloc ((strlen (buffer) + 1) *
                                          sizeof (char));
      strcpy (ugly->english[i], buffer);
      /* Compute a code number for wx&inten, as well as attrib. */
      if (WxCode[ugly->wx[i]].number != 0) {
         ugly->wx_inten[i] = 1 + (WxCode[ugly->wx[i]].number - 1) *
               (sizeof (WxIntens) / sizeof (WxTable)) +
               WxIntens[ugly->intens[i]].number;
      } else {
         ugly->wx_inten[i] = 0;
      }
      /* Compute a code number for hazards. */
      for (j = 0; j < NUM_UGLY_ATTRIB; j++) {
         HazCode[j] = WxAttrib[ugly->attrib[i][j]].number;
         if (HazCode[j] > 250) {
            HazCode[j] = 0;
         }
      }
      for (j = 0; j < NUM_UGLY_ATTRIB - 1; j++) {
         for (k = j + 1; k < NUM_UGLY_ATTRIB; k++) {
            if (HazCode[j] > HazCode[k]) {
               temp = HazCode[j];
               HazCode[j] = HazCode[k];
               HazCode[k] = temp;
            }
         }
      }
      /* Hazard is now smallest number first... we now convert from "00 00 00 
       * 04 05" to 405 */
      ugly->HazCode[i] = 0;
      for (j = 0; j < NUM_UGLY_ATTRIB; j++) {
         ugly->HazCode[i] = (ugly->HazCode[i] * 100) + HazCode[j];
      }
   }
}

/*****************************************************************************
 * ParseUglyString() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Parse an ASCII ugly string describing weather into a data structure
 * which is more easily manipulated.
 *
 * ARGUMENTS
 *      ugly = The ugly string structure to modify. (Output)
 *    wxData = The ugly string to parse. (Input)
 * simpleVer = The version of the simple Wx table to use.
 *             (1 is 6/2003 version), (2 is 1/2004 version). (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int
 *  0 = No problems
 * -1 = Had difficulties parsing the ugly string.
 *
 * HISTORY
 *   5/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 * 1) Assumes it is ok to modify the wxData ascii string.  This means that
 *    You can NOT pass in constant strings.
 *****************************************************************************
 */
int ParseUglyString (UglyStringType * ugly, char *wxData, int simpleVer)
{
   char *cur;           /* Used to help walk though the ascii string. */
   char *start;         /* Where current phrase starts. */
   uChar attNum = 0;    /* piece of attribute phrase (# of , seen) */
   uChar place = 0;     /* location in a word (# of : seen) */
   uChar word = 0;      /* Which word in sentence (# of ^ seen) */

   /* Init first message */
   ugly->SimpleCode = 0;
   InitUglyString (ugly);
   start = wxData;
   for (cur = wxData; *cur != '\0'; cur++) {
      switch (*cur) {
         case '^':
            *cur = '\0';
            if (UglyLookUp (ugly, start, word, place, attNum) != 0) {
               *cur = '^';
#ifdef VERBOSE
               printf ("(A) %s\n", wxData);
#endif
#ifdef STORE_ERRORS
               reallocSprintf (&(ugly->errors), "(A) '%s'\n", wxData);
#endif
               /* Convert what we have to "English phrase" */
               ugly->numValid = word + 1;
               Ugly2English (ugly);
               if (simpleVer == 1) {
                  ugly->SimpleCode = NDFD_WxTable1 (ugly);
               } else if (simpleVer == 2) {
                  ugly->SimpleCode = NDFD_WxTable2 (ugly);
               } else if (simpleVer == 3) {
                  ugly->SimpleCode = NDFD_WxTable3 (ugly);
               } else {
                  ugly->SimpleCode = NDFD_WxTable4 (ugly);
               }
               return -1;
            }
            *cur = '^';
            word++;
            /* Make sure we don't start writing out of bounds. */
            if (word >= NUM_UGLY_WORD) {
#ifdef VERBOSE
               printf ("(B) %s\n", wxData);
#endif
#ifdef STORE_ERRORS
               reallocSprintf (&(ugly->errors), "(B) '%s'\n", wxData);
#endif
               /* Convert what we have to "English phrase" */
/*
               ugly->numValid = word + 1;
*/
               Ugly2English (ugly);
               if (simpleVer == 1) {
                  ugly->SimpleCode = NDFD_WxTable1 (ugly);
               } else if (simpleVer == 2) {
                  ugly->SimpleCode = NDFD_WxTable2 (ugly);
               } else if (simpleVer == 3) {
                  ugly->SimpleCode = NDFD_WxTable3 (ugly);
               } else {
                  ugly->SimpleCode = NDFD_WxTable4 (ugly);
               }
               return -1;
            }
            place = 0;
            attNum = 0;
            start = cur + 1;
            break;
         case ':':
            *cur = '\0';
            if (UglyLookUp (ugly, start, word, place, attNum) != 0) {
               *cur = ':';
#ifdef VERBOSE
               printf ("(C) %s\n", wxData);
#endif
#ifdef STORE_ERRORS
               reallocSprintf (&(ugly->errors), "(C) '%s'\n", wxData);
#endif
               /* Convert what we have to "English phrase" */
               ugly->numValid = word + 1;
               Ugly2English (ugly);
               if (simpleVer == 1) {
                  ugly->SimpleCode = NDFD_WxTable1 (ugly);
               } else if (simpleVer == 2) {
                  ugly->SimpleCode = NDFD_WxTable2 (ugly);
               } else if (simpleVer == 3) {
                  ugly->SimpleCode = NDFD_WxTable3 (ugly);
               } else {
                  ugly->SimpleCode = NDFD_WxTable4 (ugly);
               }
               return -1;
            }
            *cur = ':';
            place++;
            attNum = 0;
            start = cur + 1;
            break;
         case ',':
            if (place == 4) {
               *cur = '\0';
               if (UglyLookUp (ugly, start, word, place, attNum) != 0) {
                  *cur = ',';
#ifdef VERBOSE
                  printf ("(D) %s\n", wxData);
#endif
#ifdef STORE_ERRORS
                  reallocSprintf (&(ugly->errors), "(D) '%s'\n", wxData);
#endif
                  /* Convert what we have to "English phrase" */
                  ugly->numValid = word + 1;
                  Ugly2English (ugly);
                  if (simpleVer == 1) {
                     ugly->SimpleCode = NDFD_WxTable1 (ugly);
                  } else if (simpleVer == 2) {
                     ugly->SimpleCode = NDFD_WxTable2 (ugly);
                  } else if (simpleVer == 3) {
                     ugly->SimpleCode = NDFD_WxTable3 (ugly);
                  } else {
                     ugly->SimpleCode = NDFD_WxTable4 (ugly);
                  }
                  return -1;
               }
               *cur = ',';
               attNum++;
               start = cur + 1;
            }
            break;
         default:
            break;
      }
/*
      if (word == 2) {
         return;
      }
*/
   }
   if (start != '\0') {
      if (UglyLookUp (ugly, start, word, place, attNum) != 0) {
#ifdef VERBOSE
         printf ("(E) '%s'\n", wxData);
#endif
#ifdef STORE_ERRORS
         reallocSprintf (&(ugly->errors), "(E) '%s'\n", wxData);
#endif
         /* Convert what we have to "English phrase" */
         ugly->numValid = word + 1;
         Ugly2English (ugly);
         if (simpleVer == 1) {
            ugly->SimpleCode = NDFD_WxTable1 (ugly);
         } else if (simpleVer == 2) {
            ugly->SimpleCode = NDFD_WxTable2 (ugly);
         } else if (simpleVer == 3) {
            ugly->SimpleCode = NDFD_WxTable3 (ugly);
         } else {
            ugly->SimpleCode = NDFD_WxTable4 (ugly);
         }
         return -1;
      }
   }

   ugly->numValid = word + 1;
   /* Convert what we have to "English phrase" */
   Ugly2English (ugly);
   if (simpleVer == 1) {
      ugly->SimpleCode = NDFD_WxTable1 (ugly);
   } else if (simpleVer == 2) {
      ugly->SimpleCode = NDFD_WxTable2 (ugly);
   } else if (simpleVer == 3) {
      ugly->SimpleCode = NDFD_WxTable3 (ugly);
   } else {
      ugly->SimpleCode = NDFD_WxTable4 (ugly);
   }
#ifdef VERBOSE
   if (place != 4) {
      printf ("Too few ugly words? '%s'\n", wxData);
   }
#endif
#ifdef STORE_ERRORS
/*
   if (place != 4) {
      reallocSprintf (&(ugly->errors), "Too few ugly words? '%s'\n", wxData);
   }
*/
#endif
   return 0;
}

/*****************************************************************************
 * PrintUglyString() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   Prints all the relevant data in an ugly string.  Mainly for debugging
 * purposes.
 *
 * ARGUMENTS
 *   ugly = The ugly string structure to print. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: void
 *
 * HISTORY
 *   5/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
void PrintUglyString (UglyStringType * ugly)
{
   int i;               /* Used to traverse the ugly string structure. */
   double vis;          /* Used to determine if we have "missing" vis. */

   printf ("numValid %d\n", ugly->numValid);
   for (i = 0; i < ugly->numValid; i++) {
      if (ugly->vis[i] == VIS_UNKNOWN) {
         vis = 9999;
      } else {
         vis = ugly->vis[i] / 32.;
      }
      printf ("Wx=%d, Cov=%d, inten=%d, vis=%d, attrib=%d,%d,%d,%d,%d\n",
              ugly->wx[i], ugly->cover[i], ugly->intens[i],
              ugly->vis[i], ugly->attrib[i][0], ugly->attrib[i][1],
              ugly->attrib[i][2], ugly->attrib[i][3], ugly->attrib[i][4]);
      printf ("Wx=%s, Cov=%s, intens=%s, vis=%f, attrib=%s,%s,%s,%s,%s\n",
              WxCode[ugly->wx[i]].name, WxCover[ugly->cover[i]].name,
              WxIntens[ugly->intens[i]].name, vis,
              WxAttrib[ugly->attrib[i][0]].name,
              WxAttrib[ugly->attrib[i][1]].name,
              WxAttrib[ugly->attrib[i][2]].name,
              WxAttrib[ugly->attrib[i][3]].name,
              WxAttrib[ugly->attrib[i][4]].name);
   }
   printf ("\n");
}

#ifdef DEBUG_WEATHER
/*****************************************************************************
 * main() --
 *
 * Arthur Taylor / MDL
 *
 * PURPOSE
 *   To test the ParseUglyString() procedure.
 *
 * ARGUMENTS
 * argc = The number of arguments on the command line. (Input)
 * argv = The arguments on the command line. (Input)
 *
 * FILES/DATABASES: None
 *
 * RETURNS: int
 *
 * HISTORY
 *   5/2003 Arthur Taylor (MDL/RSIS): Created.
 *
 * NOTES
 *****************************************************************************
 */
int main (int argc, char **argv)
{
   UglyStringType ugly;
   char buffer[100];

   strcpy (buffer, "Pds:R:+:<NoVis>:Mention^Ocnl:R:m:<NoVis>:^Sct:"
           "T:<NoInten>:<NoVis>:");
   ParseUglyString (&ugly, buffer, 3);
   PrintUglyString (&ugly);
   if (ugly.errors != NULL) {
      printf ("Errors: %s\n", ugly.errors);
   }
   FreeUglyString (&ugly);
   return 0;

   strcpy (buffer, "Sct:SW:-:<NoVis>:");
   ParseUglyString (&ugly, buffer, 3);
   PrintUglyString (&ugly);
   strcpy (buffer, "Ocnl:R:-:<NoVis>:^Ocnl:S:-:<NoVis>:^SChc:ZR:-:<NoVis>:");
   ParseUglyString (&ugly, buffer, 3);
   PrintUglyString (&ugly);
   strcpy (buffer, "Wide:FR:-:<NoVis>:OLA");
   ParseUglyString (&ugly, buffer, 3);
   PrintUglyString (&ugly);
   strcpy (buffer, "<NoCov>:<NoWx>:<NoInten>:<NoVis>:");
   ParseUglyString (&ugly, buffer, 3);
   PrintUglyString (&ugly);
   strcpy (buffer, "Sct:RW:-:<NoVis>:^Iso:T:m:<NoVis>:");
   ParseUglyString (&ugly, buffer, 3);
   PrintUglyString (&ugly);
   strcpy (buffer, "Sct:T:+:<NoVis>:DmgW,LgA");
   ParseUglyString (&ugly, buffer, 3);
   PrintUglyString (&ugly);
   return 0;
}
#endif

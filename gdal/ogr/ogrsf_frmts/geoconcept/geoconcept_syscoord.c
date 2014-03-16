/**********************************************************************
 * $Id: geoconcept_syscoord.c
 *
 * Name:     geoconcept_syscoord.c
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements translation between Geoconcept SysCoord
 *           and OGRSpatialRef format
 * Language: C
 *
 **********************************************************************
 * Copyright (c) 2007,  Geoconcept and IGN
 * Copyright (c) 2008-2010, Even Rouault <even dot rouault at mines-paris dot org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "geoconcept_syscoord.h"
#include "cpl_string.h"

GCSRS_CVSID("$Id: geoconcept_syscoord.c,v 1.0.0 2007-12-24 15:40:28 drichard Exp $")

#ifndef PI
#define PI 3.14159265358979323846
#endif


/* -------------------------------------------------------------------- */
/*      GCSRS globals                                                   */
/* -------------------------------------------------------------------- */

/*
 * The following information came from GEO CONCEPT PROJECTION files,
 * aka GCP files.
 * A lot of information has been added to these GCP. There are mostly
 * noticed as FIXME in the source.
 */

static GCSysCoord gk_asSysCoordList[]=
/*
 * pszSysCoordName, pszUnit, dfPM, dfLambda0, dfPhi0, dfk0, dfX0, dfY0, dfPhi1, dfPhi2, nDatumID, nProjID, coordSystemID, timeZoneValue
 *
 * #12, #14, #15, #17 : parameters listed below are "generic" ...
 *
 * Geoconcept uses cos(lat_ts) as scale factor, but cos(lat_ts)==cos(-lat_ts) : I then set dfPhi1 with lat_ts
 */
{
{"Lambert 2 extended",              NULL,  2.337229166667,   0.000000000, 46.80000000,0.99987742000, 600000.000, 2200000.000,  0.0,  0.0,  13,   2,    1,-1},
{"Lambert 1",                       NULL,  2.337229166667,   0.000000000, 49.50000000,0.99987734000, 600000.000,  200000.000,  0.0,  0.0,  13,   2,    2,-1},
{"Lambert 2",                       NULL,  2.337229166667,   0.000000000, 46.80000000,0.99987742000, 600000.000,  200000.000,  0.0,  0.0,  13,   2,    3,-1},
{"Lambert 3",                       NULL,  2.337229166667,   0.000000000, 44.10000000,0.99987750000, 600000.000,  200000.000,  0.0,  0.0,  13,   2,    4,-1},
{"Lambert 4",                       NULL,  2.337229166667,   0.000000000, 42.16500000,0.99994471000,    234.358,  185861.369,  0.0,  0.0,  13,   2,    5,-1},
{"Bonne NTF",                       NULL,  2.337222222222,   0.000000000, 48.86000000,1.00000000000,      0.000,       0.000,  0.0,  0.0,   1,   3,   11,-1},
{"UTM Nord - ED50",                 NULL,  0.000000000000,   0.000000000,  0.00000000,0.99960000000, 500000.000,       0.000,  0.0,  0.0,  14,   1,   12, 0},
{"Plate carrée",                    NULL,  0.000000000000,   0.000000000,  0.00000000,0.00000000000,      0.000,       0.000,  0.0,  0.0,  11,   4,   13,-1},
{"MGRS (Military UTM)",             NULL,  0.000000000000,   0.000000000,  0.00000000,0.99960000000,      0.000,       0.000,  0.0,  0.0,   4,  11,   14,-1},
{"UTM Sud - WGS84",                 NULL,  0.000000000000,   0.000000000,  0.00000000,0.99960000000, 500000.000,10000000.000,  0.0,  0.0,   4,   1,   15, 0},
{"National GB projection",          NULL,  0.000000000000,  -2.000000000, 49.00000000,0.99960127170, 400000.000, -100000.000,  0.0,  0.0,  12,  12,   16,-1},
{"UTM Nord - WGS84",                NULL,  0.000000000000,   0.000000000,  0.00000000,0.99960000000, 500000.000,       0.000,  0.0,  0.0,   4,   1,   17, 0},
{"UTM Nord - WGS84",                NULL,  0.000000000000,   0.000000000,  0.00000000,0.99960000000, 500000.000,       0.000,  0.0,  0.0,9990,   1,   17, 0},
{"Lambert 2 étendu - sans grille",  NULL,  2.337229166667,   0.000000000, 46.80000000,0.99987742000, 600000.000, 2200000.000,  0.0,  0.0,   1,   2,   91,-1},
{"Lambert 1 - sans grille",         NULL,  2.337229166667,   0.000000000, 49.50000000,0.99987734000, 600000.000,  200000.000,  0.0,  0.0,   1,   2,   92,-1},
{"Lambert 2 - sans grille",         NULL,  2.337229166667,   0.000000000, 46.80000000,0.99987742000, 600000.000,  200000.000,  0.0,  0.0,   1,   2,   93,-1},
{"Lambert 3 - sans grille",         NULL,  2.337229166667,   0.000000000, 44.10000000,0.99987750000, 600000.000,  200000.000,  0.0,  0.0,   1,   2,   94,-1},
{"Lambert 4 - sans grille",         NULL,  2.337229166667,   0.000000000, 42.16500000,0.99994471000,    234.358,  185861.369,  0.0,  0.0,   1,   2,   95,-1},
{"(Long/Lat) NTF",                   "d",  0.000000000000,   0.000000000,  0.00000000,0.00000000000,      0.000,       0.000,  0.0,  0.0,   1,   0,  100,-1},
{"(Long/Lat) WGS84",                 "d",  0.000000000000,   0.000000000,  0.00000000,0.00000000000,      0.000,       0.000,  0.0,  0.0,   4,   0,  101,-1},
{"(Long/Lat) ED50",                  "d",  0.000000000000,   0.000000000,  0.00000000,0.00000000000,      0.000,       0.000,  0.0,  0.0,  14,   0,  102,-1},
{"(Long/Lat) Australian 1984",       "d",  0.000000000000,   0.000000000,  0.00000000,0.00000000000,      0.000,       0.000,  0.0,  0.0,   7,   0,  103,-1},
{"(Long/Lat) Airy",                  "d",  0.000000000000,   0.000000000,  0.00000000,0.00000000000,      0.000,       0.000,  0.0,  0.0,  12,   0,  104,-1},
{"(Long/Lat) NTF Paris (gr)",       "gr",  2.337229166667,   0.000000000,  0.00000000,0.00000000000,      0.000,       0.000,  0.0,  0.0,   1,   0,  105,-1},
{"(Long/Lat) WGS 72",                "d",  0.000000000000,   0.000000000,  0.00000000,0.00000000000,      0.000,       0.000,  0.0,  0.0,   3,   0,  107,-1},
{"Geoportail MILLER",               NULL,  0.000000000000,   0.000000000,  0.00000000,0.00000000000,      0.000,       0.000,  0.0,  0.0,   4,  24,  222,-1},
{"IGN-RRAFGUADU20",                 NULL,  0.000000000000, -63.000000000,  0.00000000,0.99960000000, 500000.000,       0.000,  0.0,  0.0,9984,   1,  501,-1},/* FIXME does not exist in IGNF, use IGN-UTM20W84GUAD instead */
{"IGN-RRAFMARTU20",                 NULL,  0.000000000000, -63.000000000,  0.00000000,0.99960000000, 500000.000,       0.000,  0.0,  0.0,9984,   1,  502,-1},/* FIXME does not exist in IGNF, use IGN-UTM20W84MART instead, never reached cause identical to 501:-1 */
{"IGN-RGM04UTM38S",                 NULL,  0.000000000000,  45.000000000,  0.00000000,0.99960000000, 500000.000,10000000.000,  0.0,  0.0,9984,   1,  503,-1},/* FIXME 5030 datum changed into 9984 */
{"IGN-RGR92UTM40S",                 NULL,  0.000000000000,  57.000000000,  0.00000000,0.99960000000, 500000.000,10000000.000,  0.0,  0.0,9984,   1,  504,-1},
{"IGN-UTM22RGFG95",                 NULL,  0.000000000000, -51.000000000,  0.00000000,0.99960000000, 500000.000,       0.000,  0.0,  0.0,9984,   1,  505,-1},
{"IGN-UTM01SWG84",                  NULL,  0.000000000000,-177.000000000,  0.00000000,0.99960000000, 500000.000,10000000.000,  0.0,  0.0,9984,   1, 506,-1},/* never reached cause identical to 15:1 */
{"IGN-RGSPM06U21",                  NULL,  0.000000000000, -57.000000000,  0.00000000,0.99960000000, 500000.000,       0.000,  0.0,  0.0,9984,   1,  507,-1},
{"IGN-RGPFUTM5S",                   NULL,  0.000000000000,-153.000000000,  0.00000000,0.99960000000, 500000.000,10000000.000,  0.0,  0.0,9984,   1,  508,-1},
{"IGN-RGPFUTM6S",                   NULL,  0.000000000000,-147.000000000,  0.00000000,0.99960000000, 500000.000,10000000.000,  0.0,  0.0,9984,   1,  509,-1},
{"IGN-RGPFUTM7S",                   NULL,  0.000000000000,-141.000000000,  0.00000000,0.99960000000, 500000.000,10000000.000,  0.0,  0.0,9984,   1,  510,-1},
{"IGN-CROZ63UTM39S",                NULL,  0.000000000000,  51.000000000,  0.00000000,0.99960000000, 500000.000,10000000.000,  0.0,  0.0,9983,   1,  511,-1},
{"IGN-WGS84UTM1S",                  NULL,  0.000000000000,-177.000000000,  0.00000000,0.99960000000, 500000.000,10000000.000,  0.0,  0.0,   4,   1,  512,-1},
{"IGN-RGNCUTM57S",                  NULL,  0.000000000000, 159.000000000,  0.00000000,0.99960000000, 500000.000,10000000.000,  0.0,  0.0,9984,   1,  513,-1},
{"IGN-RGNCUTM58S",                  NULL,  0.000000000000, 165.000000000,  0.00000000,0.99960000000, 500000.000,10000000.000,  0.0,  0.0,9984,   1,  514,-1},
{"IGN-RGNCUTM59S",                  NULL,  0.000000000000, 171.000000000,  0.00000000,0.99960000000, 500000.000,10000000.000,  0.0,  0.0,9984,   1,  515,-1},
{"IGN-KERG62UTM42S",                NULL,  0.000000000000,  69.000000000,  0.00000000,0.99960000000, 500000.000,10000000.000,  0.0,  0.0,9988,   1,  516,-1},
{"IGN-REUN47GAUSSL",                NULL,  0.000000000000,  55.533333333,-21.11666667,1.00000000000, 160000.000,   50000.000,  0.0,  0.0,   2,  19,  520,-1},
{"Lambert 1 Carto",                 NULL,  2.337229166667,   0.000000000, 49.50000000,0.99987734000, 600000.000, 1200000.000,  0.0,  0.0,  13,   2, 1002,-1},
{"Lambert 2 Carto",                 NULL,  2.337229166667,   0.000000000, 46.80000000,0.99987742000, 600000.000, 2200000.000,  0.0,  0.0,  13,   2, 1003,-1},/* never reached cause identical to 1:-1 */
{"Lambert 3 Carto",                 NULL,  2.337229166667,   0.000000000, 44.10000000,0.99987750000, 600000.000, 3200000.000,  0.0,  0.0,  13,   2, 1004,-1},
{"Lambert 4 Carto",                 NULL,  2.337229166667,   0.000000000, 42.16500000,0.99994471000,    234.358, 4185861.369,  0.0,  0.0,  13,   2, 1005,-1},
{"Lambert 93",                      NULL,  0.000000000000,   3.000000000, 46.50000000,0.00000000000, 700000.000, 6600000.000, 44.0, 49.0,9984,  18, 1006,-1},
{"IGN-RGNCLAM",                     NULL,  0.000000000000, 166.000000000,-21.30000000,0.00000000000, 400000.000,  300000.000,-20.4,-22.2,9984,  18, 1007,-1},/* Added in GCP */
{"Lambert 1 Carto - sans grille",   NULL,  2.337229166667,   0.000000000, 49.50000000,0.99987734000, 600000.000, 1200000.000,  0.0,  0.0,   1,   2, 1092,-1},
{"Lambert 2 Carto - sans grille",   NULL,  2.337229166667,   0.000000000, 46.80000000,0.99987742000, 600000.000, 2200000.000,  0.0,  0.0,   1,   2, 1093,-1},
{"Lambert 3 Carto - sans grille",   NULL,  2.337229166667,   0.000000000, 44.10000000,0.99987750000, 600000.000, 3200000.000,  0.0,  0.0,   1,   2, 1094,-1},
{"Lambert 4 Carto - sans grille",   NULL,  2.337229166667,   0.000000000, 42.16500000,0.99994471000,    234.358,  185861.369,  0.0,  0.0,   1,   2, 1095,-1},
{"Suisse",                          NULL,  0.000000000000,   7.439583333, 46.95240556,1.00000000000, 600000.000,  200000.000,  0.0,  0.0,   2,  25, 1556,-1},
{"Geoportail France",               NULL,  0.000000000000,   0.000000000,  0.00000000,0.68835457569,      0.000,       0.000, 46.5,  0.0,9984,  26, 2012,-1},
{"Geoportail Antilles",             NULL,  0.000000000000,   0.000000000,  0.00000000,0.96592582629,      0.000,       0.000, 15.0,  0.0,9984,  26, 2016,-1},
{"Geoportail Guyane",               NULL,  0.000000000000,   0.000000000,  0.00000000,0.99756405026,      0.000,       0.000,  4.0,  0.0,9984,  26, 2017,-1},
{"Geoportail Reunion",              NULL,  0.000000000000,   0.000000000,  0.00000000,0.93358042649,      0.000,       0.000,-21.0,  0.0,9984,  26, 2018,-1},
{"Geoportail Mayotte",              NULL,  0.000000000000,   0.000000000,  0.00000000,0.97814760073,      0.000,       0.000,-12.0,  0.0,9984,  26, 2019,-1},
{"Geoportail ST Pierre et Miquelon",NULL,  0.000000000000,   0.000000000,  0.00000000,0.68199836006,      0.000,       0.000, 47.0,  0.0,9984,  26, 2020,-1},
{"Geoportail Nouvelle Caledonie",   NULL,  0.000000000000,   0.000000000,  0.00000000,0.92718385456,      0.000,       0.000,-22.0,  0.0,9984,  26, 2021,-1},
{"Geoportail Wallis",               NULL,  0.000000000000,   0.000000000,  0.00000000,0.97029572627,      0.000,       0.000,-14.0,  0.0,9984,  26, 2022,-1},
{"Geoportail Polynesie",            NULL,  0.000000000000,   0.000000000,  0.00000000,0.96592582628,      0.000,       0.000,-15.0,  0.0,9984,  26, 2023,-1},
{"Mercator sur sphère WGS84",       NULL,  0.000000000000,   0.000000000,  0.00000000,1.00000000000,      0.000,       0.000,  0.0,  0.0,2015,  21, 2027,-1},
{"(Long/Lat) RGF 93",                "d",  0.000000000000,   0.000000000,  0.00000000,0.00000000000,      0.000,       0.000,  0.0,  0.0,  13,   0, 2028,-1},
{"(Long/Lat) ITRS-89",               "d",  0.000000000000,   0.000000000,  0.00000000,0.00000000000,      0.000,       0.000,  0.0,  0.0,9984,   0, 2028,-1},
{"Geoportail Crozet",               NULL,  0.000000000000,   0.000000000,  0.00000000,0.69465837046,      0.000,       0.000,-46.0,  0.0,9984,  26, 2040,-1},/* FIXME : wrong scale factor was 0.69088241108 */
{"Geoportail Kerguelen",            NULL,  0.000000000000,   0.000000000,  0.00000000,0.64944804833,      0.000,       0.000,-49.5,  0.0,9984,  26, 2042,-1},/* FIXME : wrong scale factor was 0.67815966987 */
{"Lambert CC 42",                   NULL,  0.000000000000,   3.000000000, 42.00000000,0.00000000000,1700000.000, 1200000.000, 41.2, 42.8,9984,  18, 2501,-1},
{"Lambert CC 43",                   NULL,  0.000000000000,   3.000000000, 43.00000000,0.00000000000,1700000.000, 2200000.000, 42.2, 43.8,9984,  18, 2502,-1},
{"Lambert CC 44",                   NULL,  0.000000000000,   3.000000000, 44.00000000,0.00000000000,1700000.000, 3200000.000, 43.2, 44.8,9984,  18, 2503,-1},
{"Lambert CC 45",                   NULL,  0.000000000000,   3.000000000, 45.00000000,0.00000000000,1700000.000, 4200000.000, 44.2, 45.8,9984,  18, 2504,-1},
{"Lambert CC 46",                   NULL,  0.000000000000,   3.000000000, 46.00000000,0.00000000000,1700000.000, 5200000.000, 45.2, 46.8,9984,  18, 2505,-1},
{"Lambert CC 47",                   NULL,  0.000000000000,   3.000000000, 47.00000000,0.00000000000,1700000.000, 6200000.000, 46.2, 47.8,9984,  18, 2506,-1},
{"Lambert CC 48",                   NULL,  0.000000000000,   3.000000000, 48.00000000,0.00000000000,1700000.000, 7200000.000, 47.2, 48.8,9984,  18, 2507,-1},
{"Lambert CC 49",                   NULL,  0.000000000000,   3.000000000, 49.00000000,0.00000000000,1700000.000, 8200000.000, 48.2, 49.8,9984,  18, 2508,-1},
{"Lambert CC 50",                   NULL,  0.000000000000,   3.000000000, 50.00000000,0.00000000000,1700000.000, 9200000.000, 49.2, 50.8,9984,  18, 2509,-1},
{"(Long/Lat) IGN-RGM04GEO",          "d",  0.000000000000,   0.000000000,  0.00000000,0.00000000000,      0.000,       0.000,  0.0,  0.0,9984,   0,10001,-1},
{"(Long/Lat) IGN-RGFG95GEO",         "d",  0.000000000000,   0.000000000,  0.00000000,0.00000000000,      0.000,       0.000,  0.0,  0.0,9984,   0,10002,-1},/* never reached, identical to 10001:-1 */
{"(Long/Lat) IGN-WGS84RRAFGEO",      "d",  0.000000000000,   0.000000000,  0.00000000,0.00000000000,      0.000,       0.000,  0.0,  0.0,9984,   0,10003,-1},/* never reached, identical to 10001:-1 */
{"(Long/Lat) IGN-RGR92GEO",          "d",  0.000000000000,   0.000000000,  0.00000000,0.00000000000,      0.000,       0.000,  0.0,  0.0,9984,   0,10004,-1},/* never reached, identical to 10001:-1 */
{"(Long/Lat) IGN-WGS84G",            "d",  0.000000000000,   0.000000000,  0.00000000,0.00000000000,      0.000,       0.000,  0.0,  0.0,   4,   0,10005,-1},
{"(Long/Lat) CROZ63GEO",             "d",  0.000000000000,   0.000000000,  0.00000000,0.00000000000,      0.000,       0.000,  0.0,  0.0,   4,   0,10006,-1},
{"(Long/Lat) RGSPM06GEO",            "d",  0.000000000000,   0.000000000,  0.00000000,0.00000000000,      0.000,       0.000,  0.0,  0.0,9984,   0,10007,-1},/* never reached, identical to 10001:-1 */
{"(Long/Lat) RGPFGEO",               "d",  0.000000000000,   0.000000000,  0.00000000,0.00000000000,      0.000,       0.000,  0.0,  0.0,9984,   0,10008,-1},/* never reached, identical to 10001:-1 */
{"(Long/Lat) RGNCGEO",               "d",  0.000000000000,   0.000000000,  0.00000000,0.00000000000,      0.000,       0.000,  0.0,  0.0,9984,   0,10009,-1},/* never reached, identical to 10001:-1 */
{"(Long/Lat) KER62GEO",              "d",  0.000000000000,   0.000000000,  0.00000000,0.00000000000,      0.000,       0.000,  0.0,  0.0,9988,   0,10010,-1},
{"UTM Sud - ED50",                  NULL,  0.000000000000,   0.000000000,  0.00000000,0.99960000000, 500000.000,10000000.000,  0.0,  0.0,  14,   1,99912, 0},/* FIXME allow retrieving 12:0 - See _findSysCoord_GCSRS() */
{NULL,                              NULL,  0.000000000000,   0.000000000,  0.00000000,0.00000000000,      0.000,       0.000,  0.0,  0.0,  -1,  -1,   -1,-1}
};

static GCProjectionInfo gk_asProjList[]=
/*
 * pszProjName, nSphere, nProjID
 */
{
{"Geographic shift",        0,    0},
{"UTM",                     0,    1},
{"Lambert Conform Conic",   0,    2},
{"Bonne",                   0,    3},
{"Plate carrée",            0,    4},
{"MGRS (Military UTM)",     0,   11},
{"Transversal Mercator",    0,   12},
{"Lambert secant",          0,   18},
{"Gauss Laborde",           1,   19},
{"Polyconic",               0,   20},
{"Direct Mercator",         0,   21},
{"Stereographic oblic",     1,   22},
{"Miller",                  0,   24},
{"Mercator oblic",          1,   25},
{"Equi rectangular",        1,   26},

{NULL,                      0,   -1}
};

static GCDatumInfo gk_asDatumList[]=
  /*
   * pszDatumName, dfShiftX, dfShiftY, dfShiftZ, dfRotX, dfRotY, dfRotZ, dfScaleFactor, dfFA, dfFlattening, nEllipsoidID, nDatumID
   */
  /*
   * Wrong dx, dy, dz :
   * IGN-RGM04GEO, was -217, -216, 67
   * IGN-RGFG95GEO, was -2, -2, 2
   * IGN-RGSPM06GEO, was -125.593, 143.763, -194.558
   *
   * #1 and #13 are identical
   * #8, #11, #2015 are spherical views of #4
   * #5030, #5031 and #5032 are identical
   * FIXME : #5030, #5031, #5032 are ITRS89 compliant, so "compatible" with #4, better use #9999 as ellipsoid
   * FIXME : #9999 to #9986 added
   */
{
{"NTF (Clarke 1880)",               -168.0000, -60.0000, 320.0000, 0.00000, 0.00000,  0.00000,  0.0,       -112.200,-54.7388e-6,       3,   1},
{"ED50 France (International 1909)", -84.0000, -97.0000,-117.0000, 0.00000, 0.00000,  0.00000,  0.0,       -251.000,-14.1927e-6,       5,   2},
{"WGS 72",                             0.0000,  12.0000,   6.0000, 0.00000, 0.00000,  0.00000,  0.0,          2.000,  0.0312e-6,       6,   3},
{"WGS_1984",                           0.0000,   0.0000,   0.0000, 0.00000, 0.00000,  0.00000,  0.0,          0.000,  0.0,          9999,   4},
{"ED 79",                            -83.0000, -95.0000,-116.0000, 0.00000, 0.00000,  0.00000,  0.0,       -251.000,-14.1927e-6,       5,   5},
{"Australian Geodetic 1966",        -133.0000, -48.0000, 148.0000, 0.00000, 0.00000,  0.00000,  0.0,        -23.000, -0.0081e-6,       7,   6},
{"Australian Geodetic 1984",        -134.0000, -48.0000, 149.0000, 0.00000, 0.00000,  0.00000,  0.0,        -23.000, -0.0081e-6,       7,   7},
{"Sphere",                             0.0000,   0.0000,   0.0000, 0.00000, 0.00000,  0.00000,  0.0,          0.000,  0.0,             1,   8},
{"Sphere DCW",                         0.0000,   0.0000,   0.0000, 0.00000, 0.00000,  0.00000,  0.0,          0.000,  0.0,             1,  11},
{"Airy",                             375.0000,-111.0000, 431.0000, 0.00000, 0.00000,  0.00000,  0.0,        573.604, 11.96002325e-6,   8,  12},
{"NTF-Grille",                      -168.0000, -60.0000, 320.0000, 0.00000, 0.00000,  0.00000,  0.0,       -112.200,-54.7388e-6,       3,  13},
{"ED50 (International 1909)",        -87.0000, -98.0000,-121.0000, 0.00000, 0.00000,  0.00000,  0.0,       -251.000,-14.1927e-6,       5,  14},
{"WGS 84 sur sphere",                  0.0000,   0.0000,   0.0000, 0.00000, 0.00000,  0.00000,  0.0,          0.000,  0.0,             1,2015},
{"IGN-RGM04GEO",                       0.0000,   0.0000,   0.0000, 0.00000, 0.00000,  0.00000,  0.0,          0.000,  0.0,             4,5030},
{"IGN-RGFG95GEO",                      0.0000,   0.0000,   0.0000, 0.00000, 0.00000,  0.00000,  0.0,          0.000,  0.0,             4,5031},
{"IGN-RGSPM06GEO",                     0.0000,   0.0000,   0.0000, 0.00000, 0.00000,  0.00000,  0.0,          0.000,  0.0,             4,5032},
{"IGN-WALL78",                       253.0000,-133.0000,-127.0000, 0.00000, 0.00000,  0.00000,  0.0,       -251.000,-14.1927e-6,       5,9999},/* FIXME */
{"IGN-TAHA",                          72.4380, 345.9180,  79.4860,-1.60450,-0.88230, -0.55650,  1.3746e-6, -251.000,-14.1927e-6,       5,9998},/* FIXME */
{"IGN-MOOREA87",                     215.9820, 149.5930, 176.2290, 3.26240, 1.69200,  1.15710, 10.47730e-6,-251.000,-14.1927e-6,       5,9997},/* FIXME */
{"IGN-TAHI51",                       162.0000, 117.0000, 154.0000, 0.00000, 0.00000,  0.00000,  0.0,       -251.000,-14.1927e-6,       5,9996},/* FIXME */
{"IGN-NUKU72",                       165.7320, 216.7200, 180.5050,-0.64340,-0.45120, -0.07910,  7.42040e-6,-251.000,-14.1927e-6,       5,9995},/* FIXME */
{"IGN-IGN63",                        410.7210,  55.0490,  80.7460,-2.57790,-2.35140, -0.66640, 17.33110e-6,-251.000,-14.1927e-6,       5,9994},/* FIXME */
{"IGN-MART38",                       126.9260, 547.9390, 130.4090,-2.78670, 5.16124, -0.85844, 13.82265e-6,-251.000,-14.1927e-6,       5,9993},/* FIXME */
{"IGN-GUAD48",                      -472.2900,  -5.6300,-304.1200, 0.43620,-0.83740,  0.25630,  1.89840e-6,-251.000,-14.1927e-6,       5,9992},/* FIXME */
{"IGN-GUADFM49",                     136.5960, 248.1480,-429.7890, 0.00000, 0.00000,  0.00000,  0.0,       -251.000,-14.1927e-6,       5,9991},/* FIXME */
{"IGN-STPM50",                       -95.5930, 573.7630, 173.4420,-0.96020, 1.25100, -1.39180, 42.62650e-6, -69.400,-37.2957e-6,       2,9990},/* FIXME */
{"IGN-CSG67",                       -193.0660, 236.9930, 105.4470, 0.48140,-0.80740,  0.12760,  1.56490e-6,-251.000,-14.1927e-6,       5,9989},/* FIXME */
{"IGN-KERG62",                       145.0000,-187.0000, 103.0000, 0.00000, 0.00000,  0.00000,  0.0,       -251.000,-14.1927e-6,       5,9988},/* FIXME */
{"IGN-REUN47",                       789.5240,-626.4860, -89.9040, 0.60060,76.79460,-10.57880,-32.32410e-6,-251.000,-14.1927e-6,       5,9987},/* FIXME */
{"IGN-MAYO50",                      -599.9280,-275.5520,-195.6650, 0.08350, 0.47150, -0.06020,-49.28140e-6,-251.000,-14.1927e-6,       5,9986},/* FIXME */
{"IGN-TAHI79",                       221.5250, 152.9480, 176.7680, 2.38470, 1.38960,  0.87700, 11.47410e-6,-251.000,-14.1927e-6,       5,9985},/* FIXME */
{"ITRS-89",                            0.0000,   0.0000,   0.0000, 0.00000, 0.00000,  0.00000,  0.0,          0.000,  0.0,             4,9984},
{"IGN-CROZ63",                         0.0000,   0.0000,   0.0000, 0.00000, 0.00000,  0.00000,  0.0,       -251.000,-14.1927e-6,       5,9983},/* FIXME added cause the Bursa-Wolf parameters are not known */

{NULL,                                 0.0000,   0.0000,   0.0000, 0.00000, 0.00000,  0.00000,  0.0,          0.000,  0.0,            -1,  -1}
};

static GCSpheroidInfo gk_asSpheroidList[]=
  /*
   * pszSpheroidName, dfA, dfE, nEllipsoidID
   *
   * cause Geoconcept assimilates WGS84 and GRS80, WGS84 is added to the list
   */
{
{"Sphere",                     6378137.0000, 0.00000000000000,   1},
{"Clarke 1866",                6378206.4000, 0.08227185423947,   2},/* Wrong, semi-major was 6378249.4000     */
{"Clarke 1880",                6378249.2000, 0.08248325676300,   3},/* Wrong, excentricity was 0.082483256945 */
{"GRS 80",                     6378137.0000, 0.08181919104300,   4},/* Wrong, excentricity was 0.081819191060 */
{"International 1909",         6378388.0000, 0.08199188997900,   5},
{"WGS 72",                     6378135.0000, 0.08181881201777,   6},
{"Australian National",        6378160.0000, 0.08182017998700,   7},
{"Airy",                       6377563.3960, 0.08167337387420,   8},
{"WGS 84",                     6378137.0000, 0.08181919084262,9999},

{NULL,                         0,            0,                 -1}
};

/* -------------------------------------------------------------------- */
/*      GCSRS API Prototypes                                            */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
static int GCSRSAPI_CALL _areCompatibleSpheroids_GCSRS ( int id1, int id2 )
{
  if( id1==id2 ) return TRUE;
 
  switch( id1 )
  {
    case    4 :
    case 9999 :
      switch( id2 )
      {
        case    4 :
        case 9999 :
          return TRUE;
        default   :
          break;
      }
      break;
    default   :
      break;
  }

  return FALSE;
}/* _areCompatibleSpheroids_GCSRS */

/* -------------------------------------------------------------------- */
static int GCSRSAPI_CALL _areCompatibleDatums_GCSRS ( int id1, int id2 )
{
  if( id1==id2 ) return TRUE;
 
  switch( id1 )
  {
    case    1 : /* NTF */
    case   13 :
      switch( id2 )
      {
        case    1 :
        case   13 :
          return TRUE;
        default   :
          break;
      }
      break;
    case    2 : /* ED50 */
    case   14 :
    case 9983 :
    case 9985 :
    case 9986 :
    case 9987 :
    case 9989 :
    case 9991 :
    case 9992 :
    case 9993 :
    case 9994 :
    case 9995 :
    case 9997 :
    case 9998 :
    case 9999 :
      switch( id2 )
      {
        case    2 :
        case   14 :
        case 9983 :
        case 9985 :
        case 9986 :
        case 9987 :
        case 9989 :
        case 9991 :
        case 9992 :
        case 9993 :
        case 9994 :
        case 9995 :
        case 9997 :
        case 9998 :
        case 9999 :
          return TRUE;
        default   :
          break;
      }
      break;
    case    4 : /* WGS84 - ITRS89  */
    case    8 :
    case   11 :
    case 2015 :
    case 5030 :
    case 5031 :
    case 5032 :
    case 9984 :
      switch( id2 )
      {
        case    4 :
        case    8 :
        case   11 :
        case 2015 :
        case 5030 :
        case 5031 :
        case 5032 :
        case 9984 :
          return TRUE;
        default   :
          break;
      }
      break;
    default   :
      break;
  }

  return FALSE;
}/* _areCompatibleDatums_GCSRS */

#define _CPLDebugSpheroid_GCSRS(e) \
CPLDebug( "GEOCONCEPT", "SemiMajor:%.4f;Excentricity:%.10f;",\
          GetInfoSpheroidSemiMajor_GCSRS(e),\
          GetInfoSpheroidExcentricity_GCSRS(e)\
);

/* -------------------------------------------------------------------- */
static GCSpheroidInfo GCSRSAPI_CALL1(*) _findSpheroid_GCSRS ( double a, double rf )
{
  int iSpheroid, iResol= 0, nResol= 2;
  GCSpheroidInfo* ell;
  double e, p[]= {1e-10, 1e-8};

  /* f = 1 - sqrt(1 - e^2) */
  e= 1.0/rf;
  e= sqrt(e*(2.0-e));
ell_relax:
  for( iSpheroid= 0, ell= &(gk_asSpheroidList[0]);
       GetInfoSpheroidID_GCSRS(ell)!=-1;
       iSpheroid++, ell= &(gk_asSpheroidList[iSpheroid]) )
  {
    if( fabs(GetInfoSpheroidSemiMajor_GCSRS(ell) - a) > 1e-4 ) continue;
    if( fabs(GetInfoSpheroidExcentricity_GCSRS(ell) - e) > p[iResol] ) continue;
    break;
  }
  if( GetInfoSpheroidID_GCSRS(ell)==-1 && iResol!=nResol-1 )
  {
    iResol++;
    goto ell_relax;
  }

  return ell;
}/* _findSpheroid_GCSRS */

#define _CPLDebugDatum_GCSRS(d) \
CPLDebug( "GEOCONCEPT", "ID:%d;ShiftX:%.4f;ShiftY:%.4f;ShiftZ:%.4f;DiffA:%.4f;DiffFlattening:%.7f;",\
  GetInfoDatumID_GCSRS((d)),\
  GetInfoDatumShiftX_GCSRS((d)),\
  GetInfoDatumShiftY_GCSRS((d)),\
  GetInfoDatumShiftZ_GCSRS((d)),\
  GetInfoDatumDiffA_GCSRS((d)),\
  GetInfoDatumDiffFlattening_GCSRS((d))\
);

/* -------------------------------------------------------------------- */
static GCDatumInfo GCSRSAPI_CALL1(*) _findDatum_GCSRS ( double dx,
                                                        double dy,
                                                        double dz,
                                                        double a,
                                                        double f )
{
  int iDatum, bRelax= FALSE;
  GCDatumInfo* datum;

datum_relax:
  for( iDatum= 0, datum= &(gk_asDatumList[0]);
       GetInfoDatumID_GCSRS(datum)!=-1;
       iDatum++, datum= &(gk_asDatumList[iDatum]) )
  {
    if( !bRelax )
    {
      if( fabs(GetInfoDatumShiftX_GCSRS(datum) - dx) > 1e-4 ) continue;
      if( fabs(GetInfoDatumShiftY_GCSRS(datum) - dy) > 1e-4 ) continue;
      if( fabs(GetInfoDatumShiftZ_GCSRS(datum) - dz) > 1e-4 ) continue;
    }
    if( fabs(GetInfoDatumDiffA_GCSRS(datum) - (6378137.0000-a)) > 1e-4 ) continue;
    if( fabs(GetInfoDatumDiffFlattening_GCSRS(datum) - (0.003352779565406696648-f)) > 1e-7 ) continue;
    break;
  }
  if( GetInfoDatumID_GCSRS(datum)==-1 && !bRelax )
  {
    /*
     * FIXME : when both nadgrids and towgs84 are defined, bursa-wolf parameters are lost !
     *         if the projection and the ellipsoid are known, one can retrieve the datum 
     *         Try relaxed search ...
     */
    bRelax= TRUE;
    goto datum_relax;
  }

  return datum;
}/* _findDatum_GCSRS */

/* -------------------------------------------------------------------- */
static GCProjectionInfo GCSRSAPI_CALL1(*) _findProjection_GCSRS ( const char* p, double lat_ts )
{
  int iProj;
  GCProjectionInfo* proj;

  for( iProj= 0, proj= &(gk_asProjList[0]);
       GetInfoProjID_GCSRS(proj)!=-1;
       iProj++, proj= &(gk_asProjList[iProj]) )
  {
    if( iProj==0 && p==NULL)
      break;
    if( iProj==1 && 
        ( EQUAL(p,SRS_PT_TRANSVERSE_MERCATOR)               ||
          EQUAL(p,SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED) ) )
      break;
    if( iProj==2 &&
        EQUAL(p,SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP) )
      break;
    if( iProj==3 &&
        EQUAL(p,SRS_PT_BONNE) )
      break;
    if( iProj==4 &&
        EQUAL(p,SRS_PT_EQUIRECTANGULAR) &&
        lat_ts==0.0 )
      break;
    /* FIXME : iProj==6 ? */
    if( iProj==7 &&
        ( EQUAL(p,SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) ||
          EQUAL(p,SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM) ) )
      break;
    if( iProj==8 &&
        EQUAL(p,SRS_PT_GAUSSSCHREIBERTMERCATOR) )
      break;
    if( iProj==9 &&
        EQUAL(p,SRS_PT_POLYCONIC) )
      break;
    /* FIXME
    if( iProj==10 &&
        ( EQUAL(p,SRS_PT_MERCATOR_1SP) ||
          EQUAL(p,SRS_PT_MERCATOR_2SP) ) )
      break;
     */
    if( iProj==11 &&
        ( EQUAL(p,SRS_PT_OBLIQUE_STEREOGRAPHIC) ||
          EQUAL(p,SRS_PT_POLAR_STEREOGRAPHIC) ) )
      break;
    if( iProj==12 &&
        EQUAL(p,SRS_PT_MILLER_CYLINDRICAL) )
      break;
    /* FIXME
    if( iProj==13 &&
        ( EQUAL(p,SRS_PT_HOTINE_OBLIQUE_MERCATOR) ||
          EQUAL(p,SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN) ||
          EQUAL(p,SRS_PT_LABORDE_OBLIQUE_MERCATOR) ) )
      break;
     */
    if( iProj==14 &&
        EQUAL(p,SRS_PT_EQUIRECTANGULAR) &&
        lat_ts!=0.0 )
      break;
  }

  return proj;
}/* _findProjection_GCSRS */

#define _CPLDebugSysCoord_GCSRS(m,s) \
CPLDebug( "GEOCONCEPT", "[%s]ID=%d;Zone=%d;DatumID=%d;ProjID=%d;PrimeMeridian=%.10f;CentralMeridian=%.10f;LatitudeOfOrigin=%.10f;StandardParallel1=%.10f;StandardParallel2=%.10f;ScaleFactor=%.10f;FalseEasting=%.10f;FalseNorthing=%.10f;",\
          (m)? (m):"",\
          GetSysCoordSystemID_GCSRS((s)),\
          GetSysCoordTimeZone_GCSRS((s)),\
          GetSysCoordDatumID_GCSRS((s)),\
          GetSysCoordProjID_GCSRS((s)),\
          GetSysCoordPrimeMeridian_GCSRS((s)),\
          GetSysCoordCentralMeridian_GCSRS((s)),\
          GetSysCoordLatitudeOfOrigin_GCSRS((s)),\
          GetSysCoordStandardParallel1_GCSRS((s)),\
          GetSysCoordStandardParallel2_GCSRS((s)),\
          GetSysCoordScaleFactor_GCSRS((s)),\
          GetSysCoordFalseEasting_GCSRS((s)),\
          GetSysCoordFalseNorthing_GCSRS((s))\
);

/* -------------------------------------------------------------------- */
static GCSysCoord GCSRSAPI_CALL1(*) _findSysCoord_GCSRS ( GCSysCoord* theSysCoord )
{
  int iSysCoord, bestSysCoord= -1;
  GCSysCoord* gcsc;

  if( !theSysCoord) return NULL;

  SetSysCoordSystemID_GCSRS(theSysCoord, -1);
  SetSysCoordTimeZone_GCSRS(theSysCoord, -1);
  _CPLDebugSysCoord_GCSRS(NULL,theSysCoord);
  for( iSysCoord= 0, gcsc= &(gk_asSysCoordList[0]);
       GetSysCoordSystemID_GCSRS(gcsc)!=-1;
       iSysCoord++, gcsc= &(gk_asSysCoordList[iSysCoord]) )
  {
    if( !_areCompatibleDatums_GCSRS(GetSysCoordDatumID_GCSRS(gcsc), GetSysCoordDatumID_GCSRS(theSysCoord)) ) continue;

    if( GetSysCoordProjID_GCSRS(gcsc) != GetSysCoordProjID_GCSRS(theSysCoord) ) continue;

    if( fabs(GetSysCoordPrimeMeridian_GCSRS(gcsc) - GetSysCoordPrimeMeridian_GCSRS(theSysCoord) ) > 1e-8 ) continue;

    if( fabs(GetSysCoordCentralMeridian_GCSRS(gcsc) - GetSysCoordCentralMeridian_GCSRS(theSysCoord) ) > 1e-8 )
    {
      switch( GetSysCoordProjID_GCSRS(gcsc) )
      {
        case    1 :/* UTM familly : central meridian is the 6* zone - 183 (in degrees) */
          if( GetSysCoordCentralMeridian_GCSRS(gcsc)==0.0 ) /* generic UTM definition */
          {
            break;
          }
        default   :
          continue;
      }
    }
    if( fabs(GetSysCoordLatitudeOfOrigin_GCSRS(gcsc) - GetSysCoordLatitudeOfOrigin_GCSRS(theSysCoord) ) > 1e-8 ) continue;

    if( fabs(GetSysCoordStandardParallel1_GCSRS(gcsc) - GetSysCoordStandardParallel1_GCSRS(theSysCoord) ) > 1e-8 ) continue;
    if( fabs(GetSysCoordStandardParallel2_GCSRS(gcsc) - GetSysCoordStandardParallel2_GCSRS(theSysCoord) ) > 1e-8 ) continue;

    if( fabs(GetSysCoordScaleFactor_GCSRS(gcsc) - GetSysCoordScaleFactor_GCSRS(theSysCoord) ) > 1e-8 ) continue;

    if( fabs(GetSysCoordFalseEasting_GCSRS(gcsc) - GetSysCoordFalseEasting_GCSRS(theSysCoord) ) > 1e-4 ) continue;
    if( fabs(GetSysCoordFalseNorthing_GCSRS(gcsc) - GetSysCoordFalseNorthing_GCSRS(theSysCoord) ) > 1e-4 ) continue;

    /* Found a candidate : */
    if( bestSysCoord==-1)
    {
      bestSysCoord= iSysCoord;
    }
    else
    {
      switch( GetSysCoordProjID_GCSRS(gcsc) )
      {
        case    0:/* long/lat */
          if( GetSysCoordDatumID_GCSRS(gcsc)==GetSysCoordDatumID_GCSRS(theSysCoord) &&
              GetSysCoordDatumID_GCSRS(&(gk_asSysCoordList[bestSysCoord]))!=GetSysCoordDatumID_GCSRS(theSysCoord)) /* exact match */
          {
            bestSysCoord= iSysCoord;
          }
          break;
        case    1:/* UTM familly : central meridian is the 6* zone - 183 (in degrees) */
          if( GetSysCoordCentralMeridian_GCSRS(gcsc)!=0.0 &&
              GetSysCoordDatumID_GCSRS(gcsc)==GetSysCoordDatumID_GCSRS(theSysCoord) &&
              GetSysCoordDatumID_GCSRS(&(gk_asSysCoordList[bestSysCoord]))!=GetSysCoordDatumID_GCSRS(theSysCoord)) /* exact match */
          {
            bestSysCoord= iSysCoord;
          }
          break;
        default  :
          break;
      }
    }
  }
  /* Seems to be the right Geoconcept system: */
  if( bestSysCoord>=0 )
  {
    gcsc= &(gk_asSysCoordList[bestSysCoord]);
    switch( GetSysCoordSystemID_GCSRS(gcsc) )
    {
      case 99912 : /* hack */
        SetSysCoordSystemID_GCSRS(theSysCoord, 12);
        break;
      default    :
        SetSysCoordSystemID_GCSRS(theSysCoord, GetSysCoordSystemID_GCSRS(gcsc));
        break;
    }
    SetSysCoordTimeZone_GCSRS(theSysCoord, GetSysCoordTimeZone_GCSRS(gcsc));
    if( GetSysCoordName_GCSRS(gcsc) )
      SetSysCoordName_GCSRS(theSysCoord, CPLStrdup(GetSysCoordName_GCSRS(gcsc)));
    if( GetSysCoordUnit_GCSRS(gcsc) )
      SetSysCoordUnit_GCSRS(theSysCoord, CPLStrdup(GetSysCoordUnit_GCSRS(gcsc)));
  }

  return theSysCoord;
}/* _findSysCoord_GCSRS */

/* -------------------------------------------------------------------- */
static void GCSRSAPI_CALL _InitSysCoord_GCSRS (
                                                GCSysCoord* theSysCoord
                                              )
{
  SetSysCoordSystemID_GCSRS(theSysCoord, -1);
  SetSysCoordTimeZone_GCSRS(theSysCoord, -1);
  SetSysCoordName_GCSRS(theSysCoord, NULL);
  SetSysCoordUnit_GCSRS(theSysCoord, NULL);
  SetSysCoordCentralMeridian_GCSRS(theSysCoord, 0.0);
  SetSysCoordLatitudeOfOrigin_GCSRS(theSysCoord, 0.0);
  SetSysCoordStandardParallel1_GCSRS(theSysCoord, 0.0);
  SetSysCoordStandardParallel2_GCSRS(theSysCoord, 0.0);
  SetSysCoordScaleFactor_GCSRS(theSysCoord, 0.0);
  SetSysCoordFalseEasting_GCSRS(theSysCoord, 0.0);
  SetSysCoordFalseNorthing_GCSRS(theSysCoord, 0.0);
  SetSysCoordDatumID_GCSRS(theSysCoord, -1);
  SetSysCoordProjID_GCSRS(theSysCoord, -1);
  SetSysCoordPrimeMeridian_GCSRS(theSysCoord, 0);
}/* _InitSysCoord_GCSRS */

/* -------------------------------------------------------------------- */
GCSysCoord GCSRSAPI_CALL1(*) CreateSysCoord_GCSRS (
                                                    int srsid,
                                                    int timezone
                                                  )
{
  int iSysCoord;
  GCSysCoord* theSysCoord, *gcsc;

  if( !(theSysCoord= CPLMalloc(sizeof(GCSysCoord))) )
  {
    CPLError( CE_Failure, CPLE_OutOfMemory,
              "failed to create a Geoconcept coordinate system.\n"
              );
    return NULL;
  }
  _InitSysCoord_GCSRS(theSysCoord);
  if( srsid>=0)
  {
    for( iSysCoord= 0, gcsc= &(gk_asSysCoordList[0]);
         GetSysCoordSystemID_GCSRS(gcsc)!=-1;
         iSysCoord++, gcsc= &(gk_asSysCoordList[iSysCoord]) )
    {
      if( srsid==GetSysCoordSystemID_GCSRS(gcsc) )
      {
        SetSysCoordSystemID_GCSRS(theSysCoord, srsid);
        SetSysCoordTimeZone_GCSRS(theSysCoord, timezone);
        if( GetSysCoordName_GCSRS(gcsc) )
          SetSysCoordName_GCSRS(theSysCoord, CPLStrdup(GetSysCoordName_GCSRS(gcsc)));
        if( GetSysCoordUnit_GCSRS(gcsc) )
          SetSysCoordUnit_GCSRS(theSysCoord, CPLStrdup(GetSysCoordUnit_GCSRS(gcsc)));
        SetSysCoordCentralMeridian_GCSRS(theSysCoord, GetSysCoordCentralMeridian_GCSRS(gcsc));
        SetSysCoordLatitudeOfOrigin_GCSRS(theSysCoord, GetSysCoordLatitudeOfOrigin_GCSRS(gcsc));
        SetSysCoordStandardParallel1_GCSRS(theSysCoord, GetSysCoordStandardParallel1_GCSRS(gcsc));
        SetSysCoordStandardParallel2_GCSRS(theSysCoord, GetSysCoordStandardParallel2_GCSRS(gcsc));
        SetSysCoordScaleFactor_GCSRS(theSysCoord, GetSysCoordScaleFactor_GCSRS(gcsc));
        SetSysCoordFalseEasting_GCSRS(theSysCoord, GetSysCoordFalseEasting_GCSRS(gcsc));
        SetSysCoordFalseNorthing_GCSRS(theSysCoord, GetSysCoordFalseNorthing_GCSRS(gcsc));
        SetSysCoordDatumID_GCSRS(theSysCoord, GetSysCoordDatumID_GCSRS(gcsc));
        SetSysCoordProjID_GCSRS(theSysCoord, GetSysCoordProjID_GCSRS(gcsc));
        break;
      }
    }
  }

  return theSysCoord;
}/* CreateSysCoord_GCSRS */

/* -------------------------------------------------------------------- */
static void GCSRSAPI_CALL _ReInitSysCoord_GCSRS (
                                                  GCSysCoord* theSysCoord
                                                )
{
  if( GetSysCoordName_GCSRS(theSysCoord) )
  {
    CPLFree(GetSysCoordName_GCSRS(theSysCoord));
  }
  if( GetSysCoordUnit_GCSRS(theSysCoord) )
  {
    CPLFree(GetSysCoordUnit_GCSRS(theSysCoord));
  }
  _InitSysCoord_GCSRS(theSysCoord);
}/* _ReInitSysCoord_GCSRS */

/* -------------------------------------------------------------------- */
void GCSRSAPI_CALL DestroySysCoord_GCSRS (
                                           GCSysCoord** theSysCoord
                                         )
{
  _ReInitSysCoord_GCSRS(*theSysCoord);
  CPLFree(*theSysCoord);
  *theSysCoord= NULL;
}/* DestroySysCoord_GCSRS */

/* -------------------------------------------------------------------- */
GCSysCoord GCSRSAPI_CALL1(*) OGRSpatialReference2SysCoord_GCSRS ( OGRSpatialReferenceH poSR )
{
  char* pszProj4= NULL;
  GCSpheroidInfo* ell= NULL;
  GCDatumInfo* datum= NULL;
  GCProjectionInfo* gcproj= NULL;
  double a, rf, f, p[7];
  GCSysCoord* syscoord= NULL;

  if( !poSR ) return NULL;

  pszProj4= NULL;
  OSRExportToProj4(poSR, &pszProj4);
  if( !pszProj4 ) pszProj4= CPLStrdup("");

  CPLDebug("GEOCONCEPT", "SRS : %s", pszProj4);

  if( !(syscoord= CreateSysCoord_GCSRS(-1,-1)) )
  {
    goto onError;
  }
  SetSysCoordPrimeMeridian_GCSRS(syscoord, OSRGetPrimeMeridian(poSR,NULL));

  a= OSRGetSemiMajor(poSR,NULL);
  rf= OSRGetInvFlattening(poSR,NULL);
  ell= _findSpheroid_GCSRS(a, rf);
  if( GetInfoSpheroidID_GCSRS(ell)==-1 )
  {
    CPLDebug("GEOCONCEPT", "Unsupported ellipsoid : %.4f %.10f", a, rf);
    goto onError;
  }
  CPLDebug("GEOCONCEPT", "ellipsoid found : %s",
           GetInfoSpheroidName_GCSRS(ell));

  OSRGetTOWGS84(poSR,p,7);
  f= 1.0 - sqrt(1.0 - GetInfoSpheroidExcentricity_GCSRS(ell)*GetInfoSpheroidExcentricity_GCSRS(ell));
  datum= _findDatum_GCSRS(p[0], p[1], p[2], GetInfoSpheroidSemiMajor_GCSRS(ell), f);
  if( GetInfoDatumID_GCSRS(datum)==-1 )
  {
    CPLDebug("GEOCONCEPT", "Unsupported datum : %.4f %.4f; %.4f %.4f %.10f",
             p[0], p[1], p[2], a, 1.0/rf);
    goto onError;
  }
  /* FIXME : WGS 84 and GRS 80 assimilation by Geoconcept : */
  if( GetInfoSpheroidID_GCSRS(ell)==4 ) /* GRS 80 */
  {
    datum= &(gk_asDatumList[31]);
  }
  else if( GetInfoSpheroidID_GCSRS(ell)==9999 ) /* WGS 84 */
  {
    datum= &(gk_asDatumList[3]);
  }
  CPLDebug("GEOCONCEPT", "datum found : %s", GetInfoDatumName_GCSRS(datum));
  SetSysCoordDatumID_GCSRS(syscoord, GetInfoDatumID_GCSRS(datum));

  gcproj= _findProjection_GCSRS(OSRIsGeographic(poSR)?
                                  NULL
                                :
                                  OSRGetAttrValue(poSR, "PROJECTION", 0),
                                OSRGetProjParm(poSR,SRS_PP_PSEUDO_STD_PARALLEL_1,0.0,NULL));
  if( GetInfoProjID_GCSRS(gcproj)==-1 )
  {
    CPLDebug("GEOCONCEPT", "Unsupported projection : %s",
             OSRIsGeographic(poSR)? "GEOCS":OSRGetAttrValue(poSR, "PROJECTION", 0));
    goto onError;
  }
  CPLDebug("GEOCONCEPT", "projection : %s", GetInfoProjName_GCSRS(gcproj));
  SetSysCoordProjID_GCSRS(syscoord, GetInfoProjID_GCSRS(gcproj));

  /* then overwrite them with projection specific parameters ... */
  if( OSRIsProjected(poSR) )
  {
    double v;

    SetSysCoordPrimeMeridian_GCSRS(syscoord, OSRGetPrimeMeridian(poSR,NULL));
    SetSysCoordCentralMeridian_GCSRS(syscoord, OSRGetProjParm(poSR,SRS_PP_CENTRAL_MERIDIAN,0.0,NULL));
    SetSysCoordLatitudeOfOrigin_GCSRS(syscoord, OSRGetProjParm(poSR,SRS_PP_LATITUDE_OF_ORIGIN,0.0,NULL));
    SetSysCoordStandardParallel1_GCSRS(syscoord, OSRGetProjParm(poSR,SRS_PP_STANDARD_PARALLEL_1,0.0,NULL));
    SetSysCoordStandardParallel2_GCSRS(syscoord, OSRGetProjParm(poSR,SRS_PP_STANDARD_PARALLEL_2,0.0,NULL));
    SetSysCoordFalseEasting_GCSRS(syscoord, OSRGetProjParm(poSR,SRS_PP_FALSE_EASTING,0.0,NULL));
    SetSysCoordFalseNorthing_GCSRS(syscoord, OSRGetProjParm(poSR,SRS_PP_FALSE_NORTHING,0.0,NULL));
    if( (v= OSRGetProjParm(poSR,SRS_PP_SCALE_FACTOR,0.0,NULL))!= 0.0 )
    {
      SetSysCoordScaleFactor_GCSRS(syscoord, v);
    }
    if( (v= OSRGetProjParm(poSR,SRS_PP_PSEUDO_STD_PARALLEL_1,0.0,NULL))!= 0.0 )
    {
      /* should be SRS_PT_EQUIRECTANGULAR : */
      SetSysCoordScaleFactor_GCSRS(syscoord, cos(v*PI/180.0));
      SetSysCoordStandardParallel1_GCSRS(syscoord, v);/* allow keeping lat_ts sign */
    }
  }

  /* Retrieve the syscoord : */
  if( !_findSysCoord_GCSRS(syscoord) )
  {
    CPLDebug("GEOCONCEPT", "invalid syscoord ?!");
    goto onError;
  }
  if( GetSysCoordSystemID_GCSRS(syscoord)==-1 )
  {
    CPLDebug("GEOCONCEPT", "Cannot find syscoord");
    goto onError;
  }
  /* when SRS_PT_TRANSVERSE_MERCATOR, get zone : */
  if( GetSysCoordTimeZone_GCSRS(syscoord)==0 )
  {
    int pbNorth= 1;
    SetSysCoordTimeZone_GCSRS(syscoord, OSRGetUTMZone(poSR,&pbNorth));
  }

  if( pszProj4 )
  {
    CPLFree(pszProj4);
  }
  CPLDebug( "GEOCONCEPT", "SysCoord value: %d:%d",
            GetSysCoordSystemID_GCSRS(syscoord),
            GetSysCoordTimeZone_GCSRS(syscoord) );

  return syscoord;

onError:
  if( pszProj4 )
  {
    CPLDebug( "GEOCONCEPT",
              "Unhandled spatial reference system '%s'.",
              pszProj4);
    CPLFree(pszProj4);
  }
  if( syscoord )
  {
    DestroySysCoord_GCSRS(&syscoord);
  }
  return NULL;
}/* OGRSpatialReference2SysCoord_GCSRS */

/* -------------------------------------------------------------------- */
OGRSpatialReferenceH GCSRSAPI_CALL SysCoord2OGRSpatialReference_GCSRS ( GCSysCoord* syscoord )
{
  OGRSpatialReferenceH poSR;
  GCDatumInfo* datum= NULL;
  GCSpheroidInfo* ell= NULL;
  int i;
  double f;

  poSR= OSRNewSpatialReference(NULL);

  if( syscoord && GetSysCoordSystemID_GCSRS(syscoord)!=-1 )
  {
    switch( GetSysCoordProjID_GCSRS(syscoord) )
    {
      case 0   : /* long/lat */
        break;
      case    1: /* UTM */
      case   11: /* MGRS */
      case   12: /* TM */
        OSRSetTM(poSR, GetSysCoordLatitudeOfOrigin_GCSRS(syscoord),
                       GetSysCoordCentralMeridian_GCSRS(syscoord),
                       GetSysCoordScaleFactor_GCSRS(syscoord),
                       GetSysCoordFalseEasting_GCSRS(syscoord),
                       GetSysCoordFalseNorthing_GCSRS(syscoord));
        break;
      case    2: /* LCC 1SP */
        OSRSetLCC1SP(poSR, GetSysCoordLatitudeOfOrigin_GCSRS(syscoord),
                           GetSysCoordCentralMeridian_GCSRS(syscoord),
                           GetSysCoordScaleFactor_GCSRS(syscoord),
                           GetSysCoordFalseEasting_GCSRS(syscoord),
                           GetSysCoordFalseNorthing_GCSRS(syscoord));
        break;
      case    3: /* Bonne */
        OSRSetBonne(poSR, GetSysCoordLatitudeOfOrigin_GCSRS(syscoord),
                          GetSysCoordCentralMeridian_GCSRS(syscoord),
                          GetSysCoordFalseEasting_GCSRS(syscoord),
                          GetSysCoordFalseNorthing_GCSRS(syscoord));
        break;
      case    4: /* Plate Caree */
        OSRSetEquirectangular(poSR, GetSysCoordLatitudeOfOrigin_GCSRS(syscoord),
                                    GetSysCoordCentralMeridian_GCSRS(syscoord),
                                    GetSysCoordFalseEasting_GCSRS(syscoord),
                                    GetSysCoordFalseNorthing_GCSRS(syscoord));
        break;
      case   18: /* LCC 2SP */
        OSRSetLCC(poSR, GetSysCoordStandardParallel1_GCSRS(syscoord),
                        GetSysCoordStandardParallel2_GCSRS(syscoord),
                        GetSysCoordLatitudeOfOrigin_GCSRS(syscoord),
                        GetSysCoordCentralMeridian_GCSRS(syscoord),
                        GetSysCoordFalseEasting_GCSRS(syscoord),
                        GetSysCoordFalseNorthing_GCSRS(syscoord));
        break;
      case   19: /* Gauss Schreiber : Reunion */
        OSRSetGaussSchreiberTMercator(poSR, GetSysCoordLatitudeOfOrigin_GCSRS(syscoord),
                                            GetSysCoordCentralMeridian_GCSRS(syscoord),
                                            GetSysCoordScaleFactor_GCSRS(syscoord),
                                            GetSysCoordFalseEasting_GCSRS(syscoord),
                                            GetSysCoordFalseNorthing_GCSRS(syscoord));
        break;
      case   20: /* Polyconic */
        OSRSetPolyconic(poSR, GetSysCoordLatitudeOfOrigin_GCSRS(syscoord),
                              GetSysCoordCentralMeridian_GCSRS(syscoord),
                              GetSysCoordFalseEasting_GCSRS(syscoord),
                              GetSysCoordFalseNorthing_GCSRS(syscoord));
        break;
      case   21: /* Direct Mercator */
        OSRSetMercator(poSR, GetSysCoordLatitudeOfOrigin_GCSRS(syscoord),
                             GetSysCoordCentralMeridian_GCSRS(syscoord),
                             GetSysCoordScaleFactor_GCSRS(syscoord),
                             GetSysCoordFalseEasting_GCSRS(syscoord),
                             GetSysCoordFalseNorthing_GCSRS(syscoord));
        break;
      case   22: /* Stereographic oblic */
        OSRSetOS(poSR, GetSysCoordLatitudeOfOrigin_GCSRS(syscoord),
                       GetSysCoordCentralMeridian_GCSRS(syscoord),
                       GetSysCoordScaleFactor_GCSRS(syscoord),
                       GetSysCoordFalseEasting_GCSRS(syscoord),
                       GetSysCoordFalseNorthing_GCSRS(syscoord));
        break;
      case   24: /* Miller */
        OSRSetMC(poSR, GetSysCoordLatitudeOfOrigin_GCSRS(syscoord),
                       GetSysCoordCentralMeridian_GCSRS(syscoord),
                       GetSysCoordFalseEasting_GCSRS(syscoord),
                       GetSysCoordFalseNorthing_GCSRS(syscoord));
        break;
      case   26: /* Equi rectangular */
        OSRSetEquirectangular2(poSR, GetSysCoordLatitudeOfOrigin_GCSRS(syscoord),
                                     GetSysCoordCentralMeridian_GCSRS(syscoord),
                                     GetSysCoordStandardParallel1_GCSRS(syscoord),
                                     GetSysCoordFalseEasting_GCSRS(syscoord),
                                     GetSysCoordFalseNorthing_GCSRS(syscoord));
        break;
      default  :
        break;
    }
    if( GetSysCoordProjID_GCSRS(syscoord)>0 )
      OSRSetProjCS(poSR, GetSysCoordName_GCSRS(syscoord));

    for( i= 0, datum= &(gk_asDatumList[0]);
         GetInfoDatumID_GCSRS(datum)!=-1;
         i++, datum= &(gk_asDatumList[i]) )
    {
      if( GetInfoDatumID_GCSRS(datum)==GetSysCoordDatumID_GCSRS(syscoord) ) break;
    }
    for( i= 0, ell= &(gk_asSpheroidList[0]);
         GetInfoSpheroidID_GCSRS(ell)!=-1;
         i++, ell= &(gk_asSpheroidList[i]) )
    {
      if( _areCompatibleSpheroids_GCSRS(GetInfoSpheroidID_GCSRS(ell),
                                        GetInfoDatumSpheroidID_GCSRS(datum)) ) break;
    }
    /* FIXME : WGS 84 and GRS 80 assimilation by Geoconcept : */
    if( GetInfoDatumID_GCSRS(datum)==4 ) /* WGS 84 */
    {
      ell= &(gk_asSpheroidList[8]);
    }
    else if( GetInfoDatumID_GCSRS(datum)==9984 ) /* GRS 80 */
    {
      ell= &(gk_asSpheroidList[3]);
    }
    f= 1.0 - sqrt(1.0 - GetInfoSpheroidExcentricity_GCSRS(ell)*GetInfoSpheroidExcentricity_GCSRS(ell));
    OSRSetGeogCS(poSR, GetSysCoordProjID_GCSRS(syscoord)!=0 || !GetSysCoordName_GCSRS(syscoord)?
                         "unnamed":GetSysCoordName_GCSRS(syscoord),
                       GetInfoDatumID_GCSRS(datum)>=0? GetInfoDatumName_GCSRS(datum):"unknown",
                       GetInfoSpheroidID_GCSRS(ell)>=0? GetInfoSpheroidName_GCSRS(ell):"unknown",
                       GetInfoSpheroidID_GCSRS(ell)>=0? GetInfoSpheroidSemiMajor_GCSRS(ell):6378137.0,
                       GetInfoSpheroidID_GCSRS(ell)>=0? (f==0? 0:1/f):298.257223563,
                       "Greenwich",
                       GetSysCoordPrimeMeridian_GCSRS(syscoord),
                       SRS_UA_DEGREE, atof(SRS_UA_DEGREE_CONV));
    /* As Geoconcept uses Molodensky, we've got only 3 out of 7 params for Bursa-Wolf : */
    /* the 4 missing Bursa-Wolf parameters have been added to the gk_asDatumList !      */
    if( GetInfoProjID_GCSRS(syscoord)>0 && GetInfoDatumID_GCSRS(datum)!=-1 )
    {
      OSRSetTOWGS84(poSR, GetInfoDatumShiftX_GCSRS(datum),
                          GetInfoDatumShiftY_GCSRS(datum),
                          GetInfoDatumShiftZ_GCSRS(datum),
                          GetInfoDatumRotationX_GCSRS(datum),
                          GetInfoDatumRotationY_GCSRS(datum),
                          GetInfoDatumRotationZ_GCSRS(datum),
                          1e6*GetInfoDatumScaleFactor_GCSRS(datum));
    }
  }

/* -------------------------------------------------------------------- */
/*      Report on translation.                                          */
/* -------------------------------------------------------------------- */
  {
    char* pszWKT;

    OSRExportToWkt(poSR,&pszWKT);
    if( pszWKT!=NULL )
    {
        CPLDebug( "GEOCONCEPT",
                  "This SysCoord value: %d:%d was translated to : %s",
                  GetSysCoordSystemID_GCSRS(syscoord),
                  GetSysCoordTimeZone_GCSRS(syscoord),
                  pszWKT );
        CPLFree( pszWKT );
    }
  }

  return poSR;
}/* SysCoord2OGRSpatialReference_GCSRS */

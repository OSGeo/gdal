/**********************************************************************
 * $Id: mitab_spatialref.cpp,v 1.55 2011-06-11 00:35:00 fwarmerdam Exp $
 *
 * Name:     mitab_spatialref.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the SpatialRef stuff in the TABFile class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 1999-2001, Frank Warmerdam
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
 **********************************************************************
 *
 * $Log: mitab_spatialref.cpp,v $
 * Revision 1.55  2011-06-11 00:35:00  fwarmerdam
 * add support for reading google mercator (#4115)
 *
 * Revision 1.54  2010-10-07 18:46:26  aboudreault
 * Fixed bad use of atof when locale setting doesn't use . for float (GDAL bug #3775)
 *
 * Revision 1.53  2010-09-07 16:48:08  aboudreault
 * Removed incomplete patch for affine params support in mitab. (bug 1155)
 *
 * Revision 1.52  2010-07-08 17:21:12  aboudreault
 * Put back New_Zealand Datum in asDatumInfoList
 *
 * Revision 1.51  2010-07-07 19:00:15  aboudreault
 * Cleanup Win32 Compile Warnings (GDAL bug #2930)
 *
 * Revision 1.50  2010-07-05 17:20:14  aboudreault
 * Added Krovak projection suppoprt (bug 2230)
 *
 * Revision 1.49  2009-10-15 16:16:37  fwarmerdam
 * add the default EPSG/OGR name for new zealand datums (gdal #3187)
 *
 * Revision 1.48  2007/11/21 21:15:45  dmorissette
 * Fix asDatumInfoList[] and asSpheroidInfoList[] defns/refs (bug 1826)
 *
 * Revision 1.47  2006/07/10 17:58:48  fwarmerdam
 * North_American_Datum_1927 support
 *
 * Revision 1.46  2006/07/07 19:41:32  dmorissette
 * Fixed problem with uninitialized sTABProj.nAffineFlag (bug 1254,1319)
 *
 * Revision 1.45  2006/05/09 20:21:29  fwarmerdam
 * Coordsys false easting and northing are in the units of the coordsys, not
 * necessarily meters.  Adjusted mitab_spatialref.cpp to reflect this.
 * http://bugzilla.remotesensing.org/show_bug.cgi?id=1113
 *
 * Revision 1.44  2005/09/29 20:15:36  dmorissette
 * More improvements to handling of modified TM projections 21-24.
 * Added correct name stings to all datum definitions (Anthony D, bug 1155)
 *
 * Revision 1.43  2005/05/12 22:07:52  dmorissette
 * Improved handling of Danish modified TM proj#21-24 (hss, bugs 976,1010)
 *
 * Revision 1.42  2005/03/22 23:24:54  dmorissette
 * Added support for datum id in .MAP header (bug 910)
 *
 * Revision 1.41  2004/10/11 20:50:04  dmorissette
 * 7 new datum defns, 1 fixed and list of ellipsoids updated (Bug 608,Uffe K.)
 *
 * Revision 1.40  2003/03/21 14:20:42  warmerda
 * fixed up regional mercator handling, was screwing up transverse mercator
 *
 * Revision 1.39  2002/12/19 20:46:01  warmerda
 * fixed spelling of Provisional_South_American_Datum_1956
 *
 * Revision 1.38  2002/12/12 20:12:18  warmerda
 * fixed signs of rotational parameters for TOWGS84 in WKT
 *
 * Revision 1.37  2002/10/15 14:33:30  warmerda
 * Added untested support in mitab_spatialref.cpp, and mitab_coordsys.cpp for
 * projections Regional Mercator (26), Polyconic (27), Azimuthal Equidistant -
 * All origin latitudes (28), and Lambert Azimuthal Equal Area - any aspect 
 * (29).
 *
 * Revision 1.36  2002/09/05 15:38:16  warmerda
 * one more ogc datum name
 *
 * Revision 1.35  2002/09/05 15:23:22  warmerda
 * added some EPSG datum names provided by Siro Martello @ Cadcorp
 *
 * Revision 1.34  2002/04/01 19:49:24  warmerda
 * added support for cassini/soldner - proj 30
 *
 * Revision 1.33  2002/03/01 19:00:15  warmerda
 * False Easting/Northing should be in the linear units of measure in MapInfo,
 * but in OGRSpatialReference/WKT they are always in meters.  Convert accordingly.
 *
 * Revision 1.32  2001/10/25 16:13:41  warmerda
 * Added OGC string for datum 12
 *
 * Revision 1.31  2001/08/10 21:25:59  warmerda
 * SetSpatialRef() now makes a clone of the srs instead of taking a ref to it
 *
 * Revision 1.30  2001/04/23 17:38:06  warmerda
 * fixed use of freed points bug for datum 999/9999
 *
 * Revision 1.29  2001/04/04 21:43:19  warmerda
 * added code to set WGS84 values
 *
 * Revision 1.28  2001/01/23 21:23:42  daniel
 * Added projection bounds lookup table, called from TABFile::SetProjInfo()
 *
 * Revision 1.27  2001/01/22 16:00:53  warmerda
 * reworked swiss projection support
 *
 * Revision 1.26  2001/01/19 21:56:18  warmerda
 * added untested support for Swiss Oblique Mercator
 *
 * Revision 1.25  2000/12/05 14:56:55  daniel
 * Added some missing unit names (aliases) in TABFile::SetSpatialRef()
 *
 * Revision 1.24  2000/10/16 21:44:50  warmerda
 * added nonearth support
 *
 * Revision 1.23  2000/10/16 18:01:20  warmerda
 * added check for NULL on passed in spatial ref
 *
 * Revision 1.22  2000/10/02 14:46:36  daniel
 * Added 7 parameter datums with id 1000+
 *
 * Revision 1.21  2000/09/29 22:09:18  daniel
 * Added new datums/ellipsoid from MapInfo V6.0
 *
 * Revision 1.20  2000/09/28 16:39:44  warmerda
 * avoid warnings for unused, and unitialized variables
 *
 * Revision 1.19  2000/02/07 17:43:17  daniel
 * Fixed offset in parsing of custom datum string in SetSpatialRef()
 *
 * Revision 1.18  2000/02/04 05:30:50  daniel
 * Fixed problem in GetSpatialRef() with szDatumName[] buffer size and added
 * use of an epsilon in comparing of datum parameters.
 *
 * Revision 1.17  2000/01/15 22:30:45  daniel
 * Switch to MIT/X-Consortium OpenSource license
 *
 * Revision 1.16  1999/12/21 20:01:47  warmerda
 * added support for DATUM 0
 *
 * Revision 1.15  1999/11/11 02:56:17  warmerda
 * fixed problems with stereographic
 *
 * Revision 1.14  1999/11/10 20:13:12  warmerda
 * implement spheroid table
 *
 * Revision 1.13  1999/11/09 22:31:38  warmerda
 * initial implementation of MIF CoordSys support
 *
 * Revision 1.12  1999/10/19 16:31:32  warmerda
 * Improved mile support.
 *
 * Revision 1.11  1999/10/19 16:27:50  warmerda
 * Added support for Mile (units=0).  Also added support for nonearth
 * projections.
 *
 * Revision 1.10  1999/10/05 18:56:08  warmerda
 * fixed lots of bugs with projection parameters
 *
 * Revision 1.9  1999/10/04 21:17:47  warmerda
 * Make sure that asDatumInfoList comparisons include the ellipsoid code.
 * Don't include OGC name for local NAD27 values.  Put NAD83 ahead of GRS80
 * so it will be used in preference even though they are identical parms.
 *
 * Revision 1.8  1999/10/04 19:46:42  warmerda
 * assorted changes, including rework of units
 *
 * Revision 1.7  1999/09/28 04:52:17  daniel
 * Added missing param in sprintf() format for szDatumName[]
 *
 * Revision 1.6  1999/09/28 02:51:46  warmerda
 * Added ellipsoid codes, and bulk of write implementation.
 *
 * Revision 1.5  1999/09/27 21:23:41  warmerda
 * added more projections
 *
 * Revision 1.4  1999/09/24 04:01:28  warmerda
 * remember nMIDatumId changes
 *
 * Revision 1.3  1999/09/23 19:51:38  warmerda
 * added datum mapping table support
 *
 * Revision 1.2  1999/09/22 23:04:59  daniel
 * Handle reference count on OGRSpatialReference properly
 *
 * Revision 1.1  1999/09/21 19:39:22  daniel
 * Moved Get/SetSpatialRef() to a separate file
 *
 **********************************************************************/

#include "mitab.h"

/* -------------------------------------------------------------------- */
/*      This table was automatically generated by doing translations    */
/*      between mif and tab for each datum, and extracting the          */
/*      parameters from the tab file.  The OGC names were added         */
/*      afterwards and may be incomplete or inaccurate.                 */
/* -------------------------------------------------------------------- */
MapInfoDatumInfo asDatumInfoList[] =
{

{104, "WGS_1984",                   28,0, 0, 0, 0, 0, 0, 0, 0},
{74,  "North_American_Datum_1983",  0, 0, 0, 0, 0, 0, 0, 0, 0},

{0,  "",                            29, 0,   0,    0,   0, 0, 0, 0, 0}, // Datum ignore

{1,  "Adindan",                     6, -162, -12,  206, 0, 0, 0, 0, 0},
{2,  "Afgooye",                     3, -43,  -163, 45,  0, 0, 0, 0, 0},
{3,  "Ain_el_Abd_1970",             4, -150, -251, -2,  0, 0, 0, 0, 0},
{4,  "Anna_1_Astro_1965",           2, -491, -22,  435, 0, 0, 0, 0, 0},
{5,  "Arc_1950",                    15,-143, -90,  -294,0, 0, 0, 0, 0},
{6,  "Arc_1960",                    6, -160, -8,   -300,0, 0, 0, 0, 0},
{7,  "Ascension_Islands",           4, -207, 107,  52,  0, 0, 0, 0, 0},
{8,  "Astro_Beacon_E",              4, 145,  75,   -272,0, 0, 0, 0, 0},
{9,  "Astro_B4_Sorol_Atoll",        4, 114,  -116, -333,0, 0, 0, 0, 0},
{10, "Astro_Dos_71_4",              4, -320, 550,  -494,0, 0, 0, 0, 0},
{11, "Astronomic_Station_1952",     4, 124,  -234, -25, 0, 0, 0, 0, 0},
{12, "Australian_Geodetic_Datum_66",2, -133, -48,  148, 0, 0, 0, 0, 0},
{13, "Australian_Geodetic_Datum_84",2, -134, -48,  149, 0, 0, 0, 0, 0},
{14, "Bellevue_Ign",                4, -127, -769, 472, 0, 0, 0, 0, 0},
{15, "Bermuda_1957",                7, -73,  213,  296, 0, 0, 0, 0, 0},
{16, "Bogota",                      4, 307,  304,  -318,0, 0, 0, 0, 0},
{17, "Campo_Inchauspe",             4, -148, 136,  90,  0, 0, 0, 0, 0},
{18, "Canton_Astro_1966",           4, 298,  -304, -375,0, 0, 0, 0, 0},
{19, "Cape",                        6, -136, -108, -292,0, 0, 0, 0, 0},
{20, "Cape_Canaveral",              7, -2,   150,  181, 0, 0, 0, 0, 0},
{21, "Carthage",                    6, -263, 6,    431, 0, 0, 0, 0, 0},
{22, "Chatham_1971",                4, 175,  -38,  113, 0, 0, 0, 0, 0},
{23, "Chua",                        4, -134, 229,  -29, 0, 0, 0, 0, 0},
{24, "Corrego_Alegre",              4, -206, 172,  -6,  0, 0, 0, 0, 0},
{25, "Batavia",                     10,-377,681,   -50, 0, 0, 0, 0, 0},
{26, "Dos_1968",                    4, 230,  -199, -752,0, 0, 0, 0, 0},
{27, "Easter_Island_1967",          4, 211,  147,  111, 0, 0, 0, 0, 0},
{28, "European_Datum_1950",         4, -87,  -98,  -121,0, 0, 0, 0, 0},
{29, "European_Datum_1979",         4, -86,  -98,  -119,0, 0, 0, 0, 0},
{30, "Gandajika_1970",              4, -133, -321, 50,  0, 0, 0, 0, 0},
{31, "New_Zealand_GD49",            4, 84,   -22,  209, 0, 0, 0, 0, 0},
{31, "New_Zealand_Geodetic_Datum_1949",4,84, -22,  209, 0, 0, 0, 0, 0},
{32, "GRS_67",                      21,0,    0,    0,   0, 0, 0, 0, 0},
{33, "GRS_80",                      0, 0,    0,    0,   0, 0, 0, 0, 0},
{34, "Guam_1963",                   7, -100, -248, 259, 0, 0, 0, 0, 0},
{35, "Gux_1_Astro",                 4, 252,  -209, -751,0, 0, 0, 0, 0},
{36, "Hito_XVIII_1963",             4, 16,   196,  93,  0, 0, 0, 0, 0},
{37, "Hjorsey_1955",                4, -73,  46,   -86, 0, 0, 0, 0, 0},
{38, "Hong_Kong_1963",              4, -156, -271, -189,0, 0, 0, 0, 0},
{39, "Hu_Tzu_Shan",                 4, -634, -549, -201,0, 0, 0, 0, 0},
{40, "Indian_Thailand_Vietnam",     11,214,  836,  303, 0, 0, 0, 0, 0},
{41, "Indian_Bangladesh",           11,289,  734,  257, 0, 0, 0, 0, 0},
{42, "Ireland_1965",                13,506,  -122, 611, 0, 0, 0, 0, 0},
{43, "ISTS_073_Astro_1969",         4, 208,  -435, -229,0, 0, 0, 0, 0},
{44, "Johnston_Island_1961",        4, 191,  -77,  -204,0, 0, 0, 0, 0},
{45, "Kandawala",                   11,-97,  787,  86,  0, 0, 0, 0, 0},
{46, "Kerguyelen_Island",           4, 145,  -187, 103, 0, 0, 0, 0, 0},
{47, "Kertau",                      17,-11,  851,  5,   0, 0, 0, 0, 0},
{48, "L_C_5_Astro",                 7, 42,   124,  147, 0, 0, 0, 0, 0},
{49, "Liberia_1964",                6, -90,  40,   88,  0, 0, 0, 0, 0},
{50, "Luzon_Phillippines",          7, -133, -77,  -51, 0, 0, 0, 0, 0},
{51, "Luzon_Mindanao_Island",       7, -133, -79,  -72, 0, 0, 0, 0, 0},
{52, "Mahe_1971",                   6, 41,   -220, -134,0, 0, 0, 0, 0},
{53, "Marco_Astro",                 4, -289, -124, 60,  0, 0, 0, 0, 0},
{54, "Massawa",                     10,639,  405,  60,  0, 0, 0, 0, 0},
{55, "Merchich",                    16,31,   146,  47,  0, 0, 0, 0, 0},
{56, "Midway_Astro_1961",           4, 912,  -58,  1227,0, 0, 0, 0, 0},
{57, "Minna",                       6, -92,  -93,  122, 0, 0, 0, 0, 0},
{58, "Nahrwan_Masirah_Island",      6, -247, -148, 369, 0, 0, 0, 0, 0},
{59, "Nahrwan_Un_Arab_Emirates",    6, -249, -156, 381, 0, 0, 0, 0, 0},
{60, "Nahrwan_Saudi_Arabia",        6, -231, -196, 482, 0, 0, 0, 0, 0},
{61, "Naparima_1972",               4, -2,   374,  172, 0, 0, 0, 0, 0},
{62, "NAD_1927",                    7, -8,   160,  176, 0, 0, 0, 0, 0},
{62, "North_American_Datum_1927",   7, -8,   160,  176, 0, 0, 0, 0, 0},
{63, "NAD_27_Alaska",               7, -5,   135,  172, 0, 0, 0, 0, 0},
{64, "NAD_27_Bahamas",              7, -4,   154,  178, 0, 0, 0, 0, 0},
{65, "NAD_27_San_Salvador",         7, 1,    140,  165, 0, 0, 0, 0, 0},
{66, "NAD_27_Canada",               7, -10,  158,  187, 0, 0, 0, 0, 0},
{67, "NAD_27_Canal_Zone",           7, 0,    125,  201, 0, 0, 0, 0, 0},
{68, "NAD_27_Caribbean",            7, -7,   152,  178, 0, 0, 0, 0, 0},
{69, "NAD_27_Central_America",      7, 0,    125,  194, 0, 0, 0, 0, 0},
{70, "NAD_27_Cuba",                 7, -9,   152,  178, 0, 0, 0, 0, 0},
{71, "NAD_27_Greenland",            7, 11,   114,  195, 0, 0, 0, 0, 0},
{72, "NAD_27_Mexico",               7, -12,  130,  190, 0, 0, 0, 0, 0},
{73, "NAD_27_Michigan",             8, -8,   160,  176, 0, 0, 0, 0, 0},
{75, "Observatorio_1966",           4, -425, -169, 81,  0, 0, 0, 0, 0},
{76, "Old_Egyptian",                22,-130, 110, -13,  0, 0, 0, 0, 0},
{77, "Old_Hawaiian",                7, 61,   -285, -181,0, 0, 0, 0, 0},
{78, "Oman",                        6, -346, -1,   224, 0, 0, 0, 0, 0},
{79, "OSGB_1936",                   9, 375,  -111, 431, 0, 0, 0, 0, 0},
{80, "Pico_De_Las_Nieves",          4, -307, -92,  127, 0, 0, 0, 0, 0},
{81, "Pitcairn_Astro_1967",         4, 185,  165,  42,  0, 0, 0, 0, 0},
{82, "Provisional_South_American",  4, -288, 175,  -376,0, 0, 0, 0, 0},
{83, "Puerto_Rico",                 7, 11,   72,   -101,0, 0, 0, 0, 0},
{84, "Qatar_National",              4, -128, -283, 22,  0, 0, 0, 0, 0},
{85, "Qornoq",                      4, 164,  138, -189, 0, 0, 0, 0, 0},
{86, "Reunion",                     4, 94,   -948,-1262,0, 0, 0, 0, 0},
{87, "Monte_Mario",                 4, -225, -65, 9,    0, 0, 0, 0, 0},
{88, "Santo_Dos",                   4, 170,  42,  84,   0, 0, 0, 0, 0},
{89, "Sao_Braz",                    4, -203, 141, 53,   0, 0, 0, 0, 0},
{90, "Sapper_Hill_1943",            4, -355, 16,  74,   0, 0, 0, 0, 0},
{91, "Schwarzeck",                  14,616,  97,  -251, 0, 0, 0, 0, 0},
{92, "South_American_Datum_1969",   24,-57,  1,   -41,  0, 0, 0, 0, 0},
{93, "South_Asia",                  19,7,    -10, -26,  0, 0, 0, 0, 0},
{94, "Southeast_Base",              4, -499, -249,314,  0, 0, 0, 0, 0},
{95, "Southwest_Base",              4, -104, 167, -38,  0, 0, 0, 0, 0},
{96, "Timbalai_1948",               11,-689, 691, -46,  0, 0, 0, 0, 0},
{97, "Tokyo",                       10,-128, 481, 664,  0, 0, 0, 0, 0},
{98, "Tristan_Astro_1968",          4, -632, 438, -609, 0, 0, 0, 0, 0},
{99, "Viti_Levu_1916",              6, 51,   391, -36,  0, 0, 0, 0, 0},
{100, "Wake_Entiwetok_1960",        23,101,  52,  -39,  0, 0, 0, 0, 0},
{101, "WGS_60",                     26,0,    0,   0,    0, 0, 0, 0, 0},
{102, "WGS_66",                     27,0,    0,   0,    0, 0, 0, 0, 0},
{103, "WGS_1972",                   1, 0,    8,   10,   0, 0, 0, 0, 0},
{104, "WGS_1984",                   28,0,    0,   0,    0, 0, 0, 0, 0},
{105, "Yacare",                     4, -155, 171, 37,   0, 0, 0, 0, 0},
{106, "Zanderij",                   4, -265, 120, -358, 0, 0, 0, 0, 0},
{107, "NTF",                        30,-168, -60, 320,  0, 0, 0, 0, 0},
{108, "European_Datum_1987",        4, -83,  -96, -113, 0, 0, 0, 0, 0},
{109, "Netherlands_Bessel",         10,593,  26,  478,  0, 0, 0, 0, 0},
{110, "Belgium_Hayford",            4, 81,   120, 129,  0, 0, 0, 0, 0},
{111, "NWGL_10",                    1, -1,   15,  1,    0, 0, 0, 0, 0},
{112, "Rikets_koordinatsystem_1990",10,498,  -36, 568,  0, 0, 0, 0, 0},
{113, "Lisboa_DLX",                 4, -303, -62, 105,  0, 0, 0, 0, 0},
{114, "Melrica_1973_D73",           4, -223, 110, 37,   0, 0, 0, 0, 0},
{115, "Euref_98",                   0, 0,    0,   0,    0, 0, 0, 0, 0},
{116, "GDA94",                      0, 0,    0,   0,    0, 0, 0, 0, 0},
{117, "NZGD2000",                   0, 0,    0,   0,    0, 0, 0, 0, 0},
{117, "New_Zealand_Geodetic_Datum_2000",0,0, 0,   0,    0, 0, 0, 0, 0},
{118, "America_Samoa",              7, -115, 118, 426,  0, 0, 0, 0, 0},
{119, "Antigua_Astro_1965",         6, -270, 13,  62,   0, 0, 0, 0, 0},
{120, "Ayabelle_Lighthouse",        6, -79, -129, 145,  0, 0, 0, 0, 0},
{121, "Bukit_Rimpah",               10,-384, 664, -48,  0, 0, 0, 0, 0},
{122, "Estonia_1937",               10,374, 150,  588,  0, 0, 0, 0, 0},
{123, "Dabola",                     6, -83, 37,   124,  0, 0, 0, 0, 0},
{124, "Deception_Island",           6, 260, 12,   -147, 0, 0, 0, 0, 0},
{125, "Fort_Thomas_1955",           6, -7, 215,   225,  0, 0, 0, 0, 0},
{126, "Graciosa_base_1948",         4, -104, 167, -38,  0, 0, 0, 0, 0},
{127, "Herat_North",                4, -333, -222,114,  0, 0, 0, 0, 0},
{128, "Hermanns_Kogel",             10,682, -203, 480,  0, 0, 0, 0, 0},
{129, "Indian",                     50,283, 682,  231,  0, 0, 0, 0, 0},
{130, "Indian_1954",                11,217, 823,  299,  0, 0, 0, 0, 0},
{131, "Indian_1960",                11,198, 881,  317,  0, 0, 0, 0, 0},
{132, "Indian_1975",                11,210, 814,  289,  0, 0, 0, 0, 0},
{133, "Indonesian_Datum_1974",      4, -24, -15,  5,    0, 0, 0, 0, 0},
{134, "ISTS061_Astro_1968",         4, -794, 119, -298, 0, 0, 0, 0, 0},
{135, "Kusaie_Astro_1951",          4, 647, 1777, -1124,0, 0, 0, 0, 0},
{136, "Leigon",                     6, -130, 29,  364,  0, 0, 0, 0, 0},
{137, "Montserrat_Astro_1958",      6, 174, 359,  365,  0, 0, 0, 0, 0},
{138, "Mporaloko",                  6, -74, -130, 42,   0, 0, 0, 0, 0},
{139, "North_Sahara_1959",          6, -186, -93, 310,  0, 0, 0, 0, 0},
{140, "Observatorio_Met_1939",      4, -425, -169,81,   0, 0, 0, 0, 0},
{141, "Point_58",                   6, -106, -129,165,  0, 0, 0, 0, 0},
{142, "Pointe_Noire",               6, -148, 51,  -291, 0, 0, 0, 0, 0},
{143, "Porto_Santo_1936",           4, -499, -249,314,  0, 0, 0, 0, 0},
{144, "Selvagem_Grande_1938",       4, -289, -124,60,   0, 0, 0, 0, 0},
{145, "Sierra_Leone_1960",          6, -88,  4,   101,  0, 0, 0, 0, 0},
{146, "S_JTSK_Ferro",               10, 589, 76,  480,  0, 0, 0, 0, 0},
{147, "Tananarive_1925",            4, -189, -242,-91,  0, 0, 0, 0, 0},
{148, "Voirol_1874",                6, -73,  -247,227,  0, 0, 0, 0, 0},
{149, "Virol_1960",                 6, -123, -206,219,  0, 0, 0, 0, 0},
{150, "Hartebeesthoek94",           0, 0,    0,   0,    0, 0, 0, 0, 0},
{151, "ATS77",                      51, 0, 0, 0, 0, 0, 0, 0, 0},
{152, "JGD2000",                    0, 0, 0, 0, 0, 0, 0, 0, 0},
{157, "WGS_1984",                   54,0, 0, 0, 0, 0, 0, 0, 0}, // Google merc
{1000,"DHDN_Potsdam_Rauenberg",     10,582,  105, 414, -1.04, -0.35, 3.08, 8.3, 0},
{1001,"Pulkovo_1942",               3, 24,   -123, -94, -0.02, 0.25, 0.13, 1.1, 0},
{1002,"NTF_Paris_Meridian",         30,-168, -60, 320, 0, 0, 0, 0, 2.337229166667},
{1003,"Switzerland_CH_1903",        10,660.077,13.551, 369.344, 0.804816, 0.577692, 0.952236, 5.66,0},
{1004,"Hungarian_Datum_1972",       21,-56,  75.77, 15.31, -0.37, -0.2, -0.21, -1.01, 0},
{1005,"Cape_7_Parameter",           28,-134.73,-110.92, -292.66, 0, 0, 0, 1, 0},
{1006,"AGD84_7_Param_Aust",         2, -117.763,-51.51, 139.061, -0.292, -0.443, -0.277, -0.191, 0},
{1007,"AGD66_7_Param_ACT",          2, -129.193,-41.212, 130.73, -0.246, -0.374, -0.329, -2.955, 0},
{1008,"AGD66_7_Param_TAS",          2, -120.271,-64.543, 161.632, -0.2175, 0.0672, 0.1291, 2.4985, 0},
{1009,"AGD66_7_Param_VIC_NSW",      2, -119.353,-48.301, 139.484, -0.415, -0.26, -0.437, -0.613, 0},
{1010,"NZGD_7_Param_49",            4, 59.47, -5.04, 187.44, -0.47, 0.1, -1.024, -4.5993, 0},
{1011,"Rikets_Tri_7_Param_1990",    10,419.3836, 99.3335, 591.3451, -0.850389, -1.817277, 7.862238, -0.99496, 0},
{1012,"Russia_PZ90",                52, -1.08,-0.27,-0.9,0, 0, -0.16,-0.12, 0},
{1013,"Russia_SK42",                52, 23.92,-141.27,-80.9, 0, -0.35,-0.82, -0.12, 0},
{1014,"Russia_SK95",                52, 24.82,-131.21,-82.66,0,0,-0.16,-0.12, 0},
{1015,"Tokyo",                      10, -146.414, 507.337, 680.507,0,0,0,0,0},
{1016,"Finnish_KKJ",                4, -96.062, -82.428, -121.754, -4.801, -0.345, 1.376, 1.496, 0},
{1017,"Xian 1980",					53, 24, -123, -94, -0.02, -0.25, 0.13, 1.1, 0},
{1018,"Lithuanian Pulkovo 1942",	4, -40.59527, -18.54979, -69.33956, -2.508, -1.8319, 2.6114, -4.2991, 0},
{1019,"Belgian 1972 7 Parameter",   4, -99.059, 53.322, -112.486, -0.419, 0.83, -1.885, 0.999999, 0},
{1020,"S-JTSK with Ferro prime meridian", 10, 589, 76, 480, 0, 0, 0, 0, -17.666666666667}, 

{-1, NULL,                          0, 0, 0, 0, 0, 0, 0, 0, 0}
};

/* -------------------------------------------------------------------- */
/*      This table was hand entered from Appendix I of the mapinfo 6    */
/*      manuals.                                                        */
/* -------------------------------------------------------------------- */

MapInfoSpheroidInfo asSpheroidInfoList[] =
{
{ 9,"Airy 1930",                                6377563.396,    299.3249646},
{13,"Airy 1930 (modified for Ireland 1965",     6377340.189,    299.3249646},
{51,"ATS77 (Average Terrestrial System 1977)",  6378135,        298.257},
{ 2,"Australian",                               6378160.0,      298.25},
{10,"Bessel 1841",                              6377397.155,    299.1528128},
{35,"Bessel 1841 (modified for NGO 1948)",      6377492.0176,   299.15281},
{14,"Bessel 1841 (modified for Schwarzeck)",    6377483.865,    299.1528128},
{36,"Clarke 1858",                              6378293.639,    294.26068},
{ 7,"Clarke 1866",                              6378206.4,      294.9786982},
{ 8,"Clarke 1866 (modified for Michigan)",      6378450.047484481,294.9786982},
{ 6,"Clarke 1880",                              6378249.145,    293.465},
{15,"Clarke 1880 (modified for Arc 1950)",      6378249.145326, 293.4663076},
{30,"Clarke 1880 (modified for IGN)",           6378249.2,      293.4660213},
{37,"Clarke 1880 (modified for Jamaica)",       6378249.136,    293.46631},
{16,"Clarke 1880 (modified for Merchich)",      6378249.2,      293.46598},
{38,"Clarke 1880 (modified for Palestine)",     6378300.79,     293.46623},
{39,"Everest (Brunei and East Malaysia)",       6377298.556,    300.8017},
{11,"Everest (India 1830)",                     6377276.345,    300.8017},
{40,"Everest (India 1956)",                     6377301.243,    300.80174},
{50,"Everest (Pakistan)",                       6377309.613,    300.8017},
{17,"Everest (W. Malaysia and Singapore 1948)", 6377304.063,    300.8017},
{48,"Everest (West Malaysia 1969)",             6377304.063,    300.8017},
{18,"Fischer 1960",                             6378166.0,      298.3},
{19,"Fischer 1960 (modified for South Asia)",   6378155.0,      298.3},
{20,"Fischer 1968",                             6378150.0,      298.3},
{21,"GRS 67",                                   6378160.0,      298.247167427},
{ 0,"GRS 80",                                   6378137.0,      298.257222101},
{ 5,"Hayford",                                  6378388.0,      297.0},
{22,"Helmert 1906",                             6378200.0,      298.3},
{23,"Hough",                                    6378270.0,      297.0},
{31,"IAG 75",                                   6378140.0,      298.257222},
{41,"Indonesian",                               6378160.0,      298.247},
{ 4,"International 1924",                       6378388.0,      297.0},
{49,"Irish (WOFO)",                             6377542.178,    299.325},
{ 3,"Krassovsky",                               6378245.0,      298.3},
{32,"MERIT 83",                                 6378137.0,      298.257},
{33,"New International 1967",                   6378157.5,      298.25},
{42,"NWL 9D",                                   6378145.0,      298.25},
{43,"NWL 10D",                                  6378135.0,      298.26},
{44,"OSU86F",                                   6378136.2,      298.25722},
{45,"OSU91A",                                   6378136.3,      298.25722},
{46,"Plessis 1817",                             6376523.0,      308.64},
{52,"PZ90",                                     6378136.0,      298.257839303},
{24,"South American",                           6378160.0,      298.25},
{12,"Sphere",                                   6370997.0,      0.0},
{47,"Struve 1860",                              6378297.0,      294.73},
{34,"Walbeck",                                  6376896.0,      302.78},
{25,"War Office",                               6378300.583,    296.0},
{26,"WGS 60",                                   6378165.0,      298.3},
{27,"WGS 66",                                   6378145.0,      298.25},
{ 1,"WGS 72",                                   6378135.0,      298.26},
{28,"WGS 84",                                   6378137.0,      298.257223563},
{29,"WGS 84 (MAPINFO Datum 0)",                 6378137.01,     298.257223563},
{54,"WGS 84 (MAPINFO Datum 157)",               6378137.01,     298.257223563},
{-1,NULL,                                       0.0,            0.0}
};
 
/**********************************************************************
 *                   TABFile::GetSpatialRef()
 *
 * Returns a reference to an OGRSpatialReference for this dataset.
 * If the projection parameters have not been parsed yet, then we will
 * parse them before returning.
 *
 * The returned object is owned and maintained by this TABFile and
 * should not be modified or freed by the caller.
 *
 * Returns NULL if the SpatialRef cannot be accessed.
 **********************************************************************/
OGRSpatialReference *TABFile::GetSpatialRef()
{
    if (m_eAccessMode != TABRead)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GetSpatialRef() can be used only with Read access.");
        return NULL;
    }
 
    if (m_poMAPFile == NULL )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "GetSpatialRef() failed: file has not been opened yet.");
        return NULL;
    }

    /*-----------------------------------------------------------------
     * If projection params have already been processed, just use them.
     *----------------------------------------------------------------*/
    if (m_poSpatialRef != NULL)
        return m_poSpatialRef;
    

    /*-----------------------------------------------------------------
     * Fetch the parameters from the header.
     *----------------------------------------------------------------*/
    TABMAPHeaderBlock *poHeader;
    TABProjInfo     sTABProj;

    if ((poHeader = m_poMAPFile->GetHeaderBlock()) == NULL ||
        poHeader->GetProjInfo( &sTABProj ) != 0)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "GetSpatialRef() failed reading projection parameters.");
        return NULL;
    }

    /*-----------------------------------------------------------------
     * Get the units name, and translation factor.
     *----------------------------------------------------------------*/
    const char *pszUnitsName; 
    const char *pszUnitsConv;
    double      dfConv = 1.0;

    switch( sTABProj.nUnitsId )
    {
      case 0:
        pszUnitsName = "Mile";
        pszUnitsConv = "1609.344";
        break;

      case 1:
        pszUnitsName = "Kilometer";
        pszUnitsConv = "1000.0";
        break;
            
      case 2:
        pszUnitsName = "IINCH";
        pszUnitsConv = "0.0254";
        break;
            
      case 3:
        pszUnitsName = SRS_UL_FOOT;
        pszUnitsConv = SRS_UL_FOOT_CONV;
        break;
            
      case 4:
        pszUnitsName = "IYARD";
        pszUnitsConv = "0.9144";
        break;
            
      case 5:
        pszUnitsName = "Millimeter";
        pszUnitsConv = "0.001";
        break;
            
      case 6:
        pszUnitsName = "Centimeter";
        pszUnitsConv = "0.01";
        break;
            
      case 7:
        pszUnitsName = SRS_UL_METER;
        pszUnitsConv = "1.0";
        break;
            
      case 8:
        pszUnitsName = SRS_UL_US_FOOT;
        pszUnitsConv = SRS_UL_US_FOOT_CONV;
        break;
            
      case 9:
        pszUnitsName = SRS_UL_NAUTICAL_MILE;
        pszUnitsConv = SRS_UL_NAUTICAL_MILE_CONV;
        break;
            
      case 30:
        pszUnitsName = SRS_UL_LINK;
        pszUnitsConv = SRS_UL_LINK_CONV;
        break;
            
      case 31:
        pszUnitsName = SRS_UL_CHAIN;
        pszUnitsConv = SRS_UL_CHAIN_CONV;
        break;
            
      case 32:
        pszUnitsName = SRS_UL_ROD;
        pszUnitsConv = SRS_UL_ROD_CONV;
        break;
            
      default:
        pszUnitsName = SRS_UL_METER;
        pszUnitsConv = "1.0";
        break;
    }

    dfConv = CPLAtof(pszUnitsConv);

    /*-----------------------------------------------------------------
     * Transform them into an OGRSpatialReference.
     *----------------------------------------------------------------*/
    m_poSpatialRef = new OGRSpatialReference;

    /*-----------------------------------------------------------------
     * Handle the PROJCS style projections, but add the datum later.
     *----------------------------------------------------------------*/
    switch( sTABProj.nProjId )
    {
        /*--------------------------------------------------------------
         * NonEarth ... we return with an empty SpatialRef.  Eventually
         * we might want to include the units, but not for now.
         *-------------------------------------------------------------*/
      case 0:
        m_poSpatialRef->SetLocalCS( "Nonearth" );
        break;

        /*--------------------------------------------------------------
         * lat/long .. just add the GEOGCS later.
         *-------------------------------------------------------------*/
      case 1:
        break;

        /*--------------------------------------------------------------
         * Cylindrical Equal Area
         *-------------------------------------------------------------*/
      case 2:
        m_poSpatialRef->SetCEA( sTABProj.adProjParams[1],
                                sTABProj.adProjParams[0],
                                sTABProj.adProjParams[2],
                                sTABProj.adProjParams[3] );
        break;

        /*--------------------------------------------------------------
         * Lambert Conic Conformal
         *-------------------------------------------------------------*/
      case 3:
        m_poSpatialRef->SetLCC( sTABProj.adProjParams[2],
                                sTABProj.adProjParams[3],
                                sTABProj.adProjParams[1],
                                sTABProj.adProjParams[0],
                                sTABProj.adProjParams[4],
                                sTABProj.adProjParams[5] );
        break;

        /*--------------------------------------------------------------
         * Lambert Azimuthal Equal Area
         *-------------------------------------------------------------*/
      case 4:
      case 29:
        m_poSpatialRef->SetLAEA( sTABProj.adProjParams[1],
                                 sTABProj.adProjParams[0],
                                 0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Azimuthal Equidistant (Polar aspect only)
         *-------------------------------------------------------------*/
      case 5:
      case 28:
        m_poSpatialRef->SetAE( sTABProj.adProjParams[1],
                               sTABProj.adProjParams[0],
                               0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Equidistant Conic
         *-------------------------------------------------------------*/
      case 6:
        m_poSpatialRef->SetEC( sTABProj.adProjParams[2],
                               sTABProj.adProjParams[3],
                               sTABProj.adProjParams[1],
                               sTABProj.adProjParams[0],
                               sTABProj.adProjParams[4],
                               sTABProj.adProjParams[5] );
        break;

        /*--------------------------------------------------------------
         * Hotine Oblique Mercator
         *-------------------------------------------------------------*/
      case 7:
        m_poSpatialRef->SetHOM( sTABProj.adProjParams[1],
                                sTABProj.adProjParams[0], 
                                sTABProj.adProjParams[2],
                                90.0, 
                                sTABProj.adProjParams[3],
                                sTABProj.adProjParams[4],
                                sTABProj.adProjParams[5] );
        break;

        /*--------------------------------------------------------------
         * Transverse Mercator
         *-------------------------------------------------------------*/
      case 8:
        m_poSpatialRef->SetTM( sTABProj.adProjParams[1],
                               sTABProj.adProjParams[0],
                               sTABProj.adProjParams[2],
                               sTABProj.adProjParams[3],
                               sTABProj.adProjParams[4] );
        break;

        /*----------------------------------------------------------------
         * Transverse Mercator,(modified for Danish System 34 Jylland-Fyn)
         *---------------------------------------------------------------*/
      case 21:
         m_poSpatialRef->SetTMVariant( SRS_PT_TRANSVERSE_MERCATOR_MI_21,
                                       sTABProj.adProjParams[1],
                                       sTABProj.adProjParams[0],
                                       sTABProj.adProjParams[2],
                                       sTABProj.adProjParams[3],
                                       sTABProj.adProjParams[4] );
         break;

        /*--------------------------------------------------------------
         * Transverse Mercator,(modified for Danish System 34 Sjaelland)
         *-------------------------------------------------------------*/
      case 22:
         m_poSpatialRef->SetTMVariant( SRS_PT_TRANSVERSE_MERCATOR_MI_22,
                                       sTABProj.adProjParams[1],
                                       sTABProj.adProjParams[0],
                                       sTABProj.adProjParams[2],
                                       sTABProj.adProjParams[3],
                                       sTABProj.adProjParams[4] );
         break;

        /*----------------------------------------------------------------
         * Transverse Mercator,(modified for Danish System 34/45 Bornholm)
         *---------------------------------------------------------------*/
      case 23:
         m_poSpatialRef->SetTMVariant( SRS_PT_TRANSVERSE_MERCATOR_MI_23,
                                       sTABProj.adProjParams[1],
                                       sTABProj.adProjParams[0],
                                       sTABProj.adProjParams[2],
                                       sTABProj.adProjParams[3],
                                       sTABProj.adProjParams[4] );
         break;

        /*--------------------------------------------------------------
         * Transverse Mercator,(modified for Finnish KKJ)
         *-------------------------------------------------------------*/
      case 24:
         m_poSpatialRef->SetTMVariant( SRS_PT_TRANSVERSE_MERCATOR_MI_24,
                                       sTABProj.adProjParams[1],
                                       sTABProj.adProjParams[0],
                                       sTABProj.adProjParams[2],
                                       sTABProj.adProjParams[3],
                                       sTABProj.adProjParams[4] );
         break;

        /*--------------------------------------------------------------
         * Albers Conic Equal Area
         *-------------------------------------------------------------*/
      case 9:
        m_poSpatialRef->SetACEA( sTABProj.adProjParams[2],
                                 sTABProj.adProjParams[3],
                                 sTABProj.adProjParams[1],
                                 sTABProj.adProjParams[0],
                                 sTABProj.adProjParams[4],
                                 sTABProj.adProjParams[5] );
        break;

        /*--------------------------------------------------------------
         * Mercator
         *-------------------------------------------------------------*/
      case 10:
        m_poSpatialRef->SetMercator( 0.0, sTABProj.adProjParams[0],
                                     1.0, 0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Miller Cylindrical
         *-------------------------------------------------------------*/
      case 11:
        m_poSpatialRef->SetMC( 0.0, sTABProj.adProjParams[0],
                               0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Robinson
         *-------------------------------------------------------------*/
      case 12:
        m_poSpatialRef->SetRobinson( sTABProj.adProjParams[0],
                                     0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Mollweide
         *-------------------------------------------------------------*/
      case 13:
        m_poSpatialRef->SetMollweide( sTABProj.adProjParams[0],
                                      0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Eckert IV
         *-------------------------------------------------------------*/
      case 14:
        m_poSpatialRef->SetEckertIV( sTABProj.adProjParams[0], 0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Eckert VI
         *-------------------------------------------------------------*/
      case 15:
        m_poSpatialRef->SetEckertVI( sTABProj.adProjParams[0], 0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Sinusoidal
         *-------------------------------------------------------------*/
      case 16:
        m_poSpatialRef->SetSinusoidal( sTABProj.adProjParams[0],
                                       0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Gall Stereographic
         *-------------------------------------------------------------*/
      case 17:
        m_poSpatialRef->SetGS( sTABProj.adProjParams[0], 0.0, 0.0 );
        break;
        
        /*--------------------------------------------------------------
         * New Zealand Map Grid
         *-------------------------------------------------------------*/
      case 18:
        m_poSpatialRef->SetNZMG( sTABProj.adProjParams[1],
                                 sTABProj.adProjParams[0],
                                 sTABProj.adProjParams[2],
                                 sTABProj.adProjParams[3] );
        break;

        /*--------------------------------------------------------------
         * Lambert Conic Conformal (Belgium)
         *-------------------------------------------------------------*/
      case 19:
        m_poSpatialRef->SetLCCB( sTABProj.adProjParams[2],
                                 sTABProj.adProjParams[3],
                                 sTABProj.adProjParams[1],
                                 sTABProj.adProjParams[0],
                                 sTABProj.adProjParams[4],
                                 sTABProj.adProjParams[5] );
        break;

        /*--------------------------------------------------------------
         * Stereographic
         *-------------------------------------------------------------*/
      case 20:
      case 31: /* this is called Double Stereographic, whats the diff? */
        m_poSpatialRef->SetStereographic( sTABProj.adProjParams[1],
                                          sTABProj.adProjParams[0],
                                          sTABProj.adProjParams[2],
                                          sTABProj.adProjParams[3],
                                          sTABProj.adProjParams[4] );
        break;

        /*--------------------------------------------------------------
         * Swiss Oblique Mercator / Cylindrical
         *-------------------------------------------------------------*/
      case 25:
        m_poSpatialRef->SetSOC( sTABProj.adProjParams[1],
                                sTABProj.adProjParams[0],
                                sTABProj.adProjParams[2],
                                sTABProj.adProjParams[3] );
        break;

        /*--------------------------------------------------------------
         * Regional Mercator (regular mercator with a latitude).
         *-------------------------------------------------------------*/
      case 26:
        m_poSpatialRef->SetMercator( sTABProj.adProjParams[1],
                                     sTABProj.adProjParams[0],
                                     1.0, 0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Polyconic
         *-------------------------------------------------------------*/
      case 27:
        m_poSpatialRef->SetPolyconic( sTABProj.adProjParams[1],
                                      sTABProj.adProjParams[0],
                                      sTABProj.adProjParams[2],
                                      sTABProj.adProjParams[3] );
        break;

        /*--------------------------------------------------------------
         * Cassini/Soldner
         *-------------------------------------------------------------*/
      case 30:
        m_poSpatialRef->SetCS( sTABProj.adProjParams[1],
                               sTABProj.adProjParams[0],
                               sTABProj.adProjParams[2],
                               sTABProj.adProjParams[3] );
        break;

     /*--------------------------------------------------------------
      * Krovak
      *-------------------------------------------------------------*/
      case 32:
        m_poSpatialRef->SetKrovak( sTABProj.adProjParams[1],   // dfCenterLat
                                   sTABProj.adProjParams[0],   // dfCenterLong
                                   sTABProj.adProjParams[3],   // dfAzimuth
                                   sTABProj.adProjParams[2],   // dfPseudoStdParallelLat
                                   1.0,					     // dfScale
                                   sTABProj.adProjParams[4],   // dfFalseEasting
                                   sTABProj.adProjParams[5] ); // dfFalseNorthing
        break;

      default:
        break;
    }

    /*-----------------------------------------------------------------
     * Collect units definition.
     *----------------------------------------------------------------*/
    if( sTABProj.nProjId != 1 && m_poSpatialRef->GetRoot() != NULL )
    {
        OGR_SRSNode     *poUnits = new OGR_SRSNode("UNIT");
        
        m_poSpatialRef->GetRoot()->AddChild(poUnits);

        poUnits->AddChild( new OGR_SRSNode( pszUnitsName ) );
        poUnits->AddChild( new OGR_SRSNode( pszUnitsConv ) );
    }

    /*-----------------------------------------------------------------
     * Local (nonearth) coordinate systems have no Geographic relationship
     * so we just return from here. 
     *----------------------------------------------------------------*/
    if( sTABProj.nProjId == 0 )
        return m_poSpatialRef;

    /*-----------------------------------------------------------------
     * Set the datum.  We are only given the X, Y and Z shift for
     * the datum, so for now we just synthesize a name from this.
     * It would be better if we could lookup a name based on the shift.
     *
     * Since we have already encountered files in which adDatumParams[] values
     * were in the order of 1e-150 when they should have actually been zeros,
     * we will use an epsilon in our scan instead of looking for equality.
     *----------------------------------------------------------------*/
#define TAB_EQUAL(a, b) (((a)<(b) ? ((b)-(a)) : ((a)-(b))) < 1e-10)
    char        szDatumName[160];
    int         iDatumInfo;
    MapInfoDatumInfo *psDatumInfo = NULL;

    for( iDatumInfo = 0;
         asDatumInfoList[iDatumInfo].nMapInfoDatumID != -1;
         iDatumInfo++ )
    {
        psDatumInfo = asDatumInfoList + iDatumInfo;
        
        if( TAB_EQUAL(psDatumInfo->nEllipsoid, sTABProj.nEllipsoidId) &&
            ((sTABProj.nDatumId > 0 && 
              sTABProj.nDatumId == psDatumInfo->nMapInfoDatumID) ||
             (sTABProj.nDatumId <= 0
              && TAB_EQUAL(psDatumInfo->dfShiftX, sTABProj.dDatumShiftX)
              && TAB_EQUAL(psDatumInfo->dfShiftY, sTABProj.dDatumShiftY)
              && TAB_EQUAL(psDatumInfo->dfShiftZ, sTABProj.dDatumShiftZ)
              && TAB_EQUAL(psDatumInfo->dfDatumParm0,sTABProj.adDatumParams[0])
              && TAB_EQUAL(psDatumInfo->dfDatumParm1,sTABProj.adDatumParams[1])
              && TAB_EQUAL(psDatumInfo->dfDatumParm2,sTABProj.adDatumParams[2])
              && TAB_EQUAL(psDatumInfo->dfDatumParm3,sTABProj.adDatumParams[3])
              && TAB_EQUAL(psDatumInfo->dfDatumParm4,sTABProj.adDatumParams[4]))))
            break;

        psDatumInfo = NULL;
    }

    if( psDatumInfo == NULL )
    {
        if( sTABProj.adDatumParams[0] == 0.0
            && sTABProj.adDatumParams[1] == 0.0
            && sTABProj.adDatumParams[2] == 0.0
            && sTABProj.adDatumParams[3] == 0.0
            && sTABProj.adDatumParams[4] == 0.0 )
        {
            sprintf( szDatumName,
                     "MIF 999,%d,%.15g,%.15g,%.15g", 
                     sTABProj.nEllipsoidId,
                     sTABProj.dDatumShiftX, 
                     sTABProj.dDatumShiftY, 
                     sTABProj.dDatumShiftZ );
        }
        else
        {
            sprintf( szDatumName,
                     "MIF 9999,%d,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g",
                     sTABProj.nEllipsoidId,
                     sTABProj.dDatumShiftX, 
                     sTABProj.dDatumShiftY, 
                     sTABProj.dDatumShiftZ,
                     sTABProj.adDatumParams[0],
                     sTABProj.adDatumParams[1],
                     sTABProj.adDatumParams[2],
                     sTABProj.adDatumParams[3],
                     sTABProj.adDatumParams[4] );
        }
    }
    else if( strlen(psDatumInfo->pszOGCDatumName) > 0 )
    {
        strncpy( szDatumName, psDatumInfo->pszOGCDatumName,
                 sizeof(szDatumName) );
    }
    else
    {
        sprintf( szDatumName, "MIF %d", psDatumInfo->nMapInfoDatumID );
    }

    /*-----------------------------------------------------------------
     * Set the spheroid.
     *----------------------------------------------------------------*/
    double      dfSemiMajor=0.0, dfInvFlattening=0.0;
    const char *pszSpheroidName = NULL;

    for( int i = 0; asSpheroidInfoList[i].nMapInfoId != -1; i++ )
    {
        if( asSpheroidInfoList[i].nMapInfoId == sTABProj.nEllipsoidId )
        {
            dfSemiMajor = asSpheroidInfoList[i].dfA;
            dfInvFlattening = asSpheroidInfoList[i].dfInvFlattening;
            pszSpheroidName = asSpheroidInfoList[i].pszMapinfoName;
            break;
        }
    }

    // use WGS 84 if nothing is known.
    if( pszSpheroidName == NULL )
    {
        pszSpheroidName = "unknown";
        dfSemiMajor = 6378137.0;
        dfInvFlattening = 298.257223563;
    }

    /*-----------------------------------------------------------------
     * Set the prime meridian.
     *----------------------------------------------------------------*/
    double      dfPMOffset = 0.0;
    const char *pszPMName = "Greenwich";
    
    if( sTABProj.adDatumParams[4] != 0.0 )
    {
        dfPMOffset = sTABProj.adDatumParams[4];

        pszPMName = "non-Greenwich";
    }
                    
    /*-----------------------------------------------------------------
     * Create a GEOGCS definition.
     *----------------------------------------------------------------*/

    m_poSpatialRef->SetGeogCS( "unnamed",
                               szDatumName,
                               pszSpheroidName,
                               dfSemiMajor, dfInvFlattening,
                               pszPMName, dfPMOffset,
                               SRS_UA_DEGREE, CPLAtof(SRS_UA_DEGREE_CONV));

    if( psDatumInfo != NULL )
    {
        m_poSpatialRef->SetTOWGS84( psDatumInfo->dfShiftX, 
                                    psDatumInfo->dfShiftY,
                                    psDatumInfo->dfShiftZ,
                                    -psDatumInfo->dfDatumParm0, 
                                    -psDatumInfo->dfDatumParm1, 
                                    -psDatumInfo->dfDatumParm2, 
                                    psDatumInfo->dfDatumParm3 );
    }

    /*-----------------------------------------------------------------
     * Special case for Google Mercator (datum=157, ellipse=54, gdal #4115)
     *----------------------------------------------------------------*/
    if( sTABProj.nProjId == 10 
        && sTABProj.nDatumId == 157
        && sTABProj.nEllipsoidId == 54 )
    {
        m_poSpatialRef->SetNode( "PROJCS", "WGS 84 / Pseudo-Mercator" );
        m_poSpatialRef->SetExtension( "PROJCS", "PROJ4", "+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext  +no_defs" );
    }

    return m_poSpatialRef;
}

/**********************************************************************
 *                   TABFile::SetSpatialRef()
 *
 * Set the OGRSpatialReference for this dataset.
 * A reference to the OGRSpatialReference will be kept, and it will also
 * be converted into a TABProjInfo to be stored in the .MAP header.
 *
 * Returns 0 on success, and -1 on error.
 **********************************************************************/
int TABFile::SetSpatialRef(OGRSpatialReference *poSpatialRef)
{
    if (m_eAccessMode != TABWrite)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetSpatialRef() can be used only with Write access.");
        return -1;
    }

    if (m_poMAPFile == NULL )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "SetSpatialRef() failed: file has not been opened yet.");
        return -1;
    }

    if( poSpatialRef == NULL )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "SetSpatialRef() failed: Called with NULL poSpatialRef.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Keep a copy of the OGRSpatialReference...
     * Note: we have to take the reference count into account...
     *----------------------------------------------------------------*/
    if (m_poSpatialRef && m_poSpatialRef->Dereference() == 0)
        delete m_poSpatialRef;
    
    m_poSpatialRef = poSpatialRef->Clone();

    /*-----------------------------------------------------------------
     * Initialize TABProjInfo
     *----------------------------------------------------------------*/
    TABProjInfo     sTABProj;

    sTABProj.nProjId = 0;
    sTABProj.nEllipsoidId = 0; /* how will we set this? */
    sTABProj.nUnitsId = 7;
    sTABProj.adProjParams[0] = sTABProj.adProjParams[1] = 0.0;
    sTABProj.adProjParams[2] = sTABProj.adProjParams[3] = 0.0;
    sTABProj.adProjParams[4] = sTABProj.adProjParams[5] = 0.0;
    
    sTABProj.nDatumId = 0;
    sTABProj.dDatumShiftX = 0.0;
    sTABProj.dDatumShiftY = 0.0;
    sTABProj.dDatumShiftZ = 0.0;
    sTABProj.adDatumParams[0] = 0.0;
    sTABProj.adDatumParams[1] = 0.0;
    sTABProj.adDatumParams[2] = 0.0;
    sTABProj.adDatumParams[3] = 0.0;
    sTABProj.adDatumParams[4] = 0.0;

    sTABProj.nAffineFlag   = 0;
    sTABProj.nAffineUnits  = 7;
    sTABProj.dAffineParamA = 0.0;
    sTABProj.dAffineParamB = 0.0;
    sTABProj.dAffineParamC = 0.0;
    sTABProj.dAffineParamD = 0.0;
    sTABProj.dAffineParamE = 0.0;
    sTABProj.dAffineParamF = 0.0;

    /*-----------------------------------------------------------------
     * Get the linear units and conversion.
     *----------------------------------------------------------------*/
    char        *pszLinearUnits;
    double      dfLinearConv;

    dfLinearConv = poSpatialRef->GetLinearUnits( &pszLinearUnits );
    if( dfLinearConv == 0.0 )
        dfLinearConv = 1.0;

    /*-----------------------------------------------------------------
     * Transform the projection and projection parameters.
     *----------------------------------------------------------------*/
    const char *pszProjection = poSpatialRef->GetAttrValue("PROJECTION");
    double      *parms = sTABProj.adProjParams;

    if( pszProjection == NULL && poSpatialRef->GetAttrNode("LOCAL_CS") != NULL)
    {
        /* nonearth */
        sTABProj.nProjId = 0;
    }

    else if( pszProjection == NULL )
    {
        sTABProj.nProjId = 1;
    }

    else if( EQUAL(pszProjection,SRS_PT_ALBERS_CONIC_EQUAL_AREA) )
    {
        sTABProj.nProjId = 9;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0);
        parms[1] = poSpatialRef->GetProjParm(SRS_PP_LATITUDE_OF_CENTER,0.0);
        parms[2] = poSpatialRef->GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0);
        parms[3] = poSpatialRef->GetProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0);
        parms[4] = poSpatialRef->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[5] = poSpatialRef->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_AZIMUTHAL_EQUIDISTANT) )
    {
        sTABProj.nProjId = 5;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0);
        parms[1] = poSpatialRef->GetProjParm(SRS_PP_LATITUDE_OF_CENTER,0.0);
        parms[2] = 90.0;

        if( ABS((ABS(parms[1]) - 90)) > 0.001 )
            sTABProj.nProjId = 28;
    }

    else if( EQUAL(pszProjection,SRS_PT_CYLINDRICAL_EQUAL_AREA) )
    {
        sTABProj.nProjId = 2;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSpatialRef->GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_ECKERT_IV) )
    {
        sTABProj.nProjId = 14;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_ECKERT_VI) )
    {
        sTABProj.nProjId = 15;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_EQUIDISTANT_CONIC) )
    {
        sTABProj.nProjId = 6;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0);
        parms[1] = poSpatialRef->GetProjParm(SRS_PP_LATITUDE_OF_CENTER,0.0);
        parms[2] = poSpatialRef->GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0);
        parms[3] = poSpatialRef->GetProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0);
        parms[4] = poSpatialRef->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[5] = poSpatialRef->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_GALL_STEREOGRAPHIC) )
    {
        sTABProj.nProjId = 17;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_HOTINE_OBLIQUE_MERCATOR) )
    {
        sTABProj.nProjId = 7;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0);
        parms[1] = poSpatialRef->GetProjParm(SRS_PP_LATITUDE_OF_CENTER,0.0);
        parms[2] = poSpatialRef->GetProjParm(SRS_PP_AZIMUTH,0.0);
        parms[3] = poSpatialRef->GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
        parms[4] = poSpatialRef->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[5] = poSpatialRef->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
    {
        sTABProj.nProjId = 4;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0);
        parms[1] = poSpatialRef->GetProjParm(SRS_PP_LATITUDE_OF_CENTER,0.0);
        parms[2] = 90.0;

        if( ABS((ABS(parms[1]) - 90)) > 0.001 )
            sTABProj.nProjId = 28;
    }

    else if( EQUAL(pszProjection,SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) )
    {
        sTABProj.nProjId = 3;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSpatialRef->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSpatialRef->GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0);
        parms[3] = poSpatialRef->GetProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0);
        parms[4] = poSpatialRef->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[5] = poSpatialRef->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM) )
    {
        sTABProj.nProjId = 19;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSpatialRef->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSpatialRef->GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0);
        parms[3] = poSpatialRef->GetProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0);
        parms[4] = poSpatialRef->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[5] = poSpatialRef->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_MERCATOR_1SP) )
    {
        sTABProj.nProjId = 10;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSpatialRef->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSpatialRef->GetProjParm(SRS_PP_SCALE_FACTOR,1.0);

        if( parms[1] != 0.0 )
            sTABProj.nProjId = 26;
    }

    else if( EQUAL(pszProjection,SRS_PT_MILLER_CYLINDRICAL) )
    {
        sTABProj.nProjId = 11;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_MOLLWEIDE) )
    {
        sTABProj.nProjId = 13;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_NEW_ZEALAND_MAP_GRID) )
    {
        sTABProj.nProjId = 18;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSpatialRef->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSpatialRef->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[3] = poSpatialRef->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_SWISS_OBLIQUE_CYLINDRICAL) )
    {
        sTABProj.nProjId = 25;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSpatialRef->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSpatialRef->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[3] = poSpatialRef->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_ROBINSON) )
    {
        sTABProj.nProjId = 12;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_SINUSOIDAL) )
    {
        sTABProj.nProjId = 16;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_STEREOGRAPHIC) )
    {
        sTABProj.nProjId = 20;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSpatialRef->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSpatialRef->GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
        parms[3] = poSpatialRef->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[4] = poSpatialRef->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR) )
    {
        sTABProj.nProjId = 8;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSpatialRef->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSpatialRef->GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
        parms[3] = poSpatialRef->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[4] = poSpatialRef->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR_MI_21) ) // Encom 2003
    {
        sTABProj.nProjId = 21;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSpatialRef->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSpatialRef->GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
        parms[3] = poSpatialRef->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[4] = poSpatialRef->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR_MI_22) ) // Encom 2003
    {
        sTABProj.nProjId = 22;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSpatialRef->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSpatialRef->GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
        parms[3] = poSpatialRef->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[4] = poSpatialRef->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR_MI_23) ) // Encom 2003
    {
        sTABProj.nProjId = 23;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSpatialRef->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSpatialRef->GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
        parms[3] = poSpatialRef->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[4] = poSpatialRef->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR_MI_24) ) // Encom 2003
    {
        sTABProj.nProjId = 24;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSpatialRef->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSpatialRef->GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
        parms[3] = poSpatialRef->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[4] = poSpatialRef->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_CASSINI_SOLDNER) )
    {
        sTABProj.nProjId = 30;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSpatialRef->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSpatialRef->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[3] = poSpatialRef->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_NEW_ZEALAND_MAP_GRID) )
    {
        sTABProj.nProjId = 18;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSpatialRef->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSpatialRef->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[3] = poSpatialRef->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
    }

    else if( EQUAL(pszProjection,SRS_PT_POLYCONIC) )
    {
        sTABProj.nProjId = 27;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSpatialRef->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSpatialRef->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[3] = poSpatialRef->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
    }

   else if( EQUAL(pszProjection,SRS_PT_KROVAK) )
   {
        sTABProj.nProjId = 32;
        parms[0] = poSpatialRef->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSpatialRef->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSpatialRef->GetProjParm(SRS_PP_PSEUDO_STD_PARALLEL_1,0.0);
        parms[3] = poSpatialRef->GetProjParm(SRS_PP_AZIMUTH,0.0);
        parms[4] = poSpatialRef->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[5] = poSpatialRef->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
   }

    /* ==============================================================
     * Translate Datum and Ellipsoid
     * ============================================================== */
    const char *pszWKTDatum = poSpatialRef->GetAttrValue("DATUM");
    MapInfoDatumInfo *psDatumInfo = NULL;
    
    /*-----------------------------------------------------------------
     * Default to WGS83 if we have no datum at all.
     *----------------------------------------------------------------*/
    if( pszWKTDatum == NULL )
    {
        psDatumInfo = asDatumInfoList+0; /* WGS 84 */
    }
    
    /*-----------------------------------------------------------------
     * We know the MIF datum number, and need to look it up to
     * translate into datum parameters.
     *----------------------------------------------------------------*/
    else if( EQUALN(pszWKTDatum,"MIF ",4)
             && atoi(pszWKTDatum+4) != 999
             && atoi(pszWKTDatum+4) != 9999 )
    {
        int     i;

        for( i = 0; asDatumInfoList[i].nMapInfoDatumID != -1; i++ )
        {
            if( atoi(pszWKTDatum+4) == asDatumInfoList[i].nMapInfoDatumID )
            {
                psDatumInfo = asDatumInfoList + i;
                break;
            }
        }

        if( psDatumInfo == NULL )
            psDatumInfo = asDatumInfoList+0; /* WGS 84 */
    }

    /*-----------------------------------------------------------------
     * We have the MIF datum parameters, and apply those directly.
     *----------------------------------------------------------------*/
    else if( EQUALN(pszWKTDatum,"MIF ",4)
             && (atoi(pszWKTDatum+4) == 999 || atoi(pszWKTDatum+4) == 9999) )
    {
        char **papszFields;

        papszFields =
            CSLTokenizeStringComplex( pszWKTDatum+4, ",", FALSE, TRUE);

        if( CSLCount(papszFields) >= 5 )
        {
            sTABProj.nEllipsoidId = (GByte)atoi(papszFields[1]);
            sTABProj.dDatumShiftX = atof(papszFields[2]);
            sTABProj.dDatumShiftY = atof(papszFields[3]);
            sTABProj.dDatumShiftZ = atof(papszFields[4]);
        }

        if( CSLCount(papszFields) >= 10 )
        {
            sTABProj.adDatumParams[0] = atof(papszFields[5]);
            sTABProj.adDatumParams[1] = atof(papszFields[6]);
            sTABProj.adDatumParams[2] = atof(papszFields[7]);
            sTABProj.adDatumParams[3] = atof(papszFields[8]);
            sTABProj.adDatumParams[4] = atof(papszFields[9]);
        }

        if( CSLCount(papszFields) < 5 )
            psDatumInfo = asDatumInfoList+0; /* WKS84 */

        CSLDestroy( papszFields );
    }
    
    /*-----------------------------------------------------------------
     * We have a "real" datum name.  Try to look it up and get the
     * parameters.  If we don't find it just use WGS84.
     *----------------------------------------------------------------*/
    else 
    {
        int     i;

        for( i = 0; asDatumInfoList[i].nMapInfoDatumID != -1; i++ )
        {
            if( EQUAL(pszWKTDatum,asDatumInfoList[i].pszOGCDatumName) )
            {
                psDatumInfo = asDatumInfoList + i;
                break;
            }
        }

         if( psDatumInfo == NULL )
            psDatumInfo = asDatumInfoList+0; /* WGS 84 */
    }

    if( psDatumInfo != NULL )
    {
        sTABProj.nEllipsoidId = (GByte)psDatumInfo->nEllipsoid;
        sTABProj.nDatumId = (GInt16)psDatumInfo->nMapInfoDatumID;
        sTABProj.dDatumShiftX = psDatumInfo->dfShiftX;
        sTABProj.dDatumShiftY = psDatumInfo->dfShiftY;
        sTABProj.dDatumShiftZ = psDatumInfo->dfShiftZ;
        sTABProj.adDatumParams[0] = psDatumInfo->dfDatumParm0;
        sTABProj.adDatumParams[1] = psDatumInfo->dfDatumParm1;
        sTABProj.adDatumParams[2] = psDatumInfo->dfDatumParm2;
        sTABProj.adDatumParams[3] = psDatumInfo->dfDatumParm3;
        sTABProj.adDatumParams[4] = psDatumInfo->dfDatumParm4;
    }
    
    /*-----------------------------------------------------------------
     * Translate the units
     *----------------------------------------------------------------*/
    if( sTABProj.nProjId == 1 || pszLinearUnits == NULL )
        sTABProj.nUnitsId = 13;
    else if( dfLinearConv == 1000.0 )
        sTABProj.nUnitsId = 1;
    else if( dfLinearConv == 0.0254 || EQUAL(pszLinearUnits,"Inch")
             || EQUAL(pszLinearUnits,"IINCH") )
        sTABProj.nUnitsId = 2;
    else if( dfLinearConv == CPLAtof(SRS_UL_FOOT_CONV)
             || EQUAL(pszLinearUnits,SRS_UL_FOOT) )
        sTABProj.nUnitsId = 3;
    else if( EQUAL(pszLinearUnits,"YARD") || EQUAL(pszLinearUnits,"IYARD") 
             || dfLinearConv == 0.9144 )
        sTABProj.nUnitsId = 4;
    else if( dfLinearConv == 0.001 )
        sTABProj.nUnitsId = 5;
    else if( dfLinearConv == 0.01 )
        sTABProj.nUnitsId = 6;
    else if( dfLinearConv == 1.0 )
        sTABProj.nUnitsId = 7;
    else if( dfLinearConv == CPLAtof(SRS_UL_US_FOOT_CONV)
             || EQUAL(pszLinearUnits,SRS_UL_US_FOOT) )
        sTABProj.nUnitsId = 8;
    else if( EQUAL(pszLinearUnits,SRS_UL_NAUTICAL_MILE) )
        sTABProj.nUnitsId = 9;
    else if( EQUAL(pszLinearUnits,SRS_UL_LINK) 
             || EQUAL(pszLinearUnits,"GUNTERLINK") )
        sTABProj.nUnitsId = 30;
    else if( EQUAL(pszLinearUnits,SRS_UL_CHAIN) 
             || EQUAL(pszLinearUnits,"GUNTERCHAIN") )
        sTABProj.nUnitsId = 31;
    else if( EQUAL(pszLinearUnits,SRS_UL_ROD) )
        sTABProj.nUnitsId = 32;
    else if( EQUAL(pszLinearUnits,"Mile") 
             || EQUAL(pszLinearUnits,"IMILE") )
        sTABProj.nUnitsId = 0;
    else
        sTABProj.nUnitsId = 7;
    
    /*-----------------------------------------------------------------
     * Set the new parameters in the .MAP header.
     * This will also trigger lookup of default bounds for the projection.
     *----------------------------------------------------------------*/
    if ( SetProjInfo( &sTABProj ) != 0 )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "SetSpatialRef() failed setting projection parameters.");
        return -1;
    }

    return 0;
}


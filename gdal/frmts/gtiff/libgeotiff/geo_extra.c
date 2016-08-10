/******************************************************************************
 * $Id: geo_extra.c 2691 2015-12-06 21:54:31Z rouault $
 *
 * Project:  libgeotiff
 * Purpose:  Code to normalize a few common PCS values without use of CSV
 *           files.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

/*
#include "geotiff.h"
#include "geo_tiffp.h"
#include "geo_keyp.h"
*/

#include "geo_normalize.h"
#include "geovalues.h"

static const int StatePlaneTable[] =
{
    PCS_NAD83_Alabama_East,		Proj_Alabama_CS83_East,
    PCS_NAD83_Alabama_West,		Proj_Alabama_CS83_West,

    PCS_NAD83_Alaska_zone_1,		Proj_Alaska_CS83_1,
    PCS_NAD83_Alaska_zone_2,		Proj_Alaska_CS83_2,
    PCS_NAD83_Alaska_zone_3,		Proj_Alaska_CS83_3,
    PCS_NAD83_Alaska_zone_4,		Proj_Alaska_CS83_4,
    PCS_NAD83_Alaska_zone_5,		Proj_Alaska_CS83_5,
    PCS_NAD83_Alaska_zone_6,		Proj_Alaska_CS83_6,
    PCS_NAD83_Alaska_zone_7,		Proj_Alaska_CS83_7,
    PCS_NAD83_Alaska_zone_8,		Proj_Alaska_CS83_8,
    PCS_NAD83_Alaska_zone_9,		Proj_Alaska_CS83_9,
    PCS_NAD83_Alaska_zone_10,		Proj_Alaska_CS83_10,

    PCS_NAD83_California_1,		Proj_California_CS83_1,
    PCS_NAD83_California_2,		Proj_California_CS83_2,
    PCS_NAD83_California_3,		Proj_California_CS83_3,
    PCS_NAD83_California_4,		Proj_California_CS83_4,
    PCS_NAD83_California_5,		Proj_California_CS83_5,
    PCS_NAD83_California_6,		Proj_California_CS83_6,

    PCS_NAD83_Arizona_East,		Proj_Arizona_CS83_east,
    PCS_NAD83_Arizona_Central,		Proj_Arizona_CS83_Central,
    PCS_NAD83_Arizona_West,		Proj_Arizona_CS83_west,

    PCS_NAD83_Arkansas_North,		Proj_Arkansas_CS83_North,
    PCS_NAD83_Arkansas_South,		Proj_Arkansas_CS83_South,

    PCS_NAD83_Colorado_North,		Proj_Colorado_CS83_North,
    PCS_NAD83_Colorado_Central,		Proj_Colorado_CS83_Central,
    PCS_NAD83_Colorado_South,		Proj_Colorado_CS83_South,

    PCS_NAD83_Connecticut,		Proj_Connecticut_CS83,

    PCS_NAD83_Delaware,			Proj_Delaware_CS83,

    PCS_NAD83_Florida_East,		Proj_Florida_CS83_East,
    PCS_NAD83_Florida_North,		Proj_Florida_CS83_North,
    PCS_NAD83_Florida_West,		Proj_Florida_CS83_West,

    PCS_NAD83_Hawaii_zone_1,		Proj_Hawaii_CS83_1,
    PCS_NAD83_Hawaii_zone_2,		Proj_Hawaii_CS83_2,
    PCS_NAD83_Hawaii_zone_3,		Proj_Hawaii_CS83_3,
    PCS_NAD83_Hawaii_zone_4,		Proj_Hawaii_CS83_4,
    PCS_NAD83_Hawaii_zone_5,		Proj_Hawaii_CS83_5,

    PCS_NAD83_Georgia_East,		Proj_Georgia_CS83_East,
    PCS_NAD83_Georgia_West,		Proj_Georgia_CS83_West,

    PCS_NAD83_Idaho_East,		Proj_Idaho_CS83_East,
    PCS_NAD83_Idaho_Central,		Proj_Idaho_CS83_Central,
    PCS_NAD83_Idaho_West,		Proj_Idaho_CS83_West,

    PCS_NAD83_Illinois_East,		Proj_Illinois_CS83_East,
    PCS_NAD83_Illinois_West,		Proj_Illinois_CS83_West,

    PCS_NAD83_Indiana_East,		Proj_Indiana_CS83_East,
    PCS_NAD83_Indiana_West,		Proj_Indiana_CS83_West,

    PCS_NAD83_Iowa_North,      		Proj_Iowa_CS83_North,
    PCS_NAD83_Iowa_South,      		Proj_Iowa_CS83_South,

    PCS_NAD83_Kansas_North,		Proj_Kansas_CS83_North,
    PCS_NAD83_Kansas_South,		Proj_Kansas_CS83_South,

    PCS_NAD83_Kentucky_North,		Proj_Kentucky_CS83_North,
    PCS_NAD83_Kentucky_South,		Proj_Kentucky_CS83_South,

    PCS_NAD83_Louisiana_North,		Proj_Louisiana_CS83_North,
    PCS_NAD83_Louisiana_South,		Proj_Louisiana_CS83_South,

    PCS_NAD83_Maine_East,		Proj_Maine_CS83_East,
    PCS_NAD83_Maine_West,		Proj_Maine_CS83_West,

    PCS_NAD83_Maryland,			Proj_Maryland_CS83,

    PCS_NAD83_Massachusetts,		Proj_Massachusetts_CS83_Mainland,
    PCS_NAD83_Massachusetts_Is,		Proj_Massachusetts_CS83_Island,

    PCS_NAD83_Michigan_North,		Proj_Michigan_CS83_North,
    PCS_NAD83_Michigan_Central,		Proj_Michigan_CS83_Central,
    PCS_NAD83_Michigan_South,		Proj_Michigan_CS83_South,

    PCS_NAD83_Minnesota_North,		Proj_Minnesota_CS83_North,
    PCS_NAD83_Minnesota_Cent,		Proj_Minnesota_CS83_Central,
    PCS_NAD83_Minnesota_South,		Proj_Minnesota_CS83_South,

    PCS_NAD83_Mississippi_East,		Proj_Mississippi_CS83_East,
    PCS_NAD83_Mississippi_West,		Proj_Mississippi_CS83_West,

    PCS_NAD83_Missouri_East,		Proj_Missouri_CS83_East,
    PCS_NAD83_Missouri_Central,		Proj_Missouri_CS83_Central,
    PCS_NAD83_Missouri_West,		Proj_Missouri_CS83_West,

    PCS_NAD83_Montana,			Proj_Montana_CS83,

    PCS_NAD83_Nebraska,			Proj_Nebraska_CS83,

    PCS_NAD83_Nevada_East,		Proj_Nevada_CS83_East,
    PCS_NAD83_Nevada_Central,		Proj_Nevada_CS83_Central,
    PCS_NAD83_Nevada_West,		Proj_Nevada_CS83_West,

    PCS_NAD83_New_Hampshire,		Proj_New_Hampshire_CS83,

    PCS_NAD83_New_Jersey,		Proj_New_Jersey_CS83,

    PCS_NAD83_New_Mexico_East,		Proj_New_Mexico_CS83_East,
    PCS_NAD83_New_Mexico_Cent,		Proj_New_Mexico_CS83_Central,
    PCS_NAD83_New_Mexico_West,		Proj_New_Mexico_CS83_West,

    PCS_NAD83_New_York_East,		Proj_New_York_CS83_East,
    PCS_NAD83_New_York_Central,		Proj_New_York_CS83_Central,
    PCS_NAD83_New_York_West,		Proj_New_York_CS83_West,
    PCS_NAD83_New_York_Long_Is,		Proj_New_York_CS83_Long_Island,

    PCS_NAD83_North_Carolina,	       	Proj_North_Carolina_CS83,

    PCS_NAD83_North_Dakota_N,		Proj_North_Dakota_CS83_North,
    PCS_NAD83_North_Dakota_S,		Proj_North_Dakota_CS83_South,

    PCS_NAD83_Ohio_North,		Proj_Ohio_CS83_North,
    PCS_NAD83_Ohio_South,		Proj_Ohio_CS83_South,

    PCS_NAD83_Oklahoma_North,		Proj_Oklahoma_CS83_North,
    PCS_NAD83_Oklahoma_South,		Proj_Oklahoma_CS83_South,

    PCS_NAD83_Oregon_North,		Proj_Oregon_CS83_North,
    PCS_NAD83_Oregon_South,		Proj_Oregon_CS83_South,

    PCS_NAD83_Pennsylvania_N,		Proj_Pennsylvania_CS83_North,
    PCS_NAD83_Pennsylvania_S,		Proj_Pennsylvania_CS83_South,

    PCS_NAD83_Rhode_Island,		Proj_Rhode_Island_CS83,

    PCS_NAD83_South_Carolina,		Proj_South_Carolina_CS83,

    PCS_NAD83_South_Dakota_N,		Proj_South_Dakota_CS83_North,
    PCS_NAD83_South_Dakota_S,		Proj_South_Dakota_CS83_South,

    PCS_NAD83_Tennessee,		Proj_Tennessee_CS83,

    PCS_NAD83_Texas_North,		Proj_Texas_CS83_North,
    PCS_NAD83_Texas_North_Cen,		Proj_Texas_CS83_North_Central,
    PCS_NAD83_Texas_Central,		Proj_Texas_CS83_Central,
    PCS_NAD83_Texas_South_Cen,		Proj_Texas_CS83_South_Central,
    PCS_NAD83_Texas_South,		Proj_Texas_CS83_South,

    PCS_NAD83_Utah_North,		Proj_Utah_CS83_North,
    PCS_NAD83_Utah_Central,		Proj_Utah_CS83_Central,
    PCS_NAD83_Utah_South,		Proj_Utah_CS83_South,

    PCS_NAD83_Vermont,			Proj_Vermont_CS83,

    PCS_NAD83_Virginia_North,		Proj_Virginia_CS83_North,
    PCS_NAD83_Virginia_South,		Proj_Virginia_CS83_South,

    PCS_NAD83_Washington_North,		Proj_Washington_CS83_North,
    PCS_NAD83_Washington_South,		Proj_Washington_CS83_South,

    PCS_NAD83_West_Virginia_N,		Proj_West_Virginia_CS83_North,
    PCS_NAD83_West_Virginia_S,		Proj_West_Virginia_CS83_South,

    PCS_NAD83_Wisconsin_North,		Proj_Wisconsin_CS83_North,
    PCS_NAD83_Wisconsin_Cen,		Proj_Wisconsin_CS83_Central,
    PCS_NAD83_Wisconsin_South,		Proj_Wisconsin_CS83_South,

    PCS_NAD83_Wyoming_East,		Proj_Wyoming_CS83_East,
    PCS_NAD83_Wyoming_E_Cen,		Proj_Wyoming_CS83_East_Central,
    PCS_NAD83_Wyoming_W_Cen,		Proj_Wyoming_CS83_West_Central,
    PCS_NAD83_Wyoming_West,		Proj_Wyoming_CS83_West,

    PCS_NAD83_Puerto_Rico_Virgin_Is,	Proj_Puerto_Rico_Virgin_Is,

    PCS_NAD27_Alabama_East,		Proj_Alabama_CS27_East,
    PCS_NAD27_Alabama_West,		Proj_Alabama_CS27_West,

    PCS_NAD27_Alaska_zone_1,		Proj_Alaska_CS27_1,
    PCS_NAD27_Alaska_zone_2,		Proj_Alaska_CS27_2,
    PCS_NAD27_Alaska_zone_3,		Proj_Alaska_CS27_3,
    PCS_NAD27_Alaska_zone_4,		Proj_Alaska_CS27_4,
    PCS_NAD27_Alaska_zone_5,		Proj_Alaska_CS27_5,
    PCS_NAD27_Alaska_zone_6,		Proj_Alaska_CS27_6,
    PCS_NAD27_Alaska_zone_7,		Proj_Alaska_CS27_7,
    PCS_NAD27_Alaska_zone_8,		Proj_Alaska_CS27_8,
    PCS_NAD27_Alaska_zone_9,		Proj_Alaska_CS27_9,
    PCS_NAD27_Alaska_zone_10,		Proj_Alaska_CS27_10,

    PCS_NAD27_California_I,		Proj_California_CS27_I,
    PCS_NAD27_California_II,		Proj_California_CS27_II,
    PCS_NAD27_California_III,		Proj_California_CS27_III,
    PCS_NAD27_California_IV,		Proj_California_CS27_IV,
    PCS_NAD27_California_V,		Proj_California_CS27_V,
    PCS_NAD27_California_VI,		Proj_California_CS27_VI,
    PCS_NAD27_California_VII,		Proj_California_CS27_VII,

    PCS_NAD27_Arizona_East,		Proj_Arizona_Coordinate_System_east,
    PCS_NAD27_Arizona_Central,		Proj_Arizona_Coordinate_System_Central,
    PCS_NAD27_Arizona_West,		Proj_Arizona_Coordinate_System_west,

    PCS_NAD27_Arkansas_North,		Proj_Arkansas_CS27_North,
    PCS_NAD27_Arkansas_South,		Proj_Arkansas_CS27_South,

    PCS_NAD27_Colorado_North,		Proj_Colorado_CS27_North,
    PCS_NAD27_Colorado_Central,		Proj_Colorado_CS27_Central,
    PCS_NAD27_Colorado_South,		Proj_Colorado_CS27_South,

    PCS_NAD27_Connecticut,		Proj_Connecticut_CS27,

    PCS_NAD27_Delaware,			Proj_Delaware_CS27,

    PCS_NAD27_Florida_East,		Proj_Florida_CS27_East,
    PCS_NAD27_Florida_North,		Proj_Florida_CS27_North,
    PCS_NAD27_Florida_West,		Proj_Florida_CS27_West,

    PCS_NAD27_Hawaii_zone_1,		Proj_Hawaii_CS27_1,
    PCS_NAD27_Hawaii_zone_2,		Proj_Hawaii_CS27_2,
    PCS_NAD27_Hawaii_zone_3,		Proj_Hawaii_CS27_3,
    PCS_NAD27_Hawaii_zone_4,		Proj_Hawaii_CS27_4,
    PCS_NAD27_Hawaii_zone_5,		Proj_Hawaii_CS27_5,

    PCS_NAD27_Georgia_East,		Proj_Georgia_CS27_East,
    PCS_NAD27_Georgia_West,		Proj_Georgia_CS27_West,

    PCS_NAD27_Idaho_East,		Proj_Idaho_CS27_East,
    PCS_NAD27_Idaho_Central,		Proj_Idaho_CS27_Central,
    PCS_NAD27_Idaho_West,		Proj_Idaho_CS27_West,

    PCS_NAD27_Illinois_East,		Proj_Illinois_CS27_East,
    PCS_NAD27_Illinois_West,		Proj_Illinois_CS27_West,

    PCS_NAD27_Indiana_East,		Proj_Indiana_CS27_East,
    PCS_NAD27_Indiana_West,		Proj_Indiana_CS27_West,

    PCS_NAD27_Iowa_North,      		Proj_Iowa_CS27_North,
    PCS_NAD27_Iowa_South,      		Proj_Iowa_CS27_South,

    PCS_NAD27_Kansas_North,		Proj_Kansas_CS27_North,
    PCS_NAD27_Kansas_South,		Proj_Kansas_CS27_South,

    PCS_NAD27_Kentucky_North,		Proj_Kentucky_CS27_North,
    PCS_NAD27_Kentucky_South,		Proj_Kentucky_CS27_South,

    PCS_NAD27_Louisiana_North,		Proj_Louisiana_CS27_North,
    PCS_NAD27_Louisiana_South,		Proj_Louisiana_CS27_South,

    PCS_NAD27_Maine_East,		Proj_Maine_CS27_East,
    PCS_NAD27_Maine_West,		Proj_Maine_CS27_West,

    PCS_NAD27_Maryland,			Proj_Maryland_CS27,

    PCS_NAD27_Massachusetts,		Proj_Massachusetts_CS27_Mainland,
    PCS_NAD27_Massachusetts_Is,		Proj_Massachusetts_CS27_Island,

    PCS_NAD27_Michigan_North,		Proj_Michigan_CS27_North,
    PCS_NAD27_Michigan_Central,		Proj_Michigan_CS27_Central,
    PCS_NAD27_Michigan_South,		Proj_Michigan_CS27_South,

    PCS_NAD27_Minnesota_North,		Proj_Minnesota_CS27_North,
    PCS_NAD27_Minnesota_Cent,		Proj_Minnesota_CS27_Central,
    PCS_NAD27_Minnesota_South,		Proj_Minnesota_CS27_South,

    PCS_NAD27_Mississippi_East,		Proj_Mississippi_CS27_East,
    PCS_NAD27_Mississippi_West,		Proj_Mississippi_CS27_West,

    PCS_NAD27_Missouri_East,		Proj_Missouri_CS27_East,
    PCS_NAD27_Missouri_Central,		Proj_Missouri_CS27_Central,
    PCS_NAD27_Missouri_West,		Proj_Missouri_CS27_West,

    PCS_NAD27_Montana_North,		Proj_Montana_CS27_North,
    PCS_NAD27_Montana_Central,		Proj_Montana_CS27_Central,
    PCS_NAD27_Montana_South,		Proj_Montana_CS27_South,

    PCS_NAD27_Nebraska_North,		Proj_Nebraska_CS27_North,
    PCS_NAD27_Nebraska_South,		Proj_Nebraska_CS27_South,

    PCS_NAD27_Nevada_East,		Proj_Nevada_CS27_East,
    PCS_NAD27_Nevada_Central,		Proj_Nevada_CS27_Central,
    PCS_NAD27_Nevada_West,		Proj_Nevada_CS27_West,

    PCS_NAD27_New_Hampshire,		Proj_New_Hampshire_CS27,

    PCS_NAD27_New_Jersey,		Proj_New_Jersey_CS27,

    PCS_NAD27_New_Mexico_East,		Proj_New_Mexico_CS27_East,
    PCS_NAD27_New_Mexico_Cent,		Proj_New_Mexico_CS27_Central,
    PCS_NAD27_New_Mexico_West,		Proj_New_Mexico_CS27_West,

    PCS_NAD27_New_York_East,		Proj_New_York_CS27_East,
    PCS_NAD27_New_York_Central,		Proj_New_York_CS27_Central,
    PCS_NAD27_New_York_West,		Proj_New_York_CS27_West,
    PCS_NAD27_New_York_Long_Is,		Proj_New_York_CS27_Long_Island,

    PCS_NAD27_North_Carolina,	       	Proj_North_Carolina_CS27,

    PCS_NAD27_North_Dakota_N,		Proj_North_Dakota_CS27_North,
    PCS_NAD27_North_Dakota_S,		Proj_North_Dakota_CS27_South,

    PCS_NAD27_Ohio_North,		Proj_Ohio_CS27_North,
    PCS_NAD27_Ohio_South,		Proj_Ohio_CS27_South,

    PCS_NAD27_Oklahoma_North,		Proj_Oklahoma_CS27_North,
    PCS_NAD27_Oklahoma_South,		Proj_Oklahoma_CS27_South,

    PCS_NAD27_Oregon_North,		Proj_Oregon_CS27_North,
    PCS_NAD27_Oregon_South,		Proj_Oregon_CS27_South,

    PCS_NAD27_Pennsylvania_N,		Proj_Pennsylvania_CS27_North,
    PCS_NAD27_Pennsylvania_S,		Proj_Pennsylvania_CS27_South,

    PCS_NAD27_Rhode_Island,		Proj_Rhode_Island_CS27,

    PCS_NAD27_South_Carolina_N,		Proj_South_Carolina_CS27_North,
    PCS_NAD27_South_Carolina_S,		Proj_South_Carolina_CS27_South,

    PCS_NAD27_South_Dakota_N,		Proj_South_Dakota_CS27_North,
    PCS_NAD27_South_Dakota_S,		Proj_South_Dakota_CS27_South,

    PCS_NAD27_Tennessee,		Proj_Tennessee_CS27,

    PCS_NAD27_Texas_North,		Proj_Texas_CS27_North,
    PCS_NAD27_Texas_North_Cen,		Proj_Texas_CS27_North_Central,
    PCS_NAD27_Texas_Central,		Proj_Texas_CS27_Central,
    PCS_NAD27_Texas_South_Cen,		Proj_Texas_CS27_South_Central,
    PCS_NAD27_Texas_South,		Proj_Texas_CS27_South,

    PCS_NAD27_Utah_North,		Proj_Utah_CS27_North,
    PCS_NAD27_Utah_Central,		Proj_Utah_CS27_Central,
    PCS_NAD27_Utah_South,		Proj_Utah_CS27_South,

    PCS_NAD27_Vermont,			Proj_Vermont_CS27,

    PCS_NAD27_Virginia_North,		Proj_Virginia_CS27_North,
    PCS_NAD27_Virginia_South,		Proj_Virginia_CS27_South,

    PCS_NAD27_Washington_North,		Proj_Washington_CS27_North,
    PCS_NAD27_Washington_South,		Proj_Washington_CS27_South,

    PCS_NAD27_West_Virginia_N,		Proj_West_Virginia_CS27_North,
    PCS_NAD27_West_Virginia_S,		Proj_West_Virginia_CS27_South,

    PCS_NAD27_Wisconsin_North,		Proj_Wisconsin_CS27_North,
    PCS_NAD27_Wisconsin_Cen,		Proj_Wisconsin_CS27_Central,
    PCS_NAD27_Wisconsin_South,		Proj_Wisconsin_CS27_South,

    PCS_NAD27_Wyoming_East,		Proj_Wyoming_CS27_East,
    PCS_NAD27_Wyoming_E_Cen,		Proj_Wyoming_CS27_East_Central,
    PCS_NAD27_Wyoming_W_Cen,		Proj_Wyoming_CS27_West_Central,
    PCS_NAD27_Wyoming_West,		Proj_Wyoming_CS27_West,

    PCS_NAD27_Puerto_Rico,		Proj_Puerto_Rico_CS27,

    KvUserDefined
};

/************************************************************************/
/*                          GTIFMapSysToPCS()                           */
/*                                                                      */
/*      Given a Datum, MapSys and zone value generate the best PCS      */
/*      code possible.                                                  */
/************************************************************************/

int	GTIFMapSysToPCS( int MapSys, int Datum, int nZone )

{
    int		PCSCode = KvUserDefined;

    if( MapSys == MapSys_UTM_North )
    {
	if( Datum == GCS_NAD27 )
	    PCSCode = PCS_NAD27_UTM_zone_3N + nZone - 3;
	else if( Datum == GCS_NAD83 )
	    PCSCode = PCS_NAD83_UTM_zone_3N + nZone - 3;
	else if( Datum == GCS_WGS_72 )
	    PCSCode = PCS_WGS72_UTM_zone_1N + nZone - 1;
	else if( Datum == GCS_WGS_72BE )
	    PCSCode = PCS_WGS72BE_UTM_zone_1N + nZone - 1;
	else if( Datum == GCS_WGS_84 )
	    PCSCode = PCS_WGS84_UTM_zone_1N + nZone - 1;
    }
    else if( MapSys == MapSys_UTM_South )
    {
	if( Datum == GCS_WGS_72 )
	    PCSCode = PCS_WGS72_UTM_zone_1S + nZone - 1;
	else if( Datum == GCS_WGS_72BE )
	    PCSCode = PCS_WGS72BE_UTM_zone_1S + nZone - 1;
	else if( Datum == GCS_WGS_84 )
	    PCSCode = PCS_WGS84_UTM_zone_1S + nZone - 1;
    }
    else if( MapSys == MapSys_State_Plane_27 )
    {
	int		i;

        PCSCode = 10000 + nZone;
	for( i = 0; StatePlaneTable[i] != KvUserDefined; i += 2 )
	{
	    if( StatePlaneTable[i+1] == PCSCode )
	        PCSCode = StatePlaneTable[i];
	}

        /* Old EPSG code was in error for Tennesse CS27, override */
        if( nZone == 4100 )
            PCSCode = 2204;
    }
    else if( MapSys == MapSys_State_Plane_83 )
    {
	int		i;

        PCSCode = 10000 + nZone + 30;

	for( i = 0; StatePlaneTable[i] != KvUserDefined; i += 2 )
	{
	    if( StatePlaneTable[i+1] == PCSCode )
	        PCSCode = StatePlaneTable[i];
	}

        /* Old EPSG code was in error for Kentucky North CS83, override */
        if( nZone == 1601 )
            PCSCode = 2205;
    }

    return PCSCode;
}

/************************************************************************/
/*                          GTIFMapSysToProj()                          */
/*                                                                      */
/*      Given a MapSys and zone value generate the best Proj_           */
/*      code possible.                                                  */
/************************************************************************/

int	GTIFMapSysToProj( int MapSys, int nZone )

{
    int		ProjCode = KvUserDefined;

    if( MapSys == MapSys_UTM_North )
    {
        ProjCode = Proj_UTM_zone_1N + nZone - 1;
    }
    else if( MapSys == MapSys_UTM_South )
    {
        ProjCode = Proj_UTM_zone_1S + nZone - 1;
    }
    else if( MapSys == MapSys_State_Plane_27 )
    {
        ProjCode = 10000 + nZone;

        /* Tennesse override */
        if( nZone == 4100 )
            ProjCode = 15302;
    }
    else if( MapSys == MapSys_State_Plane_83 )
    {
        ProjCode = 10000 + nZone + 30;

        /* Kentucky North override */
        if( nZone == 1601 )
            ProjCode = 15303;
    }

    return ProjCode;
}

/************************************************************************/
/*                          GTIFPCSToMapSys()                           */
/************************************************************************/

/**
 * Translate a PCS_ code into a UTM or State Plane map system, a datum,
 * and a zone if possible.
 *
 * @param PCSCode The projection code (PCS_*) as would be stored in the
 * ProjectedCSTypeGeoKey of a GeoTIFF file.
 *
 * @param pDatum Pointer to an integer into which the datum code (GCS_*)
 * is put if the function succeeds.
 *
 * @param pZone Pointer to an integer into which the zone will be placed
 * if the function is successful.
 *
 * @return Returns either MapSys_UTM_North, MapSys_UTM_South,
 * MapSys_State_Plane_83, MapSys_State_Plane_27 or KvUserDefined.
 * KvUserDefined indicates that the
 * function failed to recognise the projection as UTM or State Plane.
 *
 * The zone value is only set if the return code is other than KvUserDefined.
 * For utm map system the returned zone will be between 1 and 60.  For
 * State Plane, the USGS state plane zone number is returned.  For instance,
 * Alabama East is zone 101.
 *
 * The datum (really this is the GCS) is set to a GCS_ value such as GCS_NAD27.
 *
 * This function is useful to recognise (most) UTM and State Plane coordinate
 * systems, even if CSV files aren't available to translate them automatically.
 * It is used as a fallback mechanism by GTIFGetDefn() for normalization when
 * CSV files aren't found.
 */

int GTIFPCSToMapSys( int PCSCode, int * pDatum, int * pZone )

{
    int		Datum = KvUserDefined, Proj = KvUserDefined;
    int		nZone = KvUserDefined, i;

/* -------------------------------------------------------------------- */
/*      UTM with various datums.  Note there are lots of PCS UTM        */
/*      codes not done yet which use strange datums.                    */
/* -------------------------------------------------------------------- */
    if( PCSCode >= PCS_NAD27_UTM_zone_3N && PCSCode <= PCS_NAD27_UTM_zone_22N )
    {
	Datum = GCS_NAD27;
	Proj = MapSys_UTM_North;
	nZone = PCSCode - PCS_NAD27_UTM_zone_3N + 3;
    }
    else if( PCSCode >= PCS_NAD83_UTM_zone_3N
	     && PCSCode <= PCS_NAD83_UTM_zone_23N )
    {
	Datum = GCS_NAD83;
	Proj = MapSys_UTM_North;
	nZone = PCSCode - PCS_NAD83_UTM_zone_3N + 3;
    }

    else if( PCSCode >= PCS_WGS72_UTM_zone_1N
	     && PCSCode <= PCS_WGS72_UTM_zone_60N )
    {
	Datum = GCS_WGS_72;
	Proj = MapSys_UTM_North;
	nZone = PCSCode - PCS_WGS72_UTM_zone_1N + 1;
    }
    else if( PCSCode >= PCS_WGS72_UTM_zone_1S
	     && PCSCode <= PCS_WGS72_UTM_zone_60S )
    {
	Datum = GCS_WGS_72;
	Proj = MapSys_UTM_South;
	nZone = PCSCode - PCS_WGS72_UTM_zone_1S + 1;
    }

    else if( PCSCode >= PCS_WGS72BE_UTM_zone_1N
	     && PCSCode <= PCS_WGS72BE_UTM_zone_60N )
    {
	Datum = GCS_WGS_72BE;
	Proj = MapSys_UTM_North;
	nZone = PCSCode - PCS_WGS72BE_UTM_zone_1N + 1;
    }
    else if( PCSCode >= PCS_WGS72BE_UTM_zone_1S
	     && PCSCode <= PCS_WGS72BE_UTM_zone_60S )
    {
	Datum = GCS_WGS_72BE;
	Proj = MapSys_UTM_South;
	nZone = PCSCode - PCS_WGS72BE_UTM_zone_1S + 1;
    }

    else if( PCSCode >= PCS_WGS84_UTM_zone_1N
	     && PCSCode <= PCS_WGS84_UTM_zone_60N )
    {
	Datum = GCS_WGS_84;
	Proj = MapSys_UTM_North;
	nZone = PCSCode - PCS_WGS84_UTM_zone_1N + 1;
    }
    else if( PCSCode >= PCS_WGS84_UTM_zone_1S
	     && PCSCode <= PCS_WGS84_UTM_zone_60S )
    {
	Datum = GCS_WGS_84;
	Proj = MapSys_UTM_South;
	nZone = PCSCode - PCS_WGS84_UTM_zone_1S + 1;
    }
    else if( PCSCode >= PCS_SAD69_UTM_zone_18N
	     && PCSCode <= PCS_SAD69_UTM_zone_22N )
    {
	Datum = KvUserDefined;
	Proj = MapSys_UTM_North;
	nZone = PCSCode - PCS_SAD69_UTM_zone_18N + 18;
    }
    else if( PCSCode >= PCS_SAD69_UTM_zone_17S
	     && PCSCode <= PCS_SAD69_UTM_zone_25S )
    {
	Datum = KvUserDefined;
	Proj = MapSys_UTM_South;
	nZone = PCSCode - PCS_SAD69_UTM_zone_17S + 17;
    }

/* -------------------------------------------------------------------- */
/*      State Plane zones, first we translate any PCS_ codes to		*/
/*	a Proj_ code that we can get a handle on.			*/
/* -------------------------------------------------------------------- */
    for( i = 0; StatePlaneTable[i] != KvUserDefined; i += 2 )
    {
	if( StatePlaneTable[i] == PCSCode )
	    PCSCode = StatePlaneTable[i+1];
    }

    if( PCSCode <= 15900 && PCSCode >= 10000 )
    {
	if( (PCSCode % 100) >= 30 )
        {
            Proj = MapSys_State_Plane_83;
	    Datum = GCS_NAD83;
        }
	else
        {
            Proj = MapSys_State_Plane_27;
	    Datum = GCS_NAD27;
        }

	nZone = PCSCode - 10000;
	if( Datum == GCS_NAD83 )
	    nZone -= 30;
    }

    if( pDatum != NULL )
        *pDatum = Datum;

    if( pZone != NULL )
        *pZone = nZone;

    return Proj;
}

/************************************************************************/
/*                          GTIFProjToMapSys()                          */
/************************************************************************/

/**
 * Translate a Proj_ code into a UTM or State Plane map system, and a zone
 * if possible.
 *
 * @param ProjCode The projection code (Proj_*) as would be stored in the
 * ProjectionGeoKey of a GeoTIFF file.
 * @param pZone Pointer to an integer into which the zone will be placed
 * if the function is successful.
 *
 * @return Returns either MapSys_UTM_North, MapSys_UTM_South,
 * MapSys_State_Plane_27, MapSys_State_Plane_83 or KvUserDefined.
 * KvUserDefined indicates that the
 * function failed to recognise the projection as UTM or State Plane.
 *
 * The zone value is only set if the return code is other than KvUserDefined.
 * For utm map system the returned zone will be between 1 and 60.  For
 * State Plane, the USGS state plane zone number is returned.  For instance,
 * Alabama East is zone 101.
 *
 * This function is useful to recognise UTM and State Plane coordinate
 * systems, and to extract zone numbers so the projections can be
 * represented as UTM rather than as the underlying projection method such
 * Transverse Mercator for instance.
 */

int GTIFProjToMapSys( int ProjCode, int * pZone )

{
    int		nZone = KvUserDefined;
    int		MapSys = KvUserDefined;

/* -------------------------------------------------------------------- */
/*      Handle UTM.                                                     */
/* -------------------------------------------------------------------- */
    if( ProjCode >= Proj_UTM_zone_1N && ProjCode <= Proj_UTM_zone_60N )
    {
	MapSys = MapSys_UTM_North;
	nZone = ProjCode - Proj_UTM_zone_1N + 1;
    }
    else if( ProjCode >= Proj_UTM_zone_1S && ProjCode <= Proj_UTM_zone_60S )
    {
	MapSys = MapSys_UTM_South;
	nZone = ProjCode - Proj_UTM_zone_1S + 1;
    }

/* -------------------------------------------------------------------- */
/*      Handle State Plane.  I think there are some anomalies in        */
/*      here, so this is a bit risky.                                   */
/* -------------------------------------------------------------------- */
    else if( ProjCode >= 10101 && ProjCode <= 15299 )
    {
        if( ProjCode % 100 >= 30 )
        {
            MapSys = MapSys_State_Plane_83;
            nZone = ProjCode - 10000 - 30;
        }
        else
        {
            MapSys = MapSys_State_Plane_27;
            nZone = ProjCode - 10000;
        }
    }

    if( pZone != NULL )
        *pZone = nZone;

    return MapSys;
}

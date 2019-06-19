/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implement ERMapper projection conversions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at spatialys.com>
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
 ****************************************************************************/

#include "cpl_port.h"
#include "ogr_srs_api.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OSRImportFromERM()                           */
/************************************************************************/

/**
 * \brief Create OGR WKT from ERMapper projection definitions.
 *
 * This function is the same as OGRSpatialReference::importFromERM().
 */

OGRErr OSRImportFromERM( OGRSpatialReferenceH hSRS, const char *pszProj,
                         const char *pszDatum, const char *pszUnits )

{
    VALIDATE_POINTER1( hSRS, "OSRImportFromERM", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        importFromERM(pszProj, pszDatum, pszUnits);
}

/************************************************************************/
/*                           importFromERM()                            */
/************************************************************************/

/**
 * Create OGR WKT from ERMapper projection definitions.
 *
 * Generates an OGRSpatialReference definition from an ERMapper datum
 * and projection name.  Based on the ecw_cs.wkt dictionary file from
 * gdal/data.
 *
 * @param pszProj the projection name, such as "NUTM11" or "GEOGRAPHIC".
 * @param pszDatum the datum name, such as "NAD83".
 * @param pszUnits the linear units "FEET" or "METERS".
 *
 * @return OGRERR_NONE on success or OGRERR_UNSUPPORTED_SRS if not found.
 */

OGRErr OGRSpatialReference::importFromERM( const char *pszProj,
                                           const char *pszDatum,
                                           const char *pszUnits )

{
    Clear();

/* -------------------------------------------------------------------- */
/*      do we have projection and datum?                                */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszProj, "RAW") )
        return OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      Do we have an EPSG coordinate system?                           */
/* -------------------------------------------------------------------- */

    if( STARTS_WITH_CI(pszProj, "EPSG:") )
        return importFromEPSG( atoi(pszProj+5) );

    if( STARTS_WITH_CI(pszDatum, "EPSG:") )
        return importFromEPSG( atoi(pszDatum+5) );


    CPLString osGEOGCS = lookupInDict( "ecw_cs.wkt", pszDatum );
    if( osGEOGCS.empty() )
        return OGRERR_UNSUPPORTED_SRS;

/* -------------------------------------------------------------------- */
/*      Set projection if we have it.                                   */
/* -------------------------------------------------------------------- */
    if( !EQUAL(pszProj, "GEODETIC") )
    {
        CPLString osProjWKT = lookupInDict( "ecw_cs.wkt", pszProj );
        if( osProjWKT.empty() || osProjWKT.back() != ']' )
            return OGRERR_UNSUPPORTED_SRS;

        if( osProjWKT.find("LOCAL_CS[") == 0 )
        {
            return importFromWkt(osProjWKT);
        }

        // Remove trailing ]
        osProjWKT.resize(osProjWKT.size() - 1);

        // Remove any UNIT
        auto nPos = osProjWKT.find(",UNIT");
        if( nPos != std::string::npos )
        {
            osProjWKT.resize(nPos);
        }

        // Insert GEOGCS
        nPos = osProjWKT.find(",PROJECTION");
        if( nPos == std::string::npos )
            return OGRERR_UNSUPPORTED_SRS;

        osProjWKT = osProjWKT.substr(0, nPos) + ',' + osGEOGCS + osProjWKT.substr(nPos);

        if( EQUAL(pszUnits, "FEET") )
            osProjWKT += ",UNIT[\"Foot_US\",0.3048006096012192]]";
        else
            osProjWKT += ",UNIT[\"Metre\",1.0]]";

        return importFromWkt(osProjWKT);
    }
    else
    {
        return importFromWkt(osGEOGCS);
    }
}

/************************************************************************/
/*                          OSRExportToERM()                            */
/************************************************************************/
/**
 * \brief Convert coordinate system to ERMapper format.
 *
 * This function is the same as OGRSpatialReference::exportToERM().
 */
OGRErr OSRExportToERM( OGRSpatialReferenceH hSRS,
                       char *pszProj, char *pszDatum, char *pszUnits )

{
    VALIDATE_POINTER1( hSRS, "OSRExportToERM", OGRERR_FAILURE );

    return reinterpret_cast<OGRSpatialReference *>(hSRS)->
        exportToERM(pszProj, pszDatum, pszUnits);
}

/************************************************************************/
/*                            exportToERM()                             */
/************************************************************************/

/**
 * Convert coordinate system to ERMapper format.
 *
 * @param pszProj 32 character buffer to receive projection name.
 * @param pszDatum 32 character buffer to receive datum name.
 * @param pszUnits 32 character buffer to receive units name.
 *
 * @return OGRERR_NONE on success, OGRERR_SRS_UNSUPPORTED if not translation is
 * found, or OGRERR_FAILURE on other failures.
 */

OGRErr OGRSpatialReference::exportToERM( char *pszProj, char *pszDatum,
                                         char *pszUnits )

{
    const int BUFFER_SIZE = 32;
    strcpy( pszProj, "RAW" );
    strcpy( pszDatum, "RAW" );
    strcpy( pszUnits, "METERS" );

    if( !IsProjected() && !IsGeographic() )
        return OGRERR_UNSUPPORTED_SRS;

/* -------------------------------------------------------------------- */
/*      Try to find the EPSG code.                                      */
/* -------------------------------------------------------------------- */
    int nEPSGCode = 0;

    if( IsProjected() )
    {
        const char *pszAuthName = GetAuthorityName( "PROJCS" );

        if( pszAuthName != nullptr && EQUAL(pszAuthName, "epsg") )
        {
            nEPSGCode = atoi(GetAuthorityCode( "PROJCS" ));
        }
    }
    else if( IsGeographic() )
    {
        const char *pszAuthName = GetAuthorityName( "GEOGCS" );

        if( pszAuthName != nullptr && EQUAL(pszAuthName, "epsg") )
        {
            nEPSGCode = atoi(GetAuthorityCode( "GEOGCS" ));
        }
    }

/* -------------------------------------------------------------------- */
/*      Is our GEOGCS name already defined in ecw_cs.wkt?               */
/* -------------------------------------------------------------------- */
    const char *pszWKTDatum = GetAttrValue( "DATUM" );

    if( pszWKTDatum != nullptr
        && !lookupInDict( "ecw_cs.wkt", pszWKTDatum ).empty() )
    {
        strncpy( pszDatum, pszWKTDatum, BUFFER_SIZE );
        pszDatum[BUFFER_SIZE-1] = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Is this a "well known" geographic coordinate system?            */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszDatum, "RAW") )
    {
        int nEPSGGCSCode = GetEPSGGeogCS();

        if( nEPSGGCSCode == 4326 )
            strcpy( pszDatum, "WGS84" );

        else if( nEPSGGCSCode == 4322 )
            strcpy( pszDatum, "WGS72DOD" );

        else if( nEPSGGCSCode == 4267 )
            strcpy( pszDatum, "NAD27" );

        else if( nEPSGGCSCode == 4269 )
            strcpy( pszDatum, "NAD83" );

        else if( nEPSGGCSCode == 4277 )
            strcpy( pszDatum, "OSGB36" );

        else if( nEPSGGCSCode == 4278 )
            strcpy( pszDatum, "OSGB78" );

        else if( nEPSGGCSCode == 4201 )
            strcpy( pszDatum, "ADINDAN" );

        else if( nEPSGGCSCode == 4202 )
            strcpy( pszDatum, "AGD66" );

        else if( nEPSGGCSCode == 4203 )
            strcpy( pszDatum, "AGD84" );

        else if( nEPSGGCSCode == 4209 )
            strcpy( pszDatum, "ARC1950" );

        else if( nEPSGGCSCode == 4210 )
            strcpy( pszDatum, "ARC1960" );

        else if( nEPSGGCSCode == 4275 )
            strcpy( pszDatum, "NTF" );

        else if( nEPSGGCSCode == 4283 )
            strcpy( pszDatum, "GDA94" );

        else if( nEPSGGCSCode == 4284 )
            strcpy( pszDatum, "PULKOVO" );
    }

/* -------------------------------------------------------------------- */
/*      Are we working with a geographic (geodetic) coordinate system?  */
/* -------------------------------------------------------------------- */

    if( IsGeographic() )
    {
        if( EQUAL(pszDatum, "RAW") )
            return OGRERR_UNSUPPORTED_SRS;
        else
        {
            strcpy( pszProj, "GEODETIC" );
            return OGRERR_NONE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Is this a UTM projection?                                       */
/* -------------------------------------------------------------------- */
    int bNorth = FALSE;
    int nZone = 0;

    nZone = GetUTMZone( &bNorth );
    if( nZone > 0 )
    {
        if( EQUAL(pszDatum, "GDA94") && !bNorth && nZone >= 48 && nZone <= 58)
        {
            snprintf( pszProj, BUFFER_SIZE, "MGA%02d", nZone );
        }
        else
        {
            if( bNorth )
                snprintf( pszProj, BUFFER_SIZE, "NUTM%02d", nZone );
            else
                snprintf( pszProj, BUFFER_SIZE, "SUTM%02d", nZone );
        }
    }

/* -------------------------------------------------------------------- */
/*      Is our PROJCS name already defined in ecw_cs.wkt?               */
/* -------------------------------------------------------------------- */
    else
    {
        const char *pszPROJCS = GetAttrValue( "PROJCS" );

        if( pszPROJCS != nullptr
            && lookupInDict( "ecw_cs.wkt", pszPROJCS ).find("PROJCS") == 0 )
        {
            strncpy( pszProj, pszPROJCS, BUFFER_SIZE );
            pszProj[BUFFER_SIZE-1] = '\0';
        }
    }

/* -------------------------------------------------------------------- */
/*      If we have not translated it yet, but we have an EPSG code      */
/*      then use EPSG:n notation.                                       */
/* -------------------------------------------------------------------- */
    if( (EQUAL(pszDatum, "RAW") || EQUAL(pszProj, "RAW")) && nEPSGCode != 0 )
    {
        snprintf( pszProj, BUFFER_SIZE, "EPSG:%d", nEPSGCode );
        snprintf( pszDatum, BUFFER_SIZE, "EPSG:%d", nEPSGCode );
    }

/* -------------------------------------------------------------------- */
/*      Handle the units.                                               */
/* -------------------------------------------------------------------- */
    const double dfUnits = GetLinearUnits();

    if( fabs(dfUnits-0.3048) < 0.0001 )
        strcpy( pszUnits, "FEET" );
    else
        strcpy( pszUnits, "METERS" );

    if( EQUAL(pszProj, "RAW") )
        return OGRERR_UNSUPPORTED_SRS;

    return OGRERR_NONE;
}

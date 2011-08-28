/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implement ERMapper projection conversions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_spatialref.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

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
    VALIDATE_POINTER1( hSRS, "OSRImportFromERM", CE_Failure );

    return ((OGRSpatialReference *) hSRS)->importFromERM( pszProj,
                                                          pszDatum,
                                                          pszUnits );
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
    if( EQUAL(pszProj,"RAW") )
        return OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      Do we have an EPSG coordinate system?                           */
/* -------------------------------------------------------------------- */

    if( EQUALN(pszProj,"EPSG:",5) )
        return importFromEPSG( atoi(pszProj+5) );


    if( EQUALN(pszDatum,"EPSG:",5) )
        return importFromEPSG( atoi(pszDatum+5) );

/* -------------------------------------------------------------------- */
/*      Set projection if we have it.                                   */
/* -------------------------------------------------------------------- */
    OGRErr eErr;

    if( EQUAL(pszProj,"GEODETIC") )
    {
    }
    else
    {
        eErr = importFromDict( "ecw_cs.wkt", pszProj );
        if( eErr != OGRERR_NONE )
            return eErr;

        if( EQUAL(pszUnits,"FEET") )
            SetLinearUnits( SRS_UL_US_FOOT, atof(SRS_UL_US_FOOT_CONV));
        else
            SetLinearUnits( SRS_UL_METER, 1.0 );
    }

/* -------------------------------------------------------------------- */
/*      Set the geogcs.                                                 */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oGeogCS;

    eErr = oGeogCS.importFromDict( "ecw_cs.wkt", pszDatum );
    if( eErr != OGRERR_NONE )
    {
        Clear();
        return eErr;
    }

    if( !IsLocal() )
        CopyGeogCSFrom( &oGeogCS );

    return OGRERR_NONE;
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
    VALIDATE_POINTER1( hSRS, "OSRExportToERM", CE_Failure );

    return ((OGRSpatialReference *) hSRS)->exportToERM( pszProj, pszDatum,
                                                        pszUnits );
}

/************************************************************************/
/*                            exportToERM()                             */
/************************************************************************/

/**
 * Convert coordinate system to ERMapper format.
 *
 * @param pszProj 32 character buffer to receive projection name.
 * @param pszDatum 32 character buffer to recieve datum name.
 * @param pszUnits 32 character buffer to receive units name.
 *
 * @return OGRERR_NONE on success, OGRERR_SRS_UNSUPPORTED if not translation is
 * found, or OGRERR_FAILURE on other failures.
 */

OGRErr OGRSpatialReference::exportToERM( char *pszProj, char *pszDatum, 
                                         char *pszUnits )

{
    strcpy( pszProj, "RAW" );
    strcpy( pszDatum, "RAW" );
    strcpy( pszUnits, "METERS" );

    if( !IsProjected() && !IsGeographic() )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Try to find the EPSG code.                                      */
/* -------------------------------------------------------------------- */
    int nEPSGCode = 0;

    if( IsProjected() )
    {
        const char *pszAuthName = GetAuthorityName( "PROJCS" );

        if( pszAuthName != NULL && EQUAL(pszAuthName,"epsg") )
        {
            nEPSGCode = atoi(GetAuthorityCode( "PROJCS" ));
        }
    }
    else if( IsGeographic() )
    {
        const char *pszAuthName = GetAuthorityName( "GEOGCS" );

        if( pszAuthName != NULL && EQUAL(pszAuthName,"epsg") )
        {
            nEPSGCode = atoi(GetAuthorityCode( "GEOGCS" ));
        }
    }

/* -------------------------------------------------------------------- */
/*      Is our GEOGCS name already defined in ecw_cs.dat?               */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRSWork;
    const char *pszWKTDatum = GetAttrValue( "DATUM" );

    if( pszWKTDatum != NULL 
        && oSRSWork.importFromDict( "ecw_cs.wkt", pszWKTDatum ) == OGRERR_NONE)
    {
        strncpy( pszDatum, pszWKTDatum, 32 );
        pszDatum[31] = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Is this a "well known" geographic coordinate system?            */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszDatum,"RAW") )
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
        if( EQUAL(pszDatum,"RAW") )
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
    int bNorth, nZone;

    nZone = GetUTMZone( &bNorth );
    if( nZone > 0 )
    {
        if( EQUAL(pszDatum,"GDA94") && !bNorth && nZone >= 48 && nZone <= 58)
        {
            sprintf( pszProj, "MGA%02d", nZone );
        }
        else
        {
            if( bNorth )
                sprintf( pszProj, "NUTM%02d", nZone );
            else
                sprintf( pszProj, "SUTM%02d", nZone );
        }
    }

/* -------------------------------------------------------------------- */
/*      Is our PROJCS name already defined in ecw_cs.dat?               */
/* -------------------------------------------------------------------- */
    else
    {
        const char *pszPROJCS = GetAttrValue( "PROJCS" );

        if( pszPROJCS != NULL 
            && oSRSWork.importFromDict( "ecw_cs.wkt", pszPROJCS ) == OGRERR_NONE 
            && oSRSWork.IsProjected() )
        {
            strncpy( pszProj, pszPROJCS, 32 );
            pszProj[31] = '\0';
        }
    }

/* -------------------------------------------------------------------- */
/*      If we have not translated it yet, but we have an EPSG code      */
/*      then use EPSG:n notation.                                       */
/* -------------------------------------------------------------------- */
    if( (EQUAL(pszDatum,"RAW") || EQUAL(pszProj,"RAW")) && nEPSGCode != 0 )
    {
        sprintf( pszProj, "EPSG:%d", nEPSGCode );
        sprintf( pszDatum, "EPSG:%d", nEPSGCode );
    }

/* -------------------------------------------------------------------- */
/*      Handle the units.                                               */
/* -------------------------------------------------------------------- */
    double dfUnits = GetLinearUnits();

    if( fabs(dfUnits-0.3048) < 0.0001 )
        strcpy( pszUnits, "FEET" );
    else
        strcpy( pszUnits, "METERS" );
       
    if( EQUAL(pszProj,"RAW") )
        return OGRERR_UNSUPPORTED_SRS;
    else
        return OGRERR_NONE;
}

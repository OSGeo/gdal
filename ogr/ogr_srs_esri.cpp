/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRSpatialReference translation to/from ESRI .prj definitions.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.11  2002/03/05 14:25:14  warmerda
 * expand tabs
 *
 * Revision 1.10  2001/11/09 21:06:40  warmerda
 * stripctparms may not results in null root
 *
 * Revision 1.9  2001/10/11 19:27:54  warmerda
 * worked on esri morphing
 *
 * Revision 1.8  2001/10/10 20:42:43  warmerda
 * added ESRI WKT morphing support
 *
 * Revision 1.7  2001/07/19 18:25:07  warmerda
 * expanded tabs
 *
 * Revision 1.6  2001/07/18 05:03:05  warmerda
 * added CPL_CVSID
 *
 * Revision 1.5  2001/03/16 22:15:48  warmerda
 * added support for reading WKT in importFromEPSG
 *
 * Revision 1.4  2001/01/26 14:56:11  warmerda
 * added Transverse Mercator .prj support
 *
 * Revision 1.3  2001/01/19 21:10:46  warmerda
 * replaced tabs
 *
 * Revision 1.2  2000/11/17 17:25:37  warmerda
 * added improved utm support
 *
 * Revision 1.1  2000/11/09 06:22:15  warmerda
 * New
 *
 */

#include "ogr_spatialref.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

char *apszProjMapping[] = {
    "Albers", SRS_PT_ALBERS_CONIC_EQUAL_AREA,
    "Cassini", SRS_PT_CASSINI_SOLDNER,
    "Hotine_Oblique_Mercator_Azimuth_Natural_Origin", 
                                        SRS_PT_HOTINE_OBLIQUE_MERCATOR,
    "Lambert_Conformal_Conic", SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP,
    "Van_der_Grinten_I", SRS_PT_VANDERGRINTEN,
    SRS_PT_TRANSVERSE_MERCATOR, SRS_PT_TRANSVERSE_MERCATOR,
    "Gauss_Kruger", SRS_PT_TRANSVERSE_MERCATOR,
    "Mercator", SRS_PT_MERCATOR_1SP,
    NULL, NULL }; 
 
char *apszArgMapping[] = {
    
    NULL, NULL }; 
 
char *apszDatumMapping[] = {
    "North_American_1927", SRS_DN_NAD27,
    "North_American_1983", SRS_DN_NAD83,
    NULL, NULL }; 
 

/************************************************************************/
/*                         OSRImportFromESRI()                          */
/************************************************************************/

OGRErr OSRImportFromESRI( OGRSpatialReferenceH hSRS, char **papszPrj )

{
    return ((OGRSpatialReference *) hSRS)->importFromESRI( papszPrj );
}

/************************************************************************/
/*                              OSR_GDV()                               */
/*                                                                      */
/*      Fetch a particular parameter out of the parameter list, or      */
/*      the indicated default if it isn't available.  This is a         */
/*      helper function for importFromESRI().                           */
/************************************************************************/

static double OSR_GDV( char **papszNV, const char * pszField, 
                       double dfDefaultValue )

{
    int         iLine;

    if( papszNV == NULL || papszNV[0] == NULL )
        return dfDefaultValue;

    if( EQUALN(pszField,"PARAM_",6) )
    {
        int     nOffset;

        for( iLine = 0; 
             papszNV[iLine] != NULL && !EQUALN(papszNV[iLine],"Paramet",7);
             iLine++ ) {}

        for( nOffset=atoi(pszField+6); 
             papszNV[iLine] != NULL && nOffset > 0; 
             nOffset--, iLine++ ) {}

        if( papszNV[iLine] != NULL )
        {
            char        **papszTokens, *pszLine = papszNV[iLine];
            double      dfValue;
            
            int         i;
            
            // Trim comments.
            for( i=0; pszLine[i] != '\0'; i++ )
            {
                if( pszLine[i] == '/' && pszLine[i+1] == '*' )
                    pszLine[i] = '\0';
            }

            papszTokens = CSLTokenizeString(papszNV[iLine]);
            if( CSLCount(papszTokens) == 3 )
            {
                dfValue = ABS(atof(papszTokens[0]))
                    + atof(papszTokens[1]) / 60.0
                    + atof(papszTokens[2]) / 3600.0;

                if( atof(papszTokens[0]) < 0.0 )
                    dfValue *= -1;
            }
            else
                dfValue = atof(papszTokens[0]);

            CSLDestroy( papszTokens );

            return dfValue;
        }
        else
            return dfDefaultValue;
    }
    else
    {
        for( iLine = 0; 
             papszNV[iLine] != NULL && 
                 !EQUALN(papszNV[iLine],pszField,strlen(pszField));
             iLine++ ) {}

        if( papszNV[iLine] == NULL )
            return dfDefaultValue;
        else
            return atof( papszNV[iLine] + strlen(pszField) );
    }
}

/************************************************************************/
/*                              OSR_GDS()                               */
/************************************************************************/

static const char*OSR_GDS( char **papszNV, const char * pszField, 
                           const char *pszDefaultValue )

{
    int         iLine;

    if( papszNV == NULL || papszNV[0] == NULL )
        return pszDefaultValue;

    for( iLine = 0; 
         papszNV[iLine] != NULL && 
             !EQUALN(papszNV[iLine],pszField,strlen(pszField));
         iLine++ ) {}

    if( papszNV[iLine] == NULL )
        return pszDefaultValue;
    else
    {
        static char     szResult[80];
        char    **papszTokens;
        
        papszTokens = CSLTokenizeString(papszNV[iLine]);

        if( CSLCount(papszTokens) > 1 )
            strncpy( szResult, papszTokens[1], sizeof(szResult));
        else
            strncpy( szResult, pszDefaultValue, sizeof(szResult));
        
        CSLDestroy( papszTokens );
        return szResult;
    }
}

/************************************************************************/
/*                          importFromESRI()                            */
/************************************************************************/

OGRErr OGRSpatialReference::importFromESRI( char **papszPrj )

{
    if( papszPrj == NULL || papszPrj[0] == NULL )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      Some newer ESRI products, like ArcPad, produce .prj files       */
/*      with WKT in them.  Check if that appears to be the case.        */
/*                                                                      */
/*      ESRI uses an odd datum naming scheme, so some further           */
/*      massaging may be required.                                      */
/* -------------------------------------------------------------------- */
    if( EQUALN(papszPrj[0],"GEOGCS",6)
        || EQUALN(papszPrj[0],"PROJCS",6)
        || EQUALN(papszPrj[0],"LOCAL_CS",8) )
    {
        char    *pszWKT;
        OGRErr  eErr;

        pszWKT = papszPrj[0];
        eErr = importFromWkt( &pszWKT );
        if( eErr == OGRERR_NONE )
            eErr = morphFromESRI();
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Operate on the basis of the projection name.                    */
/* -------------------------------------------------------------------- */
    const char *pszProj = OSR_GDS( papszPrj, "Projection", NULL );

    if( pszProj == NULL )
    {
        CPLDebug( "OGR_ESRI", "Can't find Projection\n" );
        return OGRERR_CORRUPT_DATA;
    }

    else if( EQUAL(pszProj,"GEOGRAPHIC") )
    {
    }
    
    else if( EQUAL(pszProj,"utm") )
    {
        if( (int) OSR_GDV( papszPrj, "zone", 0.0 ) != 0 )
        {
            double      dfYShift = OSR_GDV( papszPrj, "Yshift", 0.0 );

            SetUTM( (int) OSR_GDV( papszPrj, "zone", 0.0 ),
                    dfYShift >= 0.0 );
        }
        else
        {
            double      dfCentralMeridian, dfRefLat;
            int         nZone;

            dfCentralMeridian = OSR_GDV( papszPrj, "PARAM_1", 0.0 );
            dfRefLat = OSR_GDV( papszPrj, "PARAM_2", 0.0 );

            nZone = (int) ((dfCentralMeridian+183) / 6.0 + 0.0000001);
            SetUTM( nZone, dfRefLat >= 0.0 );
        }
    }

    else if( EQUAL(pszProj,"ALBERS") )
    {
        SetACEA( OSR_GDV( papszPrj, "PARAM_1", 0.0 ), 
                 OSR_GDV( papszPrj, "PARAM_2", 0.0 ), 
                 OSR_GDV( papszPrj, "PARAM_4", 0.0 ), 
                 OSR_GDV( papszPrj, "PARAM_3", 0.0 ), 
                 OSR_GDV( papszPrj, "PARAM_5", 0.0 ), 
                 OSR_GDV( papszPrj, "PARAM_6", 0.0 ) );
    }

    else if( EQUAL(pszProj,"EQUIDISTANT_CONIC") )
    {
        int     nStdPCount = (int) OSR_GDV( papszPrj, "PARAM_1", 0.0 );

        if( nStdPCount == 1 )
        {
            SetEC( OSR_GDV( papszPrj, "PARAM_2", 0.0 ), 
                   OSR_GDV( papszPrj, "PARAM_2", 0.0 ), 
                   OSR_GDV( papszPrj, "PARAM_4", 0.0 ), 
                   OSR_GDV( papszPrj, "PARAM_3", 0.0 ), 
                   OSR_GDV( papszPrj, "PARAM_5", 0.0 ), 
                   OSR_GDV( papszPrj, "PARAM_6", 0.0 ) );
        }
        else
        {
            SetEC( OSR_GDV( papszPrj, "PARAM_2", 0.0 ), 
                   OSR_GDV( papszPrj, "PARAM_3", 0.0 ), 
                   OSR_GDV( papszPrj, "PARAM_5", 0.0 ), 
                   OSR_GDV( papszPrj, "PARAM_4", 0.0 ), 
                   OSR_GDV( papszPrj, "PARAM_5", 0.0 ), 
                   OSR_GDV( papszPrj, "PARAM_7", 0.0 ) );
        }
    }

    else if( EQUAL(pszProj,"TRANSVERSE") )
    {
        SetTM( OSR_GDV( papszPrj, "PARAM_2", 0.0 ), 
               OSR_GDV( papszPrj, "PARAM_3", 0.0 ), 
               OSR_GDV( papszPrj, "PARAM_1", 0.0 ), 
               OSR_GDV( papszPrj, "PARAM_4", 0.0 ), 
               OSR_GDV( papszPrj, "PARAM_5", 0.0 ) );
    }

    else
    {
        CPLDebug( "OGR_ESRI", "Unsupported projection: %s", pszProj );
        SetLocalCS( pszProj );
    }

/* -------------------------------------------------------------------- */
/*      Try to translate the datum.                                     */
/* -------------------------------------------------------------------- */
    const char *pszValue;
    int        bFullDefined = FALSE;

    if( !IsLocal() )
    {
        pszValue = OSR_GDS( papszPrj, "Datum", "WGS84");
        if( EQUAL(pszValue,"NAD27") || EQUAL(pszValue,"NAD83")
            || EQUAL(pszValue,"WGS84") || EQUAL(pszValue,"WGS72") )
        {
            SetWellKnownGeogCS( pszValue );
            bFullDefined = TRUE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Linear units translation                                        */
/* -------------------------------------------------------------------- */
    if( IsLocal() || IsProjected() )
    {
        pszValue = OSR_GDS( papszPrj, "Units", NULL );
        if( pszValue == NULL )
            SetLinearUnits( SRS_UL_METER, 1.0 );
        else if( EQUAL(pszValue,"FEET") )
            SetLinearUnits( SRS_UL_FOOT, atof(SRS_UL_FOOT_CONV) );
        else
            SetLinearUnits( pszValue, 1.0 );
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                            morphToESRI()                             */
/*                                                                      */
/*      Modify this definition to fit better with the ESRI concept      */
/*      of WKT format.                                                  */
/************************************************************************/

/**
 * Convert in place from ESRI WKT format.
 *
 * The value notes of this coordinate system as modified in various manners
 * to adhere more closely to the WKT standard.  This mostly involves
 * translating a variety of ESRI names for projections, arguments and
 * datums to "standard" names, as defined by Adam Gawne-Cain's reference
 * translation of EPSG to WKT for the CT specification.
 *
 * This does the same as the C function OSRMorphToESRI().
 *
 * @return OGRERR_NONE unless something goes badly wrong.
 */

OGRErr OGRSpatialReference::morphToESRI()

{
    OGRErr      eErr;

/* -------------------------------------------------------------------- */
/*      Strip all CT parameters (AXIS, AUTHORITY, TOWGS84, etc).        */
/* -------------------------------------------------------------------- */
    eErr = StripCTParms();
    if( eErr != OGRERR_NONE )
        return eErr;

    if( GetRoot() == NULL )
        return OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      Translate PROJECTION keywords that are misnamed.                */
/* -------------------------------------------------------------------- */
    GetRoot()->applyRemapper( "PROJECTION", 
                              apszProjMapping+1, apszProjMapping, 2 );

/* -------------------------------------------------------------------- */
/*      Translate DATUM keywords that are misnamed.                     */
/* -------------------------------------------------------------------- */
    GetRoot()->applyRemapper( "DATUM", 
                              apszDatumMapping+1, apszDatumMapping, 2 );

/* -------------------------------------------------------------------- */
/*      Try to insert a D_ in front of the datum name.                  */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poDatum;

    poDatum = GetAttrNode( "DATUM" );
    if( poDatum != NULL )
        poDatum = poDatum->GetChild(0);

    if( poDatum != NULL )
    {
        if( !EQUALN(poDatum->GetValue(),"D_",2) )
        {
            char *pszNewValue;

            pszNewValue = (char *) CPLMalloc(strlen(poDatum->GetValue())+3);
            strcpy( pszNewValue, "D_" );
            strcat( pszNewValue, poDatum->GetValue() );
            poDatum->SetValue( pszNewValue );
            CPLFree( pszNewValue );
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           OSRMorphToESRI()                           */
/************************************************************************/

OGRErr OSRMorphToESRI( OGRSpatialReferenceH hSRS )

{
    return ((OGRSpatialReference *) hSRS)->morphFromESRI();
}

/************************************************************************/
/*                           morphFromESRI()                            */
/*                                                                      */
/*      modify this definition from the ESRI definition of WKT to       */
/*      the "Standard" definition.                                      */
/************************************************************************/

/**
 * Convert in place to ESRI WKT format.
 *
 * The value nodes of this coordinate system as modified in various manners
 * more closely map onto the ESRI concept of WKT format.  This includes
 * renaming a variety of projections and arguments, and stripping out 
 * nodes note recognised by ESRI (like AUTHORITY and AXIS). 
 *
 * This does the same as the C function OSRMorphFromESRI().
 *
 * @return OGRERR_NONE unless something goes badly wrong.
 */

OGRErr OGRSpatialReference::morphFromESRI()

{
    OGRErr      eErr = OGRERR_NONE;

    if( GetRoot() == NULL )
        return OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      Try to remove any D_ in front of the datum name.                */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poDatum;

    poDatum = GetAttrNode( "DATUM" );
    if( poDatum != NULL )
        poDatum = poDatum->GetChild(0);

    if( poDatum != NULL )
    {
        if( EQUALN(poDatum->GetValue(),"D_",2) )
        {
            char *pszNewValue = CPLStrdup( poDatum->GetValue() + 2 );
            poDatum->SetValue( pszNewValue );
            CPLFree( pszNewValue );
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate PROJECTION keywords that are misnamed.                */
/* -------------------------------------------------------------------- */
    GetRoot()->applyRemapper( "PROJECTION", 
                              apszProjMapping, apszProjMapping+1, 2 );

/* -------------------------------------------------------------------- */
/*      Translate DATUM keywords that are misnamed.                     */
/* -------------------------------------------------------------------- */
    GetRoot()->applyRemapper( "DATUM", 
                              apszDatumMapping, apszDatumMapping+1, 2 );

    return eErr;
}

/************************************************************************/
/*                          OSRMorphFromESRI()                          */
/************************************************************************/

OGRErr OSRMorphFromESRI( OGRSpatialReferenceH hSRS )

{
    return ((OGRSpatialReference *) hSRS)->morphFromESRI();
}


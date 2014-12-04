/**********************************************************************
 * $Id: mitab_coordsys.cpp,v 1.42 2011-06-11 00:35:00 fwarmerdam Exp $
 *
 * Name:     mitab_coordsys.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation translation between MIF CoordSys format, and
 *           and OGRSpatialRef format.
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
 * $Log: mitab_coordsys.cpp,v $
 * Revision 1.42  2011-06-11 00:35:00  fwarmerdam
 * add support for reading google mercator (#4115)
 *
 * Revision 1.41  2010-10-07 18:46:26  aboudreault
 * Fixed bad use of CPLAtof when locale setting doesn't use . for float (GDAL bug #3775)
 *
 * Revision 1.40  2010-09-07 16:48:08  aboudreault
 * Removed incomplete patch for affine params support in mitab. (bug 1155)
 *
 * Revision 1.39  2010-07-07 19:00:15  aboudreault
 * Cleanup Win32 Compile Warnings (GDAL bug #2930)
 *
 * Revision 1.38  2010-07-05 18:32:48  aboudreault
 * Fixed memory leaks in mitab_capi.cpp and mitab_coordsys.cpp
 *
 * Revision 1.37  2010-07-05 17:20:14  aboudreault
 * Added Krovak projection suppoprt (bug 2230)
 *
 * Revision 1.36  2007-11-21 21:15:45  dmorissette
 * Fix asDatumInfoList[] and asSpheroidInfoList[] defns/refs (bug 1826)
 *
 * Revision 1.35  2007/06/21 13:23:43  fwarmerdam
 * Fixed support for predefined datums with non-greenwich prime meridians
 *
 * Revision 1.34  2006/03/10 19:50:45  fwarmerdam
 * Coordsys false easting and northing are in the units of the coordsys, not
 * necessarily meters.  Adjusted mitab_coordsys.cpp to reflect this.
 * http://bugzilla.remotesensing.org/show_bug.cgi?id=1113
 *
 * Revision 1.33  2005/09/29 20:13:57  dmorissette
 * MITABCoordSys2SpatialRef() patches from Anthony D (bug 1155):
 * Improved support for modified TM projections 21-24.
 * Added support for affine parameters (inside #ifdef MITAB_AFFINE_PARAMS since
 * affine params cannot be stored directly in OGRSpatialReference)
 *
 * Revision 1.32  2005/08/07 21:00:38  fwarmerdam
 * Initialize adfDatumParm[] to avoid warnings with gcc 4.
 *
 * Revision 1.31  2005/05/12 22:07:52  dmorissette
 * Improved handling of Danish modified TM proj#21-24 (hss, bugs 976,1010)
 *
 * Revision 1.30  2005/03/22 23:24:54  dmorissette
 * Added support for datum id in .MAP header (bug 910)
 *
 * Revision 1.29  2004/06/03 19:36:53  fwarmerdam
 * fixed memory leak processing non-earth coordsys
 *
 * Revision 1.28  2003/03/21 14:20:42  warmerda
 * fixed up regional mercator handling, was screwing up transverse mercator
 *
 * Revision 1.27  2003/01/09 17:33:26  warmerda
 * fixed ellipsoid extraction for datum 999/9999
 *
 * Revision 1.26  2002/12/12 20:12:18  warmerda
 * fixed signs of rotational parameters for TOWGS84 in WKT
 *
 * Revision 1.25  2002/10/15 14:33:30  warmerda
 * Added untested support in mitab_spatialref.cpp, and mitab_coordsys.cpp for
 * projections Regional Mercator (26), Polyconic (27), Azimuthal Equidistant -
 * All origin latitudes (28), and Lambert Azimuthal Equal Area - any aspect (29).
 *
 * Revision 1.24  2002/09/23 13:16:04  warmerda
 * fixed leak in MITABExtractCoordSysBounds()
 *
 * Revision 1.23  2002/04/01 19:49:24  warmerda
 * added support for cassini/soldner - proj 30
 *
 * Revision 1.22  2002/03/01 19:00:15  warmerda
 * False Easting/Northing should be in the linear units of measure in MapInfo,
 * but in OGRSpatialReference/WKT they are always in meters.  Convert accordingly.
 *
 * Revision 1.21  2001/04/04 21:43:19  warmerda
 * added code to set WGS84 values
 *
 * Revision 1.20  2001/01/23 21:23:42  daniel
 * Added projection bounds lookup table, called from TABFile::SetProjInfo()
 *
 * Revision 1.19  2001/01/22 16:00:53  warmerda
 * reworked swiss projection support
 *
 * Revision 1.18  2001/01/19 21:56:18  warmerda
 * added untested support for Swiss Oblique Mercator
 **********************************************************************/

#include "mitab.h"
#include "mitab_utils.h"

extern const MapInfoDatumInfo asDatumInfoList[];
extern const MapInfoSpheroidInfo asSpheroidInfoList[];

/************************************************************************/
/*                      MITABCoordSys2SpatialRef()                      */
/*                                                                      */
/*      Convert a MIF COORDSYS string into a new OGRSpatialReference    */
/*      object.                                                         */
/************************************************************************/

OGRSpatialReference *MITABCoordSys2SpatialRef( const char * pszCoordSys )

{
    TABProjInfo sTABProj;
    if(MITABCoordSys2TABProjInfo(pszCoordSys, &sTABProj) < 0 )
        return NULL;
    OGRSpatialReference* poSR = TABFile::GetSpatialRefFromTABProj(sTABProj);

/* -------------------------------------------------------------------- */
/*      Report on translation.                                          */
/* -------------------------------------------------------------------- */
    char        *pszWKT;

    poSR->exportToWkt( &pszWKT );
    if( pszWKT != NULL )
    {
        CPLDebug( "MITAB",
                  "This CoordSys value:\n%s\nwas translated to:\n%s\n",
                  pszCoordSys, pszWKT );
        CPLFree( pszWKT );
    }

    return poSR;
}

/************************************************************************/
/*                      MITABSpatialRef2CoordSys()                      */
/*                                                                      */
/*      Converts a OGRSpatialReference object into a MIF COORDSYS       */
/*      string.                                                         */
/*                                                                      */
/*      The function returns a newly allocated string that should be    */
/*      CPLFree()'d by the caller.                                      */
/************************************************************************/

char *MITABSpatialRef2CoordSys( OGRSpatialReference * poSR )

{
    if( poSR == NULL )
        return NULL;
    
    TABProjInfo     sTABProj;
    int             nParmCount;
    TABFile::GetTABProjFromSpatialRef(poSR, sTABProj, nParmCount);

/* -------------------------------------------------------------------- */
/*      Do coordsys lookup                                              */
/* -------------------------------------------------------------------- */
    double dXMin, dYMin, dXMax, dYMax;
    int bHasBounds = FALSE;
    if (sTABProj.nProjId > 1 &&
        MITABLookupCoordSysBounds(&sTABProj, dXMin, dYMin, dXMax, dYMax, TRUE) == TRUE)
    {
        bHasBounds = TRUE;
    }

/*-----------------------------------------------------------------
 * Translate the units
 *----------------------------------------------------------------*/
    const char  *pszMIFUnits = TABUnitIdToString(sTABProj.nUnitsId);
    
/* -------------------------------------------------------------------- */
/*      Build coordinate system definition.                             */
/* -------------------------------------------------------------------- */
    CPLString osCoordSys;

    if( sTABProj.nProjId != 0 )
    {
        osCoordSys.Printf(
                 "Earth Projection %d",
                 sTABProj.nProjId );

    }
    else
        osCoordSys.Printf(
                 "NonEarth Units" );

/* -------------------------------------------------------------------- */
/*      Append Datum                                                    */
/* -------------------------------------------------------------------- */
    if( sTABProj.nProjId != 0 )
    {
        osCoordSys += CPLSPrintf(
                 ", %d",
                 sTABProj.nDatumId );

        if( sTABProj.nDatumId == 999 || sTABProj.nDatumId == 9999 )
        {
            osCoordSys += CPLSPrintf(
                     ", %d, %.15g, %.15g, %.15g",
                     sTABProj.nEllipsoidId,
                     sTABProj.dDatumShiftX, sTABProj.dDatumShiftY, sTABProj.dDatumShiftZ );
        }
        
        if( sTABProj.nDatumId == 9999 )
        {
            osCoordSys += CPLSPrintf(
                     ", %.15g, %.15g, %.15g, %.15g, %.15g",
                     sTABProj.adDatumParams[0], sTABProj.adDatumParams[1], sTABProj.adDatumParams[2],
                     sTABProj.adDatumParams[3], sTABProj.adDatumParams[4] );
        }
    }

/* -------------------------------------------------------------------- */
/*      Append units.                                                   */
/* -------------------------------------------------------------------- */
    if( sTABProj.nProjId != 1 && pszMIFUnits != NULL )
    {
        if( sTABProj.nProjId != 0 )
            osCoordSys += "," ;
        
        osCoordSys += CPLSPrintf(
                 " \"%s\"",
                 pszMIFUnits );
    }

/* -------------------------------------------------------------------- */
/*      Append Projection Parms.                                        */
/* -------------------------------------------------------------------- */
    for( int iParm = 0; iParm < nParmCount; iParm++ )
        osCoordSys += CPLSPrintf(
                 ", %.15g",
                 sTABProj.adProjParams[iParm] );

/* -------------------------------------------------------------------- */
/*      Append user bounds                                              */
/* -------------------------------------------------------------------- */
    if (bHasBounds)
    {
        if( fabs(dXMin - (int)floor(dXMin+0.5)) < 1e-8 &&
            fabs(dYMin - (int)floor(dYMin+0.5)) < 1e-8 &&
            fabs(dXMax - (int)floor(dXMax+0.5)) < 1e-8 &&
            fabs(dYMax - (int)floor(dYMax+0.5)) < 1e-8 )
        {
            osCoordSys += CPLSPrintf(" Bounds (%d, %d) (%d, %d)",
                                     (int)dXMin, (int)dYMin, (int)dXMax, (int)dYMax);
        }
        else
        {
            osCoordSys += CPLSPrintf(" Bounds (%f, %f) (%f, %f)",
                                     dXMin, dYMin, dXMax, dYMax);
        }
    }

/* -------------------------------------------------------------------- */
/*      Report on translation                                           */
/* -------------------------------------------------------------------- */
    char        *pszWKT = NULL;

    poSR->exportToWkt( &pszWKT );
    if( pszWKT != NULL )
    {
        CPLDebug( "MITAB",
                  "This WKT Projection:\n%s\n\ntranslates to:\n%s\n",
                  pszWKT, osCoordSys.c_str() );
        CPLFree( pszWKT );
    }

    return( CPLStrdup( osCoordSys.c_str() ) );
}


/************************************************************************/
/*                      MITABExtractCoordSysBounds                      */
/*                                                                      */
/* Return TRUE if MIF coordsys string contains a BOUNDS parameter and   */
/* Set x/y min/max values.                                              */
/************************************************************************/

GBool MITABExtractCoordSysBounds( const char * pszCoordSys,
                                  double &dXMin, double &dYMin,
                                  double &dXMax, double &dYMax )

{
    char        **papszFields;

    if( pszCoordSys == NULL )
        return FALSE;
    
    papszFields = CSLTokenizeStringComplex( pszCoordSys, " ,()", TRUE, FALSE );

    int iBounds = CSLFindString( papszFields, "Bounds" );

    if (iBounds >= 0 && iBounds + 4 < CSLCount(papszFields))
    {
        dXMin = CPLAtof(papszFields[++iBounds]);
        dYMin = CPLAtof(papszFields[++iBounds]);
        dXMax = CPLAtof(papszFields[++iBounds]);
        dYMax = CPLAtof(papszFields[++iBounds]);
        CSLDestroy( papszFields );
        return TRUE;
    }

    CSLDestroy( papszFields );
    return FALSE;
}


/**********************************************************************
 *                     MITABCoordSys2TABProjInfo()
 *
 * Convert a MIF COORDSYS string into a TABProjInfo structure.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int MITABCoordSys2TABProjInfo(const char * pszCoordSys, TABProjInfo *psProj)

{
    char        **papszFields;

    // Set all fields to zero, equivalent of NonEarth Units "mi"
    memset(psProj, 0, sizeof(TABProjInfo));

    if( pszCoordSys == NULL )
        return -1;
    
    /*-----------------------------------------------------------------
     * Parse the passed string into words.
     *----------------------------------------------------------------*/
    while(*pszCoordSys == ' ') pszCoordSys++;  // Eat leading spaces
    if( EQUALN(pszCoordSys,"CoordSys",8) )
        pszCoordSys += 9;
    
    papszFields = CSLTokenizeStringComplex( pszCoordSys, " ,", TRUE, FALSE );

    /*-----------------------------------------------------------------
     * Clip off Bounds information.
     *----------------------------------------------------------------*/
    int         iBounds = CSLFindString( papszFields, "Bounds" );

    while( iBounds != -1 && papszFields[iBounds] != NULL )
    {
        CPLFree( papszFields[iBounds] );
        papszFields[iBounds] = NULL;
        iBounds++;
    }

    /*-----------------------------------------------------------------
     * Fetch the projection.
     *----------------------------------------------------------------*/
    char        **papszNextField;

    if( CSLCount( papszFields ) >= 3
        && EQUAL(papszFields[0],"Earth")
        && EQUAL(papszFields[1],"Projection") )
    {
        psProj->nProjId = (GByte)atoi(papszFields[2]);
        papszNextField = papszFields + 3;
    }
    else if (CSLCount( papszFields ) >= 2
             && EQUAL(papszFields[0],"NonEarth") )
    {
        // NonEarth Units "..." Bounds (x, y) (x, y)
        psProj->nProjId = 0;
        papszNextField = papszFields + 2;

        if( papszNextField[0] != NULL && EQUAL(papszNextField[0],"Units") )
            papszNextField++;
    }
    else
    {
        // Invalid projection string ???
        if (CSLCount(papszFields) > 0)
            CPLError(CE_Warning, CPLE_IllegalArg,
                     "Failed parsing CoordSys: '%s'", pszCoordSys);
        CSLDestroy(papszFields);
        return -1;
    }

    if (psProj->nProjId>=3000) psProj->nProjId -=3000;
    else if (psProj->nProjId>=2000) psProj->nProjId -=2000;
    else if (psProj->nProjId>=1000) psProj->nProjId -=1000;

    /*-----------------------------------------------------------------
     * Fetch the datum information.
     *----------------------------------------------------------------*/
    int         nDatum = 0;

    if( psProj->nProjId != 0 && CSLCount(papszNextField) > 0 )
    {
        nDatum = atoi(papszNextField[0]);
        papszNextField++;
    }

    if( (nDatum == 999 || nDatum == 9999)
        && CSLCount(papszNextField) >= 4 )
    {
        psProj->nEllipsoidId = (GByte)atoi(papszNextField[0]);
        psProj->dDatumShiftX = CPLAtof(papszNextField[1]);
        psProj->dDatumShiftY = CPLAtof(papszNextField[2]);
        psProj->dDatumShiftZ = CPLAtof(papszNextField[3]);
        papszNextField += 4;

        if( nDatum == 9999
            && CSLCount(papszNextField) >= 5 )
        {
            psProj->adDatumParams[0] = CPLAtof(papszNextField[0]);
            psProj->adDatumParams[1] = CPLAtof(papszNextField[1]);
            psProj->adDatumParams[2] = CPLAtof(papszNextField[2]);
            psProj->adDatumParams[3] = CPLAtof(papszNextField[3]);
            psProj->adDatumParams[4] = CPLAtof(papszNextField[4]);
            papszNextField += 5;
        }
    }
    else if (nDatum != 999 && nDatum != 9999)
    {
    /*-----------------------------------------------------------------
     * Find the datum, and collect it's parameters if possible.
     *----------------------------------------------------------------*/
        int         iDatum;
        const MapInfoDatumInfo *psDatumInfo = NULL;
        
        for(iDatum=0; asDatumInfoList[iDatum].nMapInfoDatumID != -1; iDatum++)
        {
            if( asDatumInfoList[iDatum].nMapInfoDatumID == nDatum )
            {
                psDatumInfo = asDatumInfoList + iDatum;
                break;
            }
        }

        if( asDatumInfoList[iDatum].nMapInfoDatumID == -1
            && nDatum != 999 && nDatum != 9999 )
        {
            /* use WGS84 */
            psDatumInfo = asDatumInfoList + 0;
        }

        if( psDatumInfo != NULL )
        {
            psProj->nEllipsoidId = (GByte)psDatumInfo->nEllipsoid;
            psProj->nDatumId = (GInt16)psDatumInfo->nMapInfoDatumID;
            psProj->dDatumShiftX = psDatumInfo->dfShiftX;
            psProj->dDatumShiftY = psDatumInfo->dfShiftY;
            psProj->dDatumShiftZ = psDatumInfo->dfShiftZ;
            psProj->adDatumParams[0] = psDatumInfo->dfDatumParm0;
            psProj->adDatumParams[1] = psDatumInfo->dfDatumParm1;
            psProj->adDatumParams[2] = psDatumInfo->dfDatumParm2;
            psProj->adDatumParams[3] = psDatumInfo->dfDatumParm3;
            psProj->adDatumParams[4] = psDatumInfo->dfDatumParm4;
        }
    }    

    /*-----------------------------------------------------------------
     * Fetch the units string.
     *----------------------------------------------------------------*/
    if( CSLCount(papszNextField) > 0 )
    {
        psProj->nUnitsId = (GByte)TABUnitIdFromString(papszNextField[0]);
        papszNextField++;
    }

    /*-----------------------------------------------------------------
     * Finally the projection parameters.
     *----------------------------------------------------------------*/
    for(int iParam=0; iParam < 6 && CSLCount(papszNextField) > 0; iParam++)
    {
        psProj->adProjParams[iParam] = CPLAtof(papszNextField[0]);
        papszNextField++;         
    }
    
    CSLDestroy(papszFields);

    return 0;
}


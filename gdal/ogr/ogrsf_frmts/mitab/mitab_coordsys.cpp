/**********************************************************************
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
 **********************************************************************/

#include "cpl_port.h"
#include "mitab.h"
#include "mitab_utils.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "mitab_priv.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

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
    OGRSpatialReference *poSR = TABFile::GetSpatialRefFromTABProj(sTABProj);

    // Report on translation.
    char *pszWKT = NULL;

    poSR->exportToWkt(&pszWKT);
    if( pszWKT != NULL )
    {
        CPLDebug("MITAB",
                 "This CoordSys value:\n%s\nwas translated to:\n%s",
                 pszCoordSys, pszWKT);
        CPLFree(pszWKT);
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

    TABProjInfo sTABProj;
    int nParmCount = 0;
    TABFile::GetTABProjFromSpatialRef(poSR, sTABProj, nParmCount);

    // Do coordsys lookup.
    double dXMin = 0.0;
    double dYMin = 0.0;
    double dXMax = 0.0;
    double dYMax = 0.0;
    bool bHasBounds = false;
    if( sTABProj.nProjId > 1 &&
        MITABLookupCoordSysBounds(&sTABProj,
                                  dXMin, dYMin,
                                  dXMax, dYMax, true) )
    {
        bHasBounds = true;
    }

    // Translate the units.
    const char *pszMIFUnits = TABUnitIdToString(sTABProj.nUnitsId);

    // Build coordinate system definition.
    CPLString osCoordSys;

    if( sTABProj.nProjId != 0 )
    {
        osCoordSys.Printf("Earth Projection %d", sTABProj.nProjId);
    }
    else
    {
        osCoordSys.Printf("NonEarth Units");
    }

    // Append Datum.
    if( sTABProj.nProjId != 0 )
    {
        osCoordSys += CPLSPrintf(", %d", sTABProj.nDatumId);

        if( sTABProj.nDatumId == 999 || sTABProj.nDatumId == 9999 )
        {
            osCoordSys +=
                CPLSPrintf(", %d, %.15g, %.15g, %.15g", sTABProj.nEllipsoidId,
                           sTABProj.dDatumShiftX, sTABProj.dDatumShiftY,
                           sTABProj.dDatumShiftZ);
        }

        if( sTABProj.nDatumId == 9999 )
        {
            osCoordSys +=
                CPLSPrintf(", %.15g, %.15g, %.15g, %.15g, %.15g",
                           sTABProj.adDatumParams[0], sTABProj.adDatumParams[1],
                           sTABProj.adDatumParams[2], sTABProj.adDatumParams[3],
                           sTABProj.adDatumParams[4]);
        }
    }

    // Append units.
    if( sTABProj.nProjId != 1 && pszMIFUnits != NULL )
    {
        if( sTABProj.nProjId != 0 )
            osCoordSys += ",";

        osCoordSys += CPLSPrintf(" \"%s\"", pszMIFUnits);
    }

    // Append Projection Parms.
    for( int iParm = 0; iParm < nParmCount; iParm++ )
        osCoordSys += CPLSPrintf(", %.15g", sTABProj.adProjParams[iParm]);

    // Append user bounds.
    if( bHasBounds )
    {
        if( fabs(dXMin - floor(dXMin + 0.5)) < 1e-8 &&
            fabs(dYMin - floor(dYMin + 0.5)) < 1e-8 &&
            fabs(dXMax - floor(dXMax + 0.5)) < 1e-8 &&
            fabs(dYMax - floor(dYMax + 0.5)) < 1e-8 )
        {
            osCoordSys +=
                CPLSPrintf(" Bounds (%d, %d) (%d, %d)",
                           static_cast<int>(dXMin), static_cast<int>(dYMin),
                           static_cast<int>(dXMax), static_cast<int>(dYMax));
        }
        else
        {
            osCoordSys += CPLSPrintf(" Bounds (%f, %f) (%f, %f)",
                                     dXMin, dYMin, dXMax, dYMax);
        }
    }

    // Report on translation.
    char *pszWKT = NULL;

    poSR->exportToWkt(&pszWKT);
    if( pszWKT != NULL )
    {
        CPLDebug("MITAB",
                 "This WKT Projection:\n%s\n\ntranslates to:\n%s",
                 pszWKT, osCoordSys.c_str());
        CPLFree(pszWKT);
    }

    return CPLStrdup(osCoordSys.c_str());
}

/************************************************************************/
/*                      MITABExtractCoordSysBounds                      */
/*                                                                      */
/* Return true if MIF coordsys string contains a BOUNDS parameter and   */
/* Set x/y min/max values.                                              */
/************************************************************************/

bool MITABExtractCoordSysBounds( const char * pszCoordSys,
                                 double &dXMin, double &dYMin,
                                 double &dXMax, double &dYMax )

{
    if( pszCoordSys == NULL )
        return false;

    char **papszFields =
        CSLTokenizeStringComplex(pszCoordSys, " ,()", TRUE, FALSE);

    int iBounds = CSLFindString(papszFields, "Bounds");

    if (iBounds >= 0 && iBounds + 4 < CSLCount(papszFields))
    {
        dXMin = CPLAtof(papszFields[++iBounds]);
        dYMin = CPLAtof(papszFields[++iBounds]);
        dXMax = CPLAtof(papszFields[++iBounds]);
        dYMax = CPLAtof(papszFields[++iBounds]);
        CSLDestroy(papszFields);
        return true;
    }

    CSLDestroy(papszFields);
    return false;
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
    // Set all fields to zero, equivalent of NonEarth Units "mi"
    memset(psProj, 0, sizeof(TABProjInfo));

    if( pszCoordSys == NULL )
        return -1;

    // Parse the passed string into words.
    while(*pszCoordSys == ' ')
        pszCoordSys++;  // Eat leading spaces.
    if( STARTS_WITH_CI(pszCoordSys, "CoordSys") && pszCoordSys[8] != '\0' )
        pszCoordSys += 9;

    char **papszFields =
        CSLTokenizeStringComplex(pszCoordSys, " ,", TRUE, FALSE);

    // Clip off Bounds information.
    int iBounds = CSLFindString(papszFields, "Bounds");

    while( iBounds != -1 && papszFields[iBounds] != NULL )
    {
        CPLFree( papszFields[iBounds] );
        papszFields[iBounds] = NULL;
        iBounds++;
    }

    // Fetch the projection.
    char **papszNextField = NULL;

    if( CSLCount(papszFields) >= 3 &&
        EQUAL(papszFields[0], "Earth") &&
        EQUAL(papszFields[1], "Projection") )
    {
        int nProjId = atoi(papszFields[2]);
        if (nProjId >= 3000) nProjId -= 3000;
        else if (nProjId >= 2000) nProjId -= 2000;
        else if (nProjId >= 1000) nProjId -= 1000;

        psProj->nProjId = static_cast<GByte>(nProjId);
        papszNextField = papszFields + 3;
    }
    else if (CSLCount(papszFields) >= 2 &&
             EQUAL(papszFields[0],"NonEarth") )
    {
        // NonEarth Units "..." Bounds (x, y) (x, y)
        psProj->nProjId = 0;
        papszNextField = papszFields + 2;

        if( papszNextField[0] != NULL && EQUAL(papszNextField[0], "Units") )
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

    // Fetch the datum information.
    int nDatum = 0;

    if( psProj->nProjId != 0 && CSLCount(papszNextField) > 0 )
    {
        nDatum = atoi(papszNextField[0]);
        papszNextField++;
    }

    if( (nDatum == 999 || nDatum == 9999) &&
        CSLCount(papszNextField) >= 4 )
    {
        psProj->nEllipsoidId = static_cast<GByte>(atoi(papszNextField[0]));
        psProj->dDatumShiftX = CPLAtof(papszNextField[1]);
        psProj->dDatumShiftY = CPLAtof(papszNextField[2]);
        psProj->dDatumShiftZ = CPLAtof(papszNextField[3]);
        papszNextField += 4;

        if( nDatum == 9999 &&
            CSLCount(papszNextField) >= 5 )
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
        // Find the datum, and collect it's parameters if possible.
        const MapInfoDatumInfo *psDatumInfo = NULL;

        int iDatum = 0;  // Used after for.
        for( ; asDatumInfoList[iDatum].nMapInfoDatumID != -1; iDatum++ )
        {
            if( asDatumInfoList[iDatum].nMapInfoDatumID == nDatum )
            {
                psDatumInfo = asDatumInfoList + iDatum;
                break;
            }
        }

        if( asDatumInfoList[iDatum].nMapInfoDatumID == -1 &&
            nDatum != 999 && nDatum != 9999 )
        {
            // Use WGS84.
            psDatumInfo = asDatumInfoList + 0;
        }

        if( psDatumInfo != NULL )
        {
            psProj->nEllipsoidId = static_cast<GByte>(psDatumInfo->nEllipsoid);
            psProj->nDatumId =
                static_cast<GInt16>(psDatumInfo->nMapInfoDatumID);
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

    // Fetch the units string.
    if( CSLCount(papszNextField) > 0 )
    {
        psProj->nUnitsId =
            static_cast<GByte>(TABUnitIdFromString(papszNextField[0]));
        papszNextField++;
    }

    // Finally the projection parameters.
    for(int iParam = 0; iParam < 6 && CSLCount(papszNextField) > 0; iParam++)
    {
        psProj->adProjParams[iParam] = CPLAtof(papszNextField[0]);
        papszNextField++;
    }

    CSLDestroy(papszFields);

    return 0;
}

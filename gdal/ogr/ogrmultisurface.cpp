/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRMultiSurface class.
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot com>
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

#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogr_api.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRMultiSurface()                           */
/************************************************************************/

/**
 * \brief Create an empty multi surface collection.
 */

OGRMultiSurface::OGRMultiSurface()
{
}

/************************************************************************/
/*                         ~OGRMultiSurface()                           */
/************************************************************************/

OGRMultiSurface::~OGRMultiSurface()
{
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRMultiSurface::getGeometryType() const

{
    if( getCoordinateDimension() == 3 )
        return wkbMultiSurfaceZ;
    else
        return wkbMultiSurface;
}

/************************************************************************/
/*                            getDimension()                            */
/************************************************************************/

int OGRMultiSurface::getDimension() const

{
    return 2;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRMultiSurface::getGeometryName() const

{
    return "MULTISURFACE";
}

/************************************************************************/
/*                          isCompatibleSubType()                       */
/************************************************************************/

OGRBoolean OGRMultiSurface::isCompatibleSubType( OGRwkbGeometryType eGeomType ) const
{
    return OGR_GT_IsSurface(eGeomType);
}

/************************************************************************/
/*                           importFromWkt()                            */
/*                                                                      */
/*      Instantiate from well known text format.                        */
/************************************************************************/

OGRErr OGRMultiSurface::importFromWkt( char ** ppszInput )

{
    int bHasZ = FALSE, bHasM = FALSE;
    OGRErr      eErr = importPreambuleFromWkt(ppszInput, &bHasZ, &bHasM);
    if( eErr >= 0 )
        return eErr;

    if( bHasZ )
        setCoordinateDimension(3);

    int bIsMultiSurface = (wkbFlatten(getGeometryType()) == wkbMultiSurface);

    char        szToken[OGR_WKT_TOKEN_MAX];
    const char  *pszInput = *ppszInput;
    eErr = OGRERR_NONE;

    /* Skip first '(' */
    pszInput = OGRWktReadToken( pszInput, szToken );

/* ==================================================================== */
/*      Read each surface in turn.  Note that we try to reuse the same  */
/*      point list buffer from ring to ring to cut down on              */
/*      allocate/deallocate overhead.                                   */
/* ==================================================================== */
    OGRRawPoint *paoPoints = NULL;
    int          nMaxPoints = 0;
    double      *padfZ = NULL;

    do
    {

    /* -------------------------------------------------------------------- */
    /*      Get the first token, which should be the geometry type.         */
    /* -------------------------------------------------------------------- */
        const char* pszInputBefore = pszInput;
        pszInput = OGRWktReadToken( pszInput, szToken );

        OGRSurface* poSurface;

    /* -------------------------------------------------------------------- */
    /*      Do the import.                                                  */
    /* -------------------------------------------------------------------- */
        if (EQUAL(szToken,"("))
        {
            OGRPolygon      *poPolygon = new OGRPolygon();
            poSurface = poPolygon;
            pszInput = pszInputBefore;
            eErr = poPolygon->importFromWKTListOnly( (char**)&pszInput, bHasZ, bHasM,
                                                     paoPoints, nMaxPoints, padfZ );
        }
        else if (EQUAL(szToken, "EMPTY") )
        {
            poSurface = new OGRPolygon();
        }
        /* We accept POLYGON() but this is an extension to the BNF, also */
        /* accepted by PostGIS */
        else if (bIsMultiSurface &&
                 (EQUAL(szToken,"POLYGON") ||
                  EQUAL(szToken,"CURVEPOLYGON")))
        {
            OGRGeometry* poGeom = NULL;
            pszInput = pszInputBefore;
            eErr = OGRGeometryFactory::createFromWkt( (char **) &pszInput,
                                                       NULL, &poGeom );
            poSurface = (OGRSurface*) poGeom;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Unexpected token : %s", szToken);
            eErr = OGRERR_CORRUPT_DATA;
            break;
        }

        if( eErr == OGRERR_NONE )
            eErr = addGeometryDirectly( poSurface );
        if( eErr != OGRERR_NONE )
        {
            delete poSurface;
            break;
        }

/* -------------------------------------------------------------------- */
/*      Read the delimeter following the surface.                       */
/* -------------------------------------------------------------------- */
        pszInput = OGRWktReadToken( pszInput, szToken );

    } while( szToken[0] == ',' && eErr == OGRERR_NONE );

    CPLFree( paoPoints );
    CPLFree( padfZ );

/* -------------------------------------------------------------------- */
/*      freak if we don't get a closing bracket.                        */
/* -------------------------------------------------------------------- */

    if( eErr != OGRERR_NONE )
        return eErr;

    if( szToken[0] != ')' )
        return OGRERR_CORRUPT_DATA;
    
    *ppszInput = (char *) pszInput;
    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkt()                             */
/************************************************************************/

OGRErr OGRMultiSurface::exportToWkt( char ** ppszDstText,
                                     CPL_UNUSED OGRwkbVariant eWkbVariant ) const

{
    return exportToWktInternal( ppszDstText, wkbVariantIso, "POLYGON" );
}

/************************************************************************/
/*                         hasCurveGeometry()                           */
/************************************************************************/

OGRBoolean OGRMultiSurface::hasCurveGeometry(int bLookForNonLinear) const
{
    if( bLookForNonLinear )
        return OGRGeometryCollection::hasCurveGeometry(TRUE);
    return TRUE;
}

/************************************************************************/
/*                            PointOnSurface()                          */
/************************************************************************/

/** \brief This method relates to the SFCOM IMultiSurface::get_PointOnSurface() method.
 *
 * NOTE: Only implemented when GEOS included in build.
 *
 * @param poPoint point to be set with an internal point. 
 *
 * @return OGRERR_NONE if it succeeds or OGRERR_FAILURE otherwise. 
 */

OGRErr OGRMultiSurface::PointOnSurface( OGRPoint * poPoint ) const
{
    OGRMultiPolygon* poMPoly = (OGRMultiPolygon*) getLinearGeometry();
    OGRErr ret = poMPoly->PointOnSurface(poPoint);
    delete poMPoly;
    return ret;
}

/************************************************************************/
/*                         CastToMultiPolygon()                         */
/************************************************************************/

/**
 * \brief Cast to multipolygon.
 *
 * This method should only be called if the multisurface actually only contains
 * instances of OGRPolygon. This can be verified if hasCurveGeometry(TRUE)
 * returns FALSE. It is not intended to approximate curve polygons. For that
 * use getLinearGeometry().
 * 
 * The passed in geometry is consumed and a new one returned (or NULL in case
 * of failure). 
 * 
 * @param poMS the input geometry - ownership is passed to the method.
 * @return new geometry.
 */

OGRMultiPolygon* OGRMultiSurface::CastToMultiPolygon(OGRMultiSurface* poMS)
{
    for(int i=0;i<poMS->nGeomCount;i++)
    {
        poMS->papoGeoms[i] = OGRSurface::CastToPolygon( (OGRSurface*)poMS->papoGeoms[i] );
        if( poMS->papoGeoms[i] == NULL )
        {
            delete poMS;
            return NULL;
        }
    }
    return (OGRMultiPolygon*) TransferMembersAndDestroy(poMS, new OGRMultiPolygon());
}

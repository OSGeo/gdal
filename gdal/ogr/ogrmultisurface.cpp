/******************************************************************************
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

#include "cpl_port.h"
#include "ogr_geometry.h"

#include <cstddef>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_p.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          OGRMultiSurface()                           */
/************************************************************************/

/**
 * \brief Create an empty multi surface collection.
 */

OGRMultiSurface::OGRMultiSurface() {}

/************************************************************************/
/*                         ~OGRMultiSurface()                           */
/************************************************************************/

OGRMultiSurface::~OGRMultiSurface() {}

/************************************************************************/
/*              OGRMultiSurface( const OGRMultiSurface& )               */
/************************************************************************/

/**
 * \brief Copy constructor.
 *
 * Note: before GDAL 2.1, only the default implementation of the constructor
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRMultiSurface::OGRMultiSurface( const OGRMultiSurface& other ) :
    OGRGeometryCollection(other)
{}

/************************************************************************/
/*                  operator=( const OGRMultiCurve&)                    */
/************************************************************************/

/**
 * \brief Assignment operator.
 *
 * Note: before GDAL 2.1, only the default implementation of the operator
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRMultiSurface& OGRMultiSurface::operator=( const OGRMultiSurface& other )
{
    if( this != &other)
    {
        OGRGeometryCollection::operator=( other );
    }
    return *this;
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRMultiSurface::getGeometryType() const

{
    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
        return wkbMultiSurfaceZM;
    else if( flags & OGR_G_MEASURED )
        return wkbMultiSurfaceM;
    else if( flags & OGR_G_3D )
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

OGRBoolean
OGRMultiSurface::isCompatibleSubType( OGRwkbGeometryType eGeomType ) const
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
    int bHasZ = FALSE;
    int bHasM = FALSE;
    bool bIsEmpty = false;
    OGRErr eErr = importPreambuleFromWkt(ppszInput, &bHasZ, &bHasM, &bIsEmpty);
    flags = 0;
    if( eErr != OGRERR_NONE )
        return eErr;
    if( bHasZ ) flags |= OGR_G_3D;
    if( bHasM ) flags |= OGR_G_MEASURED;
    if( bIsEmpty )
        return OGRERR_NONE;

    char szToken[OGR_WKT_TOKEN_MAX] = {};
    const char *pszInput = *ppszInput;
    eErr = OGRERR_NONE;

    // Skip first '('.
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

        OGRSurface* poSurface = NULL;

    /* -------------------------------------------------------------------- */
    /*      Do the import.                                                  */
    /* -------------------------------------------------------------------- */
        if( EQUAL(szToken, "(") )
        {
            OGRPolygon *poPolygon = new OGRPolygon();
            poSurface = poPolygon;
            pszInput = pszInputBefore;
            eErr = poPolygon->importFromWKTListOnly(
                const_cast<char **>(&pszInput), bHasZ, bHasM,
                paoPoints, nMaxPoints, padfZ );
        }
        else if( EQUAL(szToken, "EMPTY") )
        {
            poSurface = new OGRPolygon();
        }
        // We accept POLYGON() but this is an extension to the BNF, also
        // accepted by PostGIS.
        else if( STARTS_WITH_CI(szToken, "POLYGON") ||
                 STARTS_WITH_CI(szToken, "CURVEPOLYGON") )
        {
            OGRGeometry* poGeom = NULL;
            pszInput = pszInputBefore;
            eErr = OGRGeometryFactory::createFromWkt(
                const_cast<char **>(&pszInput), NULL, &poGeom );
            poSurface = dynamic_cast<OGRSurface*>(poGeom);
            if( poSurface == NULL )
            {
                delete poGeom;
                eErr = OGRERR_CORRUPT_DATA;
                break;
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unexpected token : %s", szToken);
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
/*      Read the delimiter following the surface.                       */
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

    *ppszInput = const_cast<char *>(pszInput);
    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkt()                             */
/************************************************************************/

OGRErr OGRMultiSurface::exportToWkt( char ** ppszDstText,
                                     OGRwkbVariant /* eWkbVariant */ ) const

{
    return exportToWktInternal( ppszDstText, wkbVariantIso, "POLYGON" );
}

/************************************************************************/
/*                         hasCurveGeometry()                           */
/************************************************************************/

OGRBoolean OGRMultiSurface::hasCurveGeometry( int bLookForNonLinear ) const
{
    if( bLookForNonLinear )
        return OGRGeometryCollection::hasCurveGeometry(TRUE);
    return TRUE;
}

/************************************************************************/
/*                            PointOnSurface()                          */
/************************************************************************/

/** \brief This method relates to the SFCOM
 * IMultiSurface::get_PointOnSurface() method.
 *
 * NOTE: Only implemented when GEOS included in build.
 *
 * @param poPoint point to be set with an internal point.
 *
 * @return OGRERR_NONE if it succeeds or OGRERR_FAILURE otherwise.
 */

OGRErr OGRMultiSurface::PointOnSurface( OGRPoint * poPoint ) const
{
    return PointOnSurfaceInternal(poPoint);
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

OGRMultiPolygon* OGRMultiSurface::CastToMultiPolygon( OGRMultiSurface* poMS )
{
    for( int i = 0; i < poMS->nGeomCount; i++ )
    {
        OGRSurface* poSurface = dynamic_cast<OGRSurface *>(poMS->papoGeoms[i]);
        if( poSurface == NULL )
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                 "dynamic_cast failed.  Expected OGRSurface.");
            delete poMS;
            return NULL;
        }
        poMS->papoGeoms[i] = OGRSurface::CastToPolygon(poSurface);
        if( poMS->papoGeoms[i] == NULL )
        {
            delete poMS;
            return NULL;
        }
    }

    OGRGeometryCollection *poGC =
           TransferMembersAndDestroy(poMS, new OGRMultiPolygon());
    OGRMultiPolygon* poMultiPolygon = dynamic_cast<OGRMultiPolygon *>(poGC);
    if( poMultiPolygon == NULL )
    {
        CPLError(CE_Fatal, CPLE_AppDefined,
                 "dynamic_cast failed.  Expected OGRMultiPolygon.");
    }
    return poMultiPolygon;
}

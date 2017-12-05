/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRMultiPoint class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include <cstdio>
#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"
#include "ogr_core.h"
#include "ogr_p.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                           OGRMultiPoint()                            */
/************************************************************************/

/**
 * \brief Create an empty multi point collection.
 */

OGRMultiPoint::OGRMultiPoint() {}

/************************************************************************/
/*                OGRMultiPoint( const OGRMultiPoint& )                 */
/************************************************************************/

/**
 * \brief Copy constructor.
 *
 * Note: before GDAL 2.1, only the default implementation of the constructor
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRMultiPoint::OGRMultiPoint( const OGRMultiPoint& other ) :
    OGRGeometryCollection(other)
{}

/************************************************************************/
/*                          ~OGRMultiPoint()                            */
/************************************************************************/

OGRMultiPoint::~OGRMultiPoint() {}

/************************************************************************/
/*                  operator=( const OGRMultiPoint&)                    */
/************************************************************************/

/**
 * \brief Assignment operator.
 *
 * Note: before GDAL 2.1, only the default implementation of the operator
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRMultiPoint& OGRMultiPoint::operator=( const OGRMultiPoint& other )
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

OGRwkbGeometryType OGRMultiPoint::getGeometryType() const

{
    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
        return wkbMultiPointZM;
    else if( flags & OGR_G_MEASURED )
        return wkbMultiPointM;
    else if( flags & OGR_G_3D )
        return wkbMultiPoint25D;
    else
        return wkbMultiPoint;
}

/************************************************************************/
/*                            getDimension()                            */
/************************************************************************/

int OGRMultiPoint::getDimension() const

{
    return 0;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRMultiPoint::getGeometryName() const

{
    return "MULTIPOINT";
}

/************************************************************************/
/*                          isCompatibleSubType()                       */
/************************************************************************/

OGRBoolean
OGRMultiPoint::isCompatibleSubType( OGRwkbGeometryType eGeomType ) const
{
    return wkbFlatten(eGeomType) == wkbPoint;
}

/************************************************************************/
/*                            exportToWkt()                             */
/*                                                                      */
/*      Translate this structure into it's well known text format       */
/*      equivalent.  This could be made a lot more CPU efficient.       */
/************************************************************************/

OGRErr OGRMultiPoint::exportToWkt( char ** ppszDstText,
                                   OGRwkbVariant eWkbVariant ) const

{
    size_t nMaxString = static_cast<size_t>(getNumGeometries()) * 22 + 130;
    size_t nRetLen = 0;

/* -------------------------------------------------------------------- */
/*      Return MULTIPOINT EMPTY if we get no valid points.              */
/* -------------------------------------------------------------------- */
    if( IsEmpty() )
    {
        if( eWkbVariant == wkbVariantIso )
        {
            if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
                *ppszDstText = CPLStrdup("MULTIPOINT ZM EMPTY");
            else if( flags & OGR_G_MEASURED )
                *ppszDstText = CPLStrdup("MULTIPOINT M EMPTY");
            else if( flags & OGR_G_3D )
                *ppszDstText = CPLStrdup("MULTIPOINT Z EMPTY");
            else
                *ppszDstText = CPLStrdup("MULTIPOINT EMPTY");
        }
        else
            *ppszDstText = CPLStrdup("MULTIPOINT EMPTY");
        return OGRERR_NONE;
    }

    *ppszDstText = static_cast<char *>(VSI_MALLOC_VERBOSE( nMaxString ));
    if( *ppszDstText == NULL )
        return OGRERR_NOT_ENOUGH_MEMORY;

    if( eWkbVariant == wkbVariantIso )
    {
        if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
            snprintf( *ppszDstText, nMaxString, "%s ZM (", getGeometryName() );
        else if( flags & OGR_G_MEASURED )
            snprintf( *ppszDstText, nMaxString, "%s M (", getGeometryName() );
        else if( flags & OGR_G_3D )
            snprintf( *ppszDstText, nMaxString, "%s Z (", getGeometryName() );
        else
            snprintf( *ppszDstText, nMaxString, "%s (", getGeometryName() );
    }
    else
        snprintf( *ppszDstText, nMaxString, "%s (", getGeometryName() );

    bool bMustWriteComma = false;
    for( int i = 0; i < getNumGeometries(); i++ )
    {
        OGRPoint *poPoint = (OGRPoint *) getGeometryRef( i );

        if( poPoint->IsEmpty() )
        {
            CPLDebug("OGR",
                     "OGRMultiPoint::exportToWkt() - skipping POINT EMPTY.");
            continue;
        }

        if( bMustWriteComma )
            strcat( *ppszDstText + nRetLen, "," );
        bMustWriteComma = true;

        nRetLen += strlen(*ppszDstText + nRetLen);

        if( nMaxString < nRetLen + 100 )
        {
            nMaxString = nMaxString * 2;
            *ppszDstText =
                static_cast<char *>(CPLRealloc(*ppszDstText, nMaxString));
        }

        if( eWkbVariant == wkbVariantIso )
        {
            strcat( *ppszDstText + nRetLen, "(" );
            nRetLen++;
        }

        OGRMakeWktCoordinateM(
            *ppszDstText + nRetLen,
            poPoint->getX(),
            poPoint->getY(),
            poPoint->getZ(),
            poPoint->getM(),
            poPoint->Is3D(),
            poPoint->IsMeasured() && (eWkbVariant == wkbVariantIso));

        if( eWkbVariant == wkbVariantIso )
        {
            strcat( *ppszDstText + nRetLen, ")" );
            nRetLen++;
        }
    }

    strcat( *ppszDstText+nRetLen, ")" );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           importFromWkt()                            */
/************************************************************************/

OGRErr OGRMultiPoint::importFromWkt( char ** ppszInput )

{
    const char *pszInputBefore = *ppszInput;
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

    const char* pszPreScan = OGRWktReadToken( pszInput, szToken );
    OGRWktReadToken( pszPreScan, szToken );

    // Do we have an inner bracket?
    if( EQUAL(szToken,"(") || EQUAL(szToken, "EMPTY") )
    {
        *ppszInput = const_cast<char *>(pszInputBefore);
        return importFromWkt_Bracketed( ppszInput, bHasM, bHasZ );
    }

    if( bHasZ || bHasM )
    {
        return OGRERR_CORRUPT_DATA;
    }

/* -------------------------------------------------------------------- */
/*      Read the point list which should consist of exactly one point.  */
/* -------------------------------------------------------------------- */
    OGRRawPoint *paoPoints = NULL;
    double *padfZ = NULL;
    double *padfM = NULL;
    int flagsFromInput = flags;
    int nMaxPoint = 0;
    int nPointCount = 0;

    pszInput = OGRWktReadPointsM( pszInput, &paoPoints, &padfZ, &padfM,
                                  &flagsFromInput,
                                  &nMaxPoint, &nPointCount );
    if( pszInput == NULL )
    {
        CPLFree( paoPoints );
        CPLFree( padfZ );
        CPLFree( padfM );
        return OGRERR_CORRUPT_DATA;
    }
    if( (flagsFromInput & OGR_G_3D) && !(flags & OGR_G_3D) )
    {
        flags |= OGR_G_3D;
        bHasZ = TRUE;
    }
    if( (flagsFromInput & OGR_G_MEASURED) && !(flags & OGR_G_MEASURED) )
    {
        flags |= OGR_G_MEASURED;
        bHasM = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Transform raw points into point objects.                        */
/* -------------------------------------------------------------------- */
    for( int iGeom = 0; iGeom < nPointCount && eErr == OGRERR_NONE; iGeom++ )
    {
        OGRPoint *poPoint =
            new OGRPoint(paoPoints[iGeom].x, paoPoints[iGeom].y);
        if( bHasM )
        {
            if( padfM != NULL )
                poPoint->setM(padfM[iGeom]);
            else
                poPoint->setM(0.0);
        }
        if( bHasZ )
        {
            if( padfZ != NULL )
                poPoint->setZ(padfZ[iGeom]);
            else
                poPoint->setZ(0.0);
        }

        eErr = addGeometryDirectly( poPoint );
        if( eErr != OGRERR_NONE )
        {
            CPLFree( paoPoints );
            CPLFree( padfZ );
            CPLFree( padfM );
            delete poPoint;
            return eErr;
        }
    }

    CPLFree( paoPoints );
    CPLFree( padfZ );
    CPLFree( padfM );

    if( eErr != OGRERR_NONE )
        return eErr;

    *ppszInput = const_cast<char *>(pszInput);

    return OGRERR_NONE;
}

/************************************************************************/
/*                      importFromWkt_Bracketed()                       */
/*                                                                      */
/*      This operates similar to importFromWkt(), but reads a format    */
/*      with brackets around each point.  This is the form defined      */
/*      in the BNF of the SFSQL spec.  It is called from                */
/*      importFromWkt().                                                */
/************************************************************************/

OGRErr OGRMultiPoint::importFromWkt_Bracketed( char ** ppszInput,
                                               int bHasM, int bHasZ )

{
/* -------------------------------------------------------------------- */
/*      Skip MULTIPOINT keyword.                                        */
/* -------------------------------------------------------------------- */
    char szToken[OGR_WKT_TOKEN_MAX] = {};
    const char *pszInput = *ppszInput;
    pszInput = OGRWktReadToken( pszInput, szToken );

    if( bHasZ || bHasM )
    {
        // Skip Z, M or ZM.
        pszInput = OGRWktReadToken( pszInput, szToken );
    }

/* -------------------------------------------------------------------- */
/*      Read points till we get to the closing bracket.                 */
/* -------------------------------------------------------------------- */

    OGRRawPoint *paoPoints = NULL;
    double *padfZ = NULL;
    double *padfM = NULL;

    while( (pszInput = OGRWktReadToken( pszInput, szToken )) != NULL
           && (EQUAL(szToken, "(") || EQUAL(szToken, ",")) )
    {
        const char* pszNext = OGRWktReadToken( pszInput, szToken );
        if( EQUAL(szToken, "EMPTY") )
        {
            OGRPoint *poGeom = new OGRPoint(0.0, 0.0);
            poGeom->empty();
            const OGRErr eErr = addGeometryDirectly( poGeom );
            if( eErr != OGRERR_NONE )
            {
                CPLFree( paoPoints );
                delete poGeom;
                return eErr;
            }

            pszInput = pszNext;

            continue;
        }

        int flagsFromInput = flags;
        int nMaxPoint = 0;
        int nPointCount = 0;
        pszInput = OGRWktReadPointsM( pszInput, &paoPoints, &padfZ, &padfM,
                                      &flagsFromInput,
                                      &nMaxPoint, &nPointCount );

        if( pszInput == NULL || nPointCount != 1 )
        {
            CPLFree( paoPoints );
            CPLFree( padfZ );
            CPLFree( padfM );
            return OGRERR_CORRUPT_DATA;
        }
        if( (flagsFromInput & OGR_G_3D) && !(flags & OGR_G_3D) )
        {
            flags |= OGR_G_3D;
            bHasZ = TRUE;
        }
        if( (flagsFromInput & OGR_G_MEASURED) && !(flags & OGR_G_MEASURED) )
        {
            flags |= OGR_G_MEASURED;
            bHasM = TRUE;
        }

        OGRPoint *poPoint = new OGRPoint(paoPoints[0].x, paoPoints[0].y);
        if( bHasM )
        {
            if( padfM != NULL )
                poPoint->setM(padfM[0]);
            else
                poPoint->setM(0.0);
        }
        if( bHasZ )
        {
            if( padfZ != NULL )
                poPoint->setZ(padfZ[0]);
            else
                poPoint->setZ(0.0);
        }

        const OGRErr eErr = addGeometryDirectly( poPoint );
        if( eErr != OGRERR_NONE )
        {
            CPLFree( paoPoints );
            CPLFree( padfZ );
            CPLFree( padfM );
            delete poPoint;
            return eErr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    CPLFree( paoPoints );
    CPLFree( padfZ );
    CPLFree( padfM );

    if( !EQUAL(szToken, ")") )
        return OGRERR_CORRUPT_DATA;

    *ppszInput = const_cast<char *>(pszInput);

    return OGRERR_NONE;
}

/************************************************************************/
/*                         hasCurveGeometry()                           */
/************************************************************************/

OGRBoolean OGRMultiPoint::hasCurveGeometry( int /* bLookForNonLinear */ ) const
{
    return FALSE;
}

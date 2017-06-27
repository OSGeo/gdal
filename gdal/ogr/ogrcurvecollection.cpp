/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRCurveCollection class.
 * Author:   Even Rouault, even dot rouault at spatialys dot com
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
#include <cstring>

#include "ogr_core.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

CPL_CVSID("$Id$")

//! @cond Doxygen_Suppress

/************************************************************************/
/*                         OGRCurveCollection()                         */
/************************************************************************/

OGRCurveCollection::OGRCurveCollection()

{
    nCurveCount = 0;
    papoCurves = NULL;
}

/************************************************************************/
/*             OGRCurveCollection( const OGRCurveCollection& )          */
/************************************************************************/

/**
 * \brief Copy constructor.
 *
 * Note: before GDAL 2.1, only the default implementation of the constructor
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRCurveCollection::OGRCurveCollection( const OGRCurveCollection& other ) :
    nCurveCount(0),
    papoCurves(NULL)
{
    if( other.nCurveCount > 0 )
    {
        nCurveCount = other.nCurveCount;
        papoCurves = static_cast<OGRCurve **>(
            VSI_CALLOC_VERBOSE(sizeof(void*), nCurveCount));

        if( papoCurves )
        {
            for( int i = 0; i < nCurveCount; i++ )
            {
                OGRCurve *poCurve =
                    dynamic_cast<OGRCurve *>(other.papoCurves[i]->clone());
                if( poCurve == NULL )
                {
                    CPLError(CE_Fatal, CPLE_AppDefined,
                             "dynamic_cast failed.  Expected OGRCurve.");
                }
                papoCurves[i] = poCurve;
            }
        }
    }
}

/************************************************************************/
/*                         ~OGRCurveCollection()                        */
/************************************************************************/

OGRCurveCollection::~OGRCurveCollection()

{
    empty(NULL);
}

/************************************************************************/
/*                 operator=( const OGRCurveCollection& )               */
/************************************************************************/

/**
 * \brief Assignment operator.
 *
 * Note: before GDAL 2.1, only the default implementation of the operator
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRCurveCollection&
OGRCurveCollection::operator=( const OGRCurveCollection& other )
{
    if( this != &other)
    {
        empty(NULL);

        if( other.nCurveCount > 0 )
        {
            nCurveCount = other.nCurveCount;
            papoCurves = static_cast<OGRCurve **>(
                VSI_MALLOC2_VERBOSE(sizeof(void*), nCurveCount));

            if( papoCurves )
            {
                for( int i = 0; i < nCurveCount; i++ )
                {
                    OGRCurve *poCurve =
                        dynamic_cast<OGRCurve *>(other.papoCurves[i]->clone());
                    if( poCurve == NULL )
                    {
                        CPLError(CE_Fatal, CPLE_AppDefined,
                                 "dynamic_cast failed.  Expected OGRCurve.");
                    }

                    papoCurves[i] = poCurve;
                }
            }
        }
    }
    return *this;
}

/************************************************************************/
/*                              WkbSize()                               */
/************************************************************************/

int OGRCurveCollection::WkbSize() const
{
    int nSize = 9;

    for( int i = 0; i < nCurveCount; i++ )
    {
        nSize += papoCurves[i]->WkbSize();
    }

    return nSize;
}

/************************************************************************/
/*                          addCurveDirectly()                          */
/************************************************************************/

OGRErr OGRCurveCollection::addCurveDirectly( OGRGeometry* poGeom,
                                             OGRCurve* poCurve,
                                             int bNeedRealloc )
{
    if( poGeom->Is3D() && !poCurve->Is3D() )

        poCurve->set3D(TRUE);

    if( poGeom->IsMeasured() && !poCurve->IsMeasured() )

        poCurve->setMeasured(TRUE);

    if( !poGeom->Is3D() && poCurve->Is3D() )

        poGeom->set3D(TRUE);

    if( !poGeom->IsMeasured() && poCurve->IsMeasured() )

        poGeom->setMeasured(TRUE);

    if( bNeedRealloc )
    {
        OGRCurve** papoNewCurves = static_cast<OGRCurve **>(
            VSI_REALLOC_VERBOSE(papoCurves,
                                sizeof(OGRCurve*) * (nCurveCount + 1)));
        if( papoNewCurves == NULL )
            return OGRERR_FAILURE;
        papoCurves = papoNewCurves;
    }

    papoCurves[nCurveCount] = poCurve;

    nCurveCount++;

    return OGRERR_NONE;
}

/************************************************************************/
/*                        importPreambuleFromWkb()                      */
/************************************************************************/

OGRErr OGRCurveCollection::importPreambuleFromWkb( OGRGeometry* poGeom,
                                                   const unsigned char * pabyData,
                                                   int& nSize,
                                                   int& nDataOffset,
                                                   OGRwkbByteOrder& eByteOrder,
                                                   int nMinSubGeomSize,
                                                   OGRwkbVariant eWkbVariant )
{
    OGRErr eErr = poGeom->importPreambuleOfCollectionFromWkb(
                                                        pabyData,
                                                        nSize,
                                                        nDataOffset,
                                                        eByteOrder,
                                                        nMinSubGeomSize,
                                                        nCurveCount,
                                                        eWkbVariant );
    if( eErr != OGRERR_NONE )
        return eErr;

    // coverity[tainted_data]
    papoCurves = static_cast<OGRCurve **>(
        VSI_CALLOC_VERBOSE(sizeof(void*), nCurveCount));
    if( nCurveCount != 0 && papoCurves == NULL )
    {
        nCurveCount = 0;
        return OGRERR_NOT_ENOUGH_MEMORY;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                       importBodyFromWkb()                            */
/************************************************************************/

OGRErr OGRCurveCollection::importBodyFromWkb(
    OGRGeometry* poGeom,
    const unsigned char * pabyData,
    int nSize,
    int bAcceptCompoundCurve,
    OGRErr (*pfnAddCurveDirectlyFromWkb)(OGRGeometry* poGeom,
                                         OGRCurve* poCurve),
    OGRwkbVariant eWkbVariant,
    int& nBytesConsumedOut )
{
    nBytesConsumedOut = -1;
/* -------------------------------------------------------------------- */
/*      Get the Geoms.                                                  */
/* -------------------------------------------------------------------- */
    const int nIter = nCurveCount;
    nCurveCount = 0;
    int nDataOffset = 0;
    for( int iGeom = 0; iGeom < nIter; iGeom++ )
    {
        OGRGeometry* poSubGeom = NULL;

        // Parses sub-geometry.
        const unsigned char* pabySubData = pabyData + nDataOffset;
        if( nSize < 9 && nSize != -1 )
            return OGRERR_NOT_ENOUGH_DATA;

        OGRwkbGeometryType eFlattenSubGeomType = wkbUnknown;
        if( OGRReadWKBGeometryType( pabySubData, eWkbVariant,
                                    &eFlattenSubGeomType ) != OGRERR_NONE )
            return OGRERR_FAILURE;
        eFlattenSubGeomType = wkbFlatten(eFlattenSubGeomType);

        OGRErr eErr = OGRERR_NONE;
        int nSubGeomBytesConsumedOut = -1;
        if( (eFlattenSubGeomType != wkbCompoundCurve &&
             OGR_GT_IsCurve(eFlattenSubGeomType)) ||
            (bAcceptCompoundCurve && eFlattenSubGeomType == wkbCompoundCurve) )
        {
            eErr = OGRGeometryFactory::
                createFromWkb( pabySubData, NULL,
                               &poSubGeom, nSize, eWkbVariant,
                               nSubGeomBytesConsumedOut );
        }
        else
        {
            CPLDebug(
                "OGR",
                "Cannot add geometry of type (%d) to geometry of type (%d)",
                eFlattenSubGeomType, poGeom->getGeometryType());
            return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
        }

        if( eErr == OGRERR_NONE )
        {
            CPLAssert( nSubGeomBytesConsumedOut > 0 );
            if( nSize != -1 )
            {
                CPLAssert( nSize >= nSubGeomBytesConsumedOut );
                nSize -= nSubGeomBytesConsumedOut;
            }

            nDataOffset += nSubGeomBytesConsumedOut;

            OGRCurve *poCurve = dynamic_cast<OGRCurve *>(poSubGeom);
            if( poCurve == NULL )
            {
                CPLError(CE_Fatal, CPLE_AppDefined,
                         "dynamic_cast failed.  Expected OGRCurve.");
            }
            eErr = pfnAddCurveDirectlyFromWkb(poGeom, poCurve);
        }
        if( eErr != OGRERR_NONE )
        {
            delete poSubGeom;
            return eErr;
        }

    }
    nBytesConsumedOut = nDataOffset;

    return OGRERR_NONE;
}

/************************************************************************/
/*                            exportToWkt()                             */
/************************************************************************/

OGRErr OGRCurveCollection::exportToWkt( const OGRGeometry* poGeom,
                                        char ** ppszDstText ) const

{
    if( nCurveCount == 0 )
    {
        CPLString osEmpty;
        if( poGeom->Is3D() && poGeom->IsMeasured() )
            osEmpty.Printf("%s ZM EMPTY", poGeom->getGeometryName());
        else if( poGeom->IsMeasured() )
            osEmpty.Printf("%s M EMPTY", poGeom->getGeometryName());
        else if( poGeom->Is3D() )
            osEmpty.Printf("%s Z EMPTY", poGeom->getGeometryName());
        else
            osEmpty.Printf("%s EMPTY", poGeom->getGeometryName());
        *ppszDstText = CPLStrdup(osEmpty);
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Build a list of strings containing the stuff for each Geom.     */
/* -------------------------------------------------------------------- */
    char **papszGeoms =
        static_cast<char **>(CPLCalloc(sizeof(char *), nCurveCount));
    OGRErr eErr = OGRERR_NONE;
    size_t nCumulativeLength = 0;

    for( int iGeom = 0; iGeom < nCurveCount; iGeom++ )
    {
        eErr = papoCurves[iGeom]->exportToWkt( &(papszGeoms[iGeom]),
                                               wkbVariantIso );
        if( eErr != OGRERR_NONE )
            goto error;

        nCumulativeLength += strlen(papszGeoms[iGeom]);
    }

/* -------------------------------------------------------------------- */
/*      Allocate the right amount of space for the aggregated string    */
/* -------------------------------------------------------------------- */
    *ppszDstText = static_cast<char *>(
        VSI_MALLOC_VERBOSE(nCumulativeLength + nCurveCount +
                           strlen(poGeom->getGeometryName()) + 10));

    if( *ppszDstText == NULL )
    {
        eErr = OGRERR_NOT_ENOUGH_MEMORY;
        goto error;
    }

/* -------------------------------------------------------------------- */
/*      Build up the string, freeing temporary strings as we go.        */
/* -------------------------------------------------------------------- */
    strcpy( *ppszDstText, poGeom->getGeometryName() );
    if( poGeom->Is3D() && poGeom->IsMeasured() )
        strcat( *ppszDstText, " ZM" );
    else if( poGeom->IsMeasured() )
        strcat( *ppszDstText, " M" );
    else if( poGeom->Is3D() )
        strcat( *ppszDstText, " Z" );
    strcat( *ppszDstText, " (" );
    nCumulativeLength = strlen(*ppszDstText);

    for( int iGeom = 0; iGeom < nCurveCount; iGeom++ )
    {
        if( iGeom > 0 )
            (*ppszDstText)[nCumulativeLength++] = ',';

        // We must strip the explicit "LINESTRING " prefix.
        size_t nSkip = 0;
        if( !papoCurves[iGeom]->IsEmpty() &&
            STARTS_WITH_CI(papszGeoms[iGeom], "LINESTRING ") )
        {
            nSkip = strlen("LINESTRING ");
            if( STARTS_WITH_CI(papszGeoms[iGeom] + nSkip, "ZM ") )
                nSkip += 3;
            else if( STARTS_WITH_CI(papszGeoms[iGeom] + nSkip, "M ") )
                nSkip += 2;
            else if( STARTS_WITH_CI(papszGeoms[iGeom] + nSkip, "Z ") )
                nSkip += 2;
        }

        const size_t nGeomLength = strlen(papszGeoms[iGeom] + nSkip);
        memcpy( *ppszDstText + nCumulativeLength,
                papszGeoms[iGeom] + nSkip,
                nGeomLength );
        nCumulativeLength += nGeomLength;
        VSIFree( papszGeoms[iGeom] );
    }

    (*ppszDstText)[nCumulativeLength++] = ')';
    (*ppszDstText)[nCumulativeLength] = '\0';

    CPLFree( papszGeoms );

    return OGRERR_NONE;

error:
    for( int iGeom = 0; iGeom < nCurveCount; iGeom++ )
        CPLFree( papszGeoms[iGeom] );
    CPLFree( papszGeoms );
    return eErr;
}

/************************************************************************/
/*                            exportToWkb()                             */
/************************************************************************/

OGRErr OGRCurveCollection::exportToWkb( const OGRGeometry* poGeom,
                                        OGRwkbByteOrder eByteOrder,
                                        unsigned char * pabyData,
                                        OGRwkbVariant eWkbVariant ) const
{
/* -------------------------------------------------------------------- */
/*      Set the byte order.                                             */
/* -------------------------------------------------------------------- */
    pabyData[0] = DB2_V72_UNFIX_BYTE_ORDER((unsigned char) eByteOrder);

/* -------------------------------------------------------------------- */
/*      Set the geometry feature type, ensuring that 3D flag is         */
/*      preserved.                                                      */
/* -------------------------------------------------------------------- */
    GUInt32 nGType = poGeom->getIsoGeometryType();
    if( eWkbVariant == wkbVariantPostGIS1 )
    {
        const bool bIs3D = wkbHasZ(static_cast<OGRwkbGeometryType>(nGType));
        nGType = wkbFlatten(nGType);
        if( nGType == wkbCurvePolygon )
            nGType = POSTGIS15_CURVEPOLYGON;
        if( bIs3D )
            // Explicitly set wkb25DBit.
            // TODO(schwehr): Clean up the casting.
            nGType = (OGRwkbGeometryType)(nGType | wkb25DBitInternalUse);
    }

    if( OGR_SWAP( eByteOrder ) )
    {
        nGType = CPL_SWAP32(nGType);
    }

    memcpy( pabyData + 1, &nGType, 4 );

/* -------------------------------------------------------------------- */
/*      Copy in the raw data.                                           */
/* -------------------------------------------------------------------- */
    if( OGR_SWAP( eByteOrder ) )
    {
        const int nCount = CPL_SWAP32( nCurveCount );
        memcpy( pabyData+5, &nCount, 4 );
    }
    else
    {
        memcpy( pabyData+5, &nCurveCount, 4 );
    }

    // TODO(schwehr): Where do these 9 values come from?
    int nOffset = 9;

/* ==================================================================== */
/*      Serialize each of the Geoms.                                    */
/* ==================================================================== */
    for( int iGeom = 0; iGeom < nCurveCount; iGeom++ )
    {
        papoCurves[iGeom]->exportToWkb( eByteOrder, pabyData + nOffset,
                                        eWkbVariant );

        nOffset += papoCurves[iGeom]->WkbSize();
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                               empty()                                */
/************************************************************************/

void OGRCurveCollection::empty( OGRGeometry* poGeom )
{
    if( papoCurves != NULL )
    {
        for( int i = 0; i < nCurveCount; i++ )
        {
            delete papoCurves[i];
        }
        CPLFree( papoCurves );
    }

    nCurveCount = 0;
    papoCurves = NULL;
    if( poGeom )
        poGeom->setCoordinateDimension(2);
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRCurveCollection::getEnvelope( OGREnvelope * psEnvelope ) const
{
    OGREnvelope3D oEnv3D;
    getEnvelope(&oEnv3D);
    psEnvelope->MinX = oEnv3D.MinX;
    psEnvelope->MinY = oEnv3D.MinY;
    psEnvelope->MaxX = oEnv3D.MaxX;
    psEnvelope->MaxY = oEnv3D.MaxY;
}

/************************************************************************/
/*                            getEnvelope()                             */
/************************************************************************/

void OGRCurveCollection::getEnvelope( OGREnvelope3D * psEnvelope ) const
{
    OGREnvelope3D oGeomEnv;
    bool bExtentSet = false;

    *psEnvelope = OGREnvelope3D();
    for( int iGeom = 0; iGeom < nCurveCount; iGeom++ )
    {
        if( !papoCurves[iGeom]->IsEmpty() )
        {
            bExtentSet = true;
            papoCurves[iGeom]->getEnvelope( &oGeomEnv );
            psEnvelope->Merge( oGeomEnv );
        }
    }

    if( !bExtentSet )
    {
        // To be backward compatible when called on empty geom
        psEnvelope->MinX = 0.0;
        psEnvelope->MinY = 0.0;
        psEnvelope->MinZ = 0.0;
        psEnvelope->MaxX = 0.0;
        psEnvelope->MaxY = 0.0;
        psEnvelope->MaxZ = 0.0;
    }
}

/************************************************************************/
/*                               IsEmpty()                              */
/************************************************************************/

OGRBoolean OGRCurveCollection::IsEmpty() const
{
    for( int iGeom = 0; iGeom < nCurveCount; iGeom++ )
    {
        if( !papoCurves[iGeom]->IsEmpty() )
            return FALSE;
    }
    return TRUE;
}

/************************************************************************/
/*                               Equals()                                */
/************************************************************************/

OGRBoolean OGRCurveCollection::Equals( OGRCurveCollection *poOCC ) const
{
    if( getNumCurves() != poOCC->getNumCurves() )
        return FALSE;

    // Should eventually test the SRS.

    for( int iGeom = 0; iGeom < nCurveCount; iGeom++ )
    {
        if( !getCurve(iGeom)->Equals(poOCC->getCurve(iGeom)) )
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                       setCoordinateDimension()                       */
/************************************************************************/

void OGRCurveCollection::setCoordinateDimension( OGRGeometry* poGeom,
                                                 int nNewDimension )
{
    for( int iGeom = 0; iGeom < nCurveCount; iGeom++ )
    {
        papoCurves[iGeom]->setCoordinateDimension( nNewDimension );
    }

    poGeom->OGRGeometry::setCoordinateDimension( nNewDimension );
}

void OGRCurveCollection::set3D( OGRGeometry* poGeom, OGRBoolean bIs3D )
{
    for( int iGeom = 0; iGeom < nCurveCount; iGeom++ )
    {
        papoCurves[iGeom]->set3D( bIs3D );
    }

    poGeom->OGRGeometry::set3D( bIs3D );
}

void OGRCurveCollection::setMeasured( OGRGeometry* poGeom,
                                      OGRBoolean bIsMeasured )
{
    for( int iGeom = 0; iGeom < nCurveCount; iGeom++ )
    {
        papoCurves[iGeom]->setMeasured( bIsMeasured );
    }

    poGeom->OGRGeometry::setMeasured( bIsMeasured );
}

/************************************************************************/
/*                          getNumCurves()                              */
/************************************************************************/

int OGRCurveCollection::getNumCurves() const
{
    return nCurveCount;
}

/************************************************************************/
/*                           getCurve()                                 */
/************************************************************************/

OGRCurve *OGRCurveCollection::getCurve( int i )
{
    if( i < 0 || i >= nCurveCount )
        return NULL;
    return papoCurves[i];
}

/************************************************************************/
/*                           getCurve()                                 */
/************************************************************************/

const OGRCurve *OGRCurveCollection::getCurve( int i ) const
{
    if( i < 0 || i >= nCurveCount )
        return NULL;
    return papoCurves[i];
}

/************************************************************************/
/*                           stealCurve()                               */
/************************************************************************/

OGRCurve* OGRCurveCollection::stealCurve( int i )
{
    if( i < 0 || i >= nCurveCount )
        return NULL;
    OGRCurve* poRet = papoCurves[i];
    if( i < nCurveCount - 1 )
    {
        memmove(papoCurves + i,
                papoCurves + i + 1,
                (nCurveCount - i - 1) * sizeof(OGRCurve*));
    }
    nCurveCount--;
    return poRet;
}

/************************************************************************/
/*                             transform()                              */
/************************************************************************/

OGRErr OGRCurveCollection::transform( OGRGeometry* poGeom,
                                      OGRCoordinateTransformation *poCT )
{
    for( int iGeom = 0; iGeom < nCurveCount; iGeom++ )
    {
        const OGRErr eErr = papoCurves[iGeom]->transform( poCT );
        if( eErr != OGRERR_NONE )
        {
            if( iGeom != 0 )
            {
                CPLDebug("OGR",
                         "OGRCurveCollection::transform() failed for a "
                         "geometry other than the first, meaning some "
                         "geometries are transformed and some are not!" );

                return OGRERR_FAILURE;
            }

            return eErr;
        }
    }

    poGeom->assignSpatialReference( poCT->GetTargetCS() );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            flattenTo2D()                             */
/************************************************************************/

void OGRCurveCollection::flattenTo2D(OGRGeometry* poGeom)
{
    for( int i = 0; i < nCurveCount; i++ )
        papoCurves[i]->flattenTo2D();

    poGeom->setCoordinateDimension(2);
}

/************************************************************************/
/*                              segmentize()                            */
/************************************************************************/

void OGRCurveCollection::segmentize(double dfMaxLength)
{
    for( int i = 0; i < nCurveCount; i++ )
        papoCurves[i]->segmentize(dfMaxLength);
}

/************************************************************************/
/*                               swapXY()                               */
/************************************************************************/

void OGRCurveCollection::swapXY()
{
    for( int i = 0; i < nCurveCount; i++ )
        papoCurves[i]->swapXY();
}

/************************************************************************/
/*                         hasCurveGeometry()                           */
/************************************************************************/

OGRBoolean OGRCurveCollection::hasCurveGeometry(int bLookForNonLinear) const
{
    for( int i = 0; i < nCurveCount; i++ )
    {
        if( papoCurves[i]->hasCurveGeometry(bLookForNonLinear) )
            return TRUE;
    }
    return FALSE;
}

//! @endcond
